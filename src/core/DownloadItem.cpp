#include "DownloadItem.h"

#include <QDateTime>
#include <QFileInfo>
#include <QMetaObject>

namespace muld_gui {

DownloadItem::DownloadItem(const QString& url, const QString& savePath, QObject* parent)
    : QObject(parent) {
    m_info.url = url;
    m_info.save_path = savePath;
    m_info.filename = QFileInfo(savePath).fileName();
    m_info.state = DownloadState::Queued;
}

DownloadItem::~DownloadItem() {
    if (m_handler.has_value() && m_info.state == DownloadState::Downloading) {
        m_handler->Cancel();
    }
}

void DownloadItem::setHandler(muld::DownloadHandler handler) {
    m_handler = std::move(handler);

    muld::DownloadCallbacks callbacks;
    callbacks.on_progress = [this](const muld::DownloadProgress& progress) {
        QMetaObject::invokeMethod(this, [this, progress]() {
            m_progress.downloaded_bytes = progress.downloaded_bytes;
            m_progress.total_bytes = progress.total_bytes;
            m_progress.speed_bytes_per_sec = static_cast<double>(progress.speed_bytes_per_sec);
            m_progress.eta_seconds = static_cast<int>(progress.eta_seconds);
            m_progress.percentage = static_cast<int>(progress.percentage);
            emit progressUpdated(m_progress);
        }, Qt::QueuedConnection);
    };
    callbacks.on_chunk_progress = [this](const muld::ChunkProgressEvent& event) {
        QMetaObject::invokeMethod(this, [this, event]() {
            if (event.chunk_id >= m_allChunks.size()) {
                m_allChunks.resize(event.chunk_id + 1);
            }
            m_allChunks[event.chunk_id] = {event.downloaded_bytes, event.total_bytes};
            if (!event.finished) {
                m_activeChunks[static_cast<int>(event.chunk_id)] = m_allChunks[event.chunk_id];
            } else {
                m_activeChunks.erase(static_cast<int>(event.chunk_id));
            }

            QString summary = QStringLiteral("%1 active chunks").arg(m_activeChunks.size());
            if (!m_activeChunks.empty()) {
                summary += QStringLiteral(": ");
                int shown = 0;
                for (const auto& pair : m_activeChunks) {
                    const int id = pair.first;
                    const auto& chunk = pair.second;
                    const int pct = chunk.total_bytes > 0
                                        ? static_cast<int>((chunk.downloaded_bytes * 100) / chunk.total_bytes)
                                        : 0;
                    summary += QStringLiteral("[%1:%2%] ").arg(id).arg(pct);
                    if (++shown >= 4) {
                        summary += QStringLiteral("...");
                        break;
                    }
                }
            }
            m_chunkInfo = summary.trimmed();
            emit progressUpdated(m_progress);
        }, Qt::QueuedConnection);
    };
    callbacks.on_state_change = [this](muld::DownloadState state) {
        QMetaObject::invokeMethod(this, [this, state]() {
            switch (state) {
            case muld::DownloadState::Queued:
                setState(DownloadState::Queued);
                break;
            case muld::DownloadState::Downloading:
                setState(DownloadState::Downloading);
                break;
            case muld::DownloadState::Paused:
                setState(DownloadState::Paused);
                break;
            case muld::DownloadState::Completed:
                m_progress.percentage = 100;
                m_progress.eta_seconds = 0;
                m_completedAtText = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
                setState(DownloadState::Completed);
                emit progressUpdated(m_progress);
                emit completed();
                break;
            case muld::DownloadState::Failed:
                setState(DownloadState::Failed);
                emit failed(m_errorMessage);
                break;
            case muld::DownloadState::Canceled:
                setState(DownloadState::Cancelled);
                break;
            case muld::DownloadState::Initialized:
                break;
            }
        }, Qt::QueuedConnection);
    };
    callbacks.on_finish = [this]() {
        QMetaObject::invokeMethod(this, [this]() {
            m_progress.percentage = 100;
            m_progress.eta_seconds = 0;
            m_completedAtText = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
            setState(DownloadState::Completed);
            emit progressUpdated(m_progress);
            emit completed();
        }, Qt::QueuedConnection);
    };
    callbacks.on_error = [this](muld::MuldError error) {
        const QString message = QString::fromStdString(error.GetFormattedMessage());
        const auto errorCode = error.code;
        QMetaObject::invokeMethod(this, [this, message, errorCode]() {
            if (errorCode == muld::ErrorCode::InvalidState) {
                m_errorMessage = message;
                return;
            }
            m_errorMessage = message;
            setState(DownloadState::Failed);
            emit failed(message);
        }, Qt::QueuedConnection);
    };

    m_handler->AttachHandlerCallbacks(callbacks);
}

void DownloadItem::restoreSnapshot(DownloadState state,
                                   const DownloadProgress& progress,
                                   const QString& error,
                                   const QString& completedAt,
                                   const QString& chunkInfo) {
    m_progress = progress;
    m_errorMessage = error;
    m_completedAtText = completedAt;
    m_chunkInfo = chunkInfo;
    m_info.state = state;
}

void DownloadItem::start() {
    if (!m_handler.has_value() || m_info.state != DownloadState::Queued) {
        return;
    }
}

void DownloadItem::pause() {
    if (m_handler.has_value() && m_info.state == DownloadState::Downloading) {
        m_handler->Pause();
    }
}

void DownloadItem::resume() {
    if (m_handler.has_value() && m_info.state == DownloadState::Paused) {
        m_handler->Resume();
        
    }
}

void DownloadItem::cancel() {
    if (m_handler.has_value() && (m_info.state == DownloadState::Downloading ||
                                  m_info.state == DownloadState::Paused ||
                                  m_info.state == DownloadState::Queued)) {
        m_handler->Cancel();
    }
}

void DownloadItem::setState(DownloadState state) {
    if (m_info.state != state) {
        m_info.state = state;
        emit stateChanged(state);
    }
}

void DownloadItem::updateProgress(uint64_t downloaded, uint64_t total, double speed) {
    m_progress.downloaded_bytes = downloaded;
    m_progress.total_bytes = total;
    m_progress.speed_bytes_per_sec = speed;

    if (total > 0) {
        m_progress.percentage = static_cast<int>((downloaded * 100) / total);
    }

    emit progressUpdated(m_progress);
}

}  // namespace muld_gui
