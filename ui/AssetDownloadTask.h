#ifndef ASSETDOWNLOADTASK_H
#define ASSETDOWNLOADTASK_H

#include <QtCore/QObject>
#include <QtCore/QRunnable>
#include <QtCore/QDir>
#include <QtCore/QJsonArray>
#include <QtWidgets/QProgressDialog>
#include <atomic>          // ¡û ÐÂÔö
//#include "AssetInfo.h"
#include "phaPullFromPolyhaven.h"


class AssetDownloadTask : public QObject, public QRunnable
{
    Q_OBJECT
public:
    AssetDownloadTask(const QMap<QString, QJsonObject>& asset, const QDir& libDir, bool revalidate, phaPullFromPolyhaven* parent);
    ~AssetDownloadTask() override = default;
    void run() override;
Q_SIGNALS:
    void taskFinished(const DownloadResult& result);
private:
    QMap<QString, QJsonObject> m_asset;
    QDir m_libDir;
    bool m_revalidate;
    phaPullFromPolyhaven* m_parent;
};

#endif