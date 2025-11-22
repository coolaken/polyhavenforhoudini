#include "AssetDelegate.h"
#include <QtWidgets/qapplication.h>
#include <QtGui/qfont.h>
#include <QtGui/qpen.h>
#include <QtGui/qcolor.h>
#include <QtGui/qpixmap.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qdebug.h>
#include <QtCore/qjsondocument.h>
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
    // 1. 获取资产数据（从模型的 AssetDataRole 读取）
    QVariant assetVar = index.data(AssetModel::AssetDataRole);
    if (!assetVar.isValid() || !assetVar.canConvert<QVariantMap>()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    QVariantMap assetData = assetVar.toMap();

    // 2. 提取关键数据
    QString assetId = assetData.value("asset_id", "未知ID").toString();
    QString assetName = assetData.value("name", "未知资产").toString();
    QString localImgPath = assetData.value("local_thumbnail_path", "").toString();
    QFileInfo fileInfo(localImgPath);

    // 3. 绘制准备（禁用不必要的渲染效果，提升性能）
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false); // 关闭抗锯齿（非必要，牺牲少量画质换性能）
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false); // 关闭平滑缩放（预加载已做缩放）
    QRect cardRect = option.rect;
    bool isSelected = option.state & QStyle::State_Selected;

    // 4. 绘制卡片背景（简化绘制，减少混合运算）
    QColor bgColor = isSelected ? QColor("#4a4a4a") : QColor("#333333");
    painter->fillRect(cardRect, bgColor);
    painter->setPen(QPen(QColor("#505050"), 1));
    painter->drawRoundedRect(cardRect.adjusted(2, 2, -2, -2), 6, 6);

    // 5. 绘制缩略图区域
    QRect thumbnailRect = QRect(
        cardRect.left() + 8,
        cardRect.top() + 8,
        cardRect.width() - 16,
        60
    );
    // 缩略图背景
    painter->fillRect(thumbnailRect, QColor("#2a2a2a"));
    painter->setPen(QPen(QColor("#505050"), 1));
    painter->drawRoundedRect(thumbnailRect, 4, 4);

    // 6. 绘制图片（核心优化：从缓存读取，无实时加载/缩放）
    if (!localImgPath.isEmpty() && m_thumbCache) {
        if (m_thumbCache->contains(localImgPath) && fileInfo.size() != 0) {
            // 缓存命中：直接绘制预缩放后的图片
            
            
           

            QPixmap* scaledPix = (*m_thumbCache)[localImgPath];
            int x = thumbnailRect.left() + (thumbnailRect.width() - scaledPix->width()) / 2; // 水平居中
            int y = thumbnailRect.top(); // 垂直靠上
            painter->drawPixmap(x, y, *scaledPix);
        }
        else {
            // 缓存未命中：启动子线程加载，当前显示"加载中"
            painter->setPen(QPen(QColor("#888888")));
            painter->drawText(thumbnailRect, Qt::AlignCenter, u8"加载中");
            // 子线程加载（const 函数中通过 mutable 线程变量实现）
            loadThumbInThread(localImgPath);
        }
    }
    else if (QFile::exists(localImgPath)) {
        // 兼容无缓存场景（临时绘制，不推荐）
        QPixmap pixmap(localImgPath);
        if (!pixmap.isNull()) {
            QPixmap scaledPix = pixmap.scaled(
                thumbnailRect.size(),
                Qt::KeepAspectRatio,
                Qt::FastTransformation // 快速缩放
            );
            int x = thumbnailRect.left() + (thumbnailRect.width() - scaledPix.width()) / 2;
            int y = thumbnailRect.top();
            painter->drawPixmap(x, y, scaledPix);
        }
        else {
            painter->setPen(QPen(QColor("#ff6b6b")));
            painter->drawText(thumbnailRect, Qt::AlignCenter, "损坏");
        }
    }
    else {
        // 图片缺失
        painter->setPen(QPen(QColor("#888888")));
        painter->drawText(thumbnailRect, Qt::AlignCenter, QString("%1\n缺失").arg(assetId));
    }

    // 7. 绘制资产名称（保留自动换行+靠上）
    QRect nameRect = QRect(
        cardRect.left() + 8,
        thumbnailRect.bottom() + 1,
        cardRect.width() - 16,
        60
    );
    QFont nameFont = painter->font();
    painter->setPen(QPen(QColor("#ffffff")));
    painter->setFont(nameFont);
    // 强制换行+靠上对齐（核心）
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
    // 避免重复创建线程或重复加载同一图片
    if (!m_thumbCache || m_thumbCache->contains(imgPath)) return;
    if (m_loadThread && m_loadThread->isRunning()) return;
    QFileInfo fileInfo(imgPath);
    // 如果文件不存在，或者大小为0 (说明正在创建中但还没写入数据)，直接放弃，等待下次 paint
    if (fileInfo.size() == 0) {
        return;
    }

    m_loadThread = new QThread(const_cast<AssetDelegate*>(this));
    QObject* worker = new QObject();

    // 计算缩略图固定尺寸（与 paint() 中的 thumbnailRect 完全一致）
    int thumbWidth = m_cardSize.width() - 16; // cardRect.width() - 16（左右各8像素边距）
    int thumbHeight = 60; // 与 paint() 中 thumbnailRect 的高度一致

    connect(m_loadThread, &QThread::started, worker, [=]() {
        // 加载并预缩放图片（只做一次）
        QPixmap pix(imgPath);
        if (!pix.isNull()) {
            QPixmap thumb = pix.scaled(
                thumbWidth, thumbHeight, // 用固定尺寸，避免访问 paint() 中的局部变量
                Qt::KeepAspectRatio,
                Qt::FastTransformation // 快速缩放，优先性能
            );
            // 存入缓存（QCache 自动管理内存）
            m_thumbCache->insert(imgPath, new QPixmap(thumb));
        }
        m_loadThread->quit();
        });

    // 线程结束后自动销毁，并通知视图刷新
    connect(m_loadThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(m_loadThread, &QThread::finished, [=]() {
        m_loadThread->deleteLater();
        m_loadThread = nullptr;
        // 刷新视图（只刷新可见区域，避免全屏重绘）
        if (parent()) {
            QWidget* viewport = qobject_cast<QWidget*>(parent()->findChild<QWidget*>("viewport"));
            if (!viewport) {
                // 若找不到 viewport，直接刷新 ListView
                QWidget* listView = qobject_cast<QWidget*>(parent());
                if (listView) listView->update();
            }
            else {
                viewport->update();
            }
        }
        });

    m_loadThread->start();
}