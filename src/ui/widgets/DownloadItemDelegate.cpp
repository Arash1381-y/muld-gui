#include "DownloadItemDelegate.h"

#include <QAbstractItemModel>
#include <QDateTime>
#include <QEvent>
#include <QFileInfo>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>

#include "../../models/DownloadListModel.h"
#include "../../utils/FormatUtils.h"

namespace muld_gui {

namespace {
constexpr int kRowHeight = 56;
constexpr int kExpandedExtraHeight = 96;
constexpr int kLeadingWidth = 46;   // checkbox + caret area
constexpr int kProgressWidth = 230;
constexpr int kSpeedWidth = 90;
constexpr int kEtaWidth = 90;
constexpr int kSizeWidth = 90;
constexpr int kStatusWidth = 100;
constexpr int kActionsWidth = 84;
constexpr int kColumnGap = 4;
}  // namespace

DownloadItemDelegate::DownloadItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

void DownloadItemDelegate::setExpandedIndex(const QModelIndex& index) {
  m_expandedIndex = index;
}

void DownloadItemDelegate::clearExpandedIndex() {
  m_expandedIndex = QPersistentModelIndex();
}

QModelIndex DownloadItemDelegate::expandedIndex() const {
  return m_expandedIndex;
}

void DownloadItemDelegate::clearActionFeedback() {
  m_hoveredIndex = QPersistentModelIndex();
  m_pressedIndex = QPersistentModelIndex();
  m_flashedIndex = QPersistentModelIndex();
  m_hoveredAction = 0;
  m_pressedAction = 0;
  m_flashedAction = 0;
  m_flashUntilMs = 0;
  emit repaintRequested();
}

QSize DownloadItemDelegate::sizeHint(const QStyleOptionViewItem& option,
                                     const QModelIndex& index) const {
  const bool expanded = m_expandedIndex.isValid() && m_expandedIndex == index;
  return QSize(option.rect.width(), expanded ? (kRowHeight + kExpandedExtraHeight) : kRowHeight);
}

QRect DownloadItemDelegate::selectionRect(const QRect& rowRect) const {
  return QRect(rowRect.left() + 3, rowRect.top() + (kRowHeight - 16) / 2, 16, 16);
}

QRect DownloadItemDelegate::caretRect(const QRect& rowRect) const {
  return QRect(rowRect.left() + 27, rowRect.top() + (kRowHeight - 16) / 2, 16, 16);
}

QRect DownloadItemDelegate::statusRect(const QRect& rowRect) const {
  const int trailingWidth = kProgressWidth + kSpeedWidth + kEtaWidth + kSizeWidth +
                            kStatusWidth + kActionsWidth + (kColumnGap * 5);
  const int nameWidth = std::max(120, rowRect.width() - kLeadingWidth - trailingWidth);
  const int x = rowRect.left() + kLeadingWidth + nameWidth +
                kProgressWidth + kSpeedWidth + kEtaWidth + kSizeWidth +
                (kColumnGap * 5);
  return QRect(x, rowRect.top(), kStatusWidth, kRowHeight);
}

QRect DownloadItemDelegate::actionsRect(const QRect& rowRect) const {
  const int x = statusRect(rowRect).right() + 1 + kColumnGap;
  return QRect(x, rowRect.top() + 14, 76, 28);
}

QRect DownloadItemDelegate::detailsRect(const QRect& itemRect) const {
  return itemRect.adjusted(22, kRowHeight, -10, -8);
}

void DownloadItemDelegate::actionTypesForState(int state, bool missingPartial,
                                               bool canResume, int* first,
                                               int* second) const {
  switch (state) {
    case 1:  // Downloading
      *first = ActionPause;
      *second = ActionCancel;
      break;
    case 2:  // Paused
      *first = (missingPartial || !canResume) ? ActionRetry : ActionResume;
      *second = ActionCancel;
      break;
    case 5:  // Cancelled
      *first = missingPartial ? ActionRetry : ActionResume;
      *second = ActionRemove;
      break;
    case 3:  // Completed
      *first = ActionOpenFolder;
      *second = ActionRemove;
      break;
    case 4:  // Failed
      *first = ActionRetry;
      *second = ActionRemove;
      break;
    default:
      break;
  }
}

void DownloadItemDelegate::actionRectsForState(const QRect& area, int,
                                               QRect* first,
                                               QRect* second) const {
  *first = QRect(area.left() + 2, area.top(), 34, 28);
  *second = QRect(area.left() + 40, area.top(), 34, 28);
}

QColor DownloadItemDelegate::actionColor(int actionType) const {
  switch (actionType) {
    case ActionPause:
      return QColor("#4fc3f7");
    case ActionResume:
      return QColor("#81c784");
    case ActionCancel:
      return QColor("#d98282");
    case ActionRetry:
      return QColor("#ffb74d");
    case ActionOpenFolder:
      return QColor("#4fc3f7");
    case ActionRemove:
      return QColor("#e57373");
    case ActionCopyLocation:
    case ActionCopyLink:
      return QColor("#cccccc");
    default:
      return QColor("#cccccc");
  }
}

QPixmap DownloadItemDelegate::tintedIcon(const QString& path,
                                         const QColor& color, int size) const {
  const QPixmap src = QIcon(path).pixmap(size, size);
  if (src.isNull()) {
    return {};
  }
  QPixmap scaled = src;
  QPixmap out(scaled.size());
  out.fill(Qt::transparent);
  QPainter p(&out);
  p.drawPixmap(0, 0, scaled);
  p.setCompositionMode(QPainter::CompositionMode_SourceIn);
  p.fillRect(out.rect(), color);
  return out;
}

QString DownloadItemDelegate::actionIconPath(int actionType) const {
  switch (actionType) {
    case ActionPause:
      return QStringLiteral(":/icons/solid/pause.svg");
    case ActionResume:
      return QStringLiteral(":/icons/solid/play.svg");
    case ActionRetry:
      return QStringLiteral(":/icons/solid/arrow-path.svg");
    case ActionOpenFolder:
      return QStringLiteral(":/icons/solid/folder-open.svg");
    case ActionRemove:
      return QStringLiteral(":/icons/solid/trash.svg");
    case ActionCancel:
    default:
      return QStringLiteral(":/icons/solid/x-mark.svg");
  }
}

QString DownloadItemDelegate::actionText(int actionType) const {
  switch (actionType) {
    case ActionPause:
      return tr("Pause");
    case ActionResume:
      return tr("Resume");
    case ActionCancel:
      return tr("Cancel");
    case ActionRetry:
      return tr("Retry");
    case ActionOpenFolder:
      return tr("Open Folder");
    case ActionRemove:
      return tr("Delete");
    case ActionCopyLocation:
      return tr("Copy Path");
    case ActionCopyLink:
      return tr("Copy Link");
    default:
      return tr("Action");
  }
}

QColor DownloadItemDelegate::actionBgColor(int actionType, bool hovered,
                                           bool pressed) const {
  QColor base = actionColor(actionType);
  if (pressed) {
    base.setAlpha(95);
    return base;
  }
  if (hovered) {
    base.setAlpha(55);
    return base;
  }
  return QColor(45, 45, 48, 0);
}

void DownloadItemDelegate::flashAction(const QModelIndex& index, int actionType) {
  m_flashedIndex = index;
  m_flashedAction = actionType;
  m_flashUntilMs = QDateTime::currentMSecsSinceEpoch() + 800;
}

bool DownloadItemDelegate::isActionFlashed(const QModelIndex& index, int actionType) const {
  if (!m_flashedIndex.isValid() || m_flashedIndex != index || m_flashedAction != actionType) {
    return false;
  }
  return QDateTime::currentMSecsSinceEpoch() <= m_flashUntilMs;
}

void DownloadItemDelegate::paint(QPainter* painter,
                                 const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const {
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, false);
  painter->fillRect(option.rect, QColor("#1e1e1e"));

  if (option.state & QStyle::State_Selected) {
    painter->fillRect(option.rect, QColor("#2d2d30"));
  } else if (option.state & QStyle::State_MouseOver) {
    painter->fillRect(option.rect, QColor("#303033"));
  }

  const QRect rowRect = option.rect.adjusted(10, 0, -10, 0);
  const bool expanded = m_expandedIndex.isValid() && m_expandedIndex == index;

  const int trailingWidth = kProgressWidth + kSpeedWidth + kEtaWidth + kSizeWidth +
                            kStatusWidth + kActionsWidth + (kColumnGap * 5);
  const int nameWidth = std::max(120, rowRect.width() - kLeadingWidth - trailingWidth);
  const QRect nameRect(rowRect.left() + kLeadingWidth, rowRect.top(), nameWidth, kRowHeight);
  const QRect progressRect(nameRect.right() + kColumnGap, rowRect.top(), kProgressWidth, kRowHeight);
  const QRect speedRect(progressRect.right() + kColumnGap, rowRect.top(), kSpeedWidth, kRowHeight);
  const QRect etaRect(speedRect.right() + kColumnGap, rowRect.top(), kEtaWidth, kRowHeight);
  const QRect sizeRect(etaRect.right() + kColumnGap, rowRect.top(), kSizeWidth, kRowHeight);

  const QString filename =
      index.data(DownloadListModel::FilenameRole).toString();
  const QString url = index.data(DownloadListModel::UrlRole).toString();
  const QString savePath =
      index.data(DownloadListModel::SavePathRole).toString();
  const int percent = index.data(DownloadListModel::PercentRole).toInt();
  const int state = index.data(DownloadListModel::StateRole).toInt();
  const quint64 downloaded =
      index.data(DownloadListModel::DownloadedRole).toULongLong();
  const quint64 totalSize =
      index.data(DownloadListModel::TotalSizeRole).toULongLong();
  const double speed = index.data(DownloadListModel::SpeedRole).toDouble();
  const double eta = index.data(DownloadListModel::EtaRole).toDouble();
  const QString completedAt =
      index.data(DownloadListModel::CompletedAtRole).toString();
  const QString chunkInfo =
      index.data(DownloadListModel::ChunkInfoRole).toString();
  const bool hasHandler =
      index.data(DownloadListModel::HasHandlerRole).toBool();
  const bool missingPartial =
      index.data(DownloadListModel::MissingPartialRole).toBool();

  const bool isSelected = (option.state & QStyle::State_Selected);
  const QRect selectRect = selectionRect(rowRect);
  painter->setPen(QColor("#6f6f73"));
  painter->setBrush(isSelected ? QColor("#007acc") : QColor("#1e1e1e"));
  painter->drawRoundedRect(selectRect, 3, 3);
  if (isSelected) {
    const int checkSize = 12;
    const QPixmap check = tintedIcon(QStringLiteral(":/icons/solid/check.svg"),
                                     QColor("#ffffff"), checkSize);
    const int checkX = selectRect.x() + (selectRect.width() - checkSize) / 2;
    const int checkY = selectRect.y() + (selectRect.height() - checkSize) / 2;
    painter->drawPixmap(checkX, checkY, check);
  }

  const int caretSize = 16;
  const QRect caretSlot = caretRect(rowRect);
  const QPixmap caret =
      tintedIcon(expanded ? QStringLiteral(":/icons/solid/chevron-up.svg")
                          : QStringLiteral(":/icons/solid/chevron-down.svg"),
                 QColor("#858585"), caretSize);
  const int caretX = caretSlot.x() + (caretSlot.width() - caretSize) / 2;
  const int caretY = caretSlot.y() + (caretSlot.height() - caretSize) / 2;
  painter->drawPixmap(caretX, caretY, caret);

  QFont nameFont(QStringLiteral("Segoe UI"), 10);
  nameFont.setWeight(QFont::Medium);
  nameFont.setStrikeOut(missingPartial && state != 3);
  painter->setFont(nameFont);
  painter->setPen(QColor("#cccccc"));
  const QRect titleRect = nameRect.adjusted(0, 7, 0, -30);
  painter->save();
  painter->setClipRect(titleRect);
  painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                    painter->fontMetrics().elidedText(filename, Qt::ElideMiddle,
                                                      titleRect.width()));
  painter->restore();

