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
#include <QCursor>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPropertyAnimation>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStackedLayout>
#include <QStorageInfo>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QTextCursor>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>
#include <QUrl>
#include <QVBoxLayout>
#include <QEasingCurve>
#include <algorithm>
#include <functional>

namespace muld_gui {

enum class SidebarFilter {
    All,
    Downloading,
    Completed,
    Paused,
    Failed
};

enum class FileTypeFilter {
    All,
    Video,
    Document,
    Archive,
    Audio,
    Image
};

class DownloadFilterProxyModel : public QSortFilterProxyModel {
  public:
    explicit DownloadFilterProxyModel(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent), m_filter(SidebarFilter::All), m_typeFilter(FileTypeFilter::All) {
        setDynamicSortFilter(true);
    }

    void setFilter(SidebarFilter filter) {
        if (m_filter == filter) {
            return;
        }
        m_filter = filter;
        invalidateFilter();
    }

    void setTypeFilter(FileTypeFilter filter) {
        if (m_typeFilter == filter) {
            return;
        }
        m_typeFilter = filter;
        invalidateFilter();
    }

    void setSearchText(const QString& text) {
        const QString normalized = text.trimmed().toLower();
        if (m_searchText == normalized) {
            return;
        }
        m_searchText = normalized;
        invalidateFilter();
    }

  protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override {
        const QModelIndex itemIdx = sourceModel()->index(sourceRow, 0, sourceParent);
        if (!itemIdx.isValid()) {
            return false;
        }

        const int state = itemIdx.data(DownloadListModel::StateRole).toInt();
        const QString filename = itemIdx.data(DownloadListModel::FilenameRole).toString();
        const QString url = itemIdx.data(DownloadListModel::UrlRole).toString();
        const QString savePath = itemIdx.data(DownloadListModel::SavePathRole).toString();

        bool stateMatch = true;
        if (m_filter != SidebarFilter::All) {
            switch (m_filter) {
                case SidebarFilter::Downloading:
                    stateMatch = state == static_cast<int>(DownloadState::Downloading);
                    break;
                case SidebarFilter::Completed:
                    stateMatch = state == static_cast<int>(DownloadState::Completed);
                    break;
                case SidebarFilter::Paused:
                    stateMatch = state == static_cast<int>(DownloadState::Paused);
                    break;
                case SidebarFilter::Failed:
                    stateMatch = state == static_cast<int>(DownloadState::Failed);
                    break;
                case SidebarFilter::All:
                default:
                    stateMatch = true;
                    break;
            }
        }
        if (!stateMatch) {
            return false;
        }

        if (m_typeFilter != FileTypeFilter::All) {
            const QString ext = QFileInfo(filename).suffix().toLower();
            const QSet<QString> videos = {QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("avi"),
                                          QStringLiteral("mov"), QStringLiteral("webm"), QStringLiteral("flv")};
            const QSet<QString> documents = {QStringLiteral("pdf"), QStringLiteral("doc"), QStringLiteral("docx"),
                                             QStringLiteral("txt"), QStringLiteral("rtf"), QStringLiteral("ppt"),
                                             QStringLiteral("pptx"), QStringLiteral("xls"), QStringLiteral("xlsx")};
            const QSet<QString> archives = {QStringLiteral("zip"), QStringLiteral("rar"), QStringLiteral("7z"),
                                            QStringLiteral("tar"), QStringLiteral("gz"), QStringLiteral("bz2"),
                                            QStringLiteral("xz")};
            const QSet<QString> audio = {QStringLiteral("mp3"), QStringLiteral("wav"), QStringLiteral("aac"),
                                         QStringLiteral("ogg"), QStringLiteral("flac"), QStringLiteral("m4a")};
            const QSet<QString> images = {QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
                                          QStringLiteral("gif"), QStringLiteral("bmp"), QStringLiteral("svg"),
                                          QStringLiteral("webp")};

            bool typeMatch = false;
            switch (m_typeFilter) {
                case FileTypeFilter::Video:
                    typeMatch = videos.contains(ext);
                    break;
                case FileTypeFilter::Document:
                    typeMatch = documents.contains(ext);
                    break;
                case FileTypeFilter::Archive:
                    typeMatch = archives.contains(ext);
                    break;
                case FileTypeFilter::Audio:
                    typeMatch = audio.contains(ext);
                    break;
                case FileTypeFilter::Image:
                    typeMatch = images.contains(ext);
                    break;
                case FileTypeFilter::All:
                default:
                    typeMatch = true;
                    break;
            }
            if (!typeMatch) {
                return false;
            }
        }

        if (!m_searchText.isEmpty()) {
            const QString haystack = (filename + QStringLiteral(" ") + url + QStringLiteral(" ") + savePath).toLower();
            if (!haystack.contains(m_searchText)) {
                return false;
            }
        }

        return true;
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
    FileTypeFilter m_typeFilter;
    QString m_searchText;
};

namespace {
constexpr int kFilterCountRole = Qt::UserRole + 30;

class SidebarItemDelegate : public QStyledItemDelegate {
  public:
    explicit SidebarItemDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent) {}

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override {
        return QSize(0, 42);
    }

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, false);

