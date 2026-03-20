#include "videogen.h"
#include "../database/dbmanager.h"
#include "../network/videoapi.h"
#include "../network/imageuploader.h"
#include "../network/taskpollmanager.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDateTime>
#include <QHeaderView>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QLabel>
#include <QResizeEvent>
#include <QTabBar>
#include <QPixmap>
#include <QEvent>
#include <QMouseEvent>
#include <QScrollArea>
#include <QRandomGenerator>
#include <QStackedWidget>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <QDir>
#include <QSettings>
#include <QCryptographicHash>
#include <QCheckBox>
#include <QSet>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QStandardPaths>
#include <QMenu>
#include <QClipboard>
#include <QApplication>
#include <QToolTip>
#include <QInputDialog>
#include <QImageReader>

namespace {

bool hasLocalVideoFile(const VideoTask& task)
{
    return !task.videoPath.isEmpty() && QFile::exists(task.videoPath);
}

enum class VideoResolvedState {
    CompletedLocal,
    Failed,
    Timeout,
    Downloading,
    Waiting,
    Processing,
    PendingDownload,
    DownloadFailed,
    Unknown
};

struct VideoTaskDisplayState {
    VideoResolvedState resolvedState;
    QString statusText;
    QString statusIcon;
    bool hasLocalFile;
    bool canPlay;
    bool canDelete;
    bool canRefresh;
    bool canBrowse;
    bool canDownload;
};

VideoTaskDisplayState resolveVideoTaskDisplayState(const VideoTask& task)
{
    VideoTaskDisplayState displayState;
    displayState.hasLocalFile = hasLocalVideoFile(task);
    displayState.canDownload = false;
    displayState.resolvedState = VideoResolvedState::Unknown;

    if (displayState.hasLocalFile) {
        displayState.resolvedState = VideoResolvedState::CompletedLocal;
        displayState.statusText = "✅ 已完成";
        displayState.statusIcon = "✅";
    } else if (task.status == "failed") {
        displayState.resolvedState = VideoResolvedState::Failed;
        displayState.statusText = "❌ 失败";
        displayState.statusIcon = "❌";
    } else if (task.status == "timeout") {
        displayState.resolvedState = VideoResolvedState::Timeout;
        displayState.statusText = "⏱️ 超时";
        displayState.statusIcon = "⏱️";
    } else if (task.downloadStatus == "downloading") {
        displayState.resolvedState = VideoResolvedState::Downloading;
        displayState.statusText = "⬇️ 下载中";
        displayState.statusIcon = "⬇️";
    } else if (task.status == "queued" || task.status == "pending") {
        displayState.resolvedState = VideoResolvedState::Waiting;
        displayState.statusText = "⏳ 等待中";
        displayState.statusIcon = "⏳";
    } else if (task.status == "processing" || task.status == "video_generating") {
        displayState.resolvedState = VideoResolvedState::Processing;
        displayState.statusText = "🔄 处理中";
        displayState.statusIcon = "🔄";
    } else if (task.status == "completed") {
        if (task.downloadStatus == "failed") {
            displayState.resolvedState = VideoResolvedState::DownloadFailed;
            displayState.statusText = "⚠️ 下载失败";
            displayState.statusIcon = "⚠️";
            displayState.canDownload = !task.videoUrl.isEmpty();
        } else if (!task.videoUrl.isEmpty()) {
            displayState.resolvedState = VideoResolvedState::PendingDownload;
            displayState.statusText = "📥 已生成待下载";
            displayState.statusIcon = "📥";
            displayState.canDownload = true;
        } else {
            displayState.resolvedState = VideoResolvedState::Processing;
            displayState.statusText = "🔄 处理中";
            displayState.statusIcon = "🔄";
        }
    } else {
        displayState.resolvedState = VideoResolvedState::Unknown;
        displayState.statusText = task.status;
        displayState.statusIcon = "❔";
    }

    displayState.canPlay = displayState.hasLocalFile || displayState.canDownload;
    displayState.canBrowse = displayState.hasLocalFile;

    switch (displayState.resolvedState) {
    case VideoResolvedState::Waiting:
    case VideoResolvedState::Processing:
    case VideoResolvedState::Downloading:
        // 进行中的状态禁止删除，避免用户误删仍在变更的任务
        displayState.canDelete = false;
        break;
    default:
        // Unknown 归入可删除：未知状态视为非进行中，优先保证用户可自行清理异常记录
        displayState.canDelete = true;
        break;
    }

    // 历史记录允许用户随时主动重查状态（包含已完成）
    displayState.canRefresh = true;

    return displayState;
}

QString unresolvedVideoStateMessage(const VideoTask& task, const VideoTaskDisplayState& displayState)
{
    Q_UNUSED(task);

    if (displayState.hasLocalFile) {
        return QString();
    }
    if (displayState.canDownload) {
        return QString();
    }

    switch (displayState.resolvedState) {
    case VideoResolvedState::Failed:
        return "任务已失败，无法查看视频。";
    case VideoResolvedState::Timeout:
        return "任务已超时，当前没有可查看的视频。";
    case VideoResolvedState::Downloading:
        return "视频正在下载中，请稍后再试。";
    case VideoResolvedState::Waiting:
        return "视频仍在等待处理，请稍后再试。";
    default:
        return "视频尚未生成完成，请稍后再试。";
    }
}

bool shouldShowRetryDownloadButton(const VideoTaskDisplayState& displayState)
{
    return displayState.resolvedState == VideoResolvedState::DownloadFailed
           && displayState.canDownload;
}

} // namespace

// VideoGenWidget 实现
VideoGenWidget::VideoGenWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void VideoGenWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(40, 40, 40, 40);
    mainLayout->setSpacing(20);

    // 标题
    QLabel *titleLabel = new QLabel("🎬 AI 视频生成");
    titleLabel->setStyleSheet("font-size: 28px; font-weight: bold;");
    mainLayout->addWidget(titleLabel);

    // Tab Widget
    tabWidget = new QTabWidget();
    tabWidget->setObjectName("videoTabWidget");

    singleTab = new VideoSingleTab();
    batchTab = new VideoBatchTab();
    historyWidget = new VideoHistoryWidget();

    tabWidget->addTab(singleTab, "AI视频生成-单个");
    tabWidget->addTab(batchTab, "AI视频生成-批量");
    tabWidget->addTab(historyWidget, "生成历史记录");

    // 单向同步：生成页密钥 -> 历史页查询密钥
    if (historyWidget->getVideoSingleTab()) {
        connect(singleTab, &VideoSingleTab::apiKeySelectionChanged,
                historyWidget->getVideoSingleTab(),
                &VideoSingleHistoryTab::refreshApiKeys);
    }

    mainLayout->addWidget(tabWidget);

    // 初始化 tab 宽度
    updateTabWidths();
}

void VideoGenWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateTabWidths();
}

void VideoGenWidget::updateTabWidths()
{
    if (!tabWidget) return;

    // 获取当前窗口宽度
    int windowWidth = width();

    // 计算所有 tab 的总宽度（窗口宽度的 55%）
    int totalTabWidth = windowWidth * 0.55;

    // 获取 tab 数量
    int tabCount = tabWidget->count();
    if (tabCount == 0) return;

    // 计算每个 tab 的宽度
    int tabWidth = totalTabWidth / tabCount;

    // 设置 tab 样式
    QString tabStyle = QString(
        "QTabBar::tab {"
        "    width: %1px;"
        "    text-align: center;"
        "}"
    ).arg(tabWidth);

    tabWidget->tabBar()->setStyleSheet(tabStyle);
}

#include "veogen.h"
#include "grokgen.h"
#include "wangen.h"

// VideoSingleTab 实现（调度器）
VideoSingleTab::VideoSingleTab(QWidget *parent)
    : QWidget(parent)
{
    setupUI();

    // 连接子页的 apiKeySelectionChanged 信号
    connect(veoPage, &VeoGenPage::apiKeySelectionChanged, this, &VideoSingleTab::apiKeySelectionChanged);
    connect(grokPage, &GrokGenPage::apiKeySelectionChanged, this, &VideoSingleTab::apiKeySelectionChanged);
    connect(wanPage, &WanGenPage::apiKeySelectionChanged, this, &VideoSingleTab::apiKeySelectionChanged);
}

void VideoSingleTab::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 模型选择
    QHBoxLayout *modelLayout = new QHBoxLayout();
    modelLayout->setContentsMargins(20, 15, 20, 5);
    QLabel *modelLabel = new QLabel("视频模型:");
    modelLabel->setStyleSheet("font-size: 14px;");
    modelCombo = new QComboBox();
    modelCombo->addItems({"VEO3视频", "Grok3视频", "WAN视频"});
    modelCombo->setCurrentIndex(0);
    connect(modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::onModelChanged);
    modelLayout->addWidget(modelLabel);
    modelLayout->addWidget(modelCombo, 1);
    mainLayout->addLayout(modelLayout);

    // 创建 stackedWidget
    stack = new QStackedWidget();
    veoPage = new VeoGenPage();
    grokPage = new GrokGenPage();
    wanPage = new WanGenPage();
    stack->addWidget(veoPage);   // index 0: VEO3
    stack->addWidget(grokPage);  // index 1: Grok
    stack->addWidget(wanPage);    // index 2: WAN

    mainLayout->addWidget(stack);
}

