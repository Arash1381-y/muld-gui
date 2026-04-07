#include "DownloadItemDelegate.h"

#include <QAbstractItemModel>
#include <QEvent>
#include <QFileInfo>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>

#include "../../models/DownloadListModel.h"
#include "../../utils/FormatUtils.h"

namespace muld_gui {

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

QSize DownloadItemDelegate::sizeHint(const QStyleOptionViewItem& option,
                                     const QModelIndex& index) const {
  const bool expanded = m_expandedIndex.isValid() && m_expandedIndex == index;
  return QSize(option.rect.width(), expanded ? 132 : 42);
}

QRect DownloadItemDelegate::caretRect(const QRect& rowRect) const {
  return QRect(rowRect.left() + 3, rowRect.top() + 13, 16, 16);
}

QRect DownloadItemDelegate::statusRect(const QRect& rowRect) const {
  return QRect(rowRect.right() - 188, rowRect.top(), 100, 42);
}

QRect DownloadItemDelegate::actionsRect(const QRect& rowRect) const {
  return QRect(rowRect.right() - 84, rowRect.top() + 7, 76, 28);
}

QRect DownloadItemDelegate::detailsRect(const QRect& itemRect) const {
  return itemRect.adjusted(22, 42, -10, -8);
}

void DownloadItemDelegate::actionTypesForState(int state, bool missingPartial, bool canResume,
                                               int* first, int* second) const {
  *first = ActionCancel;
  *second = ActionCancel;

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
      *second = ActionCancel;
      break;
    case 3:  // Completed
      *first = ActionOpenFolder;
      *second = ActionRemove;
      break;
    case 4:  // Failed
      *first = ActionRetry;
      *second = ActionCancel;
      break;
    default:
      break;
  }
}

