#include "Application.h"
#include "controllers/DownloadController.h"
#include "ui/mainwindow/MainWindow.h"

namespace muld_gui {

Application::Application(int& argc, char** argv)
    : m_app(argc, argv)
{
    m_app.setApplicationName("muld-gui");
    m_app.setApplicationVersion("1.0.0");
    m_app.setOrganizationName("muld");

    m_controller = std::make_unique<DownloadController>();
    m_mainWindow = std::make_unique<MainWindow>(m_controller.get());
}

Application::~Application() = default;

int Application::exec() {
    m_mainWindow->show();
    return m_app.exec();
}

} // namespace muld_gui