  QFont metaFont(QStringLiteral("Segoe UI"), 8);
  painter->setFont(metaFont);
  painter->setPen(QColor("#5a5a5a"));
  const QRect urlRect = nameRect.adjusted(0, 30, 0, -8);
  painter->save();
  painter->setClipRect(urlRect);
  painter->drawText(
      urlRect, Qt::AlignLeft | Qt::AlignVCenter,
      painter->fontMetrics().elidedText(url, Qt::ElideMiddle, urlRect.width()));
  painter->restore();

  const QRect progressBarRect = progressRect.adjusted(8, 25, -36, -25);
  drawProgressBar(painter, progressBarRect, percent, state);
  painter->setPen(QColor("#cccccc"));
  if (state == 0) {
    painter->drawText(progressRect.adjusted(0, 0, -4, 0),
                      Qt::AlignRight | Qt::AlignVCenter, tr(""));
  } else {
    painter->drawText(progressRect.adjusted(0, 0, -1, 0),
                      Qt::AlignRight | Qt::AlignVCenter,
                      QStringLiteral("%1%").arg(percent));
  }

  painter->setPen(QColor("#858585"));
  painter->drawText(speedRect, Qt::AlignCenter,
                    FormatUtils::formatSpeed(speed));
  painter->drawText(
      etaRect, Qt::AlignCenter,
      state == 3 ? tr("Done %1").arg(completedAt.isEmpty() ? tr("--:--:--")
                                                           : completedAt)
                 : FormatUtils::formatEta(eta));
  painter->drawText(sizeRect, Qt::AlignCenter,
                    FormatUtils::formatBytes(totalSize));
  drawStatusBadge(painter, statusRect(rowRect).adjusted(8, 10, -8, -10), state);

