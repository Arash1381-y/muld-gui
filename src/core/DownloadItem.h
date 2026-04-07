// src/core/DownloadItem.h
#pragma once

#include "DownloadTypes.h"
#include <QObject>
#include <optional>
#include <unordered_map>
#include <vector>
#include <muld/muld.h>

namespace muld_gui {

class DownloadItem : public QObject {
    Q_OBJECT

public:
    explicit DownloadItem(const QString& url, 
                         const QString& savePath,
                         QObject* parent = nullptr);
    ~DownloadItem();

    // Getters
    QString url() const { return m_info.url; }
    QString filename() const { return m_info.filename; }
    QString savePath() const { return m_info.save_path; }
    DownloadState state() const { return m_info.state; }
    DownloadProgress progress() const { return m_progress; }
    QString errorMessage() const { return m_errorMessage; }
    QString chunkInfo() const { return m_chunkInfo; }
    QString completedAtText() const { return m_completedAtText; }
    bool hasHandler() const { return m_handler.has_value(); }

    // Control
    void start();
    void pause();
    void resume();
    void cancel();

    // Internal - called by DownloadManager
    void setHandler(muld::DownloadHandler handler);
    void restoreSnapshot(DownloadState state,
                         const DownloadProgress& progress,
                         const QString& error,
                         const QString& completedAt,
                         const QString& chunkInfo);

signals:
    void stateChanged(DownloadState state);
    void progressUpdated(const DownloadProgress& progress);
    void completed();
    void failed(const QString& error);

private:
    void setState(DownloadState state);
    void updateProgress(uint64_t downloaded, uint64_t total, double speed);
    
    DownloadInfo m_info;
    DownloadProgress m_progress;
    QString m_errorMessage;
    QString m_chunkInfo;
    QString m_completedAtText;
    std::vector<muld::ChunkProgress> m_allChunks;
    std::unordered_map<int, muld::ChunkProgress> m_activeChunks;
    std::optional<muld::DownloadHandler> m_handler;
};

} // namespace muld_gui
