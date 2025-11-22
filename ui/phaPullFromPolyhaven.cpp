#include "phaPullFromPolyhaven.h"
#include <QtCore/QDebug>
#include <QtCore/QCryptographicHash>
#include <atomic>        // 原子计数
#include "AssetDownloadTask.h"

// 初始化 PHPlugin 静态变量
bool PHPlugin::PH_PROGRESS_CANCEL = false;

phaPullFromPolyhaven::phaPullFromPolyhaven(QObject* parent)
    : QObject(parent)
{
    m_workerThread = new QThread(this);
    this->moveToThread(m_workerThread);
    m_workerThread->start();
}

phaPullFromPolyhaven::~phaPullFromPolyhaven()
{
    if (m_workerThread->isRunning()) {
        cancelDownload();
        m_workerThread->quit();
        m_workerThread->wait();
    }
    m_workerThread->deleteLater();
}

void phaPullFromPolyhaven::setAssetType(const QString& type)
{
    QMutexLocker locker(&m_mutex);
    m_assetType = type;
}

void phaPullFromPolyhaven::setRevalidate(bool revalidate)
{
    QMutexLocker locker(&m_mutex);
    m_revalidate = revalidate;
}

/* ---------- 异步入口 ---------- */
void phaPullFromPolyhaven::executeAsync()
{
    m_asyncResultCode = 0;
    m_isCancelled.store(false);
    m_currentProgress = 0;
    m_downloadedCount.store(0);
    m_failedCount.store(0);
    m_currentProgressText.clear();
    PHPlugin::PH_PROGRESS_CANCEL = false;

    QString assetLibPath = get_asset_lib_path();
    QDir libDir(assetLibPath);
    if (assetLibPath.isEmpty() || libDir.dirName() != "Poly Haven") {
        Q_EMIT report("ERROR", "First open Preferences > File Paths and create an asset library named \"Poly Haven\"");
        Q_EMIT executeFinished(-1);
        return;
    }
    if (!libDir.exists()) {
        Q_EMIT report("ERROR", "Asset library path not found! Please check the folder still exists");
        Q_EMIT executeFinished(-2);
        return;
    }

    QString error;
    QMap<QString, QJsonObject> assets = get_asset_list(m_assetType, true, error);
    if (!error.isEmpty()) {
        Q_EMIT report("ERROR", error);
        Q_EMIT executeFinished(-3);
        return;
    }
    if (assets.isEmpty()) {
        Q_EMIT report("INFO", "No assets found to download");
        Q_EMIT finished(0, 0);
        Q_EMIT executeFinished(0);
        return;
    }

    m_asyncAssets = assets;
    m_asyncLibDir = libDir;
    QMetaObject::invokeMethod(this, "doExecuteAsync", Qt::QueuedConnection);
}

/* ---------- 子线程真正干活 ---------- */
void phaPullFromPolyhaven::doExecuteAsync()
{
    processAssets(m_asyncAssets, m_asyncLibDir);
    /* 收尾信号由 allTasksFinished() 发出，不再这里立即调用 */
}

/* ---------- 取消 ---------- */
void phaPullFromPolyhaven::cancelDownload()
{
    QMutexLocker locker(&m_mutex);
    m_isCancelled.store(true);
    PHPlugin::PH_PROGRESS_CANCEL = true;
    
}

/* ---------- 处理资产：全局线程池 + 原子计数 ---------- */
void phaPullFromPolyhaven::processAssets(const QMap<QString, QJsonObject>& assets,
    const QDir& libDirPath)
{

    m_totalToFetch = assets.size();
    Q_EMIT progressUpdated(0, m_totalToFetch, "Preparing download tasks...");

    QThreadPool* pool = QThreadPool::globalInstance();
    pool->setMaxThreadCount(qMin(4, QThread::idealThreadCount() * 2));
    //pool->setMaxThreadCount(1);

    m_remaining.store(assets.size());
    QMap<QString, QJsonObject> asset;

    for (auto it = assets.constBegin(); it != assets.constEnd(); ++it) {
        if (m_isCancelled.load(std::memory_order_relaxed)) {
            if (m_remaining.fetch_sub(1, std::memory_order_relaxed) == 1)
                Q_EMIT allTasksFinished();
            continue;
        }
        asset.clear();
        asset.insert(it.key(), it.value());
        AssetDownloadTask* task = new AssetDownloadTask(asset, libDirPath, m_revalidate, this);
        connect(task, &AssetDownloadTask::taskFinished,
            this, &phaPullFromPolyhaven::handleTaskFinished,
            Qt::QueuedConnection);
        task->setAutoDelete(true);
        pool->start(task);
    }
    /* 无 waitForDone！函数立即返回，事件循环继续 */
}

