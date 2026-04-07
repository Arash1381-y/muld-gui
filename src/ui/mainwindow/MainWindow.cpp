#include "MainWindow.h"

#include "../../controllers/DownloadController.h"
#include "../../core/DownloadItem.h"
#include "../../core/DownloadTypes.h"
#include "../../models/DownloadListModel.h"
#include "../../utils/FormatUtils.h"
#include "../dialogs/AddDownloadDialog.h"
#include "../widgets/DownloadItemDelegate.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QSortFilterProxyModel>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QTextCursor>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace muld_gui {

enum class SidebarFilter {
    All,
    Downloading,
    Completed,
    Paused,
    Failed
};

class DownloadFilterProxyModel : public QSortFilterProxyModel {
  public:
    explicit DownloadFilterProxyModel(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent), m_filter(SidebarFilter::All) {
        setDynamicSortFilter(true);
    }

    void setFilter(SidebarFilter filter) {
        if (m_filter == filter) {
            return;
        }
        m_filter = filter;
        invalidateFilter();
    }

  protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override {
        if (m_filter == SidebarFilter::All) {
            return true;
        }

        const QModelIndex stateIdx = sourceModel()->index(sourceRow, 0, sourceParent);
        const int state = stateIdx.data(DownloadListModel::StateRole).toInt();

        switch (m_filter) {
            case SidebarFilter::Downloading:
                return state == static_cast<int>(DownloadState::Downloading);
            case SidebarFilter::Completed:
                return state == static_cast<int>(DownloadState::Completed);
            case SidebarFilter::Paused:
                return state == static_cast<int>(DownloadState::Paused);
            case SidebarFilter::Failed:
                return state == static_cast<int>(DownloadState::Failed);
            case SidebarFilter::All:
            default:
                return true;
        }
    }

    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override {
        const int ls = left.data(DownloadListModel::StateRole).toInt();
        const int rs = right.data(DownloadListModel::StateRole).toInt();
        const auto rank = [](int state) {
            switch (state) {
                case 1: return 0; // Downloading
                case 0: return 1; // Queued
                case 2: return 2; // Paused
                case 4: return 3; // Failed
                case 5: return 4; // Cancelled
                case 3: return 5; // Completed
                default: return 6;
            }
        };
        const int lr = rank(ls);
        const int rr = rank(rs);
        if (lr != rr) return lr < rr;
        return left.row() < right.row();
    }

  private:
    SidebarFilter m_filter;
};

namespace {
constexpr int kFilterCountRole = Qt::UserRole + 30;

class SidebarItemDelegate : public QStyledItemDelegate {
  public:
    explicit SidebarItemDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent) {}

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override {
        return QSize(0, 36);
    }

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, false);

        const QRect r = option.rect;
        if (option.state & QStyle::State_Selected) {
            painter->fillRect(r, QColor("#2d2d30"));
            painter->fillRect(QRect(r.left(), r.top(), 3, r.height()), QColor("#007acc"));
        } else if (option.state & QStyle::State_MouseOver) {
            painter->fillRect(r, QColor("#2d2d30"));
        } else {
            painter->fillRect(r, QColor("#252526"));
        }

        const QString label = index.data().toString();
        QString iconPath = QStringLiteral(":/icons/solid/squares-2x2.svg");
        QColor dotColor("#5a5a5a");
        if (label == QStringLiteral("Downloading")) {
            iconPath = QStringLiteral(":/icons/solid/arrow-down-tray.svg");
            dotColor = QColor("#4fc3f7");
        } else if (label == QStringLiteral("Completed")) {
            iconPath = QStringLiteral(":/icons/solid/check-circle.svg");
            dotColor = QColor("#81c784");
        } else if (label == QStringLiteral("Paused")) {
            iconPath = QStringLiteral(":/icons/solid/pause.svg");
            dotColor = QColor("#ffb74d");
        } else if (label == QStringLiteral("Failed")) {
            iconPath = QStringLiteral(":/icons/solid/x-circle.svg");
            dotColor = QColor("#e57373");
        }
        QPixmap icon = QIcon(iconPath).pixmap(14, 14);
        QPixmap tint(icon.size());
        tint.fill(Qt::transparent);
        QPainter tp(&tint);
        tp.drawPixmap(0, 0, icon);
        tp.setCompositionMode(QPainter::CompositionMode_SourceIn);
        tp.fillRect(tint.rect(), dotColor);
        painter->drawPixmap(r.left() + 8, r.center().y() - 7, tint);

        painter->setPen(QColor("#cccccc"));
        painter->setFont(QFont(QStringLiteral("Segoe UI"), 10, QFont::Medium));
        painter->drawText(r.adjusted(28, 0, -38, 0), Qt::AlignVCenter | Qt::AlignLeft, label);

        const int count = index.data(kFilterCountRole).toInt();
        const QString badge = QString::number(count);
        const QRect badgeRect(r.right() - 34, r.top() + 8, 24, 16);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor("#3c3c3c"));
        painter->drawRoundedRect(badgeRect, 8, 8);
        painter->setPen(QColor("#cccccc"));
        painter->setFont(QFont(QStringLiteral("Segoe UI"), 8, QFont::Medium));
        painter->drawText(badgeRect, Qt::AlignCenter, badge);

        painter->restore();
    }
};

}  // namespace

