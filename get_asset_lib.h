#ifndef GET_ASSET_LIB_H
#define GET_ASSET_LIB_H


#include <QtCore/qstring.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qmap.h>
#include "phaPullFromPolyhaven.h"
#include "constants.h"




//#include "AssetInfo.h"

// 资产详情结构体（替代 Python 字典，类型更安全）


/**
 * 获取 Poly Haven 资产库路径（从 polyhaven_config.json 读取）
 * 对应 Python: get_asset_lib_path()
 * @return 资产库绝对路径（空字符串表示失败）
 */
QString get_asset_lib_path();

/**
 * 获取资产列表缓存文件路径（asset_list_cache.json）
 * 对应 Python: asset_list_cache_path()
 * @return 缓存文件绝对路径（空字符串表示失败）
 */
QString asset_list_cache_path();

/**
 * 读取资产库缓存数据（从 asset_list_cache.json 解析）
 * 对应 Python: get_asset_lib()
 * @return 资产库 JSON 数据（空对象表示失败）
 */
QMap<QString, QJsonObject> get_asset_lib();

/**
 * 获取 Blender 资产分类文件路径（blender_assets.cats.txt）
 * 对应 Python: get_blender_assets_cats()
 * @return 分类文件绝对路径（空字符串表示失败）
 */
QString get_blender_assets_cats();



#endif // GET_ASSET_LIB_H