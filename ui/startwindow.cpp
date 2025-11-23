#include "startwindow.h"
#include <QtNetwork/QSslSocket>
#include <iostream>
#include <string>
#include <QtNetwork/qtcpsocket.h>
#include <QtNetwork/qsslerror.h>
#include <QtCore/qstringliteral.h>
#include <QtCore/qthread.h>
#include <QtWidgets/qscrollbar.h>
#include <QtCore/qrect.h>
#include <QtCore/QStringList>

#include "download_file.h"

// 静态成员初始化
QString StartWindow::s_lastPath = "";
StartWindow* StartWindow::s_instance = nullptr;
QMutex StartWindow::s_mutex;

StartWindow::StartWindow(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::StartWindowClass())
{
    ui->setupUi(this);

    // 缓存初始化（900张图建议缓存300张，平衡内存和加载速度）
    m_thumbCache.setMaxCost(300);
    m_assetDelegate = new AssetDelegate(this);
    m_assetDelegate->setThumbCache(&m_thumbCache); // 传递缓存给委托

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addWidget(ui->horizontalLayoutWidget);
    setLayout(mainLayout);

    m_statusBar = new QStatusBar();
    mainLayout->addWidget(m_statusBar);

    s_lastPath = loadPathFromConfig();
    ui->m_pathLabel->setText(s_lastPath);

    ui->controlLayout->addStretch();
    ui->propertyLayout->addStretch();

    m_polyhavenWorker = new phaPullFromPolyhaven(this);
    m_assetModel = nullptr;
    m_categoriesModel = nullptr;
    m_tagModel = new QStandardItemModel(this);

    // 初始化 ListView 基础优化（提前配置，避免重复设置）
    if (ui->m_assetListView) {
        ui->m_assetListView->setViewMode(QListView::IconMode);
        ui->m_assetListView->setUniformItemSizes(true); // 关键：启用项复用
        ui->m_assetListView->setBatchSize(200); // 批量绘制
        ui->m_assetListView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel); // 像素级滚动
        ui->m_assetListView->setSpacing(8); // 卡片间距
        ui->m_assetListView->setResizeMode(QListView::Adjust); // 窗口缩放自动调整
        ui->m_assetListView->setStyleSheet("QListView { show-decoration-selected: 0; background-color: #222222; }"); // 禁用选中动画
    }

    // 绑定滚动事件：滚动时预加载可见区域附近图片
    if (ui->m_assetListView) {
        connect(ui->m_assetListView->verticalScrollBar(), &QScrollBar::valueChanged, this, [=]() {
            // 延迟50ms执行，避免滚动过程中频繁调用
            QTimer::singleShot(50, this, &StartWindow::loadVisibleAreaThumbs);
            });
    }
    connect(m_polyhavenWorker, &phaPullFromPolyhaven::progressUpdated,
        this, &StartWindow::onProgressUpdated);

}

StartWindow* StartWindow::getInstance()
{
    if (!s_instance) {
        QMutexLocker locker(&s_mutex);
        if (!s_instance) {
            s_instance = new StartWindow();
        }
    }
    return s_instance;
}

void StartWindow::destroyInstance()
{
    QMutexLocker locker(&s_mutex);
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
    }
}

StartWindow::~StartWindow()
{
    delete ui;
}