  const bool showActions = (option.state & QStyle::State_MouseOver) ||
                           (option.state & QStyle::State_Selected);
  if (showActions) {
    QRect firstRect;
    QRect secondRect;
    int firstAction = 0;
    int secondAction = 0;
    actionRectsForState(actionsRect(rowRect), state, &firstRect, &secondRect);
    actionTypesForState(state, missingPartial, hasHandler, &firstAction,
                        &secondAction);

    const int iconSize = 20;
    const bool firstHovered = m_hoveredIndex == index && m_hoveredAction == firstAction;
    const bool secondHovered = m_hoveredIndex == index && m_hoveredAction == secondAction;
    const bool firstPressed = m_pressedIndex == index && m_pressedAction == firstAction;
    const bool secondPressed = m_pressedIndex == index && m_pressedAction == secondAction;
    const bool firstFlashed = isActionFlashed(index, firstAction);
    const bool secondFlashed = isActionFlashed(index, secondAction);

    const QRect firstButtonRect = firstRect.adjusted(0, 1, 0, -1);
    const QRect secondButtonRect = secondRect.adjusted(0, 1, 0, -1);

    if (firstAction != 0) {
      painter->setPen(QPen(QColor("#5a5a5f")));
      painter->setBrush(firstFlashed ? QColor("#007acc") : actionBgColor(firstAction, firstHovered, firstPressed));
      painter->drawRoundedRect(firstButtonRect, 4, 4);
    }
    if (secondAction != 0) {
      painter->setPen(QPen(QColor("#5a5a5f")));
      painter->setBrush(secondFlashed ? QColor("#007acc") : actionBgColor(secondAction, secondHovered, secondPressed));
      painter->drawRoundedRect(secondButtonRect, 4, 4);
    }

    if (firstAction != 0) {
      const QPixmap firstIcon =
          tintedIcon(actionIconPath(firstAction), actionColor(firstAction), iconSize);
      const int firstX = firstButtonRect.x() + (firstButtonRect.width() - iconSize) / 2;
      const int firstY = firstButtonRect.y() + (firstButtonRect.height() - iconSize) / 2;
      painter->drawPixmap(firstX, firstY, firstIcon);
    }
    if (secondAction != 0) {
      const QPixmap secondIcon =
          tintedIcon(actionIconPath(secondAction), actionColor(secondAction), iconSize);
      const int secondX = secondButtonRect.x() + (secondButtonRect.width() - iconSize) / 2;
      const int secondY = secondButtonRect.y() + (secondButtonRect.height() - iconSize) / 2;
      painter->drawPixmap(secondX, secondY, secondIcon);
    }

    int hoveredAction = 0;
    QRect hoveredRect;
    if (firstHovered) {
      hoveredAction = firstAction;
      hoveredRect = firstRect;
    } else if (secondHovered) {
      hoveredAction = secondAction;
      hoveredRect = secondRect;
    }
    if (hoveredAction != 0) {
      const QString label = actionText(hoveredAction);
      const int textWidth = painter->fontMetrics().horizontalAdvance(label) + 14;
      QRect tipRect(hoveredRect.center().x() - textWidth / 2, hoveredRect.top() - 15, textWidth, 16);
      if (tipRect.left() < option.rect.left() + 6) {
        tipRect.moveLeft(option.rect.left() + 6);
      }
      if (tipRect.right() > option.rect.right() - 6) {
        tipRect.moveRight(option.rect.right() - 6);
      }
      painter->setPen(Qt::NoPen);
      painter->setBrush(QColor("#0f0f10"));
      painter->drawRoundedRect(tipRect, 6, 6);
      painter->setPen(QColor("#d7d7d7"));
      painter->setFont(QFont(QStringLiteral("Segoe UI"), 7, QFont::Medium));
      painter->drawText(tipRect, Qt::AlignCenter, label);
    }
  }

