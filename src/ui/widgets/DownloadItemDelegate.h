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
  void drawStatusBadge(QPainter* painter, const QRect& rect, int state) const;
  void drawProgressBar(QPainter* painter, const QRect& rect,

                       int percent, int state) const;

  QPersistentModelIndex m_expandedIndex;
};

}  // namespace muld_gui
