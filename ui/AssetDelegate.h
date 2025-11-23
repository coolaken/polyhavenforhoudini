#ifndef ASSETDELEGATE_H
#define ASSETDELEGATE_H

#include <QtWidgets/QStyledItemDelegate>
#include <QtGui/QPainter>
#include <QtGui/QMouseEvent>
#include <QtGui/QDrag>
#include <QtCore/QMimeData>
#include <QtCore/QFile>
#include <QtGui/QStandardItemModel>
#include <QtCore/QSet>
#include <QtCore/QVector>
#include <QtCore/QStringList>
#include <QtCore/qcache.h>
#include <QtCore/qmutex.h>

// 自定义角色：存储目录完整路径（与 Python 中的 CATALOG_PATH_ROLE 对应）
const int CATALOG_PATH_ROLE = Qt::UserRole + 100;

class AssetDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit AssetDelegate(QObject* parent = nullptr);
    ~AssetDelegate() override;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override;

    // 供外部设置缓存（由主窗口传递，共享缓存）
    void setThumbCache(QCache<QString, QPixmap>* cache);

    void loadThumbInThread(const QString& imgPath) const;
private:
    void startDrag(const QModelIndex& index);
    // 新增：判断 WebP 文件是否完整
    bool isWebpComplete(const QString& imgPath) const;

private:
    QSize m_cardSize;                  // 卡片固定尺寸
    QPoint m_dragStartPos;             // 拖动起始位置
    QVariantMap m_draggedAsset;        // 拖动的资产数据
    QCache<QString, QPixmap>* m_thumbCache = nullptr; // 缩略图缓存（外部传入，共享）
    mutable QPointer<QThread> m_loadThread = nullptr; // 临时加载线程（mutable 允许 const 函数中修改）
    mutable QMutex m_cacheMutex;
};



#endif // ASSETDELEGATE_H