void VideoSingleTab::onModelChanged(int index)
{
    stack->setCurrentIndex(index);

    // 切换页面时刷新当前页的 API 密钥列表
    switch (index) {
    case 0:
        veoPage->refreshApiKeys();
        break;
    case 1:
        grokPage->refreshApiKeys();
        break;
    case 2:
        wanPage->refreshApiKeys();
        break;
    }
}

void VideoSingleTab::refreshApiKeys()
{
    veoPage->refreshApiKeys();
    grokPage->refreshApiKeys();
    wanPage->refreshApiKeys();
}

void VideoSingleTab::loadFromTask(const VideoTask& task)
{
    // 根据 task.modelName 切换到对应子页
    if (task.modelName.contains("Grok", Qt::CaseInsensitive)) {
        modelCombo->setCurrentIndex(1);
    } else if (task.modelName.contains("wan", Qt::CaseInsensitive)) {
        modelCombo->setCurrentIndex(2);
    } else {
        modelCombo->setCurrentIndex(0);
    }

    // 派发给对应子页
    QWidget *currentPage = stack->currentWidget();
    if (currentPage == veoPage) {
        veoPage->loadFromTask(task);
    } else if (currentPage == grokPage) {
        grokPage->loadFromTask(task);
    } else if (currentPage == wanPage) {
        wanPage->loadFromTask(task);
    }
}

// VideoBatchTab 实现
VideoBatchTab::VideoBatchTab(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    loadApiKeys();
}

void VideoBatchTab::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 创建滚动区域
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    // 创建内容容器
    QWidget *contentWidget = new QWidget();
    contentWidget->setObjectName("scrollContentWidget");  // 设置 objectName 用于 QSS
    contentWidget->setAutoFillBackground(false);  // 关键：不自动填充背景
    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(20, 20, 20, 20);
    contentLayout->setSpacing(15);

    // 模型选择
    QHBoxLayout *modelLayout = new QHBoxLayout();
    QLabel *modelLabel = new QLabel("视频模型:");
    modelLabel->setStyleSheet("font-size: 14px;");
    modelCombo = new QComboBox();
    modelCombo->addItems({"VEO3视频", "Grok3视频", "wan视频"});
    modelCombo->setCurrentIndex(0); // 默认选择 VEO3
    connect(modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoBatchTab::onModelChanged);
    modelLayout->addWidget(modelLabel);
    modelLayout->addWidget(modelCombo, 1);
    contentLayout->addLayout(modelLayout);

    // VEO3 模型变体选择
    QHBoxLayout *variantLayout = new QHBoxLayout();
    QLabel *variantLabel = new QLabel("模型变体:");
    variantLabel->setStyleSheet("font-size: 14px;");
    modelVariantCombo = new QComboBox();
    modelVariantCombo->addItem("veo_3_1-fast", "veo_3_1-fast");
    modelVariantCombo->addItem("veo_3_1", "veo_3_1");
    modelVariantCombo->addItem("veo_3_1-fast-4K", "veo_3_1-fast-4K");
    modelVariantCombo->addItem("veo_3_1-fast-components-4K", "veo_3_1-fast-components-4K");
    modelVariantCombo->addItem("veo3.1-fast", "veo3.1-fast");
    modelVariantCombo->addItem("veo3.1", "veo3.1");
    modelVariantCombo->addItem("veo3.1-fast-components", "veo3.1-fast-components");
    modelVariantCombo->addItem("veo3.1-components", "veo3.1-components");
    modelVariantCombo->addItem("veo3.1-4k", "veo3.1-4k");
    modelVariantCombo->addItem("veo3-pro-frames", "veo3-pro-frames");
    modelVariantCombo->addItem("veo3.1-components-4k", "veo3.1-components-4k");
    modelVariantCombo->addItem("veo3.1-pro", "veo3.1-pro");
    modelVariantCombo->addItem("veo3.1-pro-4k", "veo3.1-pro-4k");
    modelVariantCombo->addItem("veo3", "veo3");
    modelVariantCombo->addItem("veo3-fast", "veo3-fast");
    modelVariantCombo->addItem("veo3-fast-frames", "veo3-fast-frames");
    modelVariantCombo->addItem("veo3-frames", "veo3-frames");
    modelVariantCombo->addItem("veo3-pro", "veo3-pro");
    connect(modelVariantCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoBatchTab::onModelVariantChanged);
    variantLayout->addWidget(variantLabel);
    variantLayout->addWidget(modelVariantCombo, 1);
    contentLayout->addLayout(variantLayout);

    // API Key 选择
    QHBoxLayout *keyLayout = new QHBoxLayout();
    QLabel *keyLabel = new QLabel("API 密钥:");
    keyLabel->setStyleSheet("font-size: 14px;");
    apiKeyCombo = new QComboBox();
    addKeyButton = new QPushButton("➕ 添加密钥");
    connect(addKeyButton, &QPushButton::clicked, []() {
        QMessageBox::information(nullptr, "提示", "请前往配置页面添加密钥");
    });
    keyLayout->addWidget(keyLabel);
    keyLayout->addWidget(apiKeyCombo, 1);
    keyLayout->addWidget(addKeyButton);
    contentLayout->addLayout(keyLayout);

    // 请求服务器选择
    QHBoxLayout *serverLayout = new QHBoxLayout();
    QLabel *serverLabel = new QLabel("请求服务器:");
    serverLabel->setStyleSheet("font-size: 14px;");
    serverCombo = new QComboBox();
    serverCombo->addItem("【主站】https://ai.kegeai.top", "https://ai.kegeai.top");
    serverCombo->addItem("【备用】https://api.kuai.host", "https://api.kuai.host");
    serverCombo->setCurrentIndex(0); // 默认选择主站
    serverLayout->addWidget(serverLabel);
    serverLayout->addWidget(serverCombo, 1);
    contentLayout->addLayout(serverLayout);

    // 提示词输入（多行）
    QHBoxLayout *promptHeaderLayout = new QHBoxLayout();
    QLabel *promptLabel = new QLabel("批量提示词（每行一个）:");
    promptLabel->setStyleSheet("font-size: 14px;");
    QPushButton *clearPromptButton = new QPushButton("🧹 清空文本");
    clearPromptButton->setCursor(Qt::PointingHandCursor);
    connect(clearPromptButton, &QPushButton::clicked, this, [this]() {
        promptInput->clear();
    });
    promptHeaderLayout->addWidget(promptLabel);
    promptHeaderLayout->addStretch();
    promptHeaderLayout->addWidget(clearPromptButton);

    promptInput = new QTextEdit();
    promptInput->setPlaceholderText("输入多个提示词，每行一个...\n例如：\n一只猫在花园玩耍\n日落时的海滩\n城市夜景");
    promptInput->setMinimumHeight(120);  // 支持多行输入和滚动
    contentLayout->addLayout(promptHeaderLayout);
    contentLayout->addWidget(promptInput);

    // 图片拖放区域
    imageLabel = new QLabel("图片（可选，拖放图片到下方区域）:");
    imageLabel->setStyleSheet("font-size: 14px;");
    imageDropArea = new QLabel();
    imageDropArea->setObjectName("imageDropArea");
    imageDropArea->setAlignment(Qt::AlignCenter);
    imageDropArea->setText("📁 拖放图片到这里\n（图生视频）");
    imageDropArea->setMinimumHeight(100);
    imageDropArea->setAcceptDrops(true);
    contentLayout->addWidget(imageLabel);
    contentLayout->addWidget(imageDropArea);

    // 图片列表
    imageList = new QListWidget();
    imageList->setMaximumHeight(100);
    contentLayout->addWidget(imageList);

    // 尾帧提示（根据模型动态显示）
    endFrameWidget = new QWidget();
    QVBoxLayout *endFrameLayout = new QVBoxLayout(endFrameWidget);
    endFrameLayout->setContentsMargins(0, 0, 0, 0);
    endFrameLabel = new QLabel("提示：当前模型支持首尾帧垫图");
    endFrameLabel->setStyleSheet("font-size: 12px; font-style: italic;");
    endFrameLayout->addWidget(endFrameLabel);
    contentLayout->addWidget(endFrameWidget);
    endFrameWidget->setVisible(true);

    // 参数设置
    QHBoxLayout *paramsLayout = new QHBoxLayout();

    QVBoxLayout *resLayout = new QVBoxLayout();
    QLabel *resLabel = new QLabel("分辨率");
    resLabel->setStyleSheet("font-size: 14px;");
    resolutionCombo = new QComboBox();
    resolutionCombo->addItem("横屏 16:9 (1280x720)", "1280x720");
    resolutionCombo->addItem("竖屏 9:16 (720x1280)", "720x1280");
    resLayout->addWidget(resLabel);
    resLayout->addWidget(resolutionCombo);

    QVBoxLayout *durLayout = new QVBoxLayout();
    QLabel *durLabel = new QLabel("时长（秒）");
    durLabel->setStyleSheet("font-size: 14px;");
    durationCombo = new QComboBox();
    durationCombo->addItem("8秒（固定）", "8");
    durationCombo->setEnabled(false); // VEO3 固定 8 秒
    durLayout->addWidget(durLabel);
    durLayout->addWidget(durationCombo);

    QVBoxLayout *watermarkLayout = new QVBoxLayout();
    QLabel *watermarkLabel = new QLabel("水印");
    watermarkLabel->setStyleSheet("font-size: 14px;");
    watermarkCheckBox = new QCheckBox("添加水印");
    watermarkLayout->addWidget(watermarkLabel);
    watermarkLayout->addWidget(watermarkCheckBox);

    paramsLayout->addLayout(resLayout);
    paramsLayout->addLayout(durLayout);
    paramsLayout->addLayout(watermarkLayout);
    paramsLayout->addStretch();

    contentLayout->addLayout(paramsLayout);

    // 按钮行
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    importButton = new QPushButton("📄 导入 CSV");
    deleteButton = new QPushButton("🗑️ 删除选中");
    deleteAllButton = new QPushButton("🗑️ 全部删除");
    connect(importButton, &QPushButton::clicked, this, &VideoBatchTab::importCSV);
    connect(deleteButton, &QPushButton::clicked, this, &VideoBatchTab::deleteSelected);
    connect(deleteAllButton, &QPushButton::clicked, this, &VideoBatchTab::deleteAll);
    buttonLayout->addWidget(importButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addWidget(deleteAllButton);
    buttonLayout->addStretch();
    contentLayout->addLayout(buttonLayout);

    // 生成按钮
    generateButton = new QPushButton("🚀 批量生成视频");
    generateButton->setCursor(Qt::PointingHandCursor);
    connect(generateButton, &QPushButton::clicked, this, &VideoBatchTab::generateBatch);
    contentLayout->addWidget(generateButton);

    // 设置滚动区域
    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);

    // 初始化UI状态
    onModelVariantChanged(0);
}