  painter->setPen(QColor("#3e3e42"));
  painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());

  if (expanded) {
    const QRect panel = detailsRect(option.rect);
    painter->fillRect(panel, QColor("#252526"));
    painter->setPen(QColor("#3e3e42"));
    painter->drawRect(panel.adjusted(0, 0, -1, -1));

    painter->setFont(QFont(QStringLiteral("Segoe UI"), 8));
    painter->setPen(QColor("#858585"));
    painter->drawText(panel.adjusted(10, 8, -10, -34), Qt::AlignLeft,
                      tr("SAVE LOCATION"));
    painter->drawText(panel.adjusted(panel.width() / 2, 8, -10, -34),
                      Qt::AlignLeft, tr("CONNECTIONS"));
    painter->drawText(panel.adjusted(10, 42, -10, -8), Qt::AlignLeft,
                      tr("LINK DESTINATION"));
    painter->drawText(panel.adjusted(panel.width() / 2, 42, -10, -8),
                      Qt::AlignLeft, tr("CHUNK INFO"));

    painter->setPen(QColor("#cccccc"));
    painter->drawText(panel.adjusted(10, 20, -10, -22), Qt::AlignLeft,
                      painter->fontMetrics().elidedText(
                          savePath, Qt::ElideMiddle, panel.width() / 2 - 20));
    painter->drawText(panel.adjusted(panel.width() / 2, 20, -10, -22),
                      Qt::AlignLeft, tr("Auto"));
    painter->drawText(panel.adjusted(10, 54, -28, 0), Qt::AlignLeft,
                      painter->fontMetrics().elidedText(
                          url, Qt::ElideMiddle, panel.width() / 2 - 40));

    const QRect copyPathRect(panel.left() + 112, panel.top() + 8, 14, 14);
    const QRect copyLinkRect(panel.left() + 118, panel.top() + 42, 14, 14);
    const bool copyPathFlashed = isActionFlashed(index, ActionCopyLocation);
    const bool copyLinkFlashed = isActionFlashed(index, ActionCopyLink);
    if (copyPathFlashed) {
      painter->setPen(Qt::NoPen);
      painter->setBrush(QColor("#007acc"));
      painter->drawRoundedRect(copyPathRect.adjusted(-2, -2, 2, 2), 3, 3);
    }
    if (copyLinkFlashed) {
      painter->setPen(Qt::NoPen);
      painter->setBrush(QColor("#007acc"));
      painter->drawRoundedRect(copyLinkRect.adjusted(-2, -2, 2, 2), 3, 3);
    }
    painter->drawPixmap(
        copyPathRect.topLeft(),
        tintedIcon(QStringLiteral(":/icons/solid/clipboard-document.svg"),
                   copyPathFlashed ? QColor("#ffffff") : QColor("#b8b8b8"), 14));
    painter->drawPixmap(
        copyLinkRect.topLeft(),
        tintedIcon(QStringLiteral(":/icons/solid/clipboard-document.svg"),
                   copyLinkFlashed ? QColor("#ffffff") : QColor("#b8b8b8"), 14));

    painter->drawText(
        panel.adjusted(panel.width() / 2, 54, -10, 0), Qt::AlignLeft,
        painter->fontMetrics().elidedText(
            chunkInfo.isEmpty() ? tr("No chunk data yet") : chunkInfo,
            Qt::ElideRight, panel.width() / 2 - 20));
  }

  painter->restore();
}