// 核心实现：加载可见区域及附近图片（解决静止不加载问题）
void StartWindow::loadVisibleAreaThumbs()
{
    if (!ui->m_assetListView || !m_assetModel || m_assetModel->rowCount() == 0) {
        return;
    }

    // 获取可见区域矩形（视口本地坐标）
    QRect viewRect = ui->m_assetListView->viewport()->rect();
    // 转换矩形的左上角和右下角到 ListView 坐标系（mapToParent 只支持点转换）
    QPoint topLeft = ui->m_assetListView->viewport()->mapToParent(viewRect.topLeft());
    QPoint bottomRight = ui->m_assetListView->viewport()->mapToParent(viewRect.bottomRight());
    // 用转换后的两点重新构造矩形
    viewRect = QRect(topLeft, bottomRight);

    // 获取可见区域的起始和结束行索引
    int visibleStart = ui->m_assetListView->indexAt(viewRect.topLeft()).row();
    int visibleEnd = ui->m_assetListView->indexAt(viewRect.bottomRight()).row();

    // 处理边界情况（未获取到有效索引时的默认范围）
    if (visibleStart == -1) visibleStart = 0;
    if (visibleEnd == -1) visibleEnd = qMin(30, m_assetModel->rowCount() - 1); // 默认加载前30行

    // 扩展加载范围：前后各15行（提前缓存，滚动无感知）
    int loadStart = qMax(0, visibleStart - 15);
    int loadEnd = qMin(m_assetModel->rowCount() - 1, visibleEnd + 15);

    // 遍历需要加载的行，提取图片路径并预加载
    for (int i = loadStart; i <= loadEnd; ++i) {
        QModelIndex idx = m_assetModel->index(i);
        if (!idx.isValid()) continue;

        // 从模型获取资产数据（对应 AssetModel::AssetDataRole）
        QVariantMap asset = idx.data(AssetModel::AssetDataRole).toMap();
        QString imgPath = asset.value("local_thumbnail_path").toString();

        // 缓存未命中时，启动子线程加载（避免重复加载）
        if (!imgPath.isEmpty() && !m_thumbCache.contains(imgPath)) {
            m_assetDelegate->loadThumbInThread(imgPath);
        }
    }
}

void StartWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    // 窗口缩放后重新布局，并加载新可见区域图片
    if (ui->m_assetListView) {
        ui->m_assetListView->doItemsLayout();
        // 延迟加载，避免缩放过程中频繁调用
        QTimer::singleShot(30, this, &StartWindow::loadVisibleAreaThumbs);
    }
}

void StartWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_firstShow) {
        m_firstShow = false;
        loadAssets(false);
        loadTreeModel();
        // 窗口显示后，延迟加载第一屏图片（确保布局完成）
        QTimer::singleShot(50, this, &StartWindow::loadVisibleAreaThumbs);
    }
    else {
        // 非首次显示（如窗口切换回来），加载当前可见区域
        QTimer::singleShot(10, this, &StartWindow::loadVisibleAreaThumbs);
    }
}

void StartWindow::importLight()
{
    PY_InterpreterAutoLock py_lock;
    QString exrPath = this->m_exrPath;

    QString pythonScript = QStringLiteral(R"(
import toolutils
import loptoolutils
import hou

exr_path = "%1"

pane = toolutils.sceneViewer()
kwargs = {}
kwargs["pane"] = pane

if (loptoolutils.getToolCategoryForPane(pane) == hou.lopNodeTypeCategory()):
    newnode = loptoolutils.genericTool(kwargs, 'domelight::3.0', 'domelight1', clicktoplace=False)
    newnode.parm('xn__inputstexturefile_r3ah').set(exr_path)
else:
    import objecttoolutils
    from objecttoolutils import OrientInfo
    newnode = objecttoolutils.genericTool(kwargs, 'envlight', None, False, orient=OrientInfo('r'))
    newnode.parm("env_map").set(exr_path)
    )").arg(exrPath.replace("\\", "/"));

    PY_Result py_result = PYrunPythonStatements(
        pythonScript.toUtf8().constData(),
        NULL
    );
}

void StartWindow::onProgressUpdated(int current, int total, const QString& text)
{
    // 1. 更新进度条（设置最大值和当前值）
    ui->m_progressBar->setMaximum(total);
    ui->m_progressBar->setValue(current);

    // 2. 更新状态文本（显示“正在处理第 x/y 个资产”）
    //m_statusLabel->setText(text);
    m_statusBar->showMessage(text);

    // 3. 可选：任务完成后重置 UI
    if (current == total) {
        //m_statusLabel->setText("Task completed!");
        ui->m_progressBar->reset(); // 或保持 100% 状态
    }
}

