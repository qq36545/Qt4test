#include "mainwindow.h"
#include "widgets/videogen.h"
#include "widgets/imagegen.h"
#include "widgets/configwidget.h"
#include "widgets/aboutwidget.h"
#include <QFile>
#include <QGraphicsBlurEffect>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QApplication>
#include <QScreen>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupFonts();
    setupUI();
    applyStyles();

    // 默认显示视频生成页面
    showVideoGen();
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

    // 连接信号
    connect(videoButton, &QPushButton::clicked, this, &MainWindow::showVideoGen);
    connect(imageButton, &QPushButton::clicked, this, &MainWindow::showImageGen);
    connect(configButton, &QPushButton::clicked, this, &MainWindow::showConfig);
    connect(aboutButton, &QPushButton::clicked, this, &MainWindow::showAbout);
    connect(historyButton, &QPushButton::clicked, this, &MainWindow::showHistory);

    // 添加到布局
    sidebarLayout->addWidget(videoButton);
    sidebarLayout->addWidget(imageButton);
    sidebarLayout->addWidget(configButton);
    sidebarLayout->addWidget(aboutButton);
    sidebarLayout->addWidget(historyButton);
    sidebarLayout->addStretch();
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
    historyWidget = new QWidget();

    // 历史记录页面占位
    QVBoxLayout *historyLayout = new QVBoxLayout(historyWidget);
    QLabel *historyLabel = new QLabel("📜 历史记录");
    historyLabel->setAlignment(Qt::AlignCenter);
    historyLabel->setStyleSheet("font-size: 24px; color: #F8FAFC;");
    historyLayout->addWidget(historyLabel);

    // 添加到堆栈
    contentArea->addWidget(videoGenWidget);
    contentArea->addWidget(imageGenWidget);
    contentArea->addWidget(configWidget);
    contentArea->addWidget(aboutWidget);
    contentArea->addWidget(historyWidget);
}

void MainWindow::showVideoGen()
{
    contentArea->setCurrentWidget(videoGenWidget);
    videoButton->setChecked(true);
    imageButton->setChecked(false);
    configButton->setChecked(false);
    aboutButton->setChecked(false);
    historyButton->setChecked(false);
}

void MainWindow::showImageGen()
{
    contentArea->setCurrentWidget(imageGenWidget);
    videoButton->setChecked(false);
    imageButton->setChecked(true);
    configButton->setChecked(false);
    aboutButton->setChecked(false);
    historyButton->setChecked(false);
}

void MainWindow::showConfig()
{
    contentArea->setCurrentWidget(configWidget);
    videoButton->setChecked(false);
    imageButton->setChecked(false);
    configButton->setChecked(true);
    aboutButton->setChecked(false);
    historyButton->setChecked(false);
}

void MainWindow::showAbout()
{
    contentArea->setCurrentWidget(aboutWidget);
    videoButton->setChecked(false);
    imageButton->setChecked(false);
    configButton->setChecked(false);
    aboutButton->setChecked(true);
    historyButton->setChecked(false);
}

void MainWindow::showHistory()
{
    contentArea->setCurrentWidget(historyWidget);
    videoButton->setChecked(false);
    imageButton->setChecked(false);
    configButton->setChecked(false);
    aboutButton->setChecked(false);
    historyButton->setChecked(true);
}

void MainWindow::applyStyles()
{
    // 加载 QSS 样式表
    QFile styleFile(":/styles/glassmorphism.qss");
    if (styleFile.open(QFile::ReadOnly)) {
        QString styleSheet = QLatin1String(styleFile.readAll());
        setStyleSheet(styleSheet);
        styleFile.close();
    }
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
