#include "get_asset_lib.h"
#include <QtCore/qfile.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/QJsonParseError>
#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qdebug.h>
#include "startwindow.h"



static QStringList jsonStringArrayToList(const QJsonArray& arr)
{
    QStringList list;
    for (const auto& v : arr)
        list.append(v.toString());
    return list;
}

static QList<double> jsonDoubleArrayToList(const QJsonArray& arr)
{
    QList<double> list;
    for (const auto& v : arr)
        list.append(v.toDouble());
    return list;
}

static QList<int> jsonIntArrayToList(const QJsonArray& arr)
{
    QList<int> list;
    for (const auto& v : arr)
        list.append(v.toInt());
    return list;
}

// 实现全局函数：获取资产库根路径（需根据实际项目修改逻辑）
QString get_asset_lib_path()
{
    return StartWindow::s_lastPath;
}



// -----------------------------------------------------------------------------
// 对应 Python: asset_list_cache_path()
// -----------------------------------------------------------------------------
QString asset_list_cache_path() {
    QString assetLibPath = get_asset_lib_path();
    if (assetLibPath.isEmpty()) {
        LOG_ERROR("Asset library path is empty");
        return "";
    }

    // 构建 asset_list_cache.json 路径
    QString cachePath = QDir(assetLibPath).filePath("asset_list_cache.json");
    // 模拟 Path.as_posix()，返回 / 分隔的路径
    return QFileInfo(cachePath).absoluteFilePath().replace("\\", "/");
}

// -----------------------------------------------------------------------------
// 对应 Python: get_asset_lib()
// -----------------------------------------------------------------------------
QMap<QString, QJsonObject> get_asset_lib()
{
    QString path = asset_list_cache_path();   // 你的缓存文件路径
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return {};

    QByteArray raw = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return {};

    QJsonObject root = doc.object();
    QMap<QString, QJsonObject> out;

    for (auto it = root.begin(); it != root.end(); ++it) {
        if (!it.value().isObject()) continue;

        QJsonObject obj = it.value().toObject();
        

        out.insert(it.key(), obj);
    }
    return out;
}



// -----------------------------------------------------------------------------
// 对应 Python: get_blender_assets_cats()
// -----------------------------------------------------------------------------
QString get_blender_assets_cats() {

    QString assetLibPath = get_asset_lib_path();
    if (assetLibPath.isEmpty()) {
        LOG_ERROR("Asset library path is empty");
        return "";
    }

    // 构建 blender_assets.cats.txt 路径：当前文件向上 4 级目录
    QString catsPath = QDir(assetLibPath).filePath("blender_assets.cats.txt");

    // 模拟 Path.as_posix()，返回 / 分隔的路径
    return QFileInfo(catsPath).absoluteFilePath().replace("\\", "/");
}


