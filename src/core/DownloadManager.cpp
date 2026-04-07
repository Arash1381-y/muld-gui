#include "DownloadManager.h"

#include <QDir>
#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUrl>

namespace muld_gui {

DownloadManager::DownloadManager(QObject* parent)
    : QObject(parent)
{
    muld::MuldConfig config;
    m_manager = std::make_unique<muld::MuldDownloadManager>(config);
    loadState();
}

DownloadManager::~DownloadManager() {
    for (auto* d : m_downloads) {
        delete d;
    }
}

DownloadItem* DownloadManager::addDownload(const QString& url, const QString& savePath,
                                           const QString& filename, int connections) {
    m_lastError.clear();
    if (!m_manager) {
        m_lastError = QStringLiteral("Download manager not initialized");
        return nullptr;
    }
    if (url.trimmed().isEmpty()) {
        m_lastError = QStringLiteral("URL is empty");
        return nullptr;
    }

    QString outputDir;
    const QString resolvedPath = resolveDestinationPath(savePath, filename, url, &outputDir);
    if (resolvedPath.isEmpty() || outputDir.isEmpty()) {
        if (m_lastError.isEmpty()) {
            m_lastError = QStringLiteral("Invalid destination path");
        }
        return nullptr;
    }

    QDir dir(outputDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        m_lastError = QStringLiteral("Cannot create destination directory: %1").arg(outputDir);
        return nullptr;
    }

    auto* item = new DownloadItem(url, resolvedPath, this);

    for (DownloadItem* existing : m_downloads) {
        if (QFileInfo(existing->savePath()).absoluteFilePath() == QFileInfo(resolvedPath).absoluteFilePath()) {
            m_lastError = QStringLiteral("Destination already tracked: %1").arg(resolvedPath);
            delete item;
            return nullptr;
        }
    }

    const QByteArray urlUtf8 = url.toUtf8();
    const QByteArray destinationUtf8 = resolvedPath.toUtf8();
    muld::MuldRequest req{
        .url = urlUtf8.constData(),
        .destination = destinationUtf8.constData(),
        .max_connections = connections > 0 ? connections : 1,
    };

    auto resp = m_manager->Download(req);
    if (!resp || !resp.handler.has_value()) {
        m_lastError = QString::fromStdString(resp.error.GetFormattedMessage());
        if (m_lastError.isEmpty() || m_lastError == QStringLiteral("No error")) {
            m_lastError = QStringLiteral("Backend returned no handler");
        }
        delete item;
        return nullptr;
    }

    item->setHandler(std::move(resp.handler.value()));

    m_downloads.append(item);
    trackItem(item);
    const int index = m_downloads.size() - 1;

    emit downloadAdded(item, index);

    item->start();
    saveState();

    return item;
}

void DownloadManager::removeDownload(int index) {
    if (index < 0 || index >= m_downloads.size()) {
        return;
    }

    auto* item = m_downloads.at(index);
    item->cancel();
    m_downloads.removeAt(index);
    emit downloadRemoved(index);
    delete item;
    saveState();
}

void DownloadManager::pauseAll() {
    for (auto* item : m_downloads) {
        item->pause();
    }
}

void DownloadManager::resumeAll() {
    for (auto* item : m_downloads) {
        item->resume();
    }
}

void DownloadManager::cancelAll() {
    for (auto* item : m_downloads) {
        item->cancel();
    }
}

void DownloadManager::trackItem(DownloadItem* item) {
    connect(item, &DownloadItem::progressUpdated, this, [this]() {
        saveState();
    });
    connect(item, &DownloadItem::stateChanged, this, [this]() {
        saveState();
    });
    connect(item, &DownloadItem::failed, this, [this](const QString&) {
        saveState();
    });
}

QString DownloadManager::stateFilePath() const {
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("muld-gui.dat"));
}

