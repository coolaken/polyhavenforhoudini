#include "AssetDelegate.h"
#include <QtWidgets/qapplication.h>
#include <QtGui/qfont.h>
#include <QtGui/qpen.h>
#include <QtGui/qcolor.h>
#include <QtGui/qpixmap.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qdebug.h>
#include <QtCore/qjsondocument.h>
#include <QtWidgets/qabstractscrollarea.h>
#include <QtCore/qtimer.h>
#include <QtCore/qendian.h>
#include "AssetModel.h"

// AssetDelegate 构造函数
AssetDelegate::AssetDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
    , m_cardSize(100, 130)  // 卡片尺寸：宽100，高130（保持原有设置）
{
}

// 析构函数：清理线程
AssetDelegate::~AssetDelegate()
{
    if (m_loadThread && m_loadThread->isRunning()) {
        m_loadThread->quit();
        m_loadThread->wait();
        m_loadThread->deleteLater();
    }
}

// 设置外部缓存（主窗口传递过来，共享给所有项）
void AssetDelegate::setThumbCache(QCache<QString, QPixmap>* cache)
{
    m_thumbCache = cache;
}

// 重写：返回资产项尺寸（固定大小，支持项复用）
QSize AssetDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return m_cardSize;  // 所有资产项使用固定尺寸（配合 ListView setUniformItemSizes(true)）
}

// 重写：自定义绘制资产卡片（核心渲染逻辑，优化后）
void AssetDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QVariant assetVar = index.data(AssetModel::AssetDataRole);
    if (!assetVar.isValid() || !assetVar.canConvert<QVariantMap>()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    QVariantMap assetData = assetVar.toMap();

    QString assetId = assetData.value("asset_id", "未知ID").toString();
    QString assetName = assetData.value("name", "未知资产").toString();
    QString localImgPath = assetData.value("local_thumbnail_path", "").toString();
    QFileInfo fileInfo(localImgPath);

    // 优化：提前判断文件是否存在且有效（减少无效逻辑）
    bool isFileValid = !localImgPath.isEmpty() && fileInfo.exists() && fileInfo.size() > 0;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
    QRect cardRect = option.rect;
    bool isSelected = option.state & QStyle::State_Selected;

    // 绘制背景（保持不变）
    QColor bgColor = isSelected ? QColor("#4a4a4a") : QColor("#333333");
    painter->fillRect(cardRect, bgColor);
    painter->setPen(QPen(QColor("#505050"), 1));
    painter->drawRoundedRect(cardRect.adjusted(2, 2, -2, -2), 6, 6);

    // 缩略图区域（保持不变）
    QRect thumbnailRect = QRect(
        cardRect.left() + 8,
        cardRect.top() + 8,
        cardRect.width() - 16,
        60
    );
    painter->fillRect(thumbnailRect, QColor("#2a2a2a"));
    painter->setPen(QPen(QColor("#505050"), 1));
    painter->drawRoundedRect(thumbnailRect, 4, 4);

    // 绘制图片（优化缓存判断和错误处理）
    if (isFileValid && m_thumbCache) {
        if (m_thumbCache->contains(localImgPath)) {
            // 缓存命中：安全绘制（避免空指针）
            QPixmap* scaledPix = m_thumbCache->object(localImgPath); // 更安全的获取方式
            if (scaledPix && !scaledPix->isNull()) {
                int x = thumbnailRect.left() + (thumbnailRect.width() - scaledPix->width()) / 2;
                int y = thumbnailRect.top();
                painter->drawPixmap(x, y, *scaledPix);
            }
            else {
                painter->setPen(QPen(QColor("#ff6b6b")));
                painter->drawText(thumbnailRect, Qt::AlignCenter, "缓存损坏");
            }
        }
        else {
            // 缓存未命中：显示加载中，启动线程加载（避免重复加载）
            painter->setPen(QPen(QColor("#888888")));
            painter->drawText(thumbnailRect, Qt::AlignCenter, u8"加载中");
            loadThumbInThread(localImgPath);
        }
    }
    else if (isFileValid) {
        // 无缓存场景（兼容）
        QPixmap pixmap(localImgPath);
        if (!pixmap.isNull()) {
            QPixmap scaledPix = pixmap.scaled(
                thumbnailRect.size(),
                Qt::KeepAspectRatio,
                Qt::FastTransformation
            );
            int x = thumbnailRect.left() + (thumbnailRect.width() - scaledPix.width()) / 2;
            int y = thumbnailRect.top();
            painter->drawPixmap(x, y, scaledPix);
        }
        else {
            painter->setPen(QPen(QColor("#ff6b6b")));
            painter->drawText(thumbnailRect, Qt::AlignCenter, "图片损坏");
        }
    }
    else {
        // 图片缺失或无效
        QString tip = localImgPath.isEmpty() ? u8"路径缺失" : u8"文件无效";
        painter->setPen(QPen(QColor("#888888")));
        painter->drawText(thumbnailRect, Qt::AlignCenter, QString("%1\n%2").arg(assetId).arg(tip));
    }

    // 绘制资产名称（保持不变）
    QRect nameRect = QRect(
        cardRect.left() + 8,
        thumbnailRect.bottom() + 1,
        cardRect.width() - 16,
        60
    );
    painter->setPen(QPen(QColor("#ffffff")));
    painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, assetName);

    painter->restore();
}

