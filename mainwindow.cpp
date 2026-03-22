#include "mainwindow.h"
#include "widgets/videogen.h"
#include "widgets/imagegen.h"
#include "widgets/configwidget.h"
#include "widgets/aboutwidget.h"
#include "widgets/helpwidget.h"
#include "network/taskpollmanager.h"
#include "network/updatemanager.h"
#include <QFile>
#include <QGraphicsBlurEffect>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QApplication>
#include <QScreen>
#include <QSettings>
#include <QTimer>
#include <QMessageBox>
#include <QCloseEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), currentTheme(Dark)
{
    loadTheme();
    setupFonts();
    setupUI();
    applyStyles();

    // 检查是否首次启动
    QSettings settings("ChickenAI", "App");
    bool isFirstRun = settings.value("isFirstRun", true).toBool();

    if (isFirstRun) {
        // 首次启动，显示使用帮助页面
        showHelp();
        settings.setValue("isFirstRun", false);
    } else {
        // 非首次启动，恢复上次的页面
        QString lastPage = settings.value("lastPage", "video").toString();
        if (lastPage == "video") {
            showVideoGen();
        } else if (lastPage == "image") {
            showImageGen();
        } else if (lastPage == "config") {
            showConfig();
        } else if (lastPage == "about") {
            showAbout();
        } else if (lastPage == "help") {
            showHelp();
        } else {
            showVideoGen();  // 默认显示视频生成页面
        }
    }

    // 恢复待轮询任务
    TaskPollManager::getInstance()->recoverPendingTasks();

    ensureFirstRunVersionRecorded();
    setupStartupUpdateCheck();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    // 设置窗口属性
    setWindowTitle("鸡你太美ai 图片/视频批量生成器");
    setMinimumSize(1200, 800);
    resize(1400, 900);

    // 创建中心部件
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // 主布局
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 设置侧边栏和内容区域
    setupSidebar();
    setupContentArea();

    mainLayout->addWidget(sidebar);
    mainLayout->addWidget(contentArea);
}

void MainWindow::setupSidebar()
{
    sidebar = new QWidget();
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(80);

    QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(10, 20, 10, 20);
    sidebarLayout->setSpacing(15);

    // 创建侧边栏按钮
    videoButton = new QPushButton("🎬");
    videoButton->setObjectName("sidebarButton");
    videoButton->setCheckable(true);
    videoButton->setFixedSize(60, 60);
    videoButton->setToolTip("视频生成");

    imageButton = new QPushButton("🖼️");
    imageButton->setObjectName("sidebarButton");
    imageButton->setCheckable(true);
    imageButton->setFixedSize(60, 60);
    imageButton->setToolTip("图片生成");

    configButton = new QPushButton("⚙️");
    configButton->setObjectName("sidebarButton");
    configButton->setCheckable(true);
    configButton->setFixedSize(60, 60);
    configButton->setToolTip("配置");

    aboutButton = new QPushButton("ℹ️");
    aboutButton->setObjectName("sidebarButton");
    aboutButton->setCheckable(true);
    aboutButton->setFixedSize(60, 60);
    aboutButton->setToolTip("关于");

    historyButton = new QPushButton("📜");
    historyButton->setObjectName("sidebarButton");
    historyButton->setCheckable(true);
    historyButton->setFixedSize(60, 60);
    historyButton->setToolTip("历史记录");

    helpButton = new QPushButton("❓");
    helpButton->setObjectName("sidebarButton");
    helpButton->setCheckable(true);
    helpButton->setFixedSize(60, 60);
    helpButton->setToolTip("使用帮助");

    themeButton = new QPushButton(currentTheme == Dark ? "🌙" : "☀️");
    themeButton->setObjectName("sidebarButton");
    themeButton->setFixedSize(60, 60);
    themeButton->setToolTip("切换主题");

    // 连接信号
    connect(videoButton, &QPushButton::clicked, this, &MainWindow::showVideoGen);
    connect(imageButton, &QPushButton::clicked, this, &MainWindow::showImageGen);
    connect(configButton, &QPushButton::clicked, this, &MainWindow::showConfig);
    connect(aboutButton, &QPushButton::clicked, this, &MainWindow::showAbout);
    connect(historyButton, &QPushButton::clicked, this, &MainWindow::showHistory);
    connect(helpButton, &QPushButton::clicked, this, &MainWindow::showHelp);
    connect(themeButton, &QPushButton::clicked, this, &MainWindow::toggleTheme);

    // 添加到布局
    sidebarLayout->addWidget(videoButton);
    sidebarLayout->addWidget(imageButton);
    sidebarLayout->addWidget(configButton);
    sidebarLayout->addWidget(aboutButton);
    sidebarLayout->addWidget(historyButton);
    historyButton->hide();  // 隐藏历史记录按钮
    sidebarLayout->addStretch();
    sidebarLayout->addWidget(helpButton);
    sidebarLayout->addWidget(themeButton);
}

