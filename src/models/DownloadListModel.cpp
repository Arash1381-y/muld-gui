#include "DownloadListModel.h"
#include <QFileInfo>

namespace muld_gui {

DownloadListModel::DownloadListModel(DownloadManager* manager, QObject* parent)
    : QAbstractListModel(parent)
    , m_manager(manager)
{
    connect(m_manager, &DownloadManager::downloadAdded,
            this, &DownloadListModel::onDownloadAdded);
    connect(m_manager, &DownloadManager::downloadRemoved,
            this, &DownloadListModel::onDownloadRemoved);

    const auto& items = m_manager->downloads();
    for (int i = 0; i < items.size(); ++i) {
        connectItem(items[i], i);
    }
}

int DownloadListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return m_manager->count();
}

QVariant DownloadListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_manager->count())
        return {};

    auto* item = m_manager->downloads().at(index.row());
    auto prog = item->progress();

    switch (role) {
    case Qt::DisplayRole:
    case FilenameRole:   return item->filename();
    case UrlRole:        return item->url();
    case StateRole:      return static_cast<int>(item->state());
    case PercentRole:    return prog.percentage;
    case DownloadedRole: return QVariant::fromValue(prog.downloaded_bytes);
    case TotalSizeRole:  return QVariant::fromValue(prog.total_bytes);
    case SavePathRole:   return item->savePath();
    case SpeedRole:      return QVariant::fromValue(prog.speed_bytes_per_sec);
    case EtaRole:        return QVariant::fromValue(prog.eta_seconds);
    case CompletedAtRole:return item->completedAtText();
    case ChunkInfoRole:  return item->chunkInfo();
    case HasHandlerRole: return item->hasHandler();
    case MissingPartialRole: {
        const auto state = item->state();
        if (state != DownloadState::Paused &&
            state != DownloadState::Cancelled &&
            state != DownloadState::Failed) {
            return false;
        }
        return !QFileInfo(item->savePath()).exists();
    }
    case ErrorRole:      return item->errorMessage();
    case ItemPtrRole:    return QVariant::fromValue(static_cast<void*>(item));
    }
    return {};
}

QHash<int, QByteArray> DownloadListModel::roleNames() const {
    return {
        { FilenameRole,   "filename" },
        { UrlRole,        "url" },
        { StateRole,      "state" },
        { PercentRole,    "percent" },
        { DownloadedRole, "downloaded" },
        { TotalSizeRole,  "totalSize" },
        { SavePathRole,   "savePath" },
        { SpeedRole,      "speed" },
        { EtaRole,        "eta" },
        { CompletedAtRole,"completedAt" },
        { ChunkInfoRole,  "chunkInfo" },
        { HasHandlerRole, "hasHandler" },
        { MissingPartialRole, "missingPartial" },
        { ErrorRole,      "error" },
    };
}

DownloadItem* DownloadListModel::itemAt(int row) const {
    if (row < 0 || row >= m_manager->count()) return nullptr;
    return m_manager->downloads().at(row);
}

void DownloadListModel::onDownloadAdded(DownloadItem* item, int index) {
    beginInsertRows({}, index, index);
    connectItem(item, index);
    endInsertRows();
}

void DownloadListModel::onDownloadRemoved(int index) {
    beginRemoveRows({}, index, index);
    endRemoveRows();
}

void DownloadListModel::onItemProgressUpdated() {
    auto* item = qobject_cast<DownloadItem*>(sender());
    if (!item) return;

    int row = m_manager->downloads().indexOf(item);
    if (row >= 0) {
        QModelIndex idx = index(row);
        emit dataChanged(idx, idx, { PercentRole, DownloadedRole, SpeedRole, EtaRole, CompletedAtRole, ChunkInfoRole, MissingPartialRole });
    }
}

void DownloadListModel::onItemStateChanged() {
    auto* item = qobject_cast<DownloadItem*>(sender());
    if (!item) return;

    int row = m_manager->downloads().indexOf(item);
    if (row >= 0) {
        QModelIndex idx = index(row);
        emit dataChanged(idx, idx, { StateRole, CompletedAtRole, PercentRole, EtaRole, HasHandlerRole, MissingPartialRole });
    }
}

void DownloadListModel::connectItem(DownloadItem* item, int /*row*/) {
    connect(item, &DownloadItem::progressUpdated,
            this, &DownloadListModel::onItemProgressUpdated);
    connect(item, &DownloadItem::stateChanged,
            this, &DownloadListModel::onItemStateChanged);
}

} // namespace muld_gui
