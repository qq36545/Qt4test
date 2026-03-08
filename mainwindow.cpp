#include "mainwindow.h"
#include "widgets/videogen.h"
#include "widgets/imagegen.h"
#include <QFile>
#include <QGraphicsBlurEffect>
#include <QGraphicsDropShadowEffect>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
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
    setWindowTitle("Qt4test - AI 视频/图片生成");
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

    historyButton = new QPushButton("📜");
    historyButton->setObjectName("sidebarButton");
    historyButton->setCheckable(true);
    historyButton->setFixedSize(60, 60);
    historyButton->setToolTip("历史记录");

    // 连接信号
    connect(videoButton, &QPushButton::clicked, this, &MainWindow::showVideoGen);
    connect(imageButton, &QPushButton::clicked, this, &MainWindow::showImageGen);
    connect(historyButton, &QPushButton::clicked, this, &MainWindow::showHistory);

    // 添加到布局
    sidebarLayout->addWidget(videoButton);
    sidebarLayout->addWidget(imageButton);
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
    contentArea->addWidget(historyWidget);
}

void MainWindow::showVideoGen()
{
    contentArea->setCurrentWidget(videoGenWidget);
    videoButton->setChecked(true);
    imageButton->setChecked(false);
    historyButton->setChecked(false);
}

void MainWindow::showImageGen()
{
    contentArea->setCurrentWidget(imageGenWidget);
    videoButton->setChecked(false);
    imageButton->setChecked(true);
    historyButton->setChecked(false);
}

void MainWindow::showHistory()
{
    contentArea->setCurrentWidget(historyWidget);
    videoButton->setChecked(false);
    imageButton->setChecked(false);
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