void VideoBatchTab::loadApiKeys()
{
    apiKeyCombo->clear();
    QList<ApiKey> keys = DBManager::instance()->getAllApiKeys();

    if (keys.isEmpty()) {
        apiKeyCombo->addItem("无可用密钥");
        apiKeyCombo->setEnabled(false);
        addKeyButton->setVisible(true);
    } else {
        apiKeyCombo->setEnabled(true);
        addKeyButton->setVisible(false);
        for (const ApiKey& key : keys) {
            apiKeyCombo->addItem(key.name, key.id);
        }
    }
}

void VideoBatchTab::refreshApiKeys()
{
    loadApiKeys();
}

void VideoBatchTab::onModelChanged(int index)
{
    loadApiKeys();
}

void VideoBatchTab::onModelVariantChanged(int index)
{
    QString modelName = modelVariantCombo->currentData().toString();
    updateImageUploadUI(modelName);
    updateResolutionOptions(modelName.contains("4K") || modelName.contains("4k"));
}

void VideoBatchTab::updateImageUploadUI(const QString &modelName)
{
    bool isComponents = modelName.contains("components");
    bool isFrames = modelName.contains("frames");

    if (isComponents) {
        imageLabel->setText("图片（1-3张首帧，拖放到下方区域）:");
        endFrameLabel->setText("提示：当前模型仅支持首帧（1-3张图片）");
        endFrameWidget->setVisible(true);
    } else if (isFrames) {
        imageLabel->setText("图片（单张首帧，拖放到下方区域）:");
        endFrameLabel->setText("提示：当前模型仅支持首帧（单张图片）");
        endFrameWidget->setVisible(true);
    } else {
        imageLabel->setText("图片（可选，拖放到下方区域）:");
        endFrameLabel->setText("提示：当前模型支持首尾帧垫图");
        endFrameWidget->setVisible(true);
    }
}

void VideoBatchTab::updateResolutionOptions(bool is4K)
{
    resolutionCombo->clear();
    if (is4K) {
        resolutionCombo->addItem("横屏 16:9 (3840x2160)", "3840x2160");
        resolutionCombo->addItem("竖屏 9:16 (2160x3840)", "2160x3840");
    } else {
        resolutionCombo->addItem("横屏 16:9 (1280x720)", "1280x720");
        resolutionCombo->addItem("竖屏 9:16 (720x1280)", "720x1280");
    }
}

void VideoBatchTab::generateBatch()
{
    QString prompts = promptInput->toPlainText().trimmed();
    if (prompts.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入至少一个提示词");
        return;
    }

    if (apiKeyCombo->count() == 0 || !apiKeyCombo->isEnabled()) {
        QMessageBox::warning(this, "提示", "请先添加 API 密钥");
        return;
    }

    QStringList promptList = prompts.split('\n', Qt::SkipEmptyParts);
    QString model = modelCombo->currentText();
    QString modelVariant = modelVariantCombo->currentData().toString();
    QString server = serverCombo->currentData().toString();
    QString resolution = resolutionCombo->currentData().toString();
    QString duration = durationCombo->currentData().toString();
    bool watermark = watermarkCheckBox->isChecked();

    // 保存批量任务到历史记录
    for (const QString& prompt : promptList) {
        GenerationHistory history;
        history.date = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
        history.type = "batch";
        history.modelType = "video";
        history.modelName = model + " (" + modelVariant + ")";
        history.prompt = prompt.trimmed();
        history.parameters = QString("服务器:%1, 分辨率:%2, 时长:%3秒, 水印:%4, 批量生成")
            .arg(server)
            .arg(resolution)
            .arg(duration)
            .arg(watermark ? "是" : "否");
        history.status = "pending";
        DBManager::instance()->addGenerationHistory(history);
    }

    QMessageBox::information(this, "批量生成",
        QString("已添加 %1 个视频生成任务到队列\n\n"
                "模型: %2\n"
                "模型变体: %3\n"
                "服务器: %4\n"
                "分辨率: %5\n"
                "时长: %6秒\n"
                "水印: %7\n\n"
                "(演示版本，已保存到历史记录)")
        .arg(promptList.size())
        .arg(model)
        .arg(modelVariant)
        .arg(server)
        .arg(resolution)
        .arg(duration)
        .arg(watermark ? "是" : "否"));
}

void VideoBatchTab::importCSV()
{
    // 读取上次图片上传路径（与单个生成共享）
    QSettings settings("ChickenAI", "VideoGen");
    QString lastDir = settings.value("lastImageUploadDir", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();

    // 验证路径是否存在
    if (!QDir(lastDir).exists()) {
        lastDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }

    QString fileName = QFileDialog::getOpenFileName(this, "导入 CSV", lastDir, "CSV Files (*.csv)");
    if (!fileName.isEmpty()) {
        // 保存文件所在目录
        QFileInfo fileInfo(fileName);
        settings.setValue("lastImageUploadDir", fileInfo.absolutePath());

        QMessageBox::information(this, "提示", "CSV 导入功能待实现");
    }
}

void VideoBatchTab::deleteSelected()
{
    QList<QListWidgetItem*> selected = imageList->selectedItems();
    for (QListWidgetItem* item : selected) {
        delete item;
    }
}

void VideoBatchTab::deleteAll()
{
    imageList->clear();
    promptInput->clear();
}

// VideoHistoryTab 实现
VideoHistoryTab::VideoHistoryTab(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    loadHistory();
}

void VideoHistoryTab::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    // 刷新按钮
    QHBoxLayout *topLayout = new QHBoxLayout();
    refreshButton = new QPushButton("🔄 刷新");
    connect(refreshButton, &QPushButton::clicked, this, &VideoHistoryTab::refreshHistory);
    topLayout->addStretch();
    topLayout->addWidget(refreshButton);
    mainLayout->addLayout(topLayout);

    // 历史记录表格
    historyTable = new QTableWidget();
    historyTable->setColumnCount(6);
    historyTable->setHorizontalHeaderLabels({"序号", "日期", "类型", "模型", "提示词", "状态"});
    historyTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    historyTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    historyTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    historyTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    historyTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    historyTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    historyTable->setColumnWidth(0, 80);
    historyTable->setColumnWidth(1, 150);
    historyTable->setColumnWidth(2, 80);
    historyTable->setColumnWidth(3, 120);
    historyTable->setColumnWidth(5, 100);
    historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    historyTable->verticalHeader()->setVisible(false);
    historyTable->setMinimumHeight(400);

    connect(historyTable, &QTableWidget::cellDoubleClicked, this, &VideoHistoryTab::onRowDoubleClicked);

    mainLayout->addWidget(historyTable);
}

