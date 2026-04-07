#pragma once

#include <QApplication>
#include <memory>

namespace muld_gui {

class DownloadController;
class MainWindow;

class Application {
public:
    Application(int& argc, char** argv);
    ~Application();

    int exec();

private:
    QApplication m_app;
    std::unique_ptr<DownloadController> m_controller;
    std::unique_ptr<MainWindow> m_mainWindow;
};

} // namespace muld_gui
