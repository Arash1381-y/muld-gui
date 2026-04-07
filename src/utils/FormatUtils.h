#pragma once

#include <QString>

namespace muld_gui::FormatUtils {

QString formatBytes(quint64 bytes);
QString formatSpeed(double bytesPerSec);
QString formatEta(double etaSeconds);
QString stateToString(int state);

}  // namespace muld_gui::FormatUtils