void VideoHistoryTab::loadHistory()
{
    historyTable->setRowCount(0);
    QList<GenerationHistory> histories = DBManager::instance()->getAllGenerationHistory();

    // 只显示视频类型的历史记录
    int row = 0;
    for (const GenerationHistory& history : histories) {
        if (history.modelType != "video") continue;

        historyTable->insertRow(row);

        historyTable->setItem(row, 0, new QTableWidgetItem(QString::number(history.id)));
        historyTable->setItem(row, 1, new QTableWidgetItem(history.date));
        historyTable->setItem(row, 2, new QTableWidgetItem(history.type == "single" ? "单次" : "批量"));
        historyTable->setItem(row, 3, new QTableWidgetItem(history.modelName));
        historyTable->setItem(row, 4, new QTableWidgetItem(history.prompt));
        historyTable->setItem(row, 5, new QTableWidgetItem(history.status));

        // 存储完整数据
        historyTable->item(row, 0)->setData(Qt::UserRole, history.id);

        row++;
    }
}

void VideoHistoryTab::refreshHistory()
{
    loadHistory();
    QMessageBox::information(this, "提示", "历史记录已刷新");
}

void VideoHistoryTab::onRowDoubleClicked(int row, int column)
{
    int historyId = historyTable->item(row, 0)->data(Qt::UserRole).toInt();
    GenerationHistory history = DBManager::instance()->getGenerationHistory(historyId);

    // 创建详情对话框
    QDialog dialog(this);
    dialog.setWindowTitle("生成详情");
    dialog.setMinimumWidth(600);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QFormLayout *formLayout = new QFormLayout();
    formLayout->addRow("序号:", new QLabel(QString::number(history.id)));
    formLayout->addRow("日期:", new QLabel(history.date));
    formLayout->addRow("类型:", new QLabel(history.type == "single" ? "单次" : "批量"));
    formLayout->addRow("模型:", new QLabel(history.modelName));
    formLayout->addRow("提示词:", new QLabel(history.prompt));
    formLayout->addRow("参数:", new QLabel(history.parameters));
    formLayout->addRow("状态:", new QLabel(history.status));
    if (!history.resultPath.isEmpty()) {
        formLayout->addRow("结果:", new QLabel(history.resultPath));
    }

    layout->addLayout(formLayout);

    // 再次生成按钮
    QPushButton *regenerateBtn = new QPushButton("🔄 再次生成");
    regenerateBtn->setFixedHeight(40);
    connect(regenerateBtn, &QPushButton::clicked, [history, this]() {
        QMessageBox::information(this, "提示",
            QString("将使用以下参数再次生成:\n\n模型: %1\n提示词: %2\n\n(功能待实现)")
            .arg(history.modelName).arg(history.prompt));
    });
    layout->addWidget(regenerateBtn);

    QPushButton *closeBtn = new QPushButton("关闭");
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(closeBtn);

    dialog.exec();
}

// VideoHistoryWidget 实现（新的 4 tab 容器）
VideoHistoryWidget::VideoHistoryWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void VideoHistoryWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);  // 完全移除所有边距
    mainLayout->setSpacing(0);

    // 创建 Tab Widget
    tabWidget = new QTabWidget();

    // tab居中对齐
    tabWidget->tabBar()->setExpanding(true);

    // 创建 4 个子 tab
    videoSingleTab = new VideoSingleHistoryTab();
    tabWidget->addTab(videoSingleTab, "AI视频生成-单个记录");

    // 其他 tab 后续实现
    tabWidget->addTab(new QWidget(), "AI生成视频-批量记录");
    tabWidget->addTab(new QWidget(), "AI生成图片-单个记录");
    tabWidget->addTab(new QWidget(), "AI生成图片-批量记录");

    mainLayout->addWidget(tabWidget);
}

// VideoSingleHistoryTab 实现
VideoSingleHistoryTab::VideoSingleHistoryTab(QWidget *parent)
    : QWidget(parent), isListView(true), currentOffset(0), hasShownRecoveryPrompt(false)
{
    setupUI();
    loadHistory();

    // 创建API实例用于重新查询
    veo3API = new VideoAPI(this);
    connect(veo3API, &VideoAPI::taskStatusUpdated,
            this, &VideoSingleHistoryTab::onApiTaskStatusUpdated);
    connect(veo3API, &VideoAPI::errorOccurred,
            this, &VideoSingleHistoryTab::onQueryError);

    // 连接TaskPollManager的状态更新信号
    connect(TaskPollManager::getInstance(), &TaskPollManager::taskStatusUpdated,
            this, &VideoSingleHistoryTab::onTaskStatusUpdated);

    // tooltip 3秒自动消失计时器
    tooltipHideTimer = new QTimer(this);
    tooltipHideTimer->setSingleShot(true);
    tooltipHideTimer->setInterval(3000);
    connect(tooltipHideTimer, &QTimer::timeout, []() { QToolTip::hideText(); });

    // 安装事件过滤器，监听鼠标离开事件
    historyTable->installEventFilter(this);
}

void VideoSingleHistoryTab::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);  // 完全移除所有边距
    mainLayout->setSpacing(10);

    // 顶部工具栏
    QHBoxLayout *toolbarLayout = new QHBoxLayout();

    switchViewButton = new QPushButton("📷 缩略图视图");
    connect(switchViewButton, &QPushButton::clicked, this, &VideoSingleHistoryTab::switchView);

    // 查询密钥标签和下拉列表
    QLabel *apiKeyLabel = new QLabel("查询密钥:");
    apiKeyCombo = new QComboBox();
    apiKeyCombo->setMinimumWidth(150);
    loadApiKeys();

    // 服务器选择
    QLabel *serverLabel = new QLabel("服务器:");
    serverCombo = new QComboBox();
    serverCombo->addItem("【主站】https://ai.kegeai.top", "https://ai.kegeai.top");
    serverCombo->addItem("【备用】https://api.kuai.host", "https://api.kuai.host");
    serverCombo->setCurrentIndex(0);
    serverCombo->setMinimumWidth(150);

    refreshButton = new QPushButton("🔄 刷新");
    connect(refreshButton, &QPushButton::clicked, this, &VideoSingleHistoryTab::refreshHistory);

    deleteButton = new QPushButton("🗑️ 删除");
    connect(deleteButton, &QPushButton::clicked, this, &VideoSingleHistoryTab::onDeleteSelected);

    toolbarLayout->addWidget(switchViewButton);
    toolbarLayout->addWidget(apiKeyLabel);
    toolbarLayout->addWidget(apiKeyCombo);
    toolbarLayout->addWidget(serverLabel);
    toolbarLayout->addWidget(serverCombo);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(refreshButton);
    toolbarLayout->addWidget(deleteButton);

    mainLayout->addLayout(toolbarLayout);

    // 创建视图切换容器
    viewStack = new QStackedWidget();

    // 列表视图
    setupListView();

    // 缩略图视图
    setupThumbnailView();

    viewStack->addWidget(listViewWidget);
    viewStack->addWidget(thumbnailViewWidget);
    viewStack->setCurrentIndex(0);  // 默认显示列表视图

    mainLayout->addWidget(viewStack);
}

void VideoSingleHistoryTab::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // tab显示时自动刷新历史记录
    refreshHistory();

    // 检查是否有创建时间<5分钟的temp-ID任务
    if (!hasShownRecoveryPrompt) {
        QList<VideoTask> allTasks = DBManager::instance()->getTasksByType("video_single", 0, 100);
        QStringList recentTempIds;
        QDateTime now = QDateTime::currentDateTime();

        for (const VideoTask& task : allTasks) {
            if (task.taskId.startsWith("temp-")) {
                qint64 secondsElapsed = task.createdAt.secsTo(now);
                if (secondsElapsed < 300) {  // 5分钟 = 300秒
                    recentTempIds.append(task.taskId);
                }
            }
        }

        if (!recentTempIds.isEmpty()) {
            hasShownRecoveryPrompt = true;

            QMessageBox msgBox(this);
            msgBox.setWindowTitle("检测到未完成的任务恢复");
            msgBox.setText(QString("发现 %1 个最近创建的任务因超时未获取到任务ID，是否立即修复？").arg(recentTempIds.size()));
            msgBox.setIcon(QMessageBox::Question);
            QPushButton* fixNowBtn = msgBox.addButton("立即修复", QMessageBox::AcceptRole);
            QPushButton* laterBtn = msgBox.addButton("稍后处理", QMessageBox::RejectRole);

            msgBox.exec();

            if (msgBox.clickedButton() == fixNowBtn) {
                // 自动选中第一个temp-ID任务并打开修复对话框
                onFixTaskId(recentTempIds.first());
            }
        }
    }
}

bool VideoSingleHistoryTab::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == historyTable) {
        if (event->type() == QEvent::ToolTip) {
            // 取消隐藏计时器（鼠标重新悬停）
            tooltipHideTimer->stop();
            // 让Qt正常处理tooltip显示
        } else if (event->type() == QEvent::Leave) {
            // 鼠标离开表格，3秒后隐藏tooltip
            tooltipHideTimer->start();
        }
    }
    return QWidget::eventFilter(obj, event);
}