void DownloadItemDelegate::actionRectsForState(const QRect& area, int,
                                               QRect* first, QRect* second) const {
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

QPixmap DownloadItemDelegate::tintedIcon(const QString& path, const QColor& color, int size) const {
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

  const int nameWidth = rowRect.width() - 230 - 90 - 90 - 90 - 100 - 84 - 28;
  const QRect nameRect(rowRect.left() + 20, rowRect.top(), nameWidth, 42);
  const QRect progressRect(nameRect.right() + 4, rowRect.top(), 230, 42);
  const QRect speedRect(progressRect.right() + 4, rowRect.top(), 90, 42);
  const QRect etaRect(speedRect.right() + 4, rowRect.top(), 90, 42);
  const QRect sizeRect(etaRect.right() + 4, rowRect.top(), 90, 42);

  const QString filename = index.data(DownloadListModel::FilenameRole).toString();
  const QString url = index.data(DownloadListModel::UrlRole).toString();
  const QString savePath = index.data(DownloadListModel::SavePathRole).toString();
  const int percent = index.data(DownloadListModel::PercentRole).toInt();
  const int state = index.data(DownloadListModel::StateRole).toInt();
  const quint64 downloaded = index.data(DownloadListModel::DownloadedRole).toULongLong();
  const quint64 totalSize = index.data(DownloadListModel::TotalSizeRole).toULongLong();
  const double speed = index.data(DownloadListModel::SpeedRole).toDouble();
  const double eta = index.data(DownloadListModel::EtaRole).toDouble();
  const QString completedAt = index.data(DownloadListModel::CompletedAtRole).toString();
  const QString chunkInfo = index.data(DownloadListModel::ChunkInfoRole).toString();
  const bool hasHandler = index.data(DownloadListModel::HasHandlerRole).toBool();
  const bool missingPartial = index.data(DownloadListModel::MissingPartialRole).toBool();

  const QPixmap caret = tintedIcon(expanded ? QStringLiteral(":/icons/solid/chevron-up.svg")
                                            : QStringLiteral(":/icons/solid/chevron-down.svg"),
                                   QColor("#858585"), 16);
  painter->drawPixmap(caretRect(rowRect).topLeft(), caret);

  QFont nameFont(QStringLiteral("Segoe UI"), 10);
  nameFont.setWeight(QFont::Medium);
  nameFont.setStrikeOut(missingPartial && state != 3);
  painter->setFont(nameFont);
  painter->setPen(QColor("#cccccc"));
  const QRect titleRect = nameRect.adjusted(0, 4, 0, -20);
  painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                    painter->fontMetrics().elidedText(filename, Qt::ElideMiddle, titleRect.width()));

  QFont metaFont(QStringLiteral("Segoe UI"), 8);
  painter->setFont(metaFont);
  painter->setPen(QColor("#5a5a5a"));
  const QRect urlRect = nameRect.adjusted(0, 20, 0, -2);
  painter->drawText(urlRect, Qt::AlignLeft | Qt::AlignVCenter,
                    painter->fontMetrics().elidedText(url, Qt::ElideMiddle, urlRect.width()));

  const QRect progressBarRect = progressRect.adjusted(8, 18, -36, -18);
  drawProgressBar(painter, progressBarRect, percent, state);
  painter->setPen(QColor("#cccccc"));
  if (state == 0) {
    painter->drawText(progressRect.adjusted(0, 0, -8, 0), Qt::AlignRight | Qt::AlignVCenter,
                      tr("Queued"));
  } else {
    painter->drawText(progressRect.adjusted(0, 0, -8, 0), Qt::AlignRight | Qt::AlignVCenter,
                      QStringLiteral("%1%").arg(percent));
  }

  painter->setPen(QColor("#858585"));
  painter->drawText(speedRect, Qt::AlignCenter, FormatUtils::formatSpeed(speed));
  painter->drawText(etaRect, Qt::AlignCenter,
                    state == 3 ? tr("Done %1").arg(completedAt.isEmpty() ? tr("--:--:--") : completedAt)
                               : FormatUtils::formatEta(eta));
  painter->drawText(sizeRect, Qt::AlignCenter, FormatUtils::formatBytes(totalSize));
  drawStatusBadge(painter, statusRect(rowRect).adjusted(8, 10, -8, -10), state);

  const bool showActions = (option.state & QStyle::State_MouseOver) || (option.state & QStyle::State_Selected);
  if (showActions) {
    QRect firstRect;
    QRect secondRect;
    int firstAction;
    int secondAction;
    actionRectsForState(actionsRect(rowRect), state, &firstRect, &secondRect);
    actionTypesForState(state, missingPartial, hasHandler, &firstAction, &secondAction);

    const int iconSize = 20;
    painter->drawPixmap(firstRect.center().x() - iconSize / 2, firstRect.center().y() - iconSize / 2,
                        tintedIcon(actionIconPath(firstAction), actionColor(firstAction), iconSize));
    painter->drawPixmap(secondRect.center().x() - iconSize / 2, secondRect.center().y() - iconSize / 2,
                        tintedIcon(actionIconPath(secondAction), actionColor(secondAction), iconSize));
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
    painter->drawText(panel.adjusted(panel.width() / 2, 8, -10, -34), Qt::AlignLeft,
                      tr("CONNECTIONS"));
    painter->drawText(panel.adjusted(10, 42, -10, -8), Qt::AlignLeft,
                      tr("LINK DESTINATION"));
    painter->drawText(panel.adjusted(panel.width() / 2, 42, -10, -8), Qt::AlignLeft,
                      tr("CHUNK INFO"));

    painter->setPen(QColor("#cccccc"));
    painter->drawText(panel.adjusted(10, 20, -10, -22), Qt::AlignLeft,
                      painter->fontMetrics().elidedText(savePath, Qt::ElideMiddle, panel.width() / 2 - 20));
    painter->drawText(panel.adjusted(panel.width() / 2, 20, -10, -22), Qt::AlignLeft,
                      tr("Auto"));
    painter->drawText(panel.adjusted(10, 54, -28, 0), Qt::AlignLeft,
                      painter->fontMetrics().elidedText(url, Qt::ElideMiddle, panel.width() / 2 - 40));

    const QRect copyPathRect(panel.left() + 112, panel.top() + 8, 14, 14);
    const QRect copyLinkRect(panel.left() + 118, panel.top() + 42, 14, 14);
    painter->drawPixmap(copyPathRect.topLeft(),
                        tintedIcon(QStringLiteral(":/icons/solid/clipboard-document.svg"), QColor("#b8b8b8"), 14));
    painter->drawPixmap(copyLinkRect.topLeft(),
                        tintedIcon(QStringLiteral(":/icons/solid/clipboard-document.svg"), QColor("#b8b8b8"), 14));

    painter->drawText(panel.adjusted(panel.width() / 2, 54, -10, 0), Qt::AlignLeft,
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
  if (event->type() != QEvent::MouseButtonRelease) {
    return QStyledItemDelegate::editorEvent(event, model, option, index);
  }

  auto* mouseEvent = static_cast<QMouseEvent*>(event);
  const QRect rowRect = option.rect.adjusted(10, 0, -10, 0);
  if (caretRect(rowRect).contains(mouseEvent->pos())) {
    emit expandToggled(index);
    return true;
  }

  QRect firstRect;
  QRect secondRect;
  int firstAction;
  int secondAction;
  const int state = index.data(DownloadListModel::StateRole).toInt();
  const bool hasHandler = index.data(DownloadListModel::HasHandlerRole).toBool();
  const bool missingPartial = index.data(DownloadListModel::MissingPartialRole).toBool();
  actionRectsForState(actionsRect(rowRect), state, &firstRect, &secondRect);
  actionTypesForState(state, missingPartial, hasHandler, &firstAction, &secondAction);

  if (firstRect.contains(mouseEvent->pos())) {
    emit actionClicked(index, firstAction);
    return true;
  }
  if (secondRect.contains(mouseEvent->pos())) {
    emit actionClicked(index, secondAction);
    return true;
  }

  if (m_expandedIndex.isValid() && m_expandedIndex == index) {
    const QRect panel = detailsRect(option.rect);
    const QRect copyPathRect(panel.left() + 112, panel.top() + 8, 14, 14);
    const QRect copyLinkRect(panel.left() + 118, panel.top() + 42, 14, 14);
    if (copyPathRect.contains(mouseEvent->pos())) {
      emit actionClicked(index, ActionCopyLocation);
      return true;
    }
    if (copyLinkRect.contains(mouseEvent->pos())) {
      emit actionClicked(index, ActionCopyLink);
      return true;
    }
  }

  return QStyledItemDelegate::editorEvent(event, model, option, index);
}

void DownloadItemDelegate::drawStatusBadge(QPainter* painter, const QRect& rect, int state) const {
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
