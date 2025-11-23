#include "abspath.h"
#include <QtCore/qdir.h>
#include <QtCore/QJsonParseError>
#include <QtCore/qdebug.h>

#include <UT/UT_String.h>


// 日志宏（适配 Houdini 日志系统）
#define LOG_DEBUG(msg) qDebug() << "[PolyHavenLink] DEBUG:" << msg
#define LOG_ERROR(msg) qWarning() << "[PolyHavenLink] ERROR:" << msg



// -----------------------------------------------------------------------------
// 对应 Python: abspath(p)
// -----------------------------------------------------------------------------
QString abspath(const QString& p) {
    // 处理空路径或仅含空格的情况
    QString input = p.trimmed();
    return QFileInfo(input).absolutePath();
}





// -----------------------------------------------------------------------------
// 对应 Python: get_package_path()
// -----------------------------------------------------------------------------
QString get_package_path() {
    // 默认插件路径（与 Python 一致）
    QString default_path = "C:/Users/Rus/Documents/houdini21.0/packages/PolyHavenLink";
    if (QFileInfo::exists(default_path)) {
        return default_path;
    }

    // 默认路径不存在时，通过当前文件路径向上推导（对应 os.path.dirname 三次）
    try {
        // 获取当前 DLL/插件的路径（Houdini 插件场景下）
        QString current_file = QFileInfo(__FILE__).absoluteFilePath();
        QDir plugin_dir = QFileInfo(current_file).dir();

        // 向上跳转 3 级目录（对应 Python 的 os.path.dirname 三次）
        for (int i = 0; i < 3; ++i) {
            if (!plugin_dir.cdUp()) {
                LOG_ERROR("Failed to get parent directory for package path");
                return default_path;
            }
        }
        return plugin_dir.absolutePath();
    }
    catch (...) {
        LOG_ERROR("Fallback to default package path");
        return default_path;
    }
}

// -----------------------------------------------------------------------------
// 对应 Python: load_asset_path(file)
// -----------------------------------------------------------------------------
QString load_asset_path(const QString& file) {
    // 检查配置文件是否存在
    if (!QFileInfo::exists(file)) {
        LOG_ERROR(QString("Config file not found: %1").arg(file));
        return abspath("");  // 返回默认路径
    }

    QFile config_file(file);
    if (!config_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_ERROR(QString("Failed to open config file: %1").arg(config_file.errorString()));
        return abspath("");
    }

    // 解析 JSON 配置
    QJsonParseError json_error;
    QJsonDocument json_doc = QJsonDocument::fromJson(config_file.readAll(), &json_error);
    config_file.close();

    if (json_error.error != QJsonParseError::NoError) {
        LOG_ERROR(QString("Parse config file failed: %1").arg(json_error.errorString()));
        return abspath("");
    }

    if (!json_doc.isObject()) {
        LOG_ERROR("Config file is not a valid JSON object");
        return abspath("");
    }

    // 提取 asset_path 并解析绝对路径
    QJsonObject config_obj = json_doc.object();
    QString saved_path = config_obj["asset_path"].toString("");
    return abspath(saved_path);
}

// -----------------------------------------------------------------------------
// 对应 Python: save_asset_path(scf, sa)
// -----------------------------------------------------------------------------
bool save_asset_path(const QString& scf, const QString& sa) {
    try {
        QJsonObject config_obj;

        // 读取现有配置（如果文件存在）
        if (QFileInfo::exists(scf)) {
            QFile config_file(scf);
            if (!config_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                LOG_ERROR(QString("Failed to read config file: %1").arg(config_file.errorString()));
                return false;
            }

            QJsonParseError json_error;
            QJsonDocument json_doc = QJsonDocument::fromJson(config_file.readAll(), &json_error);
            config_file.close();

            if (json_error.error != QJsonParseError::NoError) {
                LOG_ERROR(QString("Parse existing config failed: %1").arg(json_error.errorString()));
                return false;
            }

            if (json_doc.isObject()) {
                config_obj = json_doc.object();
            }
        }

        // 更新 asset_path（保存绝对路径字符串）
        config_obj["asset_path"] = sa;

        // 写入配置文件
        QFile config_file(scf);
        if (!config_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            LOG_ERROR(QString("Failed to write config file: %1").arg(config_file.errorString()));
            return false;
        }

        // 格式化 JSON 输出（indent=4，与 Python 一致）
        QJsonDocument json_doc(config_obj);
        config_file.write(json_doc.toJson(QJsonDocument::Indented));
        config_file.close();

        return true;
    }
    catch (const std::exception& e) {
        // 调用 Houdini 弹窗显示错误（与 Python 的 hou.ui.displayMessage 一致）
        QString error_msg = QString("保存配置文件时出错: %1").arg(e.what());
        //HOUDINI::UI::displayMessage(error_msg.toStdString().c_str(), "错误", HOUDINI::UI::MessageType::Error);
        LOG_ERROR(error_msg);
        return false;
    }
}