#include "AssetDownloadTask.h"
/* ---------- AssetDownloadTask ---------- */


AssetDownloadTask::AssetDownloadTask(const QMap<QString, QJsonObject>& asset, const QDir& libDir, bool revalidate, phaPullFromPolyhaven* parent)
    : m_asset(asset), m_libDir(libDir), m_revalidate(revalidate), m_parent(parent)
{
    setAutoDelete(true);
}

void AssetDownloadTask::run()
{
    DownloadResult result;
    //result.slug = m_asset.slug;
    
    result.slug = m_asset.keys().constFirst();
    if (m_parent->m_isCancelled.load(std::memory_order_relaxed) || PHPlugin::PH_PROGRESS_CANCEL) {
        result.error = "Task cancelled";
        Q_EMIT taskFinished(result);
        return;
    }
    result = m_parent->updateAsset(m_asset, m_libDir, false);
    Q_EMIT taskFinished(result);
}