void VideoSingleHistoryTab::loadApiKeys()
{
    apiKeyCombo->clear();
    QList<ApiKey> keys = DBManager::instance()->getAllApiKeys();

    if (keys.isEmpty()) {
        apiKeyCombo->addItem("无可用密钥");
        apiKeyCombo->setEnabled(false);
    } else {
        apiKeyCombo->setEnabled(true);
        for (const ApiKey& key : keys) {
            apiKeyCombo->addItem(key.name, key.id);
        }
    }

    applySyncedQueryApiKeyFromSettings();
}

void VideoSingleHistoryTab::refreshApiKeys()
{
    loadApiKeys();
    applySyncedQueryApiKeyFromSettings();
}

void VideoSingleHistoryTab::applySyncedQueryApiKeyFromSettings()
{
    if (!apiKeyCombo || !apiKeyCombo->isEnabled() || apiKeyCombo->count() <= 0) {
        return;
    }

    QSettings settings("ChickenAI", "VideoGen");
    settings.beginGroup("VideoSingleTab");
    const QString syncedApiKeyValue = settings.value("selectedApiKeyValue").toString().trimmed();
    settings.endGroup();

    if (syncedApiKeyValue.isEmpty()) {
        return;
    }

    const QList<ApiKey> keys = DBManager::instance()->getAllApiKeys();
    for (const ApiKey& key : keys) {
        if (key.apiKey == syncedApiKeyValue) {
            const int targetIndex = apiKeyCombo->findData(key.id);
            if (targetIndex >= 0) {
                apiKeyCombo->setCurrentIndex(targetIndex);
            }
            return;
        }
    }
}

void VideoSingleHistoryTab::setupListView()
{
    listViewWidget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(listViewWidget);
    layout->setContentsMargins(0, 0, 0, 0);

    historyTable = new QTableWidget();
    historyTable->setFrameShape(QFrame::NoFrame);  // 移除边框
    historyTable->setColumnCount(9);  // 增加一列用于勾选框
    historyTable->setHorizontalHeaderLabels({
        "", "序号", "任务ID", "提示词", "状态", "进度", "创建时间", "视频类型", "操作"
    });

    // 选择列 - 固定宽度
    historyTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    historyTable->setColumnWidth(0, 50);

    // 序号列 - 可调整
    historyTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    historyTable->setColumnWidth(1, 50);

    // 任务ID列 - 可调整
    historyTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    historyTable->setColumnWidth(2, 100);

    // 提示词列 - 可调整
    historyTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    historyTable->setColumnWidth(3, 200);

    // 状态列 - 可调整
    historyTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);
    historyTable->setColumnWidth(4, 80);

    // 进度列 - 可调整
    historyTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Interactive);
    historyTable->setColumnWidth(5, 80);

    // 创建时间列 - 可调整
    historyTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Interactive);
    historyTable->setColumnWidth(6, 150);

    // 视频类型列 - 可调整
    historyTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Interactive);
    historyTable->setColumnWidth(7, 120);

    // 操作列 - 扩展宽度以容纳新按钮
    historyTable->horizontalHeader()->setSectionResizeMode(8, QHeaderView::Stretch);

    historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    historyTable->verticalHeader()->setVisible(false);
    historyTable->verticalHeader()->setDefaultSectionSize(90);  // 增加行高
    historyTable->setMouseTracking(true);  // 启用鼠标追踪，触发 ToolTip 事件

    // 启用右键菜单
    historyTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(historyTable, &QTableWidget::customContextMenuRequested, this, &VideoSingleHistoryTab::showContextMenu);

    layout->addWidget(historyTable);

    // 在表头"选择"列添加勾选框 - 父widget设置为header
    QHeaderView *header = historyTable->horizontalHeader();
    headerCheckBox = new QCheckBox(header);
    connect(headerCheckBox, &QCheckBox::checkStateChanged, this, &VideoSingleHistoryTab::onSelectAllChanged);

    // 初始化勾选框位置
    updateHeaderCheckBoxPosition();

    // 监听表头的resize事件，动态调整勾选框位置
    connect(header, &QHeaderView::sectionResized, this, [this](int logicalIndex, int, int) {
        if (logicalIndex == 0) {
            updateHeaderCheckBoxPosition();
        }
    });
}

void VideoSingleHistoryTab::setupThumbnailView()
{
    thumbnailViewWidget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(thumbnailViewWidget);
    layout->setContentsMargins(0, 0, 0, 0);

    thumbnailScrollArea = new QScrollArea();
    thumbnailScrollArea->setWidgetResizable(true);
    thumbnailScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    thumbnailScrollArea->setFrameShape(QFrame::NoFrame);

    thumbnailContainer = new QWidget();
    thumbnailLayout = new QGridLayout(thumbnailContainer);
    thumbnailLayout->setSpacing(15);

    thumbnailScrollArea->setWidget(thumbnailContainer);
    layout->addWidget(thumbnailScrollArea);
}