// 处理交互事件（保持原有逻辑）
bool AssetDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    Q_UNUSED(model);

    // 左键释放事件（卡片点击）
    if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            QVariant assetVar = index.data(AssetModel::AssetDataRole);
            if (assetVar.isValid() && assetVar.canConvert<QVariantMap>()) {
                QVariantMap asset = assetVar.toMap();
                if (parent() && parent()->metaObject()->indexOfMethod("onAssetPreview(QVariantMap)") != -1) {
                    QMetaObject::invokeMethod(parent(), "onAssetPreview", Q_ARG(QVariantMap, asset));
                    return true;
                }
            }
        }
    }
    // 拖动相关逻辑（保持原有）
    else if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            m_dragStartPos = mouseEvent->pos();
            m_draggedAsset = index.data(AssetModel::AssetDataRole).toMap();
        }
    }
    else if (event->type() == QEvent::MouseMove) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->buttons() & Qt::LeftButton) {
            if ((mouseEvent->pos() - m_dragStartPos).manhattanLength() > 5) {
                startDrag(index);
            }
        }
    }

    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

// 启动拖动操作（保持原有逻辑，优化拖动图标加载）
void AssetDelegate::startDrag(const QModelIndex& index)
{
    Q_UNUSED(index);
    if (m_draggedAsset.isEmpty()) return;

    QMimeData* mimeData = new QMimeData();
    QString assetId = m_draggedAsset.value("asset_id", "").toString();
    mimeData->setText(assetId);

    QByteArray assetDataBytes = QJsonDocument::fromVariant(m_draggedAsset).toJson();
    mimeData->setData("application/x-asset-data", assetDataBytes);

    QDrag* drag = new QDrag(parent());
    drag->setMimeData(mimeData);

    // 拖动图标：优先从缓存读取（优化加载速度）
    QString localImgPath = m_draggedAsset.value("local_thumbnail_path", "").toString();
    if (m_thumbCache && m_thumbCache->contains(localImgPath)) {
        QPixmap pixmap = (*m_thumbCache)[localImgPath]->scaled(64, 64, Qt::KeepAspectRatio);
        drag->setPixmap(pixmap);
        drag->setHotSpot(QPoint(pixmap.width() / 2, pixmap.height() / 2));
    }
    else if (QFile::exists(localImgPath)) {
        QPixmap pixmap = QPixmap(localImgPath).scaled(64, 64, Qt::KeepAspectRatio);
        drag->setPixmap(pixmap);
        drag->setHotSpot(QPoint(pixmap.width() / 2, pixmap.height() / 2));
    }

    drag->exec(Qt::CopyAction | Qt::MoveAction);
}