void DownloadManager::saveState() const {
    QJsonArray entries;
    for (DownloadItem* item : m_downloads) {
        const auto prog = item->progress();
        QJsonObject obj;
        obj.insert(QStringLiteral("url"), item->url());
        obj.insert(QStringLiteral("savePath"), item->savePath());
        obj.insert(QStringLiteral("state"), static_cast<int>(item->state()));
        obj.insert(QStringLiteral("downloaded"), static_cast<qint64>(prog.downloaded_bytes));
        obj.insert(QStringLiteral("total"), static_cast<qint64>(prog.total_bytes));
        obj.insert(QStringLiteral("speed"), static_cast<double>(prog.speed_bytes_per_sec));
        obj.insert(QStringLiteral("eta"), static_cast<qint64>(prog.eta_seconds));
        obj.insert(QStringLiteral("percent"), prog.percentage);
        obj.insert(QStringLiteral("error"), item->errorMessage());
        obj.insert(QStringLiteral("chunkInfo"), item->chunkInfo());
        obj.insert(QStringLiteral("completedAt"), item->completedAtText());
        entries.append(obj);
    }

    QFile out(stateFilePath());
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    out.write(QJsonDocument(entries).toJson(QJsonDocument::Compact));
}

void DownloadManager::loadState() {
    QFile in(stateFilePath());
    if (!in.exists() || !in.open(QIODevice::ReadOnly)) {
        return;
    }
    const auto doc = QJsonDocument::fromJson(in.readAll());
    if (!doc.isArray()) {
        return;
    }

    const QJsonArray entries = doc.array();
    for (const QJsonValue& value : entries) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject obj = value.toObject();
        const QString url = obj.value(QStringLiteral("url")).toString();
        const QString savePath = obj.value(QStringLiteral("savePath")).toString();
        if (url.isEmpty() || savePath.isEmpty()) {
            continue;
        }

        auto* item = new DownloadItem(url, savePath, this);
        DownloadProgress prog;
        prog.downloaded_bytes = static_cast<uint64_t>(obj.value(QStringLiteral("downloaded")).toVariant().toULongLong());
        prog.total_bytes = static_cast<uint64_t>(obj.value(QStringLiteral("total")).toVariant().toULongLong());
        prog.speed_bytes_per_sec = obj.value(QStringLiteral("speed")).toDouble();
        prog.eta_seconds = obj.value(QStringLiteral("eta")).toInt();
        prog.percentage = obj.value(QStringLiteral("percent")).toInt();

        DownloadState state = static_cast<DownloadState>(obj.value(QStringLiteral("state")).toInt());
        item->restoreSnapshot(state,
                              prog,
                              obj.value(QStringLiteral("error")).toString(),
                              obj.value(QStringLiteral("completedAt")).toString(),
                              obj.value(QStringLiteral("chunkInfo")).toString());
        m_downloads.append(item);
        trackItem(item);
        const int idx = m_downloads.size() - 1;
        emit downloadAdded(item, idx);
    }
}

QString DownloadManager::resolveDestinationPath(const QString& requestedPath,
                                                const QString& filename,
                                                const QString& url,
                                                QString* resolvedOutputDir) const {
    QString outputDir = requestedPath.trimmed();
    if (outputDir.isEmpty()) {
        outputDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    }
    if (outputDir.isEmpty()) {
        outputDir = QDir::homePath();
    }
    outputDir = QFileInfo(outputDir).absoluteFilePath();

    QString finalFilename = filename.trimmed();
    if (finalFilename.isEmpty()) {
        const QUrl parsed(url);
        QString fromUrl = QFileInfo(parsed.path()).fileName();
        if (fromUrl.isEmpty()) {
            fromUrl = QStringLiteral("download_%1.bin")
                          .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
        }
        finalFilename = fromUrl;
    }
    if (finalFilename.isEmpty()) {
        return QString();
    }

    if (resolvedOutputDir) {
        *resolvedOutputDir = outputDir;
    }
    return QDir(outputDir).filePath(finalFilename);
}

}
