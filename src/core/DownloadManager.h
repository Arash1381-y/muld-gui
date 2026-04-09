#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include <memory>
#include <cstddef>
#include <muld/muld.h>

#include "DownloadItem.h"

namespace muld_gui {

class DownloadManager : public QObject {
    Q_OBJECT

public:
    explicit DownloadManager(QObject* parent = nullptr);
    ~DownloadManager();

    DownloadItem* addDownload(const QString& url, const QString& savePath,
                              const QString& filename = {}, std::size_t speedLimitBytesPerSec = 0);
    int count() const { return m_downloads.size(); }
    DownloadItem* downloadAt(int index) const {
        return (index >= 0 && index < m_downloads.size()) ? m_downloads.at(index) : nullptr;
    }
    void removeDownload(int index);
    void pauseAll();
    void resumeAll();
    void cancelAll();

    const QVector<DownloadItem*>& downloads() const { return m_downloads; }
    QString lastError() const { return m_lastError; }

signals:
    void downloadAdded(DownloadItem* item, int index);
    void downloadRemoved(int index);

private:
    void trackItem(DownloadItem* item);
    QString stateFilePath() const;
    void saveState() const;
    void loadState();
    QString resolveDestinationPath(const QString& requestedPath,
                                   const QString& filename,
                                   const QString& url,
                                   QString* resolvedOutputDir) const;
    std::unique_ptr<muld::MuldDownloadManager> m_manager;
    QVector<DownloadItem*> m_downloads;
    QString m_lastError;
};

}