/* ---------- 单任务完成槽 ---------- */
void phaPullFromPolyhaven::handleTaskFinished(const DownloadResult& res)
{
    if (res.error.isEmpty()) {
        if (res.exists) {
            m_downloadedCount.fetch_add(1, std::memory_order_relaxed);
            Q_EMIT report("INFO", QString("Asset %1 already exists").arg(res.slug));
        }
        else {
            m_downloadedCount.fetch_add(1, std::memory_order_relaxed);
            Q_EMIT report("INFO", QString("Successfully downloaded asset: %1").arg(res.slug));
        }
    }
    else {
        m_failedCount.fetch_add(1, std::memory_order_relaxed);
        Q_EMIT report("ERROR", QString("Failed to download asset %1: %2").arg(res.slug).arg(res.error));
    }
    int cur = m_currentProgress.fetch_add(1, std::memory_order_relaxed) + 1;
    Q_EMIT progressUpdated(cur, m_totalToFetch, QString("Processing asset %1/%2...").arg(cur).arg(m_totalToFetch));

    if (m_remaining.fetch_sub(1, std::memory_order_relaxed) == 1)
        Q_EMIT allTasksFinished();
}

/* ---------- 全部任务完成槽 ---------- */
void phaPullFromPolyhaven::allTasksFinished()
{
    if (m_isCancelled.load(std::memory_order_relaxed)) {
        m_asyncResultCode = 1;
        Q_EMIT report("INFO", "Download cancelled by user");
    }
    else {
        m_asyncResultCode = 0;
    }
    Q_EMIT finished(m_downloadedCount.load(), m_failedCount.load());
    Q_EMIT executeFinished(m_asyncResultCode);

    /* 数据清空放主线程 */
    m_asyncAssets.clear();
    m_asyncLibDir = QDir();
}



/* 以下四个函数未改动，原样保留 */
DownloadResult phaPullFromPolyhaven::updateAsset(const QMap<QString, QJsonObject>& asset, const QDir& libDirPath, bool dryRun)
{
    DownloadResult result;
    result.slug = asset.keys().constFirst();
    result.exists = false;
    QDir assetDir(libDirPath.filePath(result.slug));
    if (!assetDir.exists() && !assetDir.mkpath(".")) {
        result.error = QString("Failed to create asset directory: %1").arg(assetDir.path());
        return result;
    }
    QFileInfo infoFp(assetDir.filePath("info.json"));//E:\Resource\Poly Haven\billiard_hall\info.json
    //QFileInfo thumbFp(assetDir.filePath("thumbnail.webp")); //E:\Resource\Poly Haven\billiard_hall\thumbnail.webp
    bool needUpdate = true;
    if (infoFp.exists() && !m_revalidate) {
        if (!checkAssetExists(asset, infoFp, needUpdate)) {
            result.error = QString("Failed to check asset %1 existence").arg(asset.firstKey());
            return result;
        }
        if (!needUpdate) {
            result.exists = true;
            return result;
        }
    }
    if (dryRun) {
        result.exists = infoFp.exists();
        return result;
    }
    QString err = downloadAsset(asset, libDirPath, infoFp);
    if (!err.isEmpty()) result.error = err;
    return result;
}