MainWindow::MainWindow(DownloadController* controller, QWidget* parent)
    : QMainWindow(parent)
    , m_controller(controller)
    , m_centralWidget(nullptr)
    , m_headerWidget(nullptr)
    , m_sidebar(nullptr)
    , m_listView(nullptr)
    , m_proxyModel(nullptr)
    , m_delegate(nullptr)
    , m_logView(nullptr)
    , m_logContainer(nullptr)
    , m_addAction(nullptr)
    , m_openFolderAction(nullptr)
    , m_toggleLogsAction(nullptr)
    , m_removeAction(nullptr)
    , m_settingsAction(nullptr)
    , m_statusLeftLabel(nullptr)
    , m_statusRightLabel(nullptr) {
    setupUi();
    setupToolbar();
    setupStatusBar();
    updateActionStates();
    setWindowTitle(tr("muld Download Manager"));
    resize(1120, 680);
}

void MainWindow::setupUi() {
    setStyleSheet(
        "QMainWindow { background: #1e1e1e; color: #cccccc; }"
        "QListView { background: #1e1e1e; border: none; }"
        "QListView::item { border: none; }"
        "QListWidget { background: #252526; border: 1px solid #1a1a1a; outline: none; }"
        "QPlainTextEdit { background:#1e1e1e; color:#cccccc; border:1px solid #1a1a1a; }"
        "QScrollBar:vertical { background: #252526; width: 10px; margin: 0; }"
        "QScrollBar::handle:vertical { background: #3e3e42; min-height: 24px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; border: none; }");

    m_proxyModel = new DownloadFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_controller->model());
    m_proxyModel->sort(0);

    m_centralWidget = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(m_centralWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* contentLayout = new QHBoxLayout();
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_sidebar = new QListWidget(m_centralWidget);
    m_sidebar->setFixedWidth(200);
    m_sidebar->setSpacing(0);
    m_sidebar->setSelectionMode(QAbstractItemView::SingleSelection);
    m_sidebar->setItemDelegate(new SidebarItemDelegate(m_sidebar));
    const QStringList filters = {
        tr("All Downloads"), tr("Downloading"), tr("Completed"), tr("Paused"), tr("Failed")
    };
    for (const QString& text : filters) {
        auto* item = new QListWidgetItem(text, m_sidebar);
        item->setData(kFilterCountRole, 0);
    }
    m_sidebar->setCurrentRow(0);

    auto* listArea = new QWidget(m_centralWidget);
    auto* listLayout = new QVBoxLayout(listArea);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(0);

    m_headerWidget = new QWidget(listArea);
    m_headerWidget->setFixedHeight(32);
    m_headerWidget->setStyleSheet("background:#252526; border-top:1px solid #1a1a1a; border-bottom:1px solid #3e3e42;");
    auto* headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(30, 0, 10, 0);
    headerLayout->setSpacing(4);

    auto makeHeader = [](const QString& text, int width, QWidget* parent) {
        auto* label = new QLabel(text, parent);
        label->setFixedWidth(width);
        label->setStyleSheet("color:#858585; font:600 11px 'Segoe UI'; letter-spacing:0.4px; text-transform:uppercase;");
        return label;
    };
    QLabel* nameHdr = makeHeader(tr("NAME"), 280, m_headerWidget);
    nameHdr->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    headerLayout->addWidget(nameHdr);
    headerLayout->addWidget(makeHeader(tr("PROGRESS"), 230, m_headerWidget));
    headerLayout->addWidget(makeHeader(tr("SPEED"), 90, m_headerWidget));
    headerLayout->addWidget(makeHeader(tr("ETA"), 90, m_headerWidget));
    headerLayout->addWidget(makeHeader(tr("SIZE"), 90, m_headerWidget));
    headerLayout->addWidget(makeHeader(tr("STATUS"), 100, m_headerWidget));
    headerLayout->addSpacing(84);

    m_listView = new QListView(listArea);
    m_listView->setModel(m_proxyModel);
    m_delegate = new DownloadItemDelegate(m_listView);
    m_listView->setItemDelegate(m_delegate);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_listView->setUniformItemSizes(false);
    m_listView->setSpacing(0);
    m_listView->setMouseTracking(true);

    auto* logTitle = new QLabel(tr("EVENT LOG"), listArea);
    logTitle->setStyleSheet("color:#858585; font:600 11px 'Segoe UI'; letter-spacing:0.4px; padding:6px 10px;");
    m_logView = new QPlainTextEdit(listArea);
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(1500);

    m_logContainer = new QWidget(listArea);
    auto* logLayout = new QVBoxLayout(m_logContainer);
    logLayout->setContentsMargins(0, 0, 0, 0);
    logLayout->setSpacing(0);
    logLayout->addWidget(logTitle);
    logLayout->addWidget(m_logView);
    m_logContainer->setVisible(false);

    listLayout->addWidget(m_headerWidget);
    listLayout->addWidget(m_listView);
    listLayout->addWidget(m_logContainer);

    contentLayout->addWidget(m_sidebar);
    contentLayout->addWidget(listArea, 1);
    rootLayout->addLayout(contentLayout, 1);

    setCentralWidget(m_centralWidget);

    connect(m_sidebar, &QListWidget::currentRowChanged, this, &MainWindow::onFilterChanged);
    connect(m_listView, &QListView::clicked, this, &MainWindow::onListClicked);
    connect(m_listView, &QListView::doubleClicked, this, &MainWindow::onListDoubleClicked);
    connect(m_listView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::onSelectionChanged);
    connect(m_delegate, &DownloadItemDelegate::actionClicked, this, &MainWindow::onRowAction);
    connect(m_delegate, &DownloadItemDelegate::expandToggled, this, &MainWindow::onExpandToggled);

    auto* model = m_controller->model();
    connect(model, &QAbstractItemModel::rowsInserted, this, &MainWindow::updateCountsAndStatus);
    connect(model, &QAbstractItemModel::rowsRemoved, this, &MainWindow::updateCountsAndStatus);
    connect(model, &QAbstractItemModel::dataChanged, this, &MainWindow::updateCountsAndStatus);
    connect(model, &QAbstractItemModel::modelReset, this, &MainWindow::updateCountsAndStatus);

    setupLogging();
    updateCountsAndStatus();
}