void MainWindow::setupContentArea()
{
    contentArea = new QStackedWidget();
    contentArea->setObjectName("contentArea");

    // 创建内容页面
    videoGenWidget = new VideoGenWidget();
    imageGenWidget = new ImageGenWidget();
    configWidget = new ConfigWidget();
    aboutWidget = new AboutWidget();
    historyWidget = new VideoSingleHistoryTab();
    helpWidget = new HelpWidget();

    // 添加到堆栈
    contentArea->addWidget(videoGenWidget);
    contentArea->addWidget(imageGenWidget);
    contentArea->addWidget(configWidget);
    contentArea->addWidget(aboutWidget);
    contentArea->addWidget(historyWidget);
    contentArea->addWidget(helpWidget);

    // 连接配置页面的密钥变化信号到视频生成页面
    connect(configWidget, &ConfigWidget::apiKeysChanged,
            videoGenWidget->getSingleTab(), &VideoSingleTab::refreshApiKeys);
    connect(configWidget, &ConfigWidget::apiKeysChanged,
            videoGenWidget->getBatchTab(), &VideoBatchTab::refreshApiKeys);
    connect(configWidget, &ConfigWidget::apiKeysChanged,
            imageGenWidget->getSingleTab(), &ImageSingleTab::refreshApiKeys);

    // 连接配置页面的密钥变化信号到历史记录页面
    connect(configWidget, &ConfigWidget::apiKeysChanged,
            videoGenWidget->getHistoryWidget()->getVideoSingleTab(), &VideoSingleHistoryTab::refreshApiKeys);
    connect(configWidget, &ConfigWidget::apiKeysChanged,
            historyWidget, &VideoSingleHistoryTab::refreshApiKeys);
}

void MainWindow::showVideoGen()
{
    contentArea->setCurrentWidget(videoGenWidget);
    videoButton->setChecked(true);
    imageButton->setChecked(false);
    configButton->setChecked(false);
    aboutButton->setChecked(false);
    historyButton->setChecked(false);
    helpButton->setChecked(false);

    QSettings settings("ChickenAI", "App");
    settings.setValue("lastPage", "video");
}

void MainWindow::showImageGen()
{
    contentArea->setCurrentWidget(imageGenWidget);
    videoButton->setChecked(false);
    imageButton->setChecked(true);
    configButton->setChecked(false);
    aboutButton->setChecked(false);
    historyButton->setChecked(false);
    helpButton->setChecked(false);

    QSettings settings("ChickenAI", "App");
    settings.setValue("lastPage", "image");
}

void MainWindow::showConfig()
{
    contentArea->setCurrentWidget(configWidget);
    videoButton->setChecked(false);
    imageButton->setChecked(false);
    configButton->setChecked(true);
    aboutButton->setChecked(false);
    historyButton->setChecked(false);
    helpButton->setChecked(false);

    QSettings settings("ChickenAI", "App");
    settings.setValue("lastPage", "config");
}

void MainWindow::showAbout()
{
    contentArea->setCurrentWidget(aboutWidget);
    videoButton->setChecked(false);
    imageButton->setChecked(false);
    configButton->setChecked(false);
    aboutButton->setChecked(true);
    historyButton->setChecked(false);
    helpButton->setChecked(false);

    QSettings settings("ChickenAI", "App");
    settings.setValue("lastPage", "about");
}

void MainWindow::showHistory()
{
    contentArea->setCurrentWidget(historyWidget);
    videoButton->setChecked(false);
    imageButton->setChecked(false);
    configButton->setChecked(false);
    aboutButton->setChecked(false);
    historyButton->setChecked(true);
    helpButton->setChecked(false);

    QSettings settings("ChickenAI", "App");
    settings.setValue("lastPage", "history");
}

void MainWindow::showHelp()
{
    contentArea->setCurrentWidget(helpWidget);
    videoButton->setChecked(false);
    imageButton->setChecked(false);
    configButton->setChecked(false);
    aboutButton->setChecked(false);
    historyButton->setChecked(false);
    helpButton->setChecked(true);

    QSettings settings("ChickenAI", "App");
    settings.setValue("lastPage", "help");
}

