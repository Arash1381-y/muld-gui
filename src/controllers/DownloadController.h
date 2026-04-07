#pragma once

#include "../core/DownloadManager.h"
#include "../models/DownloadListModel.h"
#include <QObject>

namespace muld_gui {

class DownloadController : public QObject {
    Q_OBJECT

public:
    explicit DownloadController(QObject* parent = nullptr);
    ~DownloadController();

    DownloadManager* manager() const { return m_manager; }
    DownloadListModel* model() const { return m_model; }

public slots:
    DownloadItem* addDownload(const QString& url, const QString& outputDir,
                              const QString& filename = {}, int connections = 4);
    void removeDownload(int index);
    void pauseDownload(int index);
    void resumeDownload(int index);
    void cancelDownload(int index);
    void pauseAll();
    void resumeAll();
    void cancelAll();

private:
    DownloadManager* m_manager;
    DownloadListModel* m_model;
};

} // namespace muld_gui