void StartWindow::showConfirmation(const QString& text)
{
    if (text != "hdris") {
        QMessageBox::information(this, u8"取消", QString(u8"Polyhaven 拉取操作已取消,只支持hdris！（类型：%1）").arg(text));
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        u8"操作确认",
        QString(u8"确认下载 %1 ？").arg(text),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        ui->fetchComboBtn->setEnabled(false);
        ui->m_cancelBtn->setEnabled(true);
        onTextChanged(text);
    }
}


//TODO
/*
void StartWindow::test()
{
    QUrl myUrl = QString("https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/2k/abandoned_church_2k.hdr");

    // 使用 Houdini 环境变量扩展路径
    // 注意：如果你用了 libUT，可以用 UT_String 转换，或者直接硬编码测试
    QString savePath = "E:/testxx.hdr";

    download_file(myUrl, savePath);
}
*/





void StartWindow::onTextChanged(const QString& text)
{
    
    if (!m_polyhavenWorker) {
        QMessageBox::critical(this, u8"错误", u8"Polyhaven 工具类实例初始化失败！");
        return;
    }

    disconnect(m_polyhavenWorker, &phaPullFromPolyhaven::executeFinished, this, nullptr);

    m_polyhavenWorker->setAssetType(text);
    m_polyhavenWorker->setRevalidate(true);//TODO

    connect(m_polyhavenWorker, &phaPullFromPolyhaven::executeFinished, this, [=](int resultCode) {
        switch (resultCode) {
        case 0:
            ui->fetchComboBtn->setEnabled(true);
            ui->m_cancelBtn->setEnabled(false);
            QMessageBox::information(this, u8"成功", QString(u8"Polyhaven 拉取操作执行完成！（类型：%1）").arg(text));
            break;
        case 1:
            ui->fetchComboBtn->setEnabled(true);
            ui->m_cancelBtn->setEnabled(false);
            QMessageBox::information(this, u8"取消", QString(u8"Polyhaven 拉取操作已取消！（类型：%1）").arg(text));
            break;
        case -1:
            ui->fetchComboBtn->setEnabled(true);
            ui->m_cancelBtn->setEnabled(false);
            QMessageBox::critical(this, u8"失败", QString(u8"未找到资产库，请先在创建 \"Poly Haven\" 文件夹！（类型：%1）").arg(text));
            break;
        case -2:
            ui->fetchComboBtn->setEnabled(true);
            ui->m_cancelBtn->setEnabled(false);
            QMessageBox::critical(this, u8"失败", QString(u8"资产库路径不存在，请检查路径有效性！（类型：%1）").arg(text));
            break;
        case -3:
            ui->fetchComboBtn->setEnabled(true);
            ui->m_cancelBtn->setEnabled(false);
            QMessageBox::critical(this, u8"失败", QString(u8"获取资产列表失败！（类型：%1）").arg(text));
            break;
        default:
            ui->fetchComboBtn->setEnabled(true);
            ui->m_cancelBtn->setEnabled(false);
            QMessageBox::critical(this, u8"失败", QString(u8"Polyhaven 拉取操作执行失败！（错误码：%1，类型：%2）").arg(resultCode).arg(text));
            break;
        }
        });

    m_polyhavenWorker->executeAsync();
}

void StartWindow::canceldownload()
{
    m_polyhavenWorker->cancelDownload();
    ui->fetchComboBtn->setEnabled(true);
    ui->m_cancelBtn->setEnabled(false);
}

void StartWindow::savePathToConfig(const QString& path)
{
    QSettings settings(ORG_NAME, APP_NAME);
    settings.setValue(PATH_KEY, path);
    s_lastPath = path;
}

QString StartWindow::loadPathFromConfig()
{
    QSettings settings(ORG_NAME, APP_NAME);
    return settings.value(PATH_KEY).toString();
}