void VideoSingleHistoryTab::loadHistory(int offset, int limit)
{
    currentOffset = offset;

    if (isListView) {
        // 加载列表视图数据
        historyTable->setRowCount(0);
        selectedTaskIds.clear();  // 清空选中状态
        QList<VideoTask> tasks = DBManager::instance()->getTasksByType("video_single", offset, limit);

        int row = 0;
        for (const VideoTask& task : tasks) {
            const VideoTaskDisplayState displayState = resolveVideoTaskDisplayState(task);
            historyTable->insertRow(row);

            // 勾选框
            QWidget *checkBoxWidget = new QWidget();
            QHBoxLayout *checkBoxLayout = new QHBoxLayout(checkBoxWidget);
            checkBoxLayout->setContentsMargins(0, 0, 0, 0);
            checkBoxLayout->setAlignment(Qt::AlignCenter);
            QCheckBox *checkBox = new QCheckBox();
            checkBox->setProperty("taskId", task.taskId);
            if (!displayState.canDelete) {
                checkBox->setEnabled(false);
                checkBox->setToolTip("当前状态不可删除");
            }
            connect(checkBox, &QCheckBox::checkStateChanged, this, &VideoSingleHistoryTab::onCheckBoxStateChanged);
            checkBoxLayout->addWidget(checkBox);
            historyTable->setCellWidget(row, 0, checkBoxWidget);

            // 序号
            historyTable->setItem(row, 1, new QTableWidgetItem(QString::number(row + 1)));

            // 任务ID - 显示完整ID，让Qt自动省略
            QTableWidgetItem *taskIdItem = new QTableWidgetItem(task.taskId);
            taskIdItem->setToolTip(task.taskId);  // 鼠标悬停显示完整ID
            historyTable->setItem(row, 2, taskIdItem);

            // 提示词 - 显示完整提示词，让Qt自动省略
            QTableWidgetItem *promptItem = new QTableWidgetItem(task.prompt);
            promptItem->setToolTip(task.prompt);  // 鼠标悬停显示完整提示词
            historyTable->setItem(row, 3, promptItem);

            // 状态
            QString statusText = displayState.statusText;
            // 检测temp-ID任务
            if (task.taskId.startsWith("temp-")) {
                statusText = "⚠️ 待恢复";
            }
            QTableWidgetItem *statusItem = new QTableWidgetItem(statusText);
            if (!task.errorMessage.isEmpty()) {
                statusItem->setToolTip(task.errorMessage);
            }
            historyTable->setItem(row, 4, statusItem);

            // 如果是temp-ID任务，设置行背景色高亮
            if (task.taskId.startsWith("temp-")) {
                QColor highlightColor = QColor(255, 250, 205);  // 浅黄色
                // 检查是否为深色主题
                QWidget* topWidget = window();
                if (topWidget) {
                    QPalette pal = topWidget->palette();
                    QColor bgColor = pal.color(QPalette::Window);
                    if (bgColor.lightness() < 128) {
                        // 深色主题，使用暖色调
                        highlightColor = QColor(80, 60, 40);
                    }
                }
                for (int col = 0; col < historyTable->columnCount(); ++col) {
                    QTableWidgetItem* item = historyTable->item(row, col);
                    if (item) {
                        item->setBackground(highlightColor);
                    }
                }
            }

            // 进度
            historyTable->setItem(row, 5, new QTableWidgetItem(QString::number(task.progress) + "%"));

            // 创建时间
            historyTable->setItem(row, 6, new QTableWidgetItem(task.createdAt.toString("yyyy-MM-dd HH:mm:ss")));

            // 视频类型（模型变体）
            QString videoType = task.modelVariant.isEmpty() ? "-" : task.modelVariant;
            historyTable->setItem(row, 7, new QTableWidgetItem(videoType));

            // 操作按钮
            QWidget *btnWidget = new QWidget();
            QHBoxLayout *btnLayout = new QHBoxLayout(btnWidget);
            btnLayout->setContentsMargins(5, 2, 5, 2);
            btnLayout->setSpacing(5);

            QPushButton *viewBtn = new QPushButton("查看");
            QPushButton *browseBtn = new QPushButton("浏览");
            QPushButton *retryDownloadBtn = new QPushButton("重新下载");
            QPushButton *refreshBtn = new QPushButton("刷新");
            QPushButton *regenerateBtn = new QPushButton("重新生成");

            viewBtn->setMaximumWidth(60);
            browseBtn->setMaximumWidth(60);
            retryDownloadBtn->setMinimumWidth(80);
            retryDownloadBtn->setMaximumWidth(80);
            refreshBtn->setMaximumWidth(60);
            regenerateBtn->setMinimumWidth(110);
            regenerateBtn->setMaximumWidth(110);

            viewBtn->setEnabled(displayState.canPlay);
            if (!displayState.canPlay) {
                viewBtn->setToolTip(unresolvedVideoStateMessage(task, displayState));
            }
            browseBtn->setEnabled(displayState.canBrowse);
            if (!displayState.canBrowse) {
                browseBtn->setToolTip("本地视频不可用");
            }
            const bool showRetryDownload = shouldShowRetryDownloadButton(displayState);
            retryDownloadBtn->setVisible(showRetryDownload);
            if (!showRetryDownload) {
                retryDownloadBtn->setEnabled(false);
            }
            refreshBtn->setEnabled(displayState.canRefresh);
            if (!displayState.canRefresh) {
                refreshBtn->setToolTip("当前状态无需刷新");
            }

            connect(viewBtn, &QPushButton::clicked, [this, task]() {
                onViewVideo(task.taskId);
            });
            connect(browseBtn, &QPushButton::clicked, [this, task]() {
                onBrowseFile(task.taskId);
            });
            connect(retryDownloadBtn, &QPushButton::clicked, [this, task]() {
                onRetryDownload(task.taskId);
            });
            connect(refreshBtn, &QPushButton::clicked, [this, task]() {
                onRetryQuery(task.taskId);
            });
            connect(regenerateBtn, &QPushButton::clicked, [this, task]() {
                onRegenerate(task.taskId);
            });

            btnLayout->addWidget(viewBtn);
            btnLayout->addWidget(browseBtn);
            if (showRetryDownload) {
                btnLayout->addWidget(retryDownloadBtn);
            }
            btnLayout->addWidget(refreshBtn);
            btnLayout->addWidget(regenerateBtn);

            historyTable->setCellWidget(row, 8, btnWidget);

            row++;
        }

        // 重置表头勾选框状态
        if (headerCheckBox) {
            headerCheckBox->blockSignals(true);
            headerCheckBox->setCheckState(Qt::Unchecked);
            headerCheckBox->blockSignals(false);
        }
    } else {
        // 加载缩略图视图数据
        // 清空现有缩略图
        QLayoutItem* item;
        while ((item = thumbnailLayout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }

        QList<VideoTask> tasks = DBManager::instance()->getTasksByType("video_single", offset, limit);

        int col = 0;
        int row = 0;
        int colsPerRow = 4;  // 每行 4 个缩略图

        for (const VideoTask& task : tasks) {
            const VideoTaskDisplayState displayState = resolveVideoTaskDisplayState(task);
            // 创建缩略图卡片
            QWidget* card = new QWidget();
            card->setFixedSize(200, 250);
            card->setStyleSheet("QWidget { background: #2a2a2a; border-radius: 8px; }");

            QVBoxLayout* cardLayout = new QVBoxLayout(card);
            cardLayout->setContentsMargins(10, 10, 10, 10);
            cardLayout->setSpacing(8);

            // 缩略图或占位符
            QLabel* thumbLabel = new QLabel();
            thumbLabel->setFixedSize(180, 120);
            thumbLabel->setAlignment(Qt::AlignCenter);

            if (!task.thumbnailPath.isEmpty() && QFile::exists(task.thumbnailPath)) {
                QPixmap pixmap(task.thumbnailPath);
                thumbLabel->setPixmap(pixmap.scaled(180, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                thumbLabel->setText("🎬");
                thumbLabel->setStyleSheet("font-size: 48px; background: #1a1a1a;");
            }

            // 状态图标
            QLabel* statusLabel = new QLabel(displayState.statusIcon);
            statusLabel->setToolTip(displayState.statusText);
            statusLabel->setStyleSheet("font-size: 20px;");

            // 提示词摘要
            QString promptPreview = task.prompt.left(30);
            if (task.prompt.length() > 30) promptPreview += "...";
            QLabel* promptLabel = new QLabel(promptPreview);
            promptLabel->setWordWrap(true);
            promptLabel->setStyleSheet("color: #cccccc; font-size: 12px;");

            // 操作按钮
            QHBoxLayout* btnLayout = new QHBoxLayout();
            QPushButton* viewBtn = new QPushButton("查看");
            QPushButton* browseBtn = new QPushButton("浏览");
            QPushButton* retryDownloadBtn = new QPushButton("重新下载");
            viewBtn->setMaximumWidth(60);
            browseBtn->setMaximumWidth(60);
            retryDownloadBtn->setMaximumWidth(60);

            viewBtn->setEnabled(displayState.canPlay);
            if (!displayState.canPlay) {
                viewBtn->setToolTip(unresolvedVideoStateMessage(task, displayState));
            }
            browseBtn->setEnabled(displayState.canBrowse);
            if (!displayState.canBrowse) {
                browseBtn->setToolTip("本地视频不可用");
            }
            const bool showRetryDownload = shouldShowRetryDownloadButton(displayState);
            retryDownloadBtn->setVisible(showRetryDownload);
            if (!showRetryDownload) {
                retryDownloadBtn->setEnabled(false);
            }

            connect(viewBtn, &QPushButton::clicked, [this, task]() {
                onViewVideo(task.taskId);
            });
            connect(browseBtn, &QPushButton::clicked, [this, task]() {
                onBrowseFile(task.taskId);
            });
            connect(retryDownloadBtn, &QPushButton::clicked, [this, task]() {
                onRetryDownload(task.taskId);
            });

            btnLayout->addWidget(viewBtn);
            btnLayout->addWidget(browseBtn);
            if (showRetryDownload) {
                btnLayout->addWidget(retryDownloadBtn);
            }

            // 组装卡片
            cardLayout->addWidget(thumbLabel);
            cardLayout->addWidget(statusLabel);
            cardLayout->addWidget(promptLabel);
            cardLayout->addLayout(btnLayout);

            // 添加到网格
            thumbnailLayout->addWidget(card, row, col);

            col++;
            if (col >= colsPerRow) {
                col = 0;
                row++;
            }
        }
    }
}

void VideoSingleHistoryTab::switchView()
{
    isListView = !isListView;

    if (isListView) {
        switchViewButton->setText("📷 缩略图视图");
        viewStack->setCurrentIndex(0);
    } else {
        switchViewButton->setText("📋 列表视图");
        viewStack->setCurrentIndex(1);
    }

    loadHistory(0, isListView ? 50 : 20);
}

void VideoSingleHistoryTab::refreshHistory()
{
    loadHistory(0, isListView ? 50 : 20);
}

void VideoSingleHistoryTab::onViewVideo(const QString& taskId)
{
    // 查询任务信息
    VideoTask task = DBManager::instance()->getTaskById(taskId);

    if (task.taskId.isEmpty()) {
        QMessageBox::warning(this, "错误", "未找到任务记录");
        return;
    }

    const VideoTaskDisplayState displayState = resolveVideoTaskDisplayState(task);

    // 本地文件可用，直接打开
    if (displayState.hasLocalFile) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(task.videoPath));
        return;
    }

    // 已生成待下载 / 下载失败：触发下载
    if (displayState.canDownload) {
        QMessageBox::StandardButton reply = QMessageBox::question(this, "下载视频",
            "视频尚未下载，是否立即下载？",
            QMessageBox::Yes | QMessageBox::No);

        if (reply != QMessageBox::Yes) {
            return;
        }

        // 从 apiKeyName 查出实际 key 值
        QString apiKey;
        const auto allKeys = DBManager::instance()->getAllApiKeys();
        for (const auto& k : allKeys) {
            if (k.name == task.apiKeyName) {
                apiKey = k.apiKey;
                break;
            }
        }
        if (apiKey.isEmpty()) {
            QMessageBox::warning(this, "提示",
                QString("未找到密钥 \"%1\"，请在设置中确认密钥是否存在。").arg(task.apiKeyName));
            return;
        }

        TaskPollManager::getInstance()->triggerDownload(
            task.taskId, task.videoUrl, apiKey, task.serverUrl, task.taskType);
        QMessageBox::information(this, "提示", "已开始下载，请稍后刷新历史记录查看进度。");
        return;
    }

    // 其余状态按展示态给提示
    QMessageBox::warning(this, "提示", unresolvedVideoStateMessage(task, displayState));
}

void VideoSingleHistoryTab::onBrowseFile(const QString& taskId)
{
    // 查询任务信息
    VideoTask task = DBManager::instance()->getTaskById(taskId);

    if (task.taskId.isEmpty()) {
        QMessageBox::warning(this, "错误", "未找到任务记录");
        return;
    }

    if (task.videoPath.isEmpty() || !QFile::exists(task.videoPath)) {
        QMessageBox::warning(this, "提示", "视频文件不存在");
        return;
    }

    // 打开文件管理器并定位文件
    QFileInfo fileInfo(task.videoPath);
    QString dirPath = fileInfo.absolutePath();

#ifdef Q_OS_MAC
    // macOS: 使用 Finder 打开并选中文件
    QProcess::execute("open", QStringList() << "-R" << task.videoPath);
#elif defined(Q_OS_WIN)
    // Windows: 使用 explorer 打开并选中文件
    QProcess::execute("explorer", QStringList() << "/select," << QDir::toNativeSeparators(task.videoPath));
#else
    // Linux: 打开目录
    QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
#endif
}

void VideoSingleHistoryTab::onRetryDownload(const QString& taskId)
{
    onViewVideo(taskId);
}

void VideoSingleHistoryTab::onRetryQuery(const QString& taskId)
{
    // 获取当前选择的密钥
    if (!apiKeyCombo || apiKeyCombo->currentIndex() < 0) {
        QMessageBox::warning(this, "错误", "请先选择一个API密钥");
        return;
    }

    int apiKeyId = apiKeyCombo->currentData().toInt();
    if (apiKeyId <= 0) {
        QMessageBox::warning(this, "错误", "无效的API密钥");
        return;
    }

    // 从数据库获取密钥详细信息
    ApiKey selectedKey = DBManager::instance()->getApiKey(apiKeyId);
    if (selectedKey.apiKey.isEmpty()) {
        QMessageBox::warning(this, "错误", "未找到选择的API密钥");
        return;
    }

    // 获取服务器URL
    QString baseUrl = serverCombo->currentData().toString();
    if (baseUrl.isEmpty()) {
        QMessageBox::warning(this, "错误", "请选择服务器");
        return;
    }

    // 查询任务信息
    VideoTask task = DBManager::instance()->getTaskById(taskId);
    if (task.taskId.isEmpty()) {
        QMessageBox::warning(this, "错误", "未找到任务记录");
        return;
    }

    // 记录当前正在刷新的任务ID
    currentRefreshingTaskId = taskId;

    // 调用API重新查询任务状态
    veo3API->queryTask(selectedKey.apiKey, baseUrl, task.modelName, taskId);
}

void VideoSingleHistoryTab::onRegenerate(const QString& taskId)
{
    // 使用getTaskById直接获取完整任务信息
    VideoTask task = DBManager::instance()->getTaskById(taskId);

    if (task.taskId.isEmpty()) {
        QMessageBox::warning(this, "错误", "未找到任务信息");
        return;
    }

    // 向上遍历找到VideoGenWidget
    // 层级：VideoSingleHistoryTab -> QTabWidget -> VideoHistoryWidget -> QTabWidget -> VideoGenWidget
    QWidget* current = this;
    VideoGenWidget* videoGenWidget = nullptr;

    // 最多向上查找10层
    for (int i = 0; i < 10 && current; ++i) {
        current = current->parentWidget();
        videoGenWidget = qobject_cast<VideoGenWidget*>(current);
        if (videoGenWidget) {
            break;
        }
    }

    if (!videoGenWidget) {
        QMessageBox::critical(this, "错误", "无法获取主窗口引用");
        return;
    }

    // 查找VideoGenWidget内部的QTabWidget并切换到index 0
    QTabWidget* mainTabWidget = videoGenWidget->findChild<QTabWidget*>();
    if (mainTabWidget) {
        mainTabWidget->setCurrentIndex(0);
    }

    // 获取VideoSingleTab并加载参数
    VideoSingleTab* singleTab = videoGenWidget->getSingleTab();
    if (singleTab) {
        singleTab->loadFromTask(task);
    } else {
        QMessageBox::critical(this, "错误", "无法获取单个视频生成标签页");
    }
}

void VideoSingleHistoryTab::showContextMenu(const QPoint &pos)
{
    // 获取点击的单元格
    QTableWidgetItem *item = historyTable->itemAt(pos);
    if (!item) {
        return;
    }

    int row = item->row();
    // 任务ID在第2列
    QTableWidgetItem *taskIdItem = historyTable->item(row, 2);
    if (!taskIdItem) {
        return;
    }
    QString taskId = taskIdItem->text();

    // 创建右键菜单
    QMenu contextMenu(this);
    contextMenu.setStyleSheet("QMenu { font-size: 14px; } QMenu::item { padding: 5px 20px; }");
    QAction *copyAction = contextMenu.addAction("📋 复制");

    // 如果是 temp-ID，添加修复选项
    QAction *fixAction = nullptr;
    if (taskId.startsWith("temp_")) {
        fixAction = contextMenu.addAction("🔧 修复任务ID");
    }

    // 显示菜单并获取用户选择
    QAction *selectedAction = contextMenu.exec(historyTable->viewport()->mapToGlobal(pos));

    if (selectedAction == copyAction) {
        // 复制单元格内容到剪贴板
        QString cellText = item->text();

        // 如果单元格文本为空，尝试获取 tooltip（完整内容）
        if (cellText.isEmpty() || cellText == "-") {
            cellText = item->toolTip();
        }

        // 如果还是空的，可能是操作列（包含按钮），跳过
        if (!cellText.isEmpty()) {
            QClipboard *clipboard = QApplication::clipboard();
            clipboard->setText(cellText);

            // 可选：显示提示
            // QMessageBox::information(this, "提示", "已复制到剪贴板");
        }
    } else if (fixAction && selectedAction == fixAction) {
        onFixTaskId(taskId);
    }
}

void VideoSingleHistoryTab::onDeleteSelected()
{
    if (selectedTaskIds.isEmpty()) {
        QMessageBox::information(this, "提示", "请先选择要删除的记录");
        return;
    }

    // 确认删除
    int count = selectedTaskIds.size();
    QMessageBox::StandardButton reply = QMessageBox::question(this, "确认删除",
        QString("确定要删除选中的 %1 条记录吗？\n\n注意：只会删除数据库记录，不会删除本地视频文件。").arg(count),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    // 执行删除
    QStringList taskIdsToDelete(selectedTaskIds.begin(), selectedTaskIds.end());
    int deletedCount = DBManager::instance()->deleteVideoTasks(taskIdsToDelete);

    if (deletedCount > 0) {
        QMessageBox::information(this, "删除成功", QString("成功删除 %1 条记录").arg(deletedCount));
        selectedTaskIds.clear();
        refreshHistory();
    } else {
        QMessageBox::warning(this, "删除失败", "删除记录失败，请查看日志");
    }
}

void VideoSingleHistoryTab::onFixTaskId(const QString& tempTaskId)
{
    // 创建自定义对话框以支持主题适配
    QDialog dialog(this);
    dialog.setWindowTitle("修复任务ID");
    dialog.setMinimumWidth(400);

    // 继承父窗口的样式表
    QWidget *topLevelWidget = window();
    if (topLevelWidget) {
        dialog.setStyleSheet(topLevelWidget->styleSheet());
    }

    // 为对话框显式设置深色主题样式
    QString dialogStyle = R"(
        QDialog {
            background: rgba(30, 27, 75, 0.95);
            color: #F8FAFC;
        }
        QLabel {
            color: #F8FAFC;
            font-size: 14px;
        }
        QLineEdit {
            background: rgba(248, 250, 252, 0.05);
            border: 1px solid rgba(248, 250, 252, 0.1);
            border-radius: 8px;
            color: #F8FAFC;
            padding: 8px 12px;
            font-size: 14px;
        }
        QLineEdit:focus {
            border: 1px solid rgba(225, 29, 72, 0.5);
            background: rgba(248, 250, 252, 0.08);
        }
        QPushButton {
            background: rgba(225, 29, 72, 0.2);
            border: 1px solid rgba(225, 29, 72, 0.5);
            border-radius: 8px;
            color: #F8FAFC;
            padding: 8px 16px;
            font-size: 14px;
            font-weight: 500;
        }
        QPushButton:hover {
            background: rgba(225, 29, 72, 0.3);
            border: 1px solid rgba(225, 29, 72, 0.7);
        }
        QPushButton:pressed {
            background: rgba(225, 29, 72, 0.4);
        }
    )";
    dialog.setStyleSheet(dialogStyle);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->setSpacing(15);
    layout->setContentsMargins(20, 20, 20, 20);

    // 提示标签
    QLabel *hintLabel = new QLabel(QString("临时任务ID: %1\n\n请输入真实的任务ID:").arg(tempTaskId));
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);

    // 输入框
    QLineEdit *lineEdit = new QLineEdit();
    layout->addWidget(lineEdit);

    // 按钮
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *okButton = new QPushButton("确定");
    QPushButton *cancelButton = new QPushButton("取消");
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);

    // 连接信号
    connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(lineEdit, &QLineEdit::returnPressed, &dialog, &QDialog::accept);

    // 显示对话框
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString realTaskId = lineEdit->text().trimmed();
    if (realTaskId.isEmpty()) {
        return;
    }

    // 验证真实任务ID格式（不能是temp-开头）
    if (realTaskId.startsWith("temp_")) {
        QMessageBox::warning(this, "错误", "真实任务ID不能以 temp_ 开头");
        return;
    }

    // 更新数据库
    if (!DBManager::instance()->updateTaskId(tempTaskId, realTaskId)) {
        QMessageBox::warning(this, "错误", "更新任务ID失败");
        return;
    }

    // 更新任务状态为 pending
    DBManager::instance()->updateTaskStatus(realTaskId, "pending", 0, "");

    // 启动轮询
    if (apiKeyCombo->count() > 0) {
        int keyId = apiKeyCombo->currentData().toInt();
        ApiKey apiKeyData = DBManager::instance()->getApiKey(keyId);
        QString server = serverCombo->currentData().toString();

        if (!apiKeyData.apiKey.isEmpty() && !server.isEmpty()) {
            TaskPollManager::getInstance()->startPolling(realTaskId, "video_single",
                apiKeyData.apiKey, server, "");
        }
    }

    QMessageBox::information(this, "成功",
        QString("任务ID已更新:\n%1\n→\n%2\n\n已启动轮询查询任务状态").arg(tempTaskId, realTaskId));

    // 刷新历史记录
    refreshHistory();
}

