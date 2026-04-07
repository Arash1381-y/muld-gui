#pragma once

#include "../core/DownloadManager.h"
#include <QAbstractListModel>

namespace muld_gui {

class DownloadListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        FilenameRole = Qt::UserRole + 1,
        UrlRole,
        StateRole,
        PercentRole,
        DownloadedRole,
        TotalSizeRole,
        SavePathRole,
        SpeedRole,
        EtaRole,
        CompletedAtRole,
        ChunkInfoRole,
        HasHandlerRole,
        MissingPartialRole,
        ErrorRole,
        ItemPtrRole   // returns DownloadItem* as QVariant
    };

    explicit DownloadListModel(DownloadManager* manager, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    DownloadItem* itemAt(int row) const;

private slots:
    void onDownloadAdded(DownloadItem* item, int index);
    void onDownloadRemoved(int index);
    void onItemProgressUpdated();
    void onItemStateChanged();

private:
    void connectItem(DownloadItem* item, int row);
    DownloadManager* m_manager;
};

} // namespace muld_gui
