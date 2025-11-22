#ifndef GET_ASSET_LIST_H
#define GET_ASSET_LIST_H

#include "ui/phaPullFromPolyhaven.h"
#include <QtCore/qmap.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/QJsonObject>
#include <QtCore/qjsonarray.h>
#include <QtCore/qfile.h>
#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qdebug.h>
#include <QtCore/qstring.h>

//#include"AssetInfo.h"
#include "abspath.h"
#include "get_asset_lib.h"
#include "constants.h"



/**
 * 获取资产列表（WinHTTP 实现，无额外依赖）
 * 缓存逻辑：本地缓存文件有效期 7 天，force=true 强制刷新
 * @param asset_type 资产类型："all"/"hdris"/"textures"/"models"
 * @param force 是否强制刷新缓存
 * @param error 输出参数：错误信息（成功则为空）
 * @return 资产列表（key: slug，value: QJsonObject）
 */
QMap<QString, QJsonObject> get_asset_list(const QString& asset_type = "all",
    bool force = false,
    QString& error = *new QString(""));

QMap<QString, QJsonObject> parseAssetJson(const QJsonObject& jsonObj);



#endif // GET_ASSET_LIST_H