void StartWindow::loadAssets(bool filtered)
{
    /* 1. 清空旧模型 */
    if (m_assetModel) {
        m_assetModel->deleteLater();
        m_assetModel = nullptr;
    }

    /* 2. 若无数据，重新加载 */
    if (m_assetsData.isEmpty()) {
        m_assetsData = get_asset_lib();
    }

    /* 3. 数据有效性检查 */
    if (m_assetsData.isEmpty() || !ui->m_assetListView) {
        m_statusBar->showMessage(u8"未找到有效资产数据");
        return;
    }

    /* 4. 创建并绑定新模型 */
    if (!filtered) {
        m_assetModel = new AssetModel(m_assetsData, this);
        ui->m_assetListView->setModel(m_assetModel);
        ui->m_assetListView->setItemDelegate(m_assetDelegate);
        m_statusBar->showMessage(
            QString(u8"加载完成：共%1个资产").arg(m_assetsData.size()));
    }
    else {
        // 筛选场景的模型设置（原有逻辑保留）
        QMap<QString, QJsonObject> filteredAssets = filterAssets();
        m_assetModel = new AssetModel(filteredAssets, this);
        ui->m_assetListView->setModel(m_assetModel);
        m_statusBar->showMessage(
            QString(u8"筛选完成：共%1个资产").arg(filteredAssets.size()));
    }

    // 资产加载完成后，触发一次可见区域加载
    QTimer::singleShot(20, this, &StartWindow::loadVisibleAreaThumbs);
}

void StartWindow::onCategoryClicked(const QModelIndex& index)
{
    if (!index.isValid()) return;

    QString fullPath = index.data(CATALOG_PATH_ROLE).toString();

    if (fullPath.contains("HDRIs"))
        m_currentCategory = 0;
    else if (fullPath.contains("Textures"))
        m_currentCategory = 1;
    else if (fullPath.contains("Models"))
        m_currentCategory = 2;
    else
        m_currentCategory = 3;

    QStringList fullList = fullPath.split('/');
    QStringList categoryList;
    for (const QString& item : fullList) {
        if (item != "ALL" && item != "HDRIs" && item != "Models" && item != "Textures")
            categoryList.append(item.toLower());
    }
    m_categoryList = categoryList;

    applyFilter();
}

void StartWindow::updateText(const QString& text)
{
    m_searchText = text.toLower().trimmed();
    applyFilter();
}

void StartWindow::onAssetPreview(const QVariantMap& asset)
{
    m_tagModel->clear();
    QStringList items;



    // 解析 tags（修改后）
    auto tagsVar = asset["tags"];
    if (tagsVar.typeName() == QString("QJsonValue")) { // 新增：处理 QJsonValue 类型（JSON 数组）
        QJsonValue tagsVal = tagsVar.value<QJsonValue>();
        if (tagsVal.isArray()) {
            Q_FOREACH(const QJsonValue & jsonVal, tagsVal.toArray()) {
                if (jsonVal.isString()) {
                    items << jsonVal.toString();
                }
            }
        }
    }
    else if (tagsVar.typeId() == QMetaType::QStringList) {
        items << tagsVar.toStringList();
    }
    else if (tagsVar.typeId() == QMetaType::QString) {
        items << tagsVar.toString().split(' ', Qt::SkipEmptyParts);
    }

    // 解析 categories（同样修改，新增 QJsonValue 分支）
    auto catVar = asset["categories"];
    if (catVar.typeName() == QString("QJsonValue")) { // 新增：处理 QJsonValue 类型（JSON 数组）
        QJsonValue catsVal = catVar.value<QJsonValue>();
        if (catsVal.isArray()) {
            Q_FOREACH(const QJsonValue & jsonVal, catsVal.toArray()) {
                if (jsonVal.isString()) {
                    items << jsonVal.toString();
                }
            }
        }
    }
    else if (catVar.typeId() == QMetaType::QStringList) {
        items << catVar.toStringList();
    }
    else if (catVar.typeId() == QMetaType::QString) {
        items << catVar.toString().split(' ', Qt::SkipEmptyParts);
    }

    // 去重（可选，避免 tags 和 categories 重复）
    items.removeDuplicates();

    // 后续添加到模型
    for (const QString& text : items) {
        auto* item = new QStandardItem(text);
        item->setEditable(false);
        m_tagModel->appendRow(item);
    }

    ui->m_tagListView->setModel(m_tagModel);

    QString libPath = get_asset_lib_path();
    QString assetId = asset["asset_id"].toString();
    if (assetId.isEmpty()) assetId = QStringLiteral("未知");
    m_exrPath = libPath + "/" + assetId + "/" + assetId + "_1k.hdr";

    ui->label_2->setText(asset["name"].toString().isEmpty()
        ? QStringLiteral("未知资产")
        : asset["name"].toString());
    ui->label_4->setText(m_exrPath);
    ui->label_6->setText(QStringLiteral("A CC0 by polyhaven.com"));

    ui->label_8->setText(asset["authors"].toString().isEmpty()
        ? QStringLiteral("未知资产")
        : asset["authors"].toString());
}