QString phaPullFromPolyhaven::downloadAsset(const QMap<QString, QJsonObject>& asset, const QDir& libDirPath, const QFileInfo& infoFp)
{
    // 你的下载逻辑（之前已修复的版本，精准下载 hdri→1k→hdr）
    QDir assetDir(libDirPath.filePath(asset.firstKey()));//assetDir E:\Resource\Poly Haven\billiard_hall
    if (!assetDir.exists() && !assetDir.mkpath(".")) {
        return QString("Failed to create asset directory: %1").arg(assetDir.path());
    }



    QFile infoFile(infoFp.filePath());//E:\Resource\Poly Haven\billiard_hall\info.json
    QFileInfo infofileinfo(infoFile);
    QJsonObject infoJson = asset.value(asset.firstKey());
    QJsonObject downloadJson;
    if (!infofileinfo.exists() || infofileinfo.size() == 0)
    {
        //QString infoUrl = QString("https://api.polyhaven.com/files/%1").arg(asset.firstKey());//https://api.polyhaven.com/files/billiard_hall
        //downloadJson = winhttp_get_json(infoUrl);
        QUrl infoUrl = QString("https://api.polyhaven.com/files/%1").arg(asset.firstKey());
        
        downloadJson = QJsonDocument::fromJson(get(infoUrl)).object();
        infoJson["files"] = downloadJson;
        if (infoJson.isEmpty()) {
            return QString("Failed to fetch asset info for %1").arg(asset.firstKey());
        }

        if (!infoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return QString("Failed to write info.json for %1: %2").arg(asset.firstKey()).arg(infoFile.errorString());
        }
        infoFile.write(QJsonDocument(infoJson).toJson(QJsonDocument::Indented));
        infoFile.close();
    }
    else
    {
        // 步骤2：打开文件（只读模式 + 文本模式，避免二进制解析问题）
        if (!infoFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            
            return QString("Failed to open %1: %2").arg(asset.firstKey()).arg(infoFile.errorString());
        }

        // 步骤3：读取文件内容（全部读取为字符串）
        QString jsonStr = infoFile.readAll();
        infoFile.close(); // 读取完成后立即关闭文件（重要！）

        // 步骤4：解析 JSON 字符串
        QJsonParseError parseError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonStr.toUtf8(), &parseError);

        // 步骤5：检查解析结果 + 转换为 QJsonObject
        if (parseError.error != QJsonParseError::NoError) {

            return QString(parseError.errorString());
        }
        if (!jsonDoc.isObject()) {
            
            return QString("shuzu");
        }

        // 成功解析为 QJsonObject
        infoJson = jsonDoc.object();
    }

    




    //QString error;

    QString thumbName = QString("thumbnail.webp");
    QString thumbPath = assetDir.filePath(thumbName);
    QFile thumbfile(thumbPath);
    QFileInfo thumbfileinfo(thumbfile);
    if (!thumbfileinfo.exists() || thumbfileinfo.size() == 0)
    {


        QUrl thumbUrl = QString("https://cdn.polyhaven.com/asset_img/thumbs/%1.png?width=256&height=256").arg(asset.firstKey());

        if (!download_file(thumbUrl, thumbPath))
        {
            return QString("Failed to download %1 for writing: %2").arg(thumbName).arg(thumbfile.errorString());
        }
    }



    /*
    QFile thumbfile(thumbPath);
    QFileInfo thumbfileinfo(thumbfile);
    if (!thumbfileinfo.exists() || thumbfileinfo.size() == 0)
    {
        if (!thumbfile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Unbuffered))
        {
            return QString("Failed to open %1 for writing: %2").arg(thumbName).arg(thumbfile.errorString());
        }

        QString thumbUrl = QString("https://cdn.polyhaven.com/asset_img/thumbs/%1.png?width=256&height=256").arg(asset.firstKey());  //("https://cdn.polyhaven.com/asset_img/thumbs/%1.png?width=256&height=256").arg(asset.slug)
        
        QByteArray thumbContent = winhttp_get_binary(thumbUrl, error);

        if (!error.isEmpty())
        {
            thumbfile.close();
            thumbfile.remove();
            return QString("Failed to download %1: %2").arg(thumbName).arg(error);
        }

        thumbfile.write(thumbContent);
        thumbfile.close();

        
    }
    */
    

    //QDir Dir(libDirPath.filePath(asset.slug));



    //TODO 有些info.json层级不一样 不知道为什么
    if (infoJson.contains("files") && infoJson["files"].isObject()) {
        QJsonObject filesJson = infoJson["files"].toObject();
        if (!filesJson.contains("hdri") || !filesJson["hdri"].isObject()) {
            return QString("Asset %1 has no '%2' hdri in files").arg(asset.firstKey()).arg("hdri");
        }

        QJsonObject hdriJson = filesJson["hdri"].toObject();


        const QString targetQuality = m_res;
        if (!hdriJson.contains(targetQuality) || !hdriJson[targetQuality].isObject()) {
            return QString("Asset %1 has no '%2' quality in hdri").arg(asset.firstKey()).arg(targetQuality);
        }

        QJsonObject qualityJson = hdriJson[targetQuality].toObject();
        const QString targetFormat = m_format;
        if (!qualityJson.contains(targetFormat) || !qualityJson[targetFormat].isObject()) {
            return QString("Asset %1 has no '%2' format in %3 quality").arg(asset.firstKey()).arg(targetFormat).arg(targetQuality);
        }

        QJsonObject formatJson = qualityJson[targetFormat].toObject();
        QString fileUrl = formatJson["url"].toString();
        if (fileUrl.isEmpty()) {
            return QString("Asset %1 has empty URL for %2-%3").arg(asset.firstKey()).arg(targetQuality).arg(targetFormat);
        }

        if (m_isCancelled || PHPlugin::PH_PROGRESS_CANCEL) {
            return "Cancelled while preparing download";
        }

        QUrl url(fileUrl);
        QString fileName = QFileInfo(url.path()).fileName();//xxx_1k.hdr
        if (fileName.isEmpty()) {
            fileName = QString("%1_%2.%3").arg(asset.firstKey()).arg(targetQuality).arg(targetFormat);
        }

        QString filePath = assetDir.filePath(fileName);
        QFile file(filePath);
        QFileInfo exrfileinfo(file);
        if (!exrfileinfo.exists() || exrfileinfo.size() == 0)
        {

            if (!download_file(url, filePath)) {
                return QString("Failed to download %1 : %2").arg(fileName).arg(file.errorString());
            }
            /*
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Unbuffered)) {
                return QString("Failed to open %1 for writing: %2").arg(fileName).arg(file.errorString());
            }


            QByteArray fileContent = winhttp_get_binary(fileUrl, error);
            if (!error.isEmpty()) {
                file.close();
                file.remove();
                return QString("Failed to download %1: %2").arg(fileName).arg(error);
            }
            file.write(fileContent);
            file.close();
            */
        }

        

        /*
        qint64 expectedSize = fileJson["size"].toInt();
        if (expectedSize > 0 && fileContent.size() != expectedSize) {
            file.close();
            file.remove();
            return QString("Size mismatch for %1 (expected %2, got %3)").arg(fileName).arg(expectedSize).arg(fileContent.size());
        }

        qint64 writtenBytes = file.write(fileContent);
        file.close();

        if (writtenBytes != fileContent.size()) {
            file.remove();
            return QString("Failed to write %1 (written %2 of %3 bytes)").arg(fileName).arg(writtenBytes).arg(fileContent.size());
        }

        QString expectedMd5 = fileJson["md5"].toString();
        if (!expectedMd5.isEmpty()) {
            QString actualMd5 = QCryptographicHash::hash(fileContent, QCryptographicHash::Md5).toHex();
            if (actualMd5 != expectedMd5) {
                file.remove();
                return QString("MD5 mismatch for %1 (expected %2, got %3)").arg(fileName).arg(expectedMd5).arg(actualMd5);
            }
        }
        */

        Q_EMIT report("INFO", QString("Downloaded %1 (%2-%3) to %4").arg(fileName).arg(targetQuality).arg(targetFormat).arg(filePath));
    }
    else {
        return QString("Asset %1 has no 'hdri' node in info.json").arg(asset.firstKey());
    }

    return "";
}

bool phaPullFromPolyhaven::checkAssetExists(const QMap<QString, QJsonObject>& asset, const QFileInfo& infoFp, bool& needUpdate)
{
    //没有info.json里面没有slug 所以这个有问题哦.
    QJsonObject oldInfo = loadOldInfo(infoFp);
    oldInfo.value("files").toObject();
    if (oldInfo.isEmpty()) {
        needUpdate = true;
        return true;
    }
    else
    {
        needUpdate = false;
        return true;
    }
    /*
    QString oldSlug = oldInfo["slug"].toString();
    needUpdate = (oldSlug != asset.slug) || m_revalidate;
    return true;
    */
}

QJsonObject phaPullFromPolyhaven::loadOldInfo(const QFileInfo& infoFp)
{
    QFile file(infoFp.filePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return QJsonObject();
    QByteArray data = file.readAll();
    file.close();
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    return doc.isObject() ? doc.object() : QJsonObject();
}



