#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <QtCore/QMap>
#include <QtCore/QString>

// 对应 Python 中的 REQ_HEADERS
static const QMap<QString, QString> REQ_HEADERS = {
    {"User-Agent", "Blender: PH Assets - GitHub"},
    {"Accept", "application/json"}
};

static const bool early_access = false;

// 全局日志宏（模拟 Python 打印调试信息）
#define LOG_DEBUG(msg) qDebug() << "[get_asset_lib] DEBUG:" << msg
#define LOG_ERROR(msg) qWarning() << "[get_asset_lib] ERROR:" << msg
// 日志宏（模拟 Python 的 logging）
#define LOG_INFO(msg) qDebug() << "[download_file] INFO:" << msg
#define LOG_WARN(msg) qWarning() << "[download_file] WARN:" << msg

#endif // CONSTANTS_H