QMap<QString, QJsonObject> StartWindow::filterAssets() const
{
    QString text = m_searchText.toLower().trimmed();
    QStringList fList = m_categoryList;

    QMap<QString, QJsonObject> filtered;

    if (m_assetsData.isEmpty())
        return filtered;

    auto it = m_assetsData.constBegin();
    while (it != m_assetsData.constEnd()) {
        const QJsonObject& asset = it.value();

        /* 1. 分类匹配（0/1/2/3）*/
        int assetType = asset.value("type").toInt();
        bool categoryMatch = (m_currentCategory == 3) || (assetType == m_currentCategory);

        /* 2. 空条件快速分支 */
        if (fList.isEmpty() && text.isEmpty()) {
            if (categoryMatch)
                filtered.insert(it.key(), asset);
            ++it;
            continue;
        }

        /* 3. 文本+分类联合过滤 */
        //QString assetName = asset.name.toLower();
        QString assetName = asset.value("name").toString().toLower();

        /* 统一把 tags & categories 合并到 QStringList */
        QStringList assetTags;
        // 1. 处理 tags 数组：转为 QStringList 并添加到 assetTags
        QJsonValue tagsValue = asset.value("tags");
        if (tagsValue.isArray()) {
            QVariantList tagsVariant = tagsValue.toArray().toVariantList();
            Q_FOREACH(const QVariant & var, tagsVariant) { // 遍历每个 QVariant
                assetTags.append(var.toString()); // 显式转为 QString
            }
        }


        // 2. 处理 categories 数组：转为 QStringList 并添加到 assetTags
        QJsonValue categoriesValue = asset.value("categories");
        if (categoriesValue.isArray()) {
            QVariantList catsVariant = categoriesValue.toArray().toVariantList();
            Q_FOREACH(const QVariant & var, catsVariant) {
                assetTags.append(var.toString());
            }
        }

        /* 文本匹配：name 或 任一 tag */
        bool textMatch = assetName.contains(text) ||
            std::any_of(assetTags.constBegin(), assetTags.constEnd(),
                [&](const QString& t) { return t.contains(text); });

        /* f_list 全部包含（不区分大小写）*/
        bool allIn = true;
        for (const QString& item : fList) {
            bool found = std::any_of(assetTags.constBegin(), assetTags.constEnd(),
                [&](const QString& t) { return t.contains(item.toLower()); });
            if (!found) {
                allIn = false;
                break;
            }
        }

        if (allIn && categoryMatch && textMatch)
            filtered.insert(it.key(), asset);

        ++it;
    }

    return filtered;
}

void StartWindow::applyFilter()
{
    QMap<QString, QJsonObject> filtered = filterAssets();

    if (m_assetModel) {
        m_assetModel->deleteLater();
        m_assetModel = nullptr;
    }

    m_assetModel = new AssetModel(filtered, this);
    ui->m_assetListView->setModel(m_assetModel);
    ui->m_assetListView->setItemDelegate(m_assetDelegate);

    m_statusBar->showMessage(QString(u8"筛选结果：共%1个资产").arg(filtered.size()));

    // 筛选完成后，加载新的可见区域图片
    QTimer::singleShot(20, this, &StartWindow::loadVisibleAreaThumbs);
}