bool DownloadItemDelegate::editorEvent(QEvent* event, QAbstractItemModel* model,
                                       const QStyleOptionViewItem& option,
                                       const QModelIndex& index) {
  Q_UNUSED(model);
  if (event->type() != QEvent::MouseButtonRelease &&
      event->type() != QEvent::MouseButtonPress &&
      event->type() != QEvent::MouseMove &&
      event->type() != QEvent::Leave) {
    return QStyledItemDelegate::editorEvent(event, model, option, index);
  }

  if (event->type() == QEvent::Leave) {
    m_hoveredIndex = QPersistentModelIndex();
    m_hoveredAction = 0;
    emit repaintRequested();
    return false;
  }

  auto* mouseEvent = static_cast<QMouseEvent*>(event);
  const QRect rowRect = option.rect.adjusted(10, 0, -10, 0);
  if (selectionRect(rowRect).contains(mouseEvent->pos()) &&
      event->type() == QEvent::MouseButtonPress) {
    return true;
  }
  if (selectionRect(rowRect).contains(mouseEvent->pos()) &&
      event->type() == QEvent::MouseButtonRelease) {
    emit selectionToggled(index);
    return true;
  }

  QRect firstRect;
  QRect secondRect;
  int firstAction = 0;
  int secondAction = 0;
  const int state = index.data(DownloadListModel::StateRole).toInt();
  const bool hasHandler =
      index.data(DownloadListModel::HasHandlerRole).toBool();
  const bool missingPartial =
      index.data(DownloadListModel::MissingPartialRole).toBool();
  actionRectsForState(actionsRect(rowRect), state, &firstRect, &secondRect);
  actionTypesForState(state, missingPartial, hasHandler, &firstAction,
                      &secondAction);

  int hoveredAction = 0;
  if (firstAction != 0 && firstRect.contains(mouseEvent->pos())) {
    hoveredAction = firstAction;
  } else if (secondAction != 0 && secondRect.contains(mouseEvent->pos())) {
    hoveredAction = secondAction;
  }

  if (event->type() == QEvent::MouseMove) {
    m_hoveredIndex = hoveredAction != 0 ? QPersistentModelIndex(index) : QPersistentModelIndex();
    m_hoveredAction = hoveredAction;
    emit repaintRequested();
    return false;
  }

  if (event->type() == QEvent::MouseButtonPress) {
    m_pressedIndex = hoveredAction != 0 ? QPersistentModelIndex(index) : QPersistentModelIndex();
    m_pressedAction = hoveredAction;
    emit repaintRequested();
    return hoveredAction != 0;
  }

  m_pressedIndex = QPersistentModelIndex();
  m_pressedAction = 0;

  if (caretRect(rowRect).contains(mouseEvent->pos())) {
    emit expandToggled(index);
    emit repaintRequested();
    return true;
  }

  if (firstRect.contains(mouseEvent->pos())) {
    flashAction(index, firstAction);
    emit actionClicked(index, firstAction);
    emit repaintRequested();
    return true;
  }
  if (secondRect.contains(mouseEvent->pos())) {
    flashAction(index, secondAction);
    emit actionClicked(index, secondAction);
    emit repaintRequested();
    return true;
  }

  if (m_expandedIndex.isValid() && m_expandedIndex == index) {
    const QRect panel = detailsRect(option.rect);
    const QRect copyPathRect(panel.left() + 112, panel.top() + 8, 14, 14);
    const QRect copyLinkRect(panel.left() + 118, panel.top() + 42, 14, 14);
    if (copyPathRect.contains(mouseEvent->pos())) {
      flashAction(index, ActionCopyLocation);
      emit actionClicked(index, ActionCopyLocation);
      emit repaintRequested();
      return true;
    }
    if (copyLinkRect.contains(mouseEvent->pos())) {
      flashAction(index, ActionCopyLink);
      emit actionClicked(index, ActionCopyLink);
      emit repaintRequested();
      return true;
    }
  }

  emit repaintRequested();
  return QStyledItemDelegate::editorEvent(event, model, option, index);
}