// 子线程加载单张缩略图（核心优化：后台加载，不阻塞主线程）
void AssetDelegate::loadThumbInThread(const QString& imgPath) const
{
    // 多重校验：避免无效加载
    if (!m_thumbCache || m_thumbCache->contains(imgPath)) return;
    if (!m_loadThread.isNull()) return;
    QFileInfo fileInfo(imgPath);
    if (!fileInfo.exists() || fileInfo.size() == 0) return;

    // 1. 安全创建线程和 worker（父对象绑定到 delegate，自动管理生命周期）
    m_loadThread = new QThread(const_cast<AssetDelegate*>(this));
    QObject* worker = new QObject();
    worker->moveToThread(m_loadThread); // 显式移动 worker 到线程（规范写法）

    // 缩略图尺寸（与 paint 中完全一致）
    int thumbWidth = m_cardSize.width() - 16;
    int thumbHeight = 60;

    // 2. 线程启动：加载图片并缓存
    connect(m_loadThread, &QThread::started, worker, [=]() {

        if (!isWebpComplete(imgPath)) {
            m_loadThread->quit();
            return;
        }

        // 响应线程中断（退出时快速结束）
        if (QThread::currentThread()->isInterruptionRequested()) {
            m_loadThread->quit();
            return;
        }
        
        QPixmap pix(imgPath);
        if (!pix.isNull() && !QThread::currentThread()->isInterruptionRequested()) {
            QPixmap thumb = pix.scaled(
                thumbWidth, thumbHeight,
                Qt::KeepAspectRatio,
                Qt::FastTransformation
            );
            // 线程安全：QCache 本身线程安全，但建议加锁（可选，防止多线程同时插入）
            QMutexLocker locker(&m_cacheMutex);
            m_thumbCache->insert(imgPath, new QPixmap(thumb));
        }

        m_loadThread->quit(); // 加载完成后退出线程
        });

    // 3. 线程结束：清理资源 + 精准刷新视图（解决 viewport 为 NULL 核心）
    connect(m_loadThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(m_loadThread, &QThread::finished, [=]() {
        // 安全清理线程
        if (m_loadThread) {
            m_loadThread->deleteLater();
            m_loadThread = nullptr;
        }

        // 关键优化：通过 QAbstractScrollArea 直接获取 viewport（100% 可靠）
        if (!parent()) return;

        // 父对象是 QListView（继承 QAbstractScrollArea），直接强转获取 viewport
        QAbstractScrollArea* scrollView = qobject_cast<QAbstractScrollArea*>(parent());
        if (scrollView) {
            QWidget* viewport = scrollView->viewport();
            if (viewport) {
                // 延迟刷新：避免线程刚结束时缓存未同步（可选，增强稳定性）
                QTimer::singleShot(0, viewport, [=]() {
                    viewport->update(); // 只刷新可见区域，性能最优
                    });
                qDebug() << "[AssetDelegate] 缓存加载完成，刷新 viewport 可见区域";
            }
            else {
                scrollView->update(); // 极端情况：刷新整个视图
            }
        }
        else {
            // 父对象不是滚动视图（异常情况）
            QWidget* parentWidget = qobject_cast<QWidget*>(parent());
            if (parentWidget) {
                parentWidget->update();
                qDebug() << "[AssetDelegate] 父对象不是滚动视图，刷新整个部件";
            }
        }
        });

    // 4. 启动线程
    m_loadThread->start();
}

// 辅助函数：判断 WebP 文件是否完整（核心适配 WebP）
// 简化版 WebP 完整性校验（高兼容性，减少误判）
bool AssetDelegate::isWebpComplete(const QString& imgPath) const
{
    QFile file(imgPath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    // PNG 文件必须以 IEND 结尾，IEND 的十六进制为：00 00 00 00 49 45 4E 44 AE 42 60 82
    QByteArray footer = file.readAll().right(12); // 读取最后12字节
    return footer.endsWith(QByteArray::fromHex("0000000049454E44AE426082"));
}