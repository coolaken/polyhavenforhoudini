#pragma once

#include <QtWidgets/qwidget.h>
#include <QtCore/qmutex.h>
#include <QtCore/qscopedpointer.h>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/qtreeview.h>
#include <QtWidgets/qprogressbar.h>
#include <QtCore/qsettings.h>

#include <QtWidgets/qlabel.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qgroupbox.h>

#include <QtWidgets/qlistview.h>
#include <QtGui/qstandarditemmodel.h>
#include <QtWidgets/qscrollarea.h>

#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qmessagebox.h>
#include <QtWidgets/qfiledialog.h>
#include <QtCore/qtimer.h>
#include <QtCore/qcache.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qfile.h>
#include <QtCore/qdir.h>
#include <QtCore/qvariantmap.h>

#include <UT/UT_String.h>
#include <PY/PY_Python.h>
#include <PY/PY_InterpreterAutoLock.h>

#include "AssetDelegate.h"
#include "AssetModel.h"
#include "ui_startwindow.h"

// 前置声明（避免未定义错误，若 AssetInfo 有单独头文件可包含）
//struct AssetInfo;
class phaPullFromPolyhaven;

QT_BEGIN_NAMESPACE
namespace Ui { class StartWindowClass; };
QT_END_NAMESPACE

class StartWindow : public QWidget
{
    Q_OBJECT

private:
    // 1. 私有构造函数：禁止外部直接 new
    explicit StartWindow(QWidget* parent = nullptr);
    // 2. 私有拷贝构造/赋值运算符：禁止拷贝
    StartWindow(const StartWindow&) = delete;
    StartWindow& operator=(const StartWindow&) = delete;

    // 3. 静态成员：存储唯一实例 + 线程安全锁
    static StartWindow* s_instance;
    static QMutex s_mutex;

public:
    // 4. 全局唯一获取接口：不存在则创建，存在则直接返回
    static StartWindow* getInstance();
    // 5. （可选）安全销毁接口（避免内存泄漏）
    static void destroyInstance();

    ~StartWindow();

private:
    Ui::StartWindowClass* ui;

    QCache<QString, QPixmap> m_thumbCache; // 全局缓存（供 Delegate 共享）
    AssetDelegate* m_assetDelegate;
    QStatusBar* m_statusBar;
    AssetModel* m_assetModel;

    phaPullFromPolyhaven* m_polyhavenWorker = nullptr;

    // 路径相关
    QString m_packagePath;
    QString m_configFile;
    QString m_assetPath;
    QString m_phcFile;
    QString m_assetListCachePath;
    QString m_exrPath;

    // 数据相关
    QMap<QString, QJsonObject> m_assetsData;
    QString m_searchText;
    QVector<QString> m_categoryList;
    int m_currentCategory = 3;  // 0:HDRIs,1:Textures,2:Models,3:All

    bool m_firstShow = true;

    // UI 组件
    QStandardItemModel* m_categoriesModel;
    QStandardItemModel* m_tagModel;

    // 属性面板标签
    QLabel* m_labelName;    // 名称
    QLabel* m_labelSource;  // 源
    QLabel* m_labelDesc;    // 描述
    QLabel* m_labelAuthor;  // 作者

    // 分类映射
    QHash<QString, QString> m_catalogNameToPathMap;

    // 核心：加载可见区域及附近的图片（解决静止不加载问题）
    void loadVisibleAreaThumbs();
    // 加载资产数据（原有函数）
    void loadAssets(bool filtered = false);

public:
    const QString ORG_NAME = "coolaken";   // 自定义（如你的公司/个人名称）
    const QString APP_NAME = "polyhavenforhoudini";       // 自定义（如你的程序名称）
    const QString PATH_KEY = "LastFilepath";// 配置项的键（用于读取/写入）
    static QString s_lastPath;

protected:
    // 重写尺寸变化事件：窗口缩放时重新布局 ListView 项
    void resizeEvent(QResizeEvent* event) override;
    // 重写显示事件：窗口显示后加载第一屏图片
    void showEvent(QShowEvent* event) override;
    // 重写关闭事件：（原有逻辑保留）
    void closeEvent(QCloseEvent* event) override;

private Q_SLOTS:
    void showDialog();
    void importLight();


    void onProgressUpdated(int current, int total, const QString& text);

    // 路径选择相关
    void showConfirmation(const QString& text = "all");

    void test();
    void onTextChanged(const QString& text);
    void canceldownload();

    void savePathToConfig(const QString& path);
    QString loadPathFromConfig();

    // 解析目录文件（原有函数）
    QStringList parseCatalogFile();
    // 构建树形模型（原有函数）
    QStandardItemModel* buildTreeModel(const QStringList& sortedPaths);
    // 应用筛选（原有函数）
    void applyFilter();
    // 筛选资产（原有函数）
    QMap<QString, QJsonObject> filterAssets() const;
    // 加载树形模型（原有函数）
    void loadTreeModel();
    // 分类点击事件（原有函数）
    void onCategoryClicked(const QModelIndex& index);
    // 更新文本（原有函数）
    void updateText(const QString& text);
    // 资产预览事件（供 Delegate 调用）
    void onAssetPreview(const QVariantMap& asset);
};