void VideoSingleHistoryTab::onSelectAllChanged(int state)
{
    bool checked = (state == Qt::Checked);

    for (int row = 0; row < historyTable->rowCount(); ++row) {
        QWidget *widget = historyTable->cellWidget(row, 0);
        if (widget) {
            QCheckBox *checkBox = widget->findChild<QCheckBox*>();
            if (checkBox && checkBox->isEnabled()) {
                checkBox->setChecked(checked);
            }
        }
    }
}

void VideoSingleHistoryTab::onCheckBoxStateChanged()
{
    selectedTaskIds.clear();
    int enabledCount = 0;  // 可用的勾选框数量
    int checkedCount = 0;  // 已勾选的数量

    for (int row = 0; row < historyTable->rowCount(); ++row) {
        QWidget *widget = historyTable->cellWidget(row, 0);
        if (widget) {
            QCheckBox *checkBox = widget->findChild<QCheckBox*>();
            if (checkBox) {
                if (checkBox->isEnabled()) {
                    enabledCount++;
                    if (checkBox->isChecked()) {
                        checkedCount++;
                        QString taskId = checkBox->property("taskId").toString();
                        if (!taskId.isEmpty()) {
                            selectedTaskIds.insert(taskId);
                        }
                    }
                }
            }
        }
    }

    // 更新表头勾选框状态
    if (headerCheckBox) {
        headerCheckBox->blockSignals(true);  // 阻止信号，避免递归调用
        if (checkedCount == 0) {
            headerCheckBox->setCheckState(Qt::Unchecked);
        } else if (checkedCount == enabledCount) {
            headerCheckBox->setCheckState(Qt::Checked);
        } else {
            headerCheckBox->setCheckState(Qt::PartiallyChecked);  // 部分选中状态
        }
        headerCheckBox->blockSignals(false);
    }
}