        const QRect r = option.rect;
        if (option.state & QStyle::State_Selected) {
            painter->fillRect(r, QColor("#313236"));
            painter->fillRect(QRect(r.left(), r.top(), 3, r.height()), QColor("#4fc3f7"));
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

        painter->setPen((option.state & QStyle::State_Selected) ? QColor("#ffffff") : QColor("#c6c6c6"));
        painter->setFont(QFont(QStringLiteral("Segoe UI"), 10, QFont::Medium));
        painter->drawText(r.adjusted(28, 0, -38, 0), Qt::AlignVCenter | Qt::AlignLeft, label);

        const int count = index.data(kFilterCountRole).toInt();
        if (count <= 0) {
            painter->restore();
            return;
        }

        const QString badge = QString::number(count);
        const QRect badgeRect(r.right() - 34, r.top() + 8, 24, 1);
        painter->setPen(Qt::NoPen);
        QColor badgeBg("#3c3c3c");
        if (label == QStringLiteral("Completed")) {
            badgeBg = QColor("#1d3f28");
        } else if (label == QStringLiteral("Failed")) {
            badgeBg = QColor("#4a1f26");
        } else if (label == QStringLiteral("Downloading")) {
            badgeBg = QColor("#18415a");
        } else if (label == QStringLiteral("Paused")) {
            badgeBg = QColor("#4a3a12");
        }
        painter->setBrush(badgeBg);
        painter->drawRoundedRect(badgeRect, 8, 8);
        painter->setPen(QColor("#cccccc"));
        painter->setFont(QFont(QStringLiteral("Segoe UI"), 8, QFont::Medium));
        painter->drawText(badgeRect, Qt::AlignCenter, badge);

        painter->restore();
    }
};

class CheckOnlySelectionListView : public QListView {
  public:
    explicit CheckOnlySelectionListView(QWidget* parent = nullptr)
        : QListView(parent) {}

  protected:
    QItemSelectionModel::SelectionFlags selectionCommand(const QModelIndex& index,
                                                         const QEvent* event = nullptr) const override {
        if (!index.isValid() || !event) {
            return QListView::selectionCommand(index, event);
        }

        switch (event->type()) {
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonRelease:
            case QEvent::MouseButtonDblClick: {
                const auto* mouseEvent = static_cast<const QMouseEvent*>(event);
                constexpr int kDelegateRowHeight = 56;
                const QRect rowRect = visualRect(index).adjusted(10, 0, -10, 0);
                const QRect selectionRect(
                    rowRect.left() + 3,
                    rowRect.top() + (kDelegateRowHeight - 16) / 2,
                    16,
                    16);
                if (!selectionRect.contains(mouseEvent->pos())) {
                    return QItemSelectionModel::NoUpdate;
                }
                break;
            }
            default:
                break;
        }
        return QListView::selectionCommand(index, event);
    }
};

}  // namespace