void MainWindow::applyStyles()
{
    // 根据当前主题加载对应的 QSS 文件
    QString styleFile = (currentTheme == Dark)
        ? ":/styles/glassmorphism.qss"
        : ":/styles/light.qss";

    QFile file(styleFile);
    if (file.open(QFile::ReadOnly)) {
        QString styleSheet = QLatin1String(file.readAll());
        setStyleSheet(styleSheet);
        file.close();
    }
}

void MainWindow::toggleTheme()
{
    // 切换主题
    currentTheme = (currentTheme == Dark) ? Light : Dark;

    // 更新主题按钮图标
    themeButton->setText(currentTheme == Dark ? "🌙" : "☀️");

    // 重新应用样式
    applyStyles();

    // 保存主题偏好
    saveTheme();
}

void MainWindow::loadTheme()
{
    QSettings settings("ChickenAI", "Theme");
    int theme = settings.value("currentTheme", Dark).toInt();
    currentTheme = static_cast<Theme>(theme);
}

void MainWindow::saveTheme()
{
    QSettings settings("ChickenAI", "Theme");
    settings.setValue("currentTheme", static_cast<int>(currentTheme));
}

double MainWindow::calculateScaleFactor()
{
    QScreen *screen = QApplication::primaryScreen();
    if (!screen) return 1.0;

    // 获取逻辑 DPI（Qt 已经处理了物理 DPI）
    qreal dpi = screen->logicalDotsPerInch();

    // 基准 DPI 为 96（Windows 标准）
    // macOS 的基准是 72，但 Qt 会自动处理
    double scaleFactor = dpi / 96.0;

    // 限制缩放范围在 0.8 到 2.0 之间
    if (scaleFactor < 0.8) scaleFactor = 0.8;
    if (scaleFactor > 2.0) scaleFactor = 2.0;

    return scaleFactor;
}

void MainWindow::setupFonts()
{
    double scale = calculateScaleFactor();

    // 设置应用全局基础字体
    QFont baseFont = QApplication::font();
    baseFont.setFamily("Inter, -apple-system, BlinkMacSystemFont, Segoe UI, sans-serif");

    // 基础字体大小：12pt（会根据 DPI 自动缩放）
    int baseFontSize = static_cast<int>(12 * scale);
    baseFont.setPointSize(baseFontSize);

    QApplication::setFont(baseFont);
}

void MainWindow::ensureFirstRunVersionRecorded()
{
    QSettings settings("ChickenAI", "Update");
    const QString currentVersion = QApplication::applicationVersion();
    const QString lastVersion = settings.value("lastVersion").toString();
    if (lastVersion != currentVersion) {
        settings.setValue("lastVersion", currentVersion);
    }
}

void MainWindow::setupStartupUpdateCheck()
{
    UpdateManager *updateManager = UpdateManager::getInstance();
    QSettings settings("ChickenAI", "Update");
    const bool mandatoryHardBlockEnabled = settings.value("mandatoryHardBlockEnabled", false).toBool();
    updateManager->setMandatoryHardBlockEnabled(mandatoryHardBlockEnabled);

    connect(updateManager, &UpdateManager::updateAvailable, this,
            [this](const UpdateManager::ReleaseInfo& info, bool isManual, bool mandatoryEffective) {
                Q_UNUSED(isManual);
                Q_UNUSED(mandatoryEffective);

                if (startupUpdatePrompted) {
                    return;
                }
                startupUpdatePrompted = true;

                const QString text = QStringLiteral("发现新版本 %1，是否立即下载？").arg(info.version);
                QMessageBox msgBox(this);
                msgBox.setWindowTitle(QStringLiteral("发现新版本"));
                msgBox.setText(text);
                msgBox.setIcon(QMessageBox::Information);
                QPushButton *downloadButton = msgBox.addButton(QStringLiteral("立即下载"), QMessageBox::AcceptRole);
                msgBox.addButton(QStringLiteral("稍后"), QMessageBox::RejectRole);
                msgBox.exec();

                if (msgBox.clickedButton() == downloadButton) {
                    UpdateManager::getInstance()->startDownloadAndInstall();
                }
            });
    connect(updateManager, &UpdateManager::checkFailed, this,
            [](bool isManual, const QString& reason) {
                if (!isManual) {
                    qDebug() << "Startup update check failed:" << reason;
                }
            });

    QTimer::singleShot(500, this, [updateManager]() {
        updateManager->checkForUpdates(false);
    });
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 程序关闭时，保存图片生成草稿
    if (imageGenWidget && imageGenWidget->getSingleTab()) {
        imageGenWidget->getSingleTab()->saveDraftOnClose();
    }
    QMainWindow::closeEvent(event);
}
