#pragma once

#include <QPersistentModelIndex>
#include <QStyledItemDelegate>

namespace muld_gui {

class DownloadItemDelegate : public QStyledItemDelegate {
  Q_OBJECT

 public:
  enum QuickAction {
    ActionPause = 1,
    ActionResume,
    ActionCancel,
    ActionRetry,
    ActionOpenFolder,
    ActionRemove,
    ActionCopyLocation,
    ActionCopyLink
  };

  explicit DownloadItemDelegate(QObject* parent = nullptr);

  void setExpandedIndex(const QModelIndex& index);
  void clearExpandedIndex();
  QModelIndex expandedIndex() const;
  void clearActionFeedback();

  void paint(QPainter* painter, const QStyleOptionViewItem& option,

             const QModelIndex& index) const override;

  QSize sizeHint(const QStyleOptionViewItem& option,

                 const QModelIndex& index) const override;
  bool editorEvent(QEvent* event, QAbstractItemModel* model,
                   const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;

 signals:
  void actionClicked(const QModelIndex& index, int action);
  void expandToggled(const QModelIndex& index);
  void selectionToggled(const QModelIndex& index);
  void repaintRequested();

 private:
  QRect caretRect(const QRect& rowRect) const;
  QRect statusRect(const QRect& rowRect) const;
  QRect actionsRect(const QRect& rowRect) const;
  QRect detailsRect(const QRect& itemRect) const;
  void actionRectsForState(const QRect& area, int state, QRect* first, QRect* second) const;
  void actionTypesForState(int state, bool missingPartial, bool canResume, int* first, int* second) const;
  QColor actionColor(int actionType) const;
  QPixmap tintedIcon(const QString& path, const QColor& color, int size) const;
  QString actionIconPath(int actionType) const;
  QString actionText(int actionType) const;
  QColor actionBgColor(int actionType, bool hovered, bool pressed) const;
  QRect selectionRect(const QRect& rowRect) const;
  void drawStatusBadge(QPainter* painter, const QRect& rect, int state) const;
  void drawProgressBar(QPainter* painter, const QRect& rect,

                       int percent, int state) const;
  void flashAction(const QModelIndex& index, int actionType);
  bool isActionFlashed(const QModelIndex& index, int actionType) const;

  QPersistentModelIndex m_expandedIndex;
  QPersistentModelIndex m_hoveredIndex;
  QPersistentModelIndex m_pressedIndex;
  QPersistentModelIndex m_flashedIndex;
  int m_hoveredAction = 0;
  int m_pressedAction = 0;
  int m_flashedAction = 0;
  qint64 m_flashUntilMs = 0;
};

}  // namespace muld_gui