MainWindow::MainWindow(DownloadController* controller, QWidget* parent)
    : QMainWindow(parent)
    , m_controller(controller)
    , m_centralWidget(nullptr)
    , m_headerWidget(nullptr)
    , m_sidebar(nullptr)
    , m_fileTypeList(nullptr)
    , m_listView(nullptr)
    , m_proxyModel(nullptr)
    , m_delegate(nullptr)
    , m_logView(nullptr)
    , m_logContainer(nullptr)
    , m_storageLabel(nullptr)
    , m_addAction(nullptr)
    , m_openFolderAction(nullptr)
    , m_toggleLogsAction(nullptr)
    , m_settingsAction(nullptr)
    , m_selectAllAction(nullptr)
    , m_bulkPauseAction(nullptr)
    , m_bulkResumeAction(nullptr)
    , m_bulkDeleteAction(nullptr)
    , m_toolbar(nullptr)
    , m_toolbarContent(nullptr)
    , m_defaultTopWidget(nullptr)
    , m_selectionTopWidget(nullptr)
    , m_toolbarStack(nullptr)
    , m_searchContainer(nullptr)
    , m_searchEdit(nullptr)
    , m_selectionCancelBtn(nullptr)
    , m_selectionCountLabel(nullptr)
    , m_selectionOpacityEffect(nullptr)
    , m_selectionOpacityAnim(nullptr)
    , m_statusLeftLabel(nullptr)
    , m_statusRightLabel(nullptr)
    , m_selectionModeActive(false) {
    setupUi();
    setupToolbar();
    setupStatusBar();
    updateSelectionModeUi();
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
        "QPlainTextEdit { background:#252528; color:#cccccc; border:1px solid #343438; }"
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

    auto* sidebarContainer = new QWidget(m_centralWidget);
    sidebarContainer->setFixedWidth(220);
    auto* sidebarLayout = new QVBoxLayout(sidebarContainer);
    sidebarLayout->setContentsMargins(0, 10, 0, 10);
    sidebarLayout->setSpacing(8);

    auto* statusLabel = new QLabel(tr("STATUS"), sidebarContainer);
    statusLabel->setStyleSheet("color:#8d8d8d; font:600 10px 'Segoe UI'; padding:0 10px;");
    sidebarLayout->addWidget(statusLabel);

    m_sidebar = new QListWidget(sidebarContainer);
    m_sidebar->setSpacing(2);
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
    sidebarLayout->addWidget(m_sidebar);

    auto* typeLabel = new QLabel(tr("FILE TYPES"), sidebarContainer);
    typeLabel->setStyleSheet("color:#8d8d8d; font:600 10px 'Segoe UI'; padding:2px 10px 0 10px;");
    sidebarLayout->addWidget(typeLabel);

    m_fileTypeList = new QListWidget(sidebarContainer);
    m_fileTypeList->setSpacing(2);
    m_fileTypeList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fileTypeList->setItemDelegate(new SidebarItemDelegate(m_fileTypeList));
    const QStringList fileTypes = {tr("All Types"), tr("Videos"), tr("Documents"), tr("Archives"), tr("Audio"), tr("Images")};
    for (const QString& text : fileTypes) {
        auto* item = new QListWidgetItem(text, m_fileTypeList);
        item->setData(kFilterCountRole, 0);
    }
    m_fileTypeList->setCurrentRow(0);
    sidebarLayout->addWidget(m_fileTypeList);

    sidebarLayout->addStretch(1);
    m_storageLabel = new QLabel(sidebarContainer);
    m_storageLabel->setStyleSheet("background:#1f1f20; color:#c8c8c8; border:1px solid #303034; border-radius:6px; padding:8px;");
    m_storageLabel->setWordWrap(true);
    sidebarLayout->addWidget(m_storageLabel);

    auto* listArea = new QWidget(m_centralWidget);
    auto* listLayout = new QVBoxLayout(listArea);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(0);

    m_headerWidget = new QWidget(listArea);
    m_headerWidget->setFixedHeight(36);
    m_headerWidget->setStyleSheet("background:#252526; border-top:1px solid #1a1a1a; border-bottom:1px solid #3e3e42;");
    auto* headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(10, 0, 10, 0);
    headerLayout->setSpacing(4);

    auto makeHeader = [](const QString& text, int width, QWidget* parent) {
        auto* label = new QLabel(text, parent);
        label->setFixedWidth(width);
        label->setStyleSheet("color:#858585; font:600 11px 'Segoe UI'; letter-spacing:0.4px; text-transform:uppercase;");
        return label;
    };
    auto* leadingSpacer = new QWidget(m_headerWidget);
    leadingSpacer->setFixedWidth(46);
    headerLayout->addWidget(leadingSpacer);

    QLabel* nameHdr = makeHeader(tr("NAME"), 280, m_headerWidget);
    nameHdr->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    nameHdr->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    headerLayout->addWidget(nameHdr);
    auto centerHeader = [this, headerLayout, makeHeader](const QString& text, int width) {
        QLabel* label = makeHeader(text, width, m_headerWidget);
        label->setAlignment(Qt::AlignCenter);
        headerLayout->addWidget(label);
    };
    centerHeader(tr("PROGRESS"), 230);
    centerHeader(tr("SPEED"), 90);
    centerHeader(tr("ETA"), 90);
    centerHeader(tr("SIZE"), 90);
    centerHeader(tr("STATUS"), 100);
    auto* actionsSpacer = new QWidget(m_headerWidget);
    actionsSpacer->setFixedWidth(84);
    headerLayout->addWidget(actionsSpacer);

    m_listView = new CheckOnlySelectionListView(listArea);
    m_listView->setModel(m_proxyModel);
    m_delegate = new DownloadItemDelegate(m_listView);
    m_listView->setItemDelegate(m_delegate);
    m_listView->setSelectionMode(QAbstractItemView::NoSelection);
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

    contentLayout->addWidget(sidebarContainer);
    contentLayout->addWidget(listArea, 1);
    rootLayout->addLayout(contentLayout, 1);

    setCentralWidget(m_centralWidget);

    connect(m_sidebar, &QListWidget::currentRowChanged, this, &MainWindow::onFilterChanged);
    connect(m_fileTypeList, &QListWidget::currentRowChanged, this, &MainWindow::onTypeFilterChanged);
    connect(m_listView, &QListView::clicked, this, &MainWindow::onListClicked);
    connect(m_listView, &QListView::doubleClicked, this, &MainWindow::onListDoubleClicked);
    connect(m_listView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::onSelectionChanged);
    connect(m_delegate, &DownloadItemDelegate::actionClicked, this, &MainWindow::onRowAction);
    connect(m_delegate, &DownloadItemDelegate::selectionToggled, this, &MainWindow::onRowSelectionToggled);
    connect(m_delegate, &DownloadItemDelegate::expandToggled, this, &MainWindow::onExpandToggled);
    connect(m_delegate, &DownloadItemDelegate::repaintRequested, m_listView->viewport(), qOverload<>(&QWidget::update));

    auto* model = m_controller->model();
    connect(model, &QAbstractItemModel::rowsInserted, this, &MainWindow::updateCountsAndStatus);
    connect(model, &QAbstractItemModel::rowsRemoved, this, &MainWindow::updateCountsAndStatus);
    connect(model, &QAbstractItemModel::dataChanged, this, &MainWindow::updateCountsAndStatus);
    connect(model, &QAbstractItemModel::modelReset, this, &MainWindow::updateCountsAndStatus);

    setupLogging();
    updateStorageWidget();
    updateCountsAndStatus();
}

