#include "DownloadController.h"

namespace muld_gui {

DownloadController::DownloadController(QObject* parent)
    : QObject(parent)
    , m_manager(new DownloadManager(this))
    , m_model(new DownloadListModel(m_manager, this))
{
}

DownloadController::~DownloadController() = default;

DownloadItem* DownloadController::addDownload(const QString& url, const QString& outputDir,
                                              const QString& filename, int connections)
{
    return m_manager->addDownload(url, outputDir, filename, connections);
}

void DownloadController::removeDownload(int index) {
    m_manager->removeDownload(index);
}

void DownloadController::pauseDownload(int index) {
    if (auto* item = m_model->itemAt(index))
        item->pause();
}

void DownloadController::resumeDownload(int index) {
    if (auto* item = m_model->itemAt(index))
        item->resume();
}

void DownloadController::cancelDownload(int index) {
    if (auto* item = m_model->itemAt(index))
        item->cancel();
}

void DownloadController::pauseAll()  { m_manager->pauseAll(); }
void DownloadController::resumeAll() { m_manager->resumeAll(); }
void DownloadController::cancelAll() { m_manager->cancelAll(); }

} // namespace muld_gui