void VideoSingleHistoryTab::onTaskStatusUpdated(const QString& taskId, const QString& status, int progress)
{
    qDebug() << "[HISTORY] onTaskStatusUpdated received: taskId=" << taskId
             << "status=" << status << "progress=" << progress;

    const int displayProgress = qBound(0, progress, 100);

    // 更新数据库
    DBManager::instance()->updateTaskStatus(taskId, status, displayProgress);

    // 基于数据库最新记录重新解析展示态，避免局部更新导致状态语义错乱
    VideoTask task = DBManager::instance()->getTaskById(taskId);
    if (task.taskId.isEmpty()) {
        return;
    }
    const VideoTaskDisplayState displayState = resolveVideoTaskDisplayState(task);

    // 只在列表视图时增量更新UI；缩略图视图下下次刷新时统一渲染
    if (!isListView) {
        return;
    }

    // 遍历表格，找到对应任务并更新状态、进度、勾选框和按钮可用性
    for (int row = 0; row < historyTable->rowCount(); ++row) {
        QTableWidgetItem *taskIdItem = historyTable->item(row, 2);
        if (!taskIdItem || taskIdItem->text() != taskId) {
            continue;
        }

        QTableWidgetItem *statusItem = historyTable->item(row, 4);
        if (statusItem) {
            statusItem->setText(displayState.statusText);
            if (!task.errorMessage.isEmpty()) {
                statusItem->setToolTip(task.errorMessage);
            } else {
                statusItem->setToolTip(QString());
            }
        }

        QTableWidgetItem *progressItem = historyTable->item(row, 5);
        if (progressItem) {
            progressItem->setText(QString::number(displayProgress) + "%");
        }

        QWidget *checkWidget = historyTable->cellWidget(row, 0);
        if (checkWidget) {
            QCheckBox *checkBox = checkWidget->findChild<QCheckBox*>();
            if (checkBox) {
                checkBox->setEnabled(displayState.canDelete);
                checkBox->setToolTip(displayState.canDelete ? QString() : "当前状态不可删除");
            }
        }

        QWidget *btnWidget = historyTable->cellWidget(row, 8);
        if (btnWidget) {
            const auto buttons = btnWidget->findChildren<QPushButton*>();
            for (QPushButton* button : buttons) {
                if (!button) {
                    continue;
                }
                if (button->text() == "查看") {
                    button->setEnabled(displayState.canPlay);
                    button->setToolTip(displayState.canPlay ? QString() : unresolvedVideoStateMessage(task, displayState));
                } else if (button->text() == "浏览") {
                    button->setEnabled(displayState.canBrowse);
                    button->setToolTip(displayState.canBrowse ? QString() : "本地视频不可用");
                } else if (button->text() == "刷新") {
                    button->setEnabled(displayState.canRefresh);
                    button->setToolTip(displayState.canRefresh ? QString() : "当前状态无需刷新");
                }
            }
        }

        break;
    }
}

void VideoSingleHistoryTab::onApiTaskStatusUpdated(const QString& taskId, const QString& status, const QString& videoUrl, int progress)
{
    // 更新数据库（包括videoUrl）
    DBManager::instance()->updateTaskStatus(taskId, status, progress, videoUrl);

    // 调用原有的更新方法处理UI更新
    onTaskStatusUpdated(taskId, status, progress);

    // 如果是手动刷新的任务，显示成功提示
    if (taskId == currentRefreshingTaskId && !currentRefreshingTaskId.isEmpty()) {
        // 显示气泡提示
        QLabel *toast = new QLabel("✅ 刷新成功", this);
        toast->setStyleSheet(
            "QLabel {"
            "    background-color: #4CAF50;"
            "    color: white;"
            "    padding: 10px 20px;"
            "    border-radius: 5px;"
            "    font-size: 14px;"
            "}"
        );
        toast->setAlignment(Qt::AlignCenter);
        toast->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
        toast->setAttribute(Qt::WA_TranslucentBackground);

        // 计算位置（屏幕中央偏上）
        QPoint globalPos = mapToGlobal(rect().center());
        toast->move(globalPos.x() - 50, globalPos.y() - 100);
        toast->show();

        // 2秒后自动消失
        QTimer::singleShot(2000, toast, &QLabel::deleteLater);

        // 清空刷新任务ID
        currentRefreshingTaskId.clear();
    }
}

void VideoSingleHistoryTab::onQueryError(const QString& error)
{
    // 如果是手动刷新的任务，显示错误弹窗
    if (!currentRefreshingTaskId.isEmpty()) {
        QMessageBox::critical(this, "刷新失败",
            QString("查询任务状态失败\n\n错误原因：%1").arg(error));

        // 清空刷新任务ID
        currentRefreshingTaskId.clear();
    }
}

void VideoSingleHistoryTab::updateHeaderCheckBoxPosition()
{
    if (!headerCheckBox || !historyTable) {
        return;
    }

    QHeaderView *header = historyTable->horizontalHeader();

    // 获取第一列的位置和宽度
    int columnWidth = header->sectionSize(0);
    int columnPos = header->sectionViewportPosition(0);

    // 获取表头高度
    int headerHeight = header->height();

    // 勾选框大小
    int checkBoxSize = 18;

    // 计算居中位置（现在勾选框的父widget是header，坐标相对于header）
    int x = columnPos + (columnWidth - checkBoxSize) / 2;
    int y = (headerHeight - checkBoxSize) / 2;

    // 设置勾选框位置和大小
    headerCheckBox->setGeometry(x, y, checkBoxSize, checkBoxSize);
    headerCheckBox->raise();  // 确保勾选框在最上层
}