void MainWindow::setupToolbar() {
    auto* toolbar = addToolBar(tr("Toolbar"));
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setIconSize(QSize(16, 16));
    toolbar->setFixedHeight(48);
    toolbar->setStyleSheet(
        "QToolBar { background:#252526; border:none; border-bottom:1px solid #1a1a1a; spacing:6px; padding:6px; }"
        "QToolButton { color:#cccccc; background:#3c3c3c; border:1px solid #3e3e42; padding:5px 10px; }"
        "QToolButton:hover { background:#505050; }"
        "QToolBar::separator { width:1px; background:#3e3e42; margin:6px; }");

    m_addAction = toolbar->addAction(QIcon(QStringLiteral(":/icons/solid/plus.svg")), tr("+ New Download"));
    if (auto* newBtn = qobject_cast<QToolButton*>(toolbar->widgetForAction(m_addAction))) {
        newBtn->setStyleSheet("background:#0e639c; border:1px solid #1177bb; color:#ffffff;");
    }

    toolbar->addSeparator();
    m_removeAction = toolbar->addAction(QIcon(QStringLiteral(":/icons/solid/trash.svg")), tr("Remove Selected"));

    toolbar->addSeparator();
    m_openFolderAction = toolbar->addAction(QIcon(QStringLiteral(":/icons/solid/folder-open.svg")), tr("Open Folder"));
    m_settingsAction = toolbar->addAction(QIcon(QStringLiteral(":/icons/solid/cog-6-tooth.svg")), tr("Settings"));
    m_toggleLogsAction = toolbar->addAction(tr("Show Logs"));
    m_toggleLogsAction->setCheckable(true);

    connect(m_addAction, &QAction::triggered, this, &MainWindow::onAddDownload);
    connect(m_openFolderAction, &QAction::triggered, this, &MainWindow::onOpenFolder);
    connect(m_removeAction, &QAction::triggered, this, &MainWindow::onRemove);
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::onSettings);
    connect(m_toggleLogsAction, &QAction::toggled, this, &MainWindow::onToggleLogs);
}

