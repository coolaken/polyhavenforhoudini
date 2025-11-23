#ifndef PHAPULLFROMPOLYHAVEN_H
#define PHAPULLFROMPOLYHAVEN_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtCore/QJsonObject>
#include <QtCore/QThread>
#include <QtCore/QThreadPool>
#include <QtCore/QRunnable>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QElapsedTimer>
#include <QtCore/QDir>
#include <QtCore/QJsonArray>
#include <QtWidgets/QProgressDialog>

// 项目相关头文件
#include "get_asset_list.h"
#include "download_file.h"
#include "AssetModel.h"
#include "get_asset_lib.h"
#include "constants.h"

#include <atomic>          // ← 新增




// 模拟 PHPlugin 类（进度取消标志）
class PHPlugin {
public:
    static bool PH_PROGRESS_CANCEL;
};

// 下载任务结果结构体
struct DownloadResult {
    QString error;         // 错误信息（空表示成功）
    QString slug;          // 成功下载的资产 slug
    bool exists;           // dry_run 模式下表示资产是否已存在
};

class phaPullFromPolyhaven : public QObject
{
    Q_OBJECT
        friend class AssetDownloadTask;

public:
    explicit phaPullFromPolyhaven(QObject* parent = nullptr);
    ~phaPullFromPolyhaven() override;

    void setAssetType(const QString& type);
    void setRevalidate(bool revalidate);

    void executeAsync();

Q_SIGNALS:
    void progressUpdated(int current, int total, const QString& text);
    void report(const QString& type, const QString& content);
    void finished(int downloadedCount, int failedCount);
    void executeFinished(int resultCode);

public Q_SLOTS:
    void cancelDownload();

private Q_SLOTS:
    void doExecuteAsync();
    void handleTaskFinished(const DownloadResult& res);   // ← 新增
    void allTasksFinished();                              // ← 新增

private:
    DownloadResult updateAsset(const QMap<QString, QJsonObject>& asset, const QDir& libDirPath, bool dryRun);
    QString downloadAsset(const QMap<QString, QJsonObject>& asset, const QDir& libDirPath, const QFileInfo& infoFp);
    bool checkAssetExists(const QMap<QString, QJsonObject>& asset, const QFileInfo& infoFp, bool& needUpdate);
    QJsonObject loadOldInfo(const QFileInfo& infoFp);
    void processAssets(const QMap<QString, QJsonObject>& assets, const QDir& libDirPath);



    QString m_assetType = "all";
    bool m_revalidate = false;
    QThread* m_workerThread = nullptr;
    QMutex m_mutex;
    QWaitCondition m_waitCond;

    std::atomic<bool> m_isCancelled{ false };              // ← 原子化
    std::atomic<int> m_remaining{ 0 };                     // ← 替代 waitForDone
    int m_totalToFetch = 0;
    std::atomic<int> m_currentProgress{ 0 };
    std::atomic<int> m_downloadedCount{ 0 };
    std::atomic<int> m_failedCount{ 0 };
    QString m_currentProgressText;

    QMap<QString, QJsonObject> m_asyncAssets;
    QDir m_asyncLibDir;
    int m_asyncResultCode = 0;
public:
    QString m_res = "1k";
    QString m_format = "hdr";
};



#endif // PHAPULLFROMPOLYHAVEN_H