void MainWindow::setupToolbar() {
    m_toolbar = addToolBar(tr("Toolbar"));
    m_toolbar->setMovable(false);
    m_toolbar->setFloatable(false);
    m_toolbar->setIconSize(QSize(22, 22));
    m_toolbar->setFixedHeight(58);
    m_toolbar->setStyleSheet(
        "QToolBar { background:#252526; border:none; border-bottom:1px solid #1a1a1a; spacing:8px; padding:8px 12px; }"
        "QToolButton { color:#d0d0d0; background:transparent; border:1px solid transparent; padding:8px 12px; border-radius:8px; }"
        "QToolButton:hover { background:#35353a; border-color:transparent; }");
    m_addAction = new QAction(QIcon(QStringLiteral(":/icons/solid/plus.svg")), tr("Add URL"), this);
    m_openFolderAction = new QAction(QIcon(QStringLiteral(":/icons/solid/folder-open.svg")), tr("Open Folder"), this);
    m_toggleLogsAction = new QAction(QIcon(QStringLiteral(":/icons/solid/command-line.svg")), tr("Toggle Logs"), this);
    m_toggleLogsAction->setCheckable(true);
    m_settingsAction = new QAction(QIcon(QStringLiteral(":/icons/solid/cog-6-tooth.svg")), tr("Settings"), this);
    m_selectAllAction = new QAction(tr("Select all"), this);
    m_bulkPauseAction = new QAction(QIcon(QStringLiteral(":/icons/solid/pause.svg")), tr("Pause"), this);
    m_bulkResumeAction = new QAction(QIcon(QStringLiteral(":/icons/solid/play.svg")), tr("Resume"), this);
    m_bulkDeleteAction = new QAction(QIcon(QStringLiteral(":/icons/solid/trash.svg")), tr("Delete"), this);

    m_toolbarContent = new QWidget(m_toolbar);
    m_toolbarContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolbarStack = new QStackedLayout(m_toolbarContent);
    m_toolbarStack->setContentsMargins(0, 0, 0, 0);
    m_toolbarStack->setSpacing(0);

    m_defaultTopWidget = new QWidget(m_toolbarContent);
    auto* defaultLayout = new QHBoxLayout(m_defaultTopWidget);
    defaultLayout->setContentsMargins(0, 0, 0, 0);
    defaultLayout->setSpacing(10);

    auto* addBtn = new QToolButton(m_defaultTopWidget);
    addBtn->setDefaultAction(m_addAction);
    const QPixmap plusSrc = QIcon(QStringLiteral(":/icons/solid/plus.svg")).pixmap(14, 14);
    QPixmap plusWhite(plusSrc.size());
    plusWhite.fill(Qt::transparent);
    {
        QPainter p(&plusWhite);
        p.drawPixmap(0, 0, plusSrc);
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(plusWhite.rect(), QColor("#ffffff"));
    }
    addBtn->setIcon(QIcon(plusWhite));
    addBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    addBtn->setText(tr("Add URL"));
    addBtn->setStyleSheet(
        "QToolButton { background:#0b67b1; border:1px solid #2a8cd8; color:#ffffff; border-radius:20px; padding:8px 20px; font:700 13px 'Segoe UI'; }"
        "QToolButton:hover { background:#1478c9; border-color:#4aa2e6; }"
        "QToolButton:pressed { background:#0a5a9a; border-color:#2a8cd8; }");

    m_searchContainer = new QWidget(m_defaultTopWidget);
    auto* searchLayout = new QHBoxLayout(m_searchContainer);
    searchLayout->setContentsMargins(12, 3, 12, 3);
    searchLayout->setSpacing(8);
    auto* searchIcon = new QLabel(m_searchContainer);
    searchIcon->setPixmap(QIcon(QStringLiteral(":/icons/solid/magnifying-glass.svg")).pixmap(14, 14));
    m_searchEdit = new QLineEdit(m_searchContainer);
    m_searchEdit->setPlaceholderText(tr("Search downloads"));
    m_searchEdit->setStyleSheet(
        "QLineEdit { background:transparent; color:#d0d0d0; border:none; font:13px 'Segoe UI'; }"
        "QLineEdit::placeholder { color:#8c8c8c; }");
    searchLayout->addWidget(searchIcon);
    searchLayout->addWidget(m_searchEdit, 1);
    m_searchContainer->setStyleSheet(
        "QWidget { background:#2a2a2a; border:1px solid transparent; border-radius:8px; }"
        "QWidget:focus-within { border-color:#444444; }");
    m_searchContainer->setFixedWidth(300);
    m_searchContainer->setFixedHeight(32);

    auto createRightButton = [this]() {
        auto* button = new QToolButton(m_defaultTopWidget);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setFixedSize(38, 38);
        button->setStyleSheet(
            "QToolButton { background:transparent; border:1px solid transparent; border-radius:19px; padding:6px; }"
            "QToolButton:hover { background:#3a3a3f; }");
        return button;
    };
    auto* openBtn = createRightButton();
    openBtn->setDefaultAction(m_openFolderAction);
    auto* logsBtn = createRightButton();
    logsBtn->setDefaultAction(m_toggleLogsAction);
    auto* settingsBtn = createRightButton();
    settingsBtn->setDefaultAction(m_settingsAction);

    defaultLayout->addWidget(addBtn, 0, Qt::AlignVCenter);
    defaultLayout->addWidget(m_searchContainer, 0, Qt::AlignVCenter);
    defaultLayout->addStretch(1);
    defaultLayout->addWidget(openBtn, 0, Qt::AlignVCenter);
    defaultLayout->addWidget(logsBtn, 0, Qt::AlignVCenter);
    defaultLayout->addWidget(settingsBtn, 0, Qt::AlignVCenter);

    m_selectionTopWidget = new QWidget(m_toolbarContent);
    auto* selectionLayout = new QHBoxLayout(m_selectionTopWidget);
    selectionLayout->setContentsMargins(0, 0, 0, 0);
    selectionLayout->setSpacing(8);

    m_selectionCancelBtn = new QToolButton(m_selectionTopWidget);
    m_selectionCancelBtn->setText(QStringLiteral("✕"));
    m_selectionCancelBtn->setStyleSheet(
        "QToolButton { background:transparent; border:none; color:#e6e6e6; font:700 15px 'Segoe UI'; border-radius:4px; padding:6px; }"
        "QToolButton:hover { background:rgba(255, 255, 255, 0.10); }");

    m_selectionCountLabel = new QLabel(m_selectionTopWidget);
    m_selectionCountLabel->setStyleSheet("color:#ffffff; font:700 14px 'Segoe UI'; padding-left:2px;");

    auto styleSelectionAction = [](QToolButton* button, bool danger = false) {
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        if (danger) {
            button->setStyleSheet(
                "QToolButton { background:transparent; border:none; color:#d1d5db; border-radius:6px; padding:6px 12px; font:13px 'Segoe UI'; }"
                "QToolButton:hover { background:rgba(255, 69, 58, 0.15); color:#FF453A; }");
        } else {
            button->setStyleSheet(
                "QToolButton { background:transparent; border:none; color:#d1d5db; border-radius:6px; padding:6px 12px; font:13px 'Segoe UI'; }"
                "QToolButton:hover { background:rgba(255, 255, 255, 0.10); color:#ffffff; }");
        }
    };

    auto* pauseBtn = new QToolButton(m_selectionTopWidget);
    pauseBtn->setDefaultAction(m_bulkPauseAction);
    styleSelectionAction(pauseBtn);
    auto* selectAllBtn = new QToolButton(m_selectionTopWidget);
    selectAllBtn->setDefaultAction(m_selectAllAction);
    styleSelectionAction(selectAllBtn);
    auto* resumeBtn = new QToolButton(m_selectionTopWidget);
    resumeBtn->setDefaultAction(m_bulkResumeAction);
    styleSelectionAction(resumeBtn);
    auto* deleteBtn = new QToolButton(m_selectionTopWidget);
    deleteBtn->setDefaultAction(m_bulkDeleteAction);
    styleSelectionAction(deleteBtn, true);

    selectionLayout->addWidget(m_selectionCancelBtn, 0, Qt::AlignVCenter);
    selectionLayout->addWidget(m_selectionCountLabel, 0, Qt::AlignVCenter);
    selectionLayout->addStretch(1);
    selectionLayout->addWidget(selectAllBtn, 0, Qt::AlignVCenter);
    selectionLayout->addWidget(pauseBtn, 0, Qt::AlignVCenter);
    selectionLayout->addWidget(resumeBtn, 0, Qt::AlignVCenter);
    selectionLayout->addWidget(deleteBtn, 0, Qt::AlignVCenter);

    m_toolbarStack->addWidget(m_defaultTopWidget);
    m_toolbarStack->addWidget(m_selectionTopWidget);
    m_toolbarStack->setCurrentWidget(m_defaultTopWidget);

    m_selectionOpacityEffect = new QGraphicsOpacityEffect(m_selectionTopWidget);
    m_selectionTopWidget->setGraphicsEffect(m_selectionOpacityEffect);
    m_selectionOpacityEffect->setOpacity(1.0);
    m_selectionOpacityAnim = new QPropertyAnimation(m_selectionOpacityEffect, "opacity", this);
    m_selectionOpacityAnim->setDuration(180);
    m_selectionOpacityAnim->setEasingCurve(QEasingCurve::InOutCubic);

    m_toolbar->addWidget(m_toolbarContent);

    connect(m_addAction, &QAction::triggered, this, &MainWindow::onAddDownload);
    connect(m_openFolderAction, &QAction::triggered, this, &MainWindow::onOpenFolder);
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::onSettings);
    connect(m_toggleLogsAction, &QAction::toggled, this, &MainWindow::onToggleLogs);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    connect(m_selectionCancelBtn, &QToolButton::clicked, this, &MainWindow::onSelectionCancel);
    connect(m_selectAllAction, &QAction::triggered, this, &MainWindow::onSelectionSelectAll);
    connect(m_bulkPauseAction, &QAction::triggered, this, &MainWindow::onBulkPause);
    connect(m_bulkResumeAction, &QAction::triggered, this, &MainWindow::onBulkResume);
    connect(m_bulkDeleteAction, &QAction::triggered, this, &MainWindow::onBulkDelete);
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