void MainWindow::setupStatusBar() {
    statusBar()->setSizeGripEnabled(false);
    statusBar()->setFixedHeight(24);
    statusBar()->setStyleSheet("QStatusBar { background:#007acc; border:none; color:#ffffff; }");

    m_statusLeftLabel = new QLabel(this);
    m_statusRightLabel = new QLabel(this);
    m_statusLeftLabel->setStyleSheet("color:#ffffff; font:12px 'Segoe UI';");
    m_statusRightLabel->setStyleSheet("color:#ffffff; font:12px 'Segoe UI';");

    statusBar()->addWidget(m_statusLeftLabel, 1);
    statusBar()->addPermanentWidget(m_statusRightLabel);
}

int MainWindow::selectedSourceRow() const {
    if (!m_listView || !m_listView->selectionModel()) {
        return -1;
    }
    const QModelIndexList indexes = m_listView->selectionModel()->selectedIndexes();
    if (indexes.isEmpty()) {
        return -1;
    }
    const QModelIndex sourceIndex = m_proxyModel->mapToSource(indexes.first());
    return sourceIndex.isValid() ? sourceIndex.row() : -1;
}

void MainWindow::onAddDownload() {
    QStringList trackedTargets;
    const auto& existing = m_controller->manager()->downloads();
    for (DownloadItem* item : existing) {
        trackedTargets.append(QFileInfo(item->savePath()).absoluteFilePath());
    }
    AddDownloadDialog dlg(trackedTargets, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    const QString url = dlg.url();
    const QString outputDir = dlg.outputDir();
    const QString filename = dlg.filename();
    const int connections = dlg.connections();

    logInfo(tr("Request add: url=%1 savePath=%2 filename=%3 connections=%4")
                .arg(url, outputDir, filename.isEmpty() ? tr("<auto>") : filename)
                .arg(connections));

    auto* item = m_controller->addDownload(url, outputDir, filename, connections);
    if (!item) {
        const QString reason = m_controller->manager()->lastError();
        logError(tr("Add failed: url=%1 destinationDir=%2 filename=%3 reason=%4")
                     .arg(url,
                          outputDir,
                          filename.isEmpty() ? tr("<auto>") : filename,
                          reason.isEmpty() ? tr("unknown") : reason));
        QMessageBox::critical(this, tr("Add Download Failed"),
                              tr("Could not start this download.\nReason: %1")
                                  .arg(reason.isEmpty() ? tr("unknown error") : reason));
        return;
    }
    logInfo(tr("Added: %1 -> %2").arg(url, item->savePath()));
}

void MainWindow::onRemove() {
    const int row = selectedSourceRow();
    if (row < 0) return;
    runActionForRow(row, DownloadItemDelegate::ActionRemove);
}

void MainWindow::onOpenFolder() {
    const int row = selectedSourceRow();
    if (row < 0) return;
    runActionForRow(row, DownloadItemDelegate::ActionOpenFolder);
}

void MainWindow::onSettings() {
    logInfo(tr("Settings clicked"));
    QMessageBox::information(this, tr("Settings"), tr("Settings are not available yet in this build."));
}

void MainWindow::onFilterChanged(int row) {
    SidebarFilter filter = SidebarFilter::All;
    switch (row) {
        case 1:
            filter = SidebarFilter::Downloading;
            break;
        case 2:
            filter = SidebarFilter::Completed;
            break;
        case 3:
            filter = SidebarFilter::Paused;
            break;
        case 4:
            filter = SidebarFilter::Failed;
            break;
        default:
            break;
    }
    m_proxyModel->setFilter(filter);
    m_delegate->clearExpandedIndex();
    m_listView->clearSelection();
    m_listView->doItemsLayout();
    m_proxyModel->sort(0);
    updateActionStates();
    const QString filterLabel =
        (m_sidebar && m_sidebar->currentItem()) ? m_sidebar->currentItem()->text() : tr("Unknown");
    logInfo(tr("Filter changed: %1").arg(filterLabel));
}

void MainWindow::onListClicked(const QModelIndex& index) {
    if (index.isValid()) {
        m_listView->setCurrentIndex(index);
    }
    updateActionStates();
}

void MainWindow::onListDoubleClicked(const QModelIndex& index) {
    if (!index.isValid()) {
        return;
    }
    const int row = m_proxyModel->mapToSource(index).row();
    DownloadItem* item = m_controller->manager()->downloadAt(row);
    if (!item || item->state() != DownloadState::Completed) {
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(item->savePath()));
    logInfo(tr("Open file: %1").arg(item->savePath()));
}

void MainWindow::onRowAction(const QModelIndex& index, int actionType) {
    if (!index.isValid()) {
        return;
    }
    const int sourceRow = m_proxyModel->mapToSource(index).row();
    runActionForRow(sourceRow, actionType);
}

void MainWindow::onExpandToggled(const QModelIndex& index) {
    if (!index.isValid()) {
        return;
    }
    if (m_delegate->expandedIndex() == index) {
        m_delegate->clearExpandedIndex();
    } else {
        m_delegate->setExpandedIndex(index);
    }
    m_listView->doItemsLayout();
    m_listView->viewport()->update();
}

void MainWindow::onToggleLogs(bool enabled) {
    if (!m_logContainer || !m_toggleLogsAction) {
        return;
    }
    m_logContainer->setVisible(enabled);
    m_toggleLogsAction->setText(enabled ? tr("Hide Logs") : tr("Show Logs"));
}

void MainWindow::onSelectionChanged() {
    updateActionStates();
}

void MainWindow::updateActionStates() {
    if (!m_openFolderAction || !m_removeAction) {
        return;
    }

    const int row = selectedSourceRow();
    const bool hasSelection = row >= 0;
    m_openFolderAction->setEnabled(hasSelection);
    m_removeAction->setEnabled(false);

    if (!hasSelection) {
        return;
    }

    auto* item = m_controller->manager()->downloadAt(row);
    if (!item) {
        return;
    }

    const auto state = item->state();
    m_removeAction->setEnabled(state == DownloadState::Completed ||
                               state == DownloadState::Failed ||
                               state == DownloadState::Cancelled);
}

void MainWindow::updateCountsAndStatus() {
    int allCount = 0;
    int downloadingCount = 0;
    int completedCount = 0;
    int pausedCount = 0;
    int failedCount = 0;
    double totalActiveSpeed = 0.0;

    const auto& downloads = m_controller->manager()->downloads();
    for (DownloadItem* item : downloads) {
        ++allCount;
        switch (item->state()) {
            case DownloadState::Downloading:
                ++downloadingCount;
                totalActiveSpeed += item->progress().speed_bytes_per_sec;
                break;
            case DownloadState::Completed:
                ++completedCount;
                break;
            case DownloadState::Paused:
                ++pausedCount;
                break;
            case DownloadState::Failed:
                ++failedCount;
                break;
            default:
                break;
        }
    }

    if (m_sidebar && m_sidebar->count() >= 5) {
        m_sidebar->item(0)->setData(kFilterCountRole, allCount);
        m_sidebar->item(1)->setData(kFilterCountRole, downloadingCount);
        m_sidebar->item(2)->setData(kFilterCountRole, completedCount);
        m_sidebar->item(3)->setData(kFilterCountRole, pausedCount);
        m_sidebar->item(4)->setData(kFilterCountRole, failedCount);
        m_sidebar->viewport()->update();
    }

    if (m_statusLeftLabel) {
        m_statusLeftLabel->setText(
            tr("%1 active  |  %2")
                .arg(downloadingCount)
                .arg(FormatUtils::formatSpeed(totalActiveSpeed)));
    }
    if (m_statusRightLabel) {
        m_statusRightLabel->setText(tr("Completed: %1   Failed: %2").arg(completedCount).arg(failedCount));
    }
}

void MainWindow::setupLogging() {
    auto* manager = m_controller->manager();
    connect(manager, &DownloadManager::downloadAdded, this, [this](DownloadItem* item, int index) {
        attachItemLogging(item, index);
    });
    connect(manager, &DownloadManager::downloadRemoved, this, [this](int index) {
        logInfo(tr("Removed row index=%1").arg(index));
    });

    const auto& items = manager->downloads();
    for (int i = 0; i < items.size(); ++i) {
        attachItemLogging(items.at(i), i);
    }
    logInfo(tr("UI logger initialized"));
}

void MainWindow::attachItemLogging(DownloadItem* item, int index) {
    if (!item) {
        return;
    }
    m_lastLoggedPercent[item] = -1;
    logInfo(tr("Tracked row=%1 file=%2 url=%3 path=%4")
                .arg(index)
                .arg(item->filename(), item->url(), item->savePath()));

    connect(item, &QObject::destroyed, this, [this, item]() {
        m_lastLoggedPercent.remove(item);
    });
    connect(item, &DownloadItem::stateChanged, this, [this, item](DownloadState state) {
        logInfo(tr("State change: file=%1 state=%2")
                    .arg(item->filename(), FormatUtils::stateToString(static_cast<int>(state))));
        updateActionStates();
        if (m_listView) {
            m_listView->viewport()->update();
        }
    });
    connect(item, &DownloadItem::progressUpdated, this, [this, item](const DownloadProgress& progress) {
        const int last = m_lastLoggedPercent.value(item, -1);
        const bool milestone = progress.percentage >= 100 || progress.percentage / 10 > last / 10;
        if (!milestone) {
            return;
        }
        m_lastLoggedPercent[item] = progress.percentage;
        logInfo(tr("Progress: file=%1 percent=%2 downloaded=%3 total=%4 speed=%5 eta=%6s")
                    .arg(item->filename())
                    .arg(progress.percentage)
                    .arg(FormatUtils::formatBytes(progress.downloaded_bytes))
                    .arg(FormatUtils::formatBytes(progress.total_bytes))
                    .arg(FormatUtils::formatSpeed(progress.speed_bytes_per_sec))
                    .arg(progress.eta_seconds));
    });
    connect(item, &DownloadItem::failed, this, [this, item](const QString& error) {
        const auto progress = item->progress();
        logError(tr("Backend error: file=%1 error=%2")
                     .arg(item->filename(), error));
        logError(tr("Diagnostics: url=%1 path=%2 state=%3 downloaded=%4 total=%5 speed=%6")
                     .arg(item->url(),
                          item->savePath(),
                          FormatUtils::stateToString(static_cast<int>(item->state())),
                          FormatUtils::formatBytes(progress.downloaded_bytes),
                          FormatUtils::formatBytes(progress.total_bytes),
                          FormatUtils::formatSpeed(progress.speed_bytes_per_sec)));
    });
    connect(item, &DownloadItem::completed, this, [this, item]() {
        const auto progress = item->progress();
        logInfo(tr("Completed: file=%1 total=%2")
                    .arg(item->filename(), FormatUtils::formatBytes(progress.total_bytes)));
    });
}

void MainWindow::runDetailAction(int actionType) {
    const int row = selectedSourceRow();
    if (row < 0) {
        return;
    }
    runActionForRow(row, actionType);
}

void MainWindow::runActionForRow(int sourceRow, int actionType) {
    DownloadItem* item = m_controller->manager()->downloadAt(sourceRow);
    if (!item) {
        return;
    }
    const QModelIndex idx = m_controller->model()->index(sourceRow);
    const bool missingPartial = idx.data(DownloadListModel::MissingPartialRole).toBool();
    const bool hasHandler = idx.data(DownloadListModel::HasHandlerRole).toBool();

    if (actionType == DownloadItemDelegate::ActionPause) {
        if (item->state() == DownloadState::Downloading) {
            m_controller->pauseDownload(sourceRow);
            logInfo(tr("Action pause: %1").arg(item->filename()));
        }
    } else if (actionType == DownloadItemDelegate::ActionResume) {
        if (item->state() == DownloadState::Paused && !missingPartial && hasHandler) {
            m_controller->resumeDownload(sourceRow);
            logInfo(tr("Action resume: %1").arg(item->filename()));
        } else {
            actionType = DownloadItemDelegate::ActionRetry;
        }
    }

    if (actionType == DownloadItemDelegate::ActionCancel) {
        if (item->state() == DownloadState::Downloading || item->state() == DownloadState::Paused ||
            item->state() == DownloadState::Queued) {
            m_controller->cancelDownload(sourceRow);
            logWarn(tr("Action cancel: %1").arg(item->filename()));
        }
    } else if (actionType == DownloadItemDelegate::ActionOpenFolder) {
        const QFileInfo pathInfo(item->savePath());
        const QString folderPath = pathInfo.isDir() ? pathInfo.absoluteFilePath() : pathInfo.absolutePath();
        QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
        logInfo(tr("Open folder: %1").arg(folderPath));
    } else if (actionType == DownloadItemDelegate::ActionRemove) {
        if (item->state() == DownloadState::Downloading || item->state() == DownloadState::Paused) {
            logWarn(tr("Remove blocked: %1 is active (%2)")
                        .arg(item->filename(), FormatUtils::stateToString(static_cast<int>(item->state()))));
            return;
        }
        const auto answer = QMessageBox::question(
            this,
            tr("Confirm Remove"),
            tr("Remove '%1' from the list?\nThis does not resume automatically later.")
                .arg(item->filename()),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
        logInfo(tr("Remove: %1 (%2)").arg(item->filename(), item->savePath()));
        m_controller->removeDownload(sourceRow);
        m_delegate->clearExpandedIndex();
    } else if (actionType == DownloadItemDelegate::ActionRetry) {
        if (item->state() != DownloadState::Failed && item->state() != DownloadState::Cancelled) {
            if (!(item->state() == DownloadState::Paused && (missingPartial || !hasHandler))) {
                return;
            }
        }
        const QString url = item->url();
        const QFileInfo info(item->savePath());
        const QString outputDir = info.absolutePath();
        const QString filename = info.fileName();
        logInfo(tr("Action retry: file=%1 url=%2").arg(filename, url));
        m_controller->removeDownload(sourceRow);
        auto* restarted = m_controller->addDownload(url, outputDir, filename, 4);
        if (!restarted) {
            logError(tr("Retry failed: url=%1 path=%2 reason=%3")
                         .arg(url, outputDir, m_controller->manager()->lastError()));
        } else {
            logInfo(tr("Retry started: %1").arg(restarted->savePath()));
        }
    } else if (actionType == DownloadItemDelegate::ActionCopyLocation) {
        QApplication::clipboard()->setText(item->savePath());
        logInfo(tr("Copied location: %1").arg(item->savePath()));
    } else if (actionType == DownloadItemDelegate::ActionCopyLink) {
        QApplication::clipboard()->setText(item->url());
        logInfo(tr("Copied link: %1").arg(item->url()));
    }

    updateActionStates();
    if (m_listView) {
        m_listView->viewport()->update();
    }
}

void MainWindow::logInfo(const QString& message) {
    if (!m_logView) {
        return;
    }
    m_logView->appendPlainText(
        QStringLiteral("[%1] [INFO] %2")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")), message));
    m_logView->moveCursor(QTextCursor::End);
}

void MainWindow::logWarn(const QString& message) {
    if (!m_logView) {
        return;
    }
    m_logView->appendPlainText(
        QStringLiteral("[%1] [WARN] %2")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")), message));
    m_logView->moveCursor(QTextCursor::End);
}

void MainWindow::logError(const QString& message) {
    if (!m_logView) {
        return;
    }
    m_logView->appendPlainText(
        QStringLiteral("[%1] [ERROR] %2")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")), message));
    m_logView->moveCursor(QTextCursor::End);
}

}  // namespace muld_gui