void DownloadItemDelegate::drawStatusBadge(QPainter* painter, const QRect& rect,
                                           int state) const {
  QColor bg("#3c3c3c");
  QColor fg("#cccccc");

  switch (state) {
    case 0:
      bg = QColor("#2d2d30");
      fg = QColor("#858585");
      break;
    case 1:
      bg = QColor("#1a3a52");
      fg = QColor("#4fc3f7");
      break;
    case 2:
      bg = QColor("#3a2e10");
      fg = QColor("#ffb74d");
      break;
    case 3:
      bg = QColor("#1a3a1a");
      fg = QColor("#81c784");
      break;
    case 4:
      bg = QColor("#3a1010");
      fg = QColor("#e57373");
      break;
    default:
      break;
  }

  painter->setPen(Qt::NoPen);
  painter->setBrush(bg);
  painter->drawRoundedRect(rect, 3, 3);
  painter->setPen(fg);
  painter->setFont(QFont(QStringLiteral("Segoe UI"), 7, QFont::Medium));
  painter->drawText(rect, Qt::AlignCenter, FormatUtils::stateToString(state));
}

void DownloadItemDelegate::drawProgressBar(QPainter* painter, const QRect& rect,
                                           int percent, int state) const {
  painter->setPen(Qt::NoPen);
  painter->setBrush(QColor("#3c3c3c"));
  painter->drawRect(rect);

  if (state == 0) {
    painter->setBrush(QColor("#555555"));
    for (int x = rect.left(); x < rect.right(); x += 8) {
      painter->drawRect(QRect(x, rect.top(), 4, rect.height()));
    }
    return;
  }

  QColor fillColor;
  switch (state) {
    case 1:
      fillColor = QColor("#007acc");
      break;
    case 3:
      fillColor = QColor("#4caf50");
      break;
    case 4:
      fillColor = QColor("#c0392b");
      break;
    default:
      fillColor = QColor("#007acc");
      break;
  }

  const int boundedPercent = state == 3 ? 100 : percent;
  int fillWidth = static_cast<int>(rect.width() * (boundedPercent / 100.0));
  if (state == 1 && fillWidth <= 0) {
    fillWidth = 4;
  }
  if (fillWidth <= 0) return;

  QRect fillRect(rect.left(), rect.top(), fillWidth, rect.height());
  painter->setBrush(fillColor);
  painter->drawRect(fillRect);
}

}  // namespace muld_gui
