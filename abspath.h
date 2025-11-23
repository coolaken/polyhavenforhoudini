#ifndef ABS_PATH_H
#define ABS_PATH_H

#include <QtCore/qstring.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qfile.h>


/**
 * 解析绝对路径（兼容 Houdini 环境变量和路径规则）
 * 对应 Python: abspath(p)
 * @param p 输入路径字符串（支持 Houdini 环境变量如 $HIP、$JOB）
 * @return 解析后的绝对路径（QString）
 */
QString abspath(const QString& p);

/**
 * 获取插件包路径（PolyHavenLink 包路径）
 * 对应 Python: get_package_path()
 * @return 插件包绝对路径
 */
QString get_package_path();

/**
 * 从配置文件加载资产路径
 * 对应 Python: load_asset_path(file)
 * @param file 配置文件路径（如 polyhaven_config.json）
 * @return 解析后的资产绝对路径
 */
QString load_asset_path(const QString& file);

/**
 * 保存资产路径到配置文件
 * 对应 Python: save_asset_path(scf, sa)
 * @param scf 配置文件路径
 * @param sa 要保存的资产路径（绝对路径）
 * @return 是否保存成功
 */
bool save_asset_path(const QString& scf, const QString& sa);

#endif // ABS_PATH_H