QStringList StartWindow::parseCatalogFile()
{
    QString fileContent = QStringLiteral("6b43eea6-2dee-4960-8ff0-edfb4f283858:HDRIs:HDRIs\n"
        "e2328e9a-1cec-4c44-b542-5cd52bfbce19:HDRIs/Indoor:HDRIs-Indoor\n"
        "1f0e3686-4835-433f-a076-074d204d3a43:HDRIs/Indoor/Natural Light:HDRIs-Indoor-Natural Light\n"
        "00459860-c6d8-4632-b5e9-19929853cf3b:HDRIs/Indoor/Natural Light/Medium Contrast:HDRIs-Indoor-Natural Light-Medium Contrast\n"
        "7c6ef490-2383-461c-b6ee-dcdd6714a2c4:HDRIs/Indoor/Natural Light/Low Contrast:HDRIs-Indoor-Natural Light-Low Contrast\n"
        "5cb78b4d-9bcc-4b91-8a30-ff185da05e18:HDRIs/Indoor/Natural Light/High Contrast:HDRIs-Indoor-Natural Light-High Contrast\n"
        "768ae7a1-5518-495d-9176-7cccad60a71c:HDRIs/Indoor/Artificial Light:HDRIs-Indoor-Artificial Light\n"
        "5f70a139-9202-4fc9-a218-bcea5f9ab263:HDRIs/Indoor/Artificial Light/Medium Contrast:HDRIs-Indoor-Artificial Light-Medium Contrast\n"
        "1134e281-8fda-4913-9662-06d42776b3e9:HDRIs/Indoor/Artificial Light/Low Contrast:HDRIs-Indoor-Artificial Light-Low Contrast\n"
        "1325556c-7986-4568-a694-1134b6e16d32:HDRIs/Indoor/Artificial Light/High Contrast:HDRIs-Indoor-Artificial Light-High Contrast\n"
        "f1e775ea-3173-41fe-b0e1-e315a61ab694:HDRIs/Indoor/Studio:HDRIs-Indoor-Studio\n"
        "feae2c70-9016-4506-af27-6db147f93d00:HDRIs/Indoor/Studio/Medium Contrast:HDRIs-Indoor-Studio-Medium Contrast\n"
        "af336f94-da70-4e9e-b3aa-9444d6b270fc:HDRIs/Indoor/Studio/Low Contrast:HDRIs-Indoor-Studio-Low Contrast\n"
        "0ecbbd9d-7504-41d6-988c-1470362f5fc4:HDRIs/Indoor/Studio/High Contrast:HDRIs-Indoor-Studio-High Contrast\n"
        "a40d8a97-03b5-4a77-aafc-4f31126329fe:HDRIs/Outdoor:HDRIs-Outdoor\n"
        "eac57a1b-31a6-4542-a7c6-797799cc49c9:HDRIs/Outdoor/Nature:HDRIs-Outdoor-Nature\n"
        "41ce4c4f-8ff1-4aa9-ae68-0120b44f0b77:HDRIs/Outdoor/Nature/Midday:HDRIs-Outdoor-Nature-Midday\n"
        "8d14dcb5-f464-43ea-bbf5-15df36cf406e:HDRIs/Outdoor/Nature/Morning-afternoon:HDRIs-Outdoor-Nature-Morning-afternoon\n"
        "415760a2-a428-4781-bf8e-636735f059fe:HDRIs/Outdoor/Nature/Night:HDRIs-Outdoor-Nature-Night\n"
        "0f03b383-10da-4c89-819f-02a29bbc4bce:HDRIs/Outdoor/Nature/Sunrise-sunset:HDRIs-Outdoor-Nature-Sunrise-sunset\n"
        "8908b21d-a3fe-48ab-a59c-3d1d4553344a:HDRIs/Outdoor/Urban:HDRIs-Outdoor-Urban\n"
        "5f2c9621-3c96-4944-8206-d11cef84b586:HDRIs/Outdoor/Urban/Midday:HDRIs-Outdoor-Urban-Midday\n"
        "18a3a12c-9d33-404c-9c95-811fe23052a8:HDRIs/Outdoor/Urban/Morning-afternoon:HDRIs-Outdoor-Urban-Morning-afternoon\n"
        "44100f90-2dc1-4d97-b8dc-d192afd42b89:HDRIs/Outdoor/Urban/Night:HDRIs-Outdoor-Urban-Night\n"
        "b35909d0-be3d-4be5-a888-caf24f881148:HDRIs/Outdoor/Urban/Sunrise-sunset:HDRIs-Outdoor-Urban-Sunrise-sunset\n"
        "f2d63417-5e13-4efd-9987-93697bbf0a40:HDRIs/Outdoor/Skies:HDRIs-Outdoor-Skies\n"
        "ee1c2a23-f241-40e2-a0f1-46f3469b6ac7:HDRIs/Outdoor/Skies/Midday:HDRIs-Outdoor-Skies-Midday\n"
        "f21f81e2-3dde-4f2c-b9b4-3180a98d3d2e:HDRIs/Outdoor/Skies/Morning-afternoon:HDRIs-Outdoor-Skies-Morning-afternoon\n"
        "8e41e6f9-b630-40c3-8427-3bace6c8268c:HDRIs/Outdoor/Skies/Night:HDRIs-Outdoor-Skies-Night\n"
        "e64bbf5b-1ddf-47b2-9a6f-adf38150e68c:HDRIs/Outdoor/Skies/Sunrise-sunset:HDRIs-Outdoor-Skies-Sunrise-sunset\n"
        "0b4a1940-abfc-435d-ae06-1b1912fcb0fa:Textures:Textures\n"
        "fc0b7e40-beca-4085-a2c0-62fcff980615:Textures/Rock:Textures-Rock\n"
        "57a6a6bc-1144-4fb9-9858-226c85c1062d:Textures/Terrain:Textures-Terrain\n"
        "f0457b2f-0212-4fb7-968b-35cb79bf6c8b:Textures/Terrain/Aerial:Textures-Terrain-Aerial\n"
        "5314108e-cb5a-43a5-aee0-0dc00fc2ed88:Textures/Terrain/Rock:Textures-Terrain-Rock\n"
        "c79f5bda-6ad7-4556-972a-8dcc07a2d3f2:Textures/Terrain/Sand:Textures-Terrain-Sand\n"
        "8753a952-0631-4b2d-9dbf-1122a725268e:Textures/Terrain/Snow:Textures-Terrain-Snow\n"
        "eba19344-dabb-4949-b152-04bad720a3f7:Textures/Roofing:Textures-Roofing\n"
        "e267e464-8444-4213-b8ab-426d0aae55c3:Textures/Wood:Textures-Wood\n"
        "cb20759c-8417-4047-aca2-19755b99e892:Textures/Brick:Textures-Brick\n"
        "dde53271-57f5-4d03-9d49-0384142ce2f4:Textures/Fabric:Textures-Fabric\n"
        "09cf6a6c-a358-4f01-b38e-fe17f027f493:Textures/Metal:Textures-Metal\n"
        "18ac3bd9-c74b-419b-b275-31047f5ade74:Textures/Plaster-concrete:Textures-Plaster-concrete\n"
        "38881529-9e8f-474e-81c8-d535cba978f4:Models:Models\n"
        "419127fb-d621-4dd9-b881-8e0d625119fb:Models/Props:Models-Props\n"
        "1082487c-9383-4e5d-bbbd-13fe8007dc9e:Models/Props/Appliances:Models-Props-Appliances\n"
        "b02575d8-d0fb-4a8c-8233-ec3ac8b4b052:Models/Props/Electronics:Models-Props-Electronics\n"
        "62daa2e2-0e53-4623-a9ed-fee4190c99e0:Models/Props/Tools:Models-Props-Tools\n"
        "d006c372-76c4-4f2b-b22d-42d60b89d086:Models/Industrial:Models-Industrial\n"
        "3dfced29-5b6c-4a9c-8e4a-19e1c1330c4c:Models/Lighting:Models-Lighting\n"
        "ae0813db-d65b-447f-8535-2a4856d98b9a:Models/Nature:Models-Nature\n"
        "d88a690f-556e-4936-a8a6-9b675b8c2031:Models/Nature/Food:Models-Nature-Food\n"
        "e60a325b-60d7-4b45-9e0d-1bb268c763bb:Models/Nature/Ground Cover:Models-Nature-Ground Cover\n"
        "f55affcc-c81f-4e51-9b76-b4273189824a:Models/Nature/Ground Cover/Grass:Models-Nature-Ground Cover-Grass\n"
        "35e80462-1b62-4736-95dc-ed8bb0c4cb1a:Models/Nature/Plants:Models-Nature-Plants\n"
        "05825054-8739-4c42-8bc7-f70e15625a9b:Models/Nature/Potted Plants:Models-Nature-Potted Plants\n"
        "1d68844f-b0e7-40ee-8819-d81eb1ee6baf:Models/Nature/Rocks:Models-Nature-Rocks\n"
        "5eb59ce6-abcf-4c4c-8b72-d3540ceda629:Models/Furniture:Models-Furniture\n"
        "c362fb1a-f3cf-4afc-ab4c-9ac979d7a20d:Models/Furniture/Seating:Models-Furniture-Seating\n"
        "f7d9a241-6a68-4104-bdf8-1c13109e8e6c:Models/Furniture/Shelves:Models-Furniture-Shelves\n"
        "dac0d06c-3e0d-4530-981c-530b89fb0cb2:Models/Furniture/Table:Models-Furniture-Table");

    QTextStream in(&fileContent);
    QSet<QString> allPaths;
    QString line;

    while (!in.atEnd()) {
        line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#') || line.startsWith("VERSION"))
            continue;

        QStringList parts = line.split(':');
        if (parts.size() < 2) continue;

        QString path = parts[1].trimmed().replace(" ", " ");
        QString fullPath = "ALL/" + path;

        QStringList segs = fullPath.split('/');
        for (int i = 1; i <= segs.size(); ++i) {
            QString cur = segs.mid(0, i).join('/');
            allPaths.insert(cur);
        }
    }

    QStringList sorted = allPaths.values();
    std::sort(sorted.begin(), sorted.end(),
        [](const QString& a, const QString& b) {
            int la = a.count('/');
            int lb = b.count('/');
            return la != lb ? la < lb : a < b;
        });
    return sorted;
}

