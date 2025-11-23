#include "get_asset_list.h"
#include <QtCore/qdebug.h>

// 外部常量声明（需在 constants.h 中定义）
extern const bool early_access;  // 对应 Python 中的 early_access（是否包含未来资产）



// -----------------------------------------------------------------------------
// 核心函数：获取资产列表（缓存逻辑不变，仅替换网络请求）
// -----------------------------------------------------------------------------
QMap<QString, QJsonObject> get_asset_list(const QString& asset_type, bool force, QString& error)
{
    QMap<QString, QJsonObject> assetList;
    error.clear();

    // 1. 检查资产库是否存在


    // 2. 构建缓存文件路径
    QString assetLibPath = get_asset_lib_path();
    QFileInfo cacheFileInfo(QDir(assetLibPath).filePath("asset_list_cache.json"));
    QFile cacheFile(cacheFileInfo.filePath());

    // 3. 检查缓存（未强制刷新且缓存存在）
    if (!force && cacheFileInfo.exists()) {
        qint64 cacheAgeSec = QDateTime::currentDateTime().toSecsSinceEpoch() - cacheFileInfo.lastModified().toSecsSinceEpoch();
        double cacheAgeDays = cacheAgeSec / (60.0 * 60.0 * 24.0);

        if (cacheAgeDays <= 7.0) {  // 缓存未过期（7天内）
            if (cacheFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QJsonParseError jsonError;
                QJsonDocument jsonDoc = QJsonDocument::fromJson(cacheFile.readAll(), &jsonError);
                cacheFile.close();

                if (jsonError.error == QJsonParseError::NoError && jsonDoc.isObject()) {
                    assetList = parseAssetJson(jsonDoc.object());
                    LOG_DEBUG(QString("Using cached asset list (%1 days old)").arg(cacheAgeDays, 0, 'f', 2));
                    return assetList;
                }
                else {
                    LOG_ERROR(QString("Error decoding asset list cache: %1").arg(jsonError.errorString()));
                }
            }
            else {
                LOG_ERROR(QString("Failed to open cache file: %1").arg(cacheFile.errorString()));
            }
        }
        else {
            LOG_DEBUG(QString("Asset list cache expired (%1 days old), forcing refresh").arg(cacheAgeDays, 0, 'f', 2));
        }
    }

    // 4. 从 API 获取数据
    QString apiUrl = QString("https://api.polyhaven.com/assets?t=%1").arg(asset_type);
    if (early_access) {
        apiUrl += "&future=true";
    }
    LOG_DEBUG(QString("Getting asset list from %1").arg(apiUrl));
    QUrl url = QString(apiUrl);

    // 5. 发起 HTTPS 请求（WinHTTP 实现）
    
    QByteArray jsonData = get(url);
    
    

    // 6. 解析 JSON
    QJsonParseError json_error;
    
    QJsonDocument json_doc = QJsonDocument::fromJson(jsonData, &json_error);

    
    if (json_error.error != QJsonParseError::NoError) {
        error = QString("JSON parse failed: %1 ")
            .arg(json_error.errorString());
            
        LOG_ERROR(error);
        return assetList;
    }

    if (!json_doc.isObject()) {
        error = "API response is not a valid JSON object";
        LOG_ERROR(error);
        return assetList;
    }

    QJsonObject jsonObj = json_doc.object();
    assetList = parseAssetJson(jsonObj);

    // 7. 缓存数据到本地
    QDir cacheDir(cacheFileInfo.path());
    if (!cacheDir.exists()) {
        cacheDir.mkpath(".");
    }

    if (cacheFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        cacheFile.write(json_doc.toJson(QJsonDocument::Indented));
        cacheFile.close();
        LOG_DEBUG("Asset list cached successfully");
    }
    else {
        LOG_ERROR(QString("Failed to write cache file: %1").arg(cacheFile.errorString()));
    }

    return assetList;
}

// -----------------------------------------------------------------------------
// 辅助函数：解析 JSON 为 AssetInfo 列表（无修改，完全复用）
// -----------------------------------------------------------------------------
QMap<QString, QJsonObject> parseAssetJson(const QJsonObject& jsonObj)
{
    QMap<QString, QJsonObject> assetList;

    for (auto it = jsonObj.constBegin(); it != jsonObj.constEnd(); ++it) {
        const QString& slug = it.key();
        const QJsonValue& assetValue = it.value();

        if (!assetValue.isObject()) {
            LOG_DEBUG(QString("Skipping invalid asset data for slug: %1").arg(slug));
            continue;
        }

        QJsonObject assetObj = assetValue.toObject();
 
        assetList.insert(slug, assetObj);
    }

    return assetList;
}