QList<int> MainWindow::selectedSourceRows() const {
    QList<int> rows;
    if (!m_listView || !m_listView->selectionModel()) {
        return rows;
    }
    for (const QModelIndex& proxyIndex : m_listView->selectionModel()->selectedIndexes()) {
        const QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
        if (sourceIndex.isValid() && !rows.contains(sourceIndex.row())) {
            rows.push_back(sourceIndex.row());
        }
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    return rows;
}

void MainWindow::applySourceSelectionSnapshot(const QSet<int>& sourceRows,
                                              const QModelIndex& preferredCurrentIndex,
                                              bool updateSnapshot) {
    if (!m_listView || !m_listView->selectionModel() || !m_proxyModel) {
        return;
    }

    QItemSelectionModel* sel = m_listView->selectionModel();
    sel->clearSelection();

    QSet<int> restoredRows;
    for (int sourceRow : sourceRows) {
        const QModelIndex sourceIndex = m_controller->model()->index(sourceRow, 0);
        const QModelIndex proxyIndex = m_proxyModel->mapFromSource(sourceIndex);
        if (!proxyIndex.isValid()) {
            continue;
        }
        sel->select(proxyIndex, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        restoredRows.insert(sourceRow);
    }

    if (preferredCurrentIndex.isValid()) {
        sel->setCurrentIndex(preferredCurrentIndex, QItemSelectionModel::NoUpdate);
    } else {
        sel->setCurrentIndex(QModelIndex(), QItemSelectionModel::NoUpdate);
    }

    if (updateSnapshot) {
        m_selectedSourceRowsSnapshot = restoredRows;
    }
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
    const std::size_t speedLimitBytesPerSec = dlg.speedLimitBytesPerSec();

    const QString speedLimitLabel = speedLimitBytesPerSec > 0
        ? FormatUtils::formatSpeed(static_cast<double>(speedLimitBytesPerSec))
        : tr("unlimited");
    logInfo(tr("Request add: url=%1 savePath=%2 filename=%3 speedLimit=%4")
                .arg(url, outputDir, filename.isEmpty() ? tr("<auto>") : filename, speedLimitLabel));

    auto* item = m_controller->addDownload(url, outputDir, filename, speedLimitBytesPerSec);
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
    m_delegate->clearActionFeedback();
    m_selectedSourceRowsSnapshot.clear();
    m_listView->clearSelection();
    m_listView->doItemsLayout();
    m_proxyModel->sort(0);
    updateSelectionModeUi();
    updateActionStates();
    const QString filterLabel =
        (m_sidebar && m_sidebar->currentItem()) ? m_sidebar->currentItem()->text() : tr("Unknown");
    logInfo(tr("Filter changed: %1").arg(filterLabel));
}

void MainWindow::onListClicked(const QModelIndex& index) {
    if (!index.isValid() || !m_listView || !m_listView->selectionModel()) {
        updateActionStates();
        return;
    }
    m_listView->selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
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

void MainWindow::onRowSelectionToggled(const QModelIndex& index) {
    if (!index.isValid() || !m_listView || !m_listView->selectionModel() || !m_proxyModel) {
        return;
    }

    QSet<int> selectedRows = m_selectedSourceRowsSnapshot;
    const int sourceRow = m_proxyModel->mapToSource(index).row();
    if (sourceRow < 0) {
        return;
    }
    if (selectedRows.contains(sourceRow)) {
        selectedRows.remove(sourceRow);
    } else {
        selectedRows.insert(sourceRow);
    }
    m_selectedSourceRowsSnapshot = selectedRows;

    QTimer::singleShot(0, this, [this, selectedRows, sourceRow]() {
        if (!m_listView || !m_listView->selectionModel() || !m_proxyModel) {
            return;
        }
        const QModelIndex sourceIndex = m_controller->model()->index(sourceRow, 0);
        const QModelIndex proxyIndex = m_proxyModel->mapFromSource(sourceIndex);
        applySourceSelectionSnapshot(selectedRows, proxyIndex, true);
    });
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
    m_toggleLogsAction->setToolTip(enabled ? tr("Hide Logs") : tr("Show Logs"));
}

void MainWindow::onSelectionChanged() {
    updateSelectionModeUi();
    updateActionStates();
}

void MainWindow::onSearchTextChanged(const QString& text) {
    m_proxyModel->setSearchText(text);
    m_proxyModel->sort(0);
}

void MainWindow::onTypeFilterChanged(int row) {
    FileTypeFilter typeFilter = FileTypeFilter::All;
    switch (row) {
        case 1:
            typeFilter = FileTypeFilter::Video;
            break;
        case 2:
            typeFilter = FileTypeFilter::Document;
            break;
        case 3:
            typeFilter = FileTypeFilter::Archive;
            break;
        case 4:
            typeFilter = FileTypeFilter::Audio;
            break;
        case 5:
            typeFilter = FileTypeFilter::Image;
            break;
        default:
            break;
    }
    m_proxyModel->setTypeFilter(typeFilter);
    m_delegate->clearExpandedIndex();
    m_selectedSourceRowsSnapshot.clear();
    m_listView->clearSelection();
    m_listView->doItemsLayout();
    m_proxyModel->sort(0);
    updateSelectionModeUi();
    updateActionStates();
}

void MainWindow::onSelectionCancel() {
    m_selectedSourceRowsSnapshot.clear();
    if (m_listView && m_listView->selectionModel()) {
        m_listView->clearSelection();
    }
    updateSelectionModeUi();
    updateActionStates();
}

void MainWindow::onSelectionSelectAll() {
    if (!m_listView || !m_listView->selectionModel() || !m_listView->model() || !m_proxyModel) {
        return;
    }

    QSet<int> sourceRows;
    const int rowCount = m_listView->model()->rowCount();
    QModelIndex firstProxyIndex;
    for (int proxyRow = 0; proxyRow < rowCount; ++proxyRow) {
        const QModelIndex proxyIndex = m_listView->model()->index(proxyRow, 0);
        const QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
        if (sourceIndex.isValid()) {
            if (!firstProxyIndex.isValid()) {
                firstProxyIndex = proxyIndex;
            }
            sourceRows.insert(sourceIndex.row());
        }
    }
    applySourceSelectionSnapshot(sourceRows, firstProxyIndex, true);
}

void MainWindow::updateActionStates() {
    if (!m_openFolderAction) {
        return;
    }

    const QList<int> rows = selectedSourceRows();
    const int visibleCount = (m_listView && m_listView->model()) ? m_listView->model()->rowCount() : 0;
    const bool hasSelection = !rows.isEmpty();
    m_openFolderAction->setEnabled(hasSelection && rows.size() == 1);
    if (m_selectAllAction) {
        m_selectAllAction->setEnabled(hasSelection && visibleCount > 0 && rows.size() < visibleCount);
    }
    if (m_bulkPauseAction) {
        m_bulkPauseAction->setEnabled(hasSelection);
    }
    if (m_bulkResumeAction) {
        m_bulkResumeAction->setEnabled(hasSelection);
    }
    if (m_bulkDeleteAction) {
        m_bulkDeleteAction->setEnabled(hasSelection);
    }
}

void MainWindow::updateSelectionModeUi() {
    const int selectedCount = selectedSourceRows().size();
    const bool active = selectedCount > 0;
    const bool wasActive = m_selectionModeActive;
    m_selectionModeActive = active;

    if (m_selectionCountLabel) {
        m_selectionCountLabel->setText(tr("%1 selected").arg(selectedCount));
    }

    if (!m_toolbar || !m_toolbarStack || !m_defaultTopWidget || !m_selectionTopWidget ||
        !m_selectionOpacityEffect || !m_selectionOpacityAnim) {
        return;
    }

    if (active) {
        m_toolbar->setStyleSheet(
            "QToolBar { background:#2b3440; border:none; border-bottom:1px solid #3b4a5d; spacing:8px; padding:8px 12px; }"
            "QToolButton { color:#d0d0d0; background:transparent; border:1px solid transparent; padding:8px 12px; border-radius:8px; }"
            "QToolButton:hover { background:#7a8ca11f; }");
        if (!wasActive) {
            m_toolbarStack->setCurrentWidget(m_selectionTopWidget);
            m_selectionOpacityAnim->stop();
            m_selectionOpacityEffect->setOpacity(0.0);
            m_selectionOpacityAnim->setStartValue(0.0);
            m_selectionOpacityAnim->setEndValue(1.0);
            m_selectionOpacityAnim->start();
        } else if (m_toolbarStack->currentWidget() != m_selectionTopWidget) {
            m_toolbarStack->setCurrentWidget(m_selectionTopWidget);
            m_selectionOpacityEffect->setOpacity(1.0);
        }
    } else {
        m_toolbar->setStyleSheet(
            "QToolBar { background:#252526; border:none; border-bottom:1px solid #1a1a1a; spacing:8px; padding:8px 12px; }"
            "QToolButton { color:#d0d0d0; background:transparent; border:1px solid transparent; padding:8px 12px; border-radius:8px; }"
            "QToolButton:hover { background:#35353a; border-color:transparent; }");
        m_selectionOpacityAnim->stop();
        m_selectionOpacityEffect->setOpacity(1.0);
        if (wasActive || m_toolbarStack->currentWidget() != m_defaultTopWidget) {
            m_toolbarStack->setCurrentWidget(m_defaultTopWidget);
        }
    }
}

void MainWindow::updateStorageWidget() {
    if (!m_storageLabel) {
        return;
    }
    const QStorageInfo storage = QStorageInfo::root();
    if (!storage.isValid() || !storage.isReady()) {
        m_storageLabel->setText(tr("Storage unavailable"));
        return;
    }
    const quint64 bytesAvailable = storage.bytesAvailable();
    const quint64 bytesTotal = storage.bytesTotal();
    m_storageLabel->setText(
        tr("Storage Space\nFree: %1\nTotal: %2")
            .arg(FormatUtils::formatBytes(bytesAvailable))
            .arg(FormatUtils::formatBytes(bytesTotal)));
}

void MainWindow::onBulkPause() {
    const QList<int> rows = selectedSourceRows();
    if (rows.isEmpty()) {
        return;
    }
    int attempted = 0;
    for (int row : rows) {
        if (row < 0) {
            continue;
        }
        m_controller->pauseDownload(row);
        ++attempted;
    }
    logInfo(tr("Bulk pause requested for %1 items").arg(attempted));
}

void MainWindow::onBulkResume() {
    const QList<int> rows = selectedSourceRows();
    if (rows.isEmpty()) {
        return;
    }
    int attempted = 0;
    for (int row : rows) {
        if (row < 0) {
            continue;
        }
        m_controller->resumeDownload(row);
        ++attempted;
    }
    logInfo(tr("Bulk resume requested for %1 items").arg(attempted));
}

void MainWindow::onBulkDelete() {
    const QList<int> rows = selectedSourceRows();
    if (rows.isEmpty()) {
        return;
    }
    for (int row : rows) {
        DownloadItem* item = m_controller->manager()->downloadAt(row);
        if (!item) {
            continue;
        }
        if (item->state() == DownloadState::Downloading || item->state() == DownloadState::Paused ||
            item->state() == DownloadState::Queued) {
            item->cancel();
        }
        m_controller->removeDownload(row);
    }
    logInfo(tr("Bulk delete executed for %1 items").arg(rows.size()));
    onSelectionCancel();
}

void MainWindow::updateCountsAndStatus() {
    int allCount = 0;
    int downloadingCount = 0;
    int completedCount = 0;
    int pausedCount = 0;
    int failedCount = 0;
    int videosCount = 0;
    int documentsCount = 0;
    int archivesCount = 0;
    int audioCount = 0;
    int imageCount = 0;
    double totalActiveSpeed = 0.0;

    const auto& downloads = m_controller->manager()->downloads();
    for (DownloadItem* item : downloads) {
        ++allCount;
        const QString ext = QFileInfo(item->filename()).suffix().toLower();
        if (ext == QStringLiteral("mp4") || ext == QStringLiteral("mkv") || ext == QStringLiteral("avi") ||
            ext == QStringLiteral("mov") || ext == QStringLiteral("webm") || ext == QStringLiteral("flv")) {
            ++videosCount;
        } else if (ext == QStringLiteral("pdf") || ext == QStringLiteral("doc") || ext == QStringLiteral("docx") ||
                   ext == QStringLiteral("txt") || ext == QStringLiteral("rtf") || ext == QStringLiteral("ppt") ||
                   ext == QStringLiteral("pptx") || ext == QStringLiteral("xls") || ext == QStringLiteral("xlsx")) {
            ++documentsCount;
        } else if (ext == QStringLiteral("zip") || ext == QStringLiteral("rar") || ext == QStringLiteral("7z") ||
                   ext == QStringLiteral("tar") || ext == QStringLiteral("gz") || ext == QStringLiteral("bz2") ||
                   ext == QStringLiteral("xz")) {
            ++archivesCount;
        } else if (ext == QStringLiteral("mp3") || ext == QStringLiteral("wav") || ext == QStringLiteral("aac") ||
                   ext == QStringLiteral("ogg") || ext == QStringLiteral("flac") || ext == QStringLiteral("m4a")) {
            ++audioCount;
        } else if (ext == QStringLiteral("jpg") || ext == QStringLiteral("jpeg") || ext == QStringLiteral("png") ||
                   ext == QStringLiteral("gif") || ext == QStringLiteral("bmp") || ext == QStringLiteral("svg") ||
                   ext == QStringLiteral("webp")) {
            ++imageCount;
        }
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
    if (m_fileTypeList && m_fileTypeList->count() >= 6) {
        m_fileTypeList->item(0)->setData(kFilterCountRole, allCount);
        m_fileTypeList->item(1)->setData(kFilterCountRole, videosCount);
        m_fileTypeList->item(2)->setData(kFilterCountRole, documentsCount);
        m_fileTypeList->item(3)->setData(kFilterCountRole, archivesCount);
        m_fileTypeList->item(4)->setData(kFilterCountRole, audioCount);
        m_fileTypeList->item(5)->setData(kFilterCountRole, imageCount);
        m_fileTypeList->viewport()->update();
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
    updateStorageWidget();
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
        // logInfo(tr("Progress: file=%1 percent=%2 downloaded=%3 total=%4 speed=%5 eta=%6s")
        //             .arg(item->filename())
        //             .arg(progress.percentage)
        //             .arg(FormatUtils::formatBytes(progress.downloaded_bytes))
        //             .arg(FormatUtils::formatBytes(progress.total_bytes))
        //             .arg(FormatUtils::formatSpeed(progress.speed_bytes_per_sec))
        //             .arg(progress.eta_seconds));
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
        auto* restarted = m_controller->addDownload(url, outputDir, filename);
        if (!restarted) {
            logError(tr("Retry failed: url=%1 path=%2 reason=%3")
                         .arg(url, outputDir, m_controller->manager()->lastError()));
        } else {
            logInfo(tr("Retry started: %1").arg(restarted->savePath()));
        }
    } else if (actionType == DownloadItemDelegate::ActionCopyLocation) {
        QApplication::clipboard()->setText(item->savePath());
        QToolTip::showText(QCursor::pos(), tr("Save location copied"), m_listView);
        logInfo(tr("Copied location: %1").arg(item->savePath()));
    } else if (actionType == DownloadItemDelegate::ActionCopyLink) {
        QApplication::clipboard()->setText(item->url());
        QToolTip::showText(QCursor::pos(), tr("Download URL copied"), m_listView);
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
