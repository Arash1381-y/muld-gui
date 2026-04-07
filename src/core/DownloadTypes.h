#pragma once

#include <QString>
#include <cstdint>

namespace muld_gui {

enum class DownloadState {
    Queued,
    Downloading,
    Paused,
    Completed,
    Failed,
    Cancelled
};

struct DownloadProgress {
    uint64_t downloaded_bytes = 0;
    uint64_t total_bytes = 0;
    uint64_t speed_bytes_per_sec = 0.0;
    uint64_t eta_seconds = 0;
    float percentage = 0;
};

struct DownloadInfo {
    QString url;
    QString filename;
    QString save_path;
    uint64_t file_size = 0;
    DownloadState state = DownloadState::Queued;
};

} // namespace muld_gui
