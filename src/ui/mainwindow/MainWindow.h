#pragma once

#include <QMainWindow>
#include <QHash>

class QListView;
class QListWidget;
class QPlainTextEdit;
class QWidget;

class QToolBar;

class QAction;

class QLabel;
class QModelIndex;

namespace muld_gui {

class DownloadController;
class DownloadItemDelegate;
class DownloadFilterProxyModel;
class DownloadItem;

class MainWindow : public QMainWindow {

Q_OBJECT

public:

explicit MainWindow(DownloadController* controller, QWidget* parent = nullptr);

private slots:

void onAddDownload();

void onRemove();
void onOpenFolder();
void onSettings();
void onFilterChanged(int row);
void onListClicked(const QModelIndex& index);
void onListDoubleClicked(const QModelIndex& index);
void onRowAction(const QModelIndex& index, int actionType);
void onExpandToggled(const QModelIndex& index);
void onToggleLogs(bool enabled);
void updateCountsAndStatus();

void onSelectionChanged();

private:

void setupUi();

void setupToolbar();

void setupStatusBar();
void setupLogging();
void attachItemLogging(DownloadItem* item, int index);
void updateActionStates();
void runDetailAction(int actionType);
void runActionForRow(int sourceRow, int actionType);
void logInfo(const QString& message);
void logWarn(const QString& message);
void logError(const QString& message);
int selectedSourceRow() const;

DownloadController* m_controller;

QWidget* m_centralWidget;
QWidget* m_headerWidget;
QListWidget* m_sidebar;
QListView* m_listView;
DownloadFilterProxyModel* m_proxyModel;
DownloadItemDelegate* m_delegate;
QPlainTextEdit* m_logView;
QWidget* m_logContainer;

QAction* m_addAction;

QAction* m_openFolderAction;
QAction* m_toggleLogsAction;

QAction* m_removeAction;
QAction* m_settingsAction;

QLabel* m_statusLeftLabel;
QLabel* m_statusRightLabel;
QHash<DownloadItem*, int> m_lastLoggedPercent;

};

} // namespace muld_gui
