#include "AssetModel.h"



// 构造函数
AssetModel::AssetModel(const QMap<QString, QJsonObject>& assets, QObject* parent)
    : QAbstractListModel(parent)
    , m_assets(convertMapToList(assets))  // 初始化资产列表
{
}

// 重写：返回数据总行数（QListView必须）
int AssetModel::rowCount(const QModelIndex& parent) const
{
    // 父索引有效时返回0（列表模型无层级结构）
    if (parent.isValid())
        return 0;
    return m_assets.size();
}

// 重写：根据索引和角色返回数据（核心方法）
QVariant AssetModel::data(const QModelIndex& index, int role) const
{
    // 基础校验：索引无效/行号越界
    if (!index.isValid() || index.row() < 0 || index.row() >= m_assets.size())
        return QVariant();

    const AssetItem& currentAsset = m_assets[index.row()];
    const QString& assetId = currentAsset.assetId;
    const QJsonObject& details = currentAsset.details;

    // 1. Qt标准角色：DisplayRole（列表默认显示文本）
    if (role == Qt::DisplayRole) {
        //return QString("%1 (%2)").arg(details.name).arg(assetId);  // 显示"名称(ID)"
        return "";//TODO
    }

    // 2. 自定义角色：返回完整资产数据（QVariantMap，适配UI自定义渲染）
    else if (role == AssetDataRole) {
        QVariantMap assetData;
        assetData["asset_id"] = assetId;
        assetData["name"] = details.value("name");
        assetData["type"] = details.value("type");
        assetData["tags"] = details.value("tags");
        assetData["categories"] = details.value("categories");
        assetData["authors"] = details.value("authors").toObject().keys().first();
        assetData["local_thumbnail_path"] = getLocalThumbnailPath(assetId);
        return assetData;
    }

    // 3. 自定义角色：返回缩略图图标（直接返回QIcon，UI可直接使用）
    else if (role == ThumbnailRole) {
        const QString thumbnailPath = getLocalThumbnailPath(assetId);
        // 若图片不存在，返回系统默认"图片缺失"图标
        if (QFile::exists(thumbnailPath)) {
            return QIcon(thumbnailPath);
        }
        else {
            // 适配Qt内置图标主题（跨平台兼容）
            return QIcon::fromTheme("image-missing", QIcon(":/icons/default_thumbnail.png"));
        }
    }

    return QVariant();
}

// 重写：定义角色名称映射（支持QML和元对象系统）
QHash<int, QByteArray> AssetModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[Qt::DisplayRole] = "display";          // 标准角色名称
    roles[AssetDataRole] = "assetData";          // 完整资产数据
    roles[ThumbnailRole] = "thumbnailIcon";      // 缩略图图标
    return roles;
}

// 公共接口：更新资产数据（会触发UI刷新）
void AssetModel::updateAssets(const QMap<QString, QJsonObject>& newAssets)
{
    beginResetModel();  // Qt模型标准用法：开始重置数据（通知UI准备更新）
    m_assets = convertMapToList(newAssets);       // 替换旧数据
    endResetModel();    // 结束重置（通知UI刷新）
}

// 辅助函数：将QMap转换为QVector（适配列表索引访问）
QVector<AssetModel::AssetItem> AssetModel::convertMapToList(const QMap<QString, QJsonObject>& assets) const
{
    QVector<AssetItem> assetList;
    assetList.reserve(assets.size());  // 预分配内存，提升性能

    // 遍历QMap，转换为列表结构
    for (auto it = assets.constBegin(); it != assets.constEnd(); ++it) {
        AssetItem item;
        item.assetId = it.key();       // 资产ID（QMap的key）
        item.details = it.value();     // 资产详情（QMap的value）
        assetList.append(item);
    }

    return assetList;
}

// 辅助函数：获取本地缩略图路径（.webp格式）
QString AssetModel::getLocalThumbnailPath(const QString& assetId) const
{
    // 构建路径：资产库根路径 / 资产ID / thumbnail.webp
    const QString thumbnailFolder = QString("%1/%2").arg(get_asset_lib_path()).arg(assetId);
    const QString thumbnailPath = QString("%1/thumbnail.webp").arg(thumbnailFolder);

    // 确保文件夹存在（不存在则创建）
    QDir dir(thumbnailFolder);
    if (!dir.exists()) {
        dir.mkpath(".");  // 创建多级目录（包括父目录）
        qDebug() << "[AssetModel] 创建缩略图文件夹：" << thumbnailFolder;
    }

    return thumbnailPath;
}