#ifndef ASSETMODEL_H
#define ASSETMODEL_H

#include "get_asset_lib.h"
#include <QtCore/QAbstractListModel>
#include <QtCore/QMap>
#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtCore/QDir>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtCore/QDebug>
//#include "AssetInfo.h"






class AssetModel : public QAbstractListModel
{
    Q_OBJECT

public:
    // 自定义数据角色（替代魔法值，增强可读性）
    enum AssetRoles {
        AssetDataRole = Qt::UserRole + 1,  // 返回完整资产数据（QVariantMap）
        ThumbnailRole = Qt::UserRole + 2   // 返回缩略图图标（QIcon）
    };

    // 构造函数：接收资产字典（key: 资产ID，value: 资产详情）
    explicit AssetModel(const QMap<QString, QJsonObject>& assets = QMap<QString, QJsonObject>(),
        QObject* parent = nullptr);

    // QAbstractListModel 纯虚函数重写（必须实现）
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    // 可选但推荐：定义角色名称（支持QML/Qt元对象系统）
    QHash<int, QByteArray> roleNames() const override;

    // 公共接口：更新资产数据（支持动态刷新UI）
    void updateAssets(const QMap<QString, QJsonObject>& newAssets);

private:
    // 资产存储结构（适配列表索引访问）
    struct AssetItem {
        QString assetId;       // 资产ID（如"ArmChair_01"）
        QJsonObject details;  // 资产详情
    };

    QVector<AssetItem> m_assets;  // 存储资产列表（QVector比QList更适合连续访问）

    // 辅助函数：将QMap转换为QVector（适配列表模型）
    QVector<AssetItem> convertMapToList(const QMap<QString, QJsonObject>& assets) const;

    // 辅助函数：获取本地缩略图路径
    QString getLocalThumbnailPath(const QString& assetId) const;
};

#endif // ASSETMODEL_H