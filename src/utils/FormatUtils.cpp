#include "FormatUtils.h"

namespace muld_gui::FormatUtils {

QString formatBytes(quint64 bytes) {
    static constexpr quint64 kKiB = 1024;
    static constexpr quint64 kMiB = 1024 * 1024;
    static constexpr quint64 kGiB = 1024 * 1024 * 1024;

    if (bytes >= kGiB) {
        return QString::number(static_cast<double>(bytes) / static_cast<double>(kGiB), 'f', 2) + " GB";
    }
    if (bytes >= kMiB) {
        return QString::number(static_cast<double>(bytes) / static_cast<double>(kMiB), 'f', 2) + " MB";
    }
    if (bytes >= kKiB) {
        return QString::number(static_cast<double>(bytes) / static_cast<double>(kKiB), 'f', 2) + " KB";
    }
    return QString::number(bytes) + " B";
}

QString formatSpeed(double bytesPerSec) {
    if (bytesPerSec <= 0.0) {
        return QStringLiteral("0 B/s");
    }
    return formatBytes(static_cast<quint64>(bytesPerSec)) + QStringLiteral("/s");
}

QString formatEta(double etaSeconds) {
    if (etaSeconds <= 0.0) {
        return QStringLiteral("ETA --");
    }

    const int total = static_cast<int>(etaSeconds);
    const int hours = total / 3600;
    const int minutes = (total % 3600) / 60;
    const int seconds = total % 60;

    if (hours > 0) {
        return QStringLiteral("ETA %1h %2m").arg(hours).arg(minutes);
    }
    if (minutes > 0) {
        return QStringLiteral("ETA %1m %2s").arg(minutes).arg(seconds);
    }
    return QStringLiteral("ETA %1s").arg(seconds);
}

QString stateToString(int state) {
    switch (state) {
    case 0:
        return QStringLiteral("Queued");
    case 1:
        return QStringLiteral("Downloading");
    case 2:
        return QStringLiteral("Paused");
    case 3:
        return QStringLiteral("Completed");
    case 4:
        return QStringLiteral("Failed");
    case 5:
        return QStringLiteral("Cancelled");
    default:
        return QStringLiteral("Unknown");
    }
}

}  // namespace muld_gui::FormatUtils