QStandardItemModel* StartWindow::buildTreeModel(const QStringList& sortedPaths)
{
    auto* model = new QStandardItemModel();
    QStandardItem* rootItem = model->invisibleRootItem();

    QHash<QString, QStandardItem*> itemMap;
    itemMap[""] = rootItem;

    for (const QString& path : sortedPaths) {
        if (path.isEmpty()) continue;

        QString displayName = path.split('/').last();
        auto* item = new QStandardItem(displayName);
        item->setData(path, CATALOG_PATH_ROLE);
        itemMap[path] = item;

        QString parentPath = path.section('/', 0, -2);
        auto* parentItem = itemMap.value(parentPath);
        if (parentItem && item->parent() != parentItem)
            parentItem->appendRow(item);
    }
    return model;
}

void StartWindow::loadTreeModel()
{
    if (m_categoriesModel) {
        m_categoriesModel->deleteLater();
        m_categoriesModel = nullptr;
    }

    QStringList paths = parseCatalogFile();
    m_categoriesModel = buildTreeModel(paths);

    ui->m_categoriesTreeView->setModel(m_categoriesModel);
    ui->m_categoriesTreeView->setHeaderHidden(true);

    // 绑定分类点击事件（如果之前没绑定）
    connect(ui->m_categoriesTreeView, &QTreeView::clicked, this, &StartWindow::onCategoryClicked);
}

// 重写 closeEvent，确保关闭时重置实例指针
void StartWindow::closeEvent(QCloseEvent* event)
{
    m_polyhavenWorker->cancelDownload();
    s_instance = nullptr;
    QWidget::closeEvent(event);
}

void StartWindow::showDialog()
{
    QString newPath = QFileDialog::getExistingDirectory();
    if (!newPath.isEmpty()) {
        savePathToConfig(newPath);
        ui->m_pathLabel->setText(newPath);
    }
}