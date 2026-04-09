#pragma once

#include <QMainWindow>
#include <QHash>
#include <QList>
#include <QSet>

class QListView;
class QListWidget;
class QPlainTextEdit;
class QWidget;

class QToolBar;

class QAction;
class QLineEdit;
class QToolButton;
class QItemSelection;
class QGraphicsOpacityEffect;
class QPropertyAnimation;
class QStackedLayout;

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
void onRowSelectionToggled(const QModelIndex& index);
void onExpandToggled(const QModelIndex& index);
void onToggleLogs(bool enabled);
void updateCountsAndStatus();

void onSelectionChanged();
void onSearchTextChanged(const QString& text);
void onTypeFilterChanged(int row);
void onSelectionCancel();
void onSelectionSelectAll();
void onBulkPause();
void onBulkResume();
void onBulkDelete();

private:

void setupUi();

void setupToolbar();

void setupStatusBar();
void setupLogging();
void attachItemLogging(DownloadItem* item, int index);
void updateActionStates();
void updateSelectionModeUi();
void updateStorageWidget();
void applySourceSelectionSnapshot(const QSet<int>& sourceRows,
                                  const QModelIndex& preferredCurrentIndex,
                                  bool updateSnapshot);
QList<int> selectedSourceRows() const;
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
QListWidget* m_fileTypeList;
QListView* m_listView;
DownloadFilterProxyModel* m_proxyModel;
DownloadItemDelegate* m_delegate;
QPlainTextEdit* m_logView;
QWidget* m_logContainer;
QLabel* m_storageLabel;

QAction* m_addAction;
QAction* m_openFolderAction;
QAction* m_toggleLogsAction;
QAction* m_settingsAction;
QAction* m_selectAllAction;
QAction* m_bulkPauseAction;
QAction* m_bulkResumeAction;
QAction* m_bulkDeleteAction;

QToolBar* m_toolbar;
QWidget* m_toolbarContent;
QWidget* m_defaultTopWidget;
QWidget* m_selectionTopWidget;
QStackedLayout* m_toolbarStack;
QWidget* m_searchContainer;
QLineEdit* m_searchEdit;
QToolButton* m_selectionCancelBtn;
QLabel* m_selectionCountLabel;
QGraphicsOpacityEffect* m_selectionOpacityEffect;
QPropertyAnimation* m_selectionOpacityAnim;

QLabel* m_statusLeftLabel;
QLabel* m_statusRightLabel;
QHash<DownloadItem*, int> m_lastLoggedPercent;
QSet<int> m_selectedSourceRowsSnapshot;
bool m_selectionModeActive;

};

} // namespace muld_gui
