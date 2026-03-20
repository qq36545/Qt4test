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

// VideoSingleTab 实现
VideoSingleTab::VideoSingleTab(QWidget *parent)
    : QWidget(parent),
      suppressDuplicateWarning(false),
      parametersModified(false),
      pendingSaveSettings(false),
      suppressAutoSave(false),
      wanAudioUploading(false)
{
    veo3API = new VideoAPI(this);
    connect(veo3API, &VideoAPI::videoCreated, this, &VideoSingleTab::onVideoCreated);
    connect(veo3API, &VideoAPI::taskStatusUpdated, this, &VideoSingleTab::onTaskStatusUpdated);
    connect(veo3API, &VideoAPI::errorOccurred, this, &VideoSingleTab::onApiError);

    setupUI();
    loadApiKeys();
    loadSettings();

    // 在加载配置后再连接信号，避免加载时触发保存
    connectSignals();
}

bool VideoSingleTab::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        if (obj == imagePreviewLabel) {
            uploadImage();
            return true;
        } else if (obj == endFramePreviewLabel) {
            uploadEndFrameImage();
            return true;
        } else if (obj == middleFramePreviewLabel) {
            uploadMiddleFrameImage();
            return true;
        } else if (obj == grokImage2PreviewLabel) {
            uploadGrokImage2();
            return true;
        } else if (obj == grokImage3PreviewLabel) {
            uploadGrokImage3();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void VideoSingleTab::setupUI()
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
    connect(modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::onModelChanged);
    modelLayout->addWidget(modelLabel);
    modelLayout->addWidget(modelCombo, 1);
    contentLayout->addLayout(modelLayout);

    // 变体类型单选按钮（VEO3专用，默认隐藏）
    variantTypeWidget = new QWidget();
    QHBoxLayout *variantTypeLayout = new QHBoxLayout(variantTypeWidget);
    variantTypeLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *variantTypeLabel = new QLabel("API格式:");
    variantTypeLabel->setStyleSheet("font-size: 14px;");
    variantType1Radio = new QRadioButton("OpenAI格式");
    variantType2Radio = new QRadioButton("统一格式");
    variantType1Radio->setStyleSheet("color: white; font-size: 14px;");
    variantType2Radio->setStyleSheet("color: white; font-size: 14px;");
    variantType1Radio->setChecked(true);
    variantTypeLayout->addWidget(variantTypeLabel);
    variantTypeLayout->addWidget(variantType1Radio);
    variantTypeLayout->addWidget(variantType2Radio);
    variantTypeLayout->addStretch();
    contentLayout->addWidget(variantTypeWidget);
    variantTypeWidget->setVisible(false);

    // VEO3 模型变体选择
    QHBoxLayout *variantLayout = new QHBoxLayout();
    QLabel *variantLabel = new QLabel("模型变体:");
    variantLabel->setStyleSheet("font-size: 14px;");
    modelVariantCombo = new QComboBox();
    modelVariantCombo->addItem("veo_3_1-fast", "veo_3_1-fast");
    modelVariantCombo->addItem("veo_3_1", "veo_3_1");
    modelVariantCombo->addItem("veo_3_1-fast-4K", "veo_3_1-fast-4K");
    modelVariantCombo->addItem("veo_3_1-fast-components-4K", "veo_3_1-fast-components-4K");
    connect(modelVariantCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::onModelVariantChanged);
    variantLayout->addWidget(variantLabel);
    variantLayout->addWidget(modelVariantCombo, 1);
    contentLayout->addLayout(variantLayout);

    // WAN 视频时长（仅 WAN 显示，插入模型变体下方）
    QHBoxLayout *wanDurationLayout = new QHBoxLayout();
    wanDurationLayout->setSpacing(20);
    wanDurationLabel = new QLabel("视频时长:");
    wanDurationLabel->setStyleSheet("font-size: 14px;");
    wanDurationLabel->setVisible(false);
    wanDurationCombo = new QComboBox();
    wanDurationCombo->addItem("5秒", 5);
    wanDurationCombo->addItem("10秒", 10);
    wanDurationCombo->setVisible(false);
    wanResolutionLabel = new QLabel("视频分辨率:");
    wanResolutionLabel->setStyleSheet("font-size: 14px;");
    wanResolutionLabel->setVisible(false);
    wanResolutionCombo = new QComboBox();
    wanResolutionCombo->addItem("480P", "480P");
    wanResolutionCombo->addItem("720P", "720P");
    wanResolutionCombo->addItem("1080P", "1080P");
    wanResolutionCombo->setVisible(false);
    wanDurationLayout->addWidget(wanDurationLabel);
    wanDurationLayout->addWidget(wanDurationCombo);
    wanDurationLayout->addSpacing(20);
    wanDurationLayout->addWidget(wanResolutionLabel);
    wanDurationLayout->addWidget(wanResolutionCombo);
    wanDurationLayout->addStretch();
    contentLayout->addLayout(wanDurationLayout);

    // WAN 正向提示词标签（仅 WAN 显示，覆盖通用 promptLabel）
    wanPromptLabel = new QLabel("正向提示词:");
    wanPromptLabel->setStyleSheet("font-size: 14px;");
    wanPromptLabel->setVisible(false);

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

    // 提示词输入
    QHBoxLayout *promptHeaderLayout = new QHBoxLayout();
    promptLabel = new QLabel("提示词:");
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
    promptInput->setPlaceholderText("输入视频生成提示词...\n例如：一只可爱的猫咪在花园里玩耍，阳光明媚，电影级画质");
    promptInput->setMinimumHeight(240);  // 支持多行输入和滚动
    promptInput->setStyleSheet("font-size: 15px;");
    contentLayout->addLayout(promptHeaderLayout);
    contentLayout->addWidget(promptInput);

    // 图片上传区域（首帧）
    imageLabel = new QLabel("首帧图片:");
    imageLabel->setStyleSheet("font-size: 14px;");
    imageUploadHintLabel = new QLabel("💡提示：垫图后，视频尺寸跟垫图的图片尺寸保持一致，下面\"宽高比\"参数会自动忽略处理");
    imageUploadHintLabel->setStyleSheet("font-size: 12px; color: #888888;");
    imageUploadHintLabel->setWordWrap(false);
    imageUploadHintLabel->setVisible(false);
    QHBoxLayout *imageLabelLayout = new QHBoxLayout();
    imageLabelLayout->setContentsMargins(0, 0, 0, 0);
    imageLabelLayout->setSpacing(8);
    imageLabelLayout->addWidget(imageLabel);
    imageLabelLayout->addWidget(imageUploadHintLabel);
    imageLabelLayout->addStretch();
    contentLayout->addLayout(imageLabelLayout);

    QHBoxLayout *imageLayout = new QHBoxLayout();
    imagePreviewLabel = new QLabel("未选择图片\n点击此处上传");
    imagePreviewLabel->setObjectName("imagePreviewLabel");
    imagePreviewLabel->setAlignment(Qt::AlignCenter);
    imagePreviewLabel->setCursor(Qt::PointingHandCursor);
    imagePreviewLabel->setScaledContents(false);
    imagePreviewLabel->installEventFilter(this);  // 安装事件过滤器以捕获点击

    uploadImageButton = new QPushButton("📁 选择首帧图片");
    uploadImageButton->setFixedWidth(150);
    connect(uploadImageButton, &QPushButton::clicked, this, &VideoSingleTab::uploadImage);

    clearImageButton = new QPushButton("🗑️ 清空");
    clearImageButton->setFixedWidth(80);
    connect(clearImageButton, &QPushButton::clicked, [this]() { clearGrokImage(0); });

    imageLayout->addWidget(imagePreviewLabel, 1);
    imageLayout->addWidget(uploadImageButton);
    imageLayout->addWidget(clearImageButton);
    contentLayout->addLayout(imageLayout);

    // 尾帧图片上传区域（默认隐藏，根据模型动态显示）
    endFrameWidget = new QWidget();
    QVBoxLayout *endFrameLayout = new QVBoxLayout(endFrameWidget);
    endFrameLayout->setContentsMargins(0, 0, 0, 0);
    endFrameLayout->setSpacing(10);

    endFrameLabel = new QLabel("尾帧图片（可选）:");
    endFrameLabel->setStyleSheet("font-size: 14px;");
    endFrameLayout->addWidget(endFrameLabel);

    QHBoxLayout *endFrameImageLayout = new QHBoxLayout();
    endFramePreviewLabel = new QLabel("未选择图片\n点击此处上传");
    endFramePreviewLabel->setObjectName("imagePreviewLabel");
    endFramePreviewLabel->setAlignment(Qt::AlignCenter);
    endFramePreviewLabel->setCursor(Qt::PointingHandCursor);
    endFramePreviewLabel->setScaledContents(false);
    endFramePreviewLabel->installEventFilter(this);  // 安装事件过滤器以捕获点击
    uploadEndFrameButton = new QPushButton("📁 选择尾帧图片");
    uploadEndFrameButton->setFixedWidth(150);
    connect(uploadEndFrameButton, &QPushButton::clicked, this, &VideoSingleTab::uploadEndFrameImage);
    endFrameImageLayout->addWidget(endFramePreviewLabel, 1);
    endFrameImageLayout->addWidget(uploadEndFrameButton);
    endFrameLayout->addLayout(endFrameImageLayout);

    contentLayout->addWidget(endFrameWidget);
    endFrameWidget->setVisible(true); // 默认显示，支持首尾帧

    // 中间帧图片上传区域（components 模型用，默认隐藏）
    middleFrameWidget = new QWidget();
    QVBoxLayout *middleFrameLayout = new QVBoxLayout(middleFrameWidget);
    middleFrameLayout->setContentsMargins(0, 0, 0, 0);
    middleFrameLayout->setSpacing(10);

    middleFrameLabel = new QLabel("图片2（中间帧，可选）:");
    middleFrameLabel->setStyleSheet("font-size: 14px;");
    middleFrameLayout->addWidget(middleFrameLabel);

    QHBoxLayout *middleFrameImageLayout = new QHBoxLayout();
    middleFramePreviewLabel = new QLabel("未选择图片\n点击此处上传");
    middleFramePreviewLabel->setObjectName("imagePreviewLabel");
    middleFramePreviewLabel->setAlignment(Qt::AlignCenter);
    middleFramePreviewLabel->setCursor(Qt::PointingHandCursor);
    middleFramePreviewLabel->setScaledContents(false);
    middleFramePreviewLabel->installEventFilter(this);
    uploadMiddleFrameButton = new QPushButton("📁 选择图片2");
    uploadMiddleFrameButton->setFixedWidth(150);
    connect(uploadMiddleFrameButton, &QPushButton::clicked, this, &VideoSingleTab::uploadMiddleFrameImage);
    middleFrameImageLayout->addWidget(middleFramePreviewLabel, 1);
    middleFrameImageLayout->addWidget(uploadMiddleFrameButton);
    middleFrameLayout->addLayout(middleFrameImageLayout);

    contentLayout->addWidget(middleFrameWidget);
    middleFrameWidget->setVisible(false);

    // Grok 图片2 上传区域（默认隐藏）
    grokImage2Widget = new QWidget();
    QVBoxLayout *grokImage2Layout = new QVBoxLayout(grokImage2Widget);
    grokImage2Layout->setContentsMargins(0, 0, 0, 0);
    grokImage2Layout->setSpacing(10);

    QLabel *grokImage2Label = new QLabel("图片2（可选）:");
    grokImage2Label->setStyleSheet("font-size: 14px;");
    grokImage2Layout->addWidget(grokImage2Label);

    QHBoxLayout *grokImage2ImageLayout = new QHBoxLayout();
    grokImage2PreviewLabel = new QLabel("未选择图片\n点击此处上传");
    grokImage2PreviewLabel->setObjectName("imagePreviewLabel");
    grokImage2PreviewLabel->setAlignment(Qt::AlignCenter);
    grokImage2PreviewLabel->setCursor(Qt::PointingHandCursor);
    grokImage2PreviewLabel->setScaledContents(false);
    grokImage2PreviewLabel->installEventFilter(this);

    uploadGrokImage2Button = new QPushButton("📁 选择图片2");
    uploadGrokImage2Button->setFixedWidth(150);
    connect(uploadGrokImage2Button, &QPushButton::clicked, this, &VideoSingleTab::uploadGrokImage2);

    clearGrokImage2Button = new QPushButton("🗑️ 清空");
    clearGrokImage2Button->setFixedWidth(80);
    connect(clearGrokImage2Button, &QPushButton::clicked, [this]() { clearGrokImage(1); });

    grokImage2ImageLayout->addWidget(grokImage2PreviewLabel, 1);
    grokImage2ImageLayout->addWidget(uploadGrokImage2Button);
    grokImage2ImageLayout->addWidget(clearGrokImage2Button);
    grokImage2Layout->addLayout(grokImage2ImageLayout);

    contentLayout->addWidget(grokImage2Widget);
    grokImage2Widget->setVisible(false);

    // Grok 图片3 上传区域（默认隐藏）
    grokImage3Widget = new QWidget();
    QVBoxLayout *grokImage3Layout = new QVBoxLayout(grokImage3Widget);
    grokImage3Layout->setContentsMargins(0, 0, 0, 0);
    grokImage3Layout->setSpacing(10);

    QLabel *grokImage3Label = new QLabel("图片3（可选）:");
    grokImage3Label->setStyleSheet("font-size: 14px;");
    grokImage3Layout->addWidget(grokImage3Label);

    QHBoxLayout *grokImage3ImageLayout = new QHBoxLayout();
    grokImage3PreviewLabel = new QLabel("未选择图片\n点击此处上传");
    grokImage3PreviewLabel->setObjectName("imagePreviewLabel");
    grokImage3PreviewLabel->setAlignment(Qt::AlignCenter);
    grokImage3PreviewLabel->setCursor(Qt::PointingHandCursor);
    grokImage3PreviewLabel->setScaledContents(false);
    grokImage3PreviewLabel->installEventFilter(this);

    uploadGrokImage3Button = new QPushButton("📁 选择图片3");
    uploadGrokImage3Button->setFixedWidth(150);
    connect(uploadGrokImage3Button, &QPushButton::clicked, this, &VideoSingleTab::uploadGrokImage3);

    clearGrokImage3Button = new QPushButton("🗑️ 清空");
    clearGrokImage3Button->setFixedWidth(80);
    connect(clearGrokImage3Button, &QPushButton::clicked, [this]() { clearGrokImage(2); });

    grokImage3ImageLayout->addWidget(grokImage3PreviewLabel, 1);
    grokImage3ImageLayout->addWidget(uploadGrokImage3Button);
    grokImage3ImageLayout->addWidget(clearGrokImage3Button);
    grokImage3Layout->addLayout(grokImage3ImageLayout);

    contentLayout->addWidget(grokImage3Widget);
    grokImage3Widget->setVisible(false);

    // 参数设置
    QHBoxLayout *paramsLayout = new QHBoxLayout();

    QVBoxLayout *resLayout = new QVBoxLayout();
    resolutionLabel = new QLabel("分辨率");
    resolutionLabel->setStyleSheet("font-size: 14px;");
    resolutionCombo = new QComboBox();
    resolutionCombo->addItem("横屏 16:9 (1280x720)", "1280x720");
    resolutionCombo->addItem("竖屏 9:16 (720x1280)", "720x1280");
    resLayout->addWidget(resolutionLabel);
    resLayout->addWidget(resolutionCombo);

    QVBoxLayout *durLayout = new QVBoxLayout();
    durationLabel = new QLabel("时长（秒）");
    durationLabel->setStyleSheet("font-size: 14px;");
    durationCombo = new QComboBox();
    durationCombo->addItem("8秒（固定）", "8");
    durationCombo->setEnabled(false); // VEO3 固定 8 秒
    durLayout->addWidget(durationLabel);
    durLayout->addWidget(durationCombo);

    QVBoxLayout *sizeLayout = new QVBoxLayout();
    sizeLabel = new QLabel("分辨率规格");
    sizeLabel->setStyleSheet("font-size: 14px;");
    sizeCombo = new QComboBox();
    sizeCombo->addItem("720P", "720P");
    sizeCombo->addItem("1080P", "1080P");
    sizeCombo->setVisible(false); // 默认隐藏，Grok模型时显示
    sizeLabel->setVisible(false);
    sizeLayout->addWidget(sizeLabel);

    QHBoxLayout *sizeRowLayout = new QHBoxLayout();
    sizeRowLayout->setContentsMargins(0, 0, 0, 0);
    sizeRowLayout->setSpacing(8);
    sizeRowLayout->addWidget(sizeCombo);

    sizeRowLayout->addStretch();

    sizeLayout->addLayout(sizeRowLayout);

    QVBoxLayout *watermarkLayout = new QVBoxLayout();
    watermarkLabel = new QLabel("水印");
    watermarkLabel->setStyleSheet("font-size: 14px;");
    watermarkCheckBox = new QCheckBox("保留水印");
    watermarkCheckBox->setStyleSheet("color: white; font-size: 12px;");
    watermarkCheckBox->setChecked(false);
    watermarkLayout->addWidget(watermarkLabel);
    watermarkLayout->addWidget(watermarkCheckBox);

    // enhance_prompt / enable_upsample（Variant 2 专用，默认隐藏）
    QVBoxLayout *enhanceLayout = new QVBoxLayout();
    enhancePromptLabel = new QLabel("增强提示词");
    enhancePromptLabel->setStyleSheet("font-size: 14px;");
    enhancePromptCheckBox = new QCheckBox("enhance_prompt");
    enhancePromptCheckBox->setStyleSheet("color: white; font-size: 12px;");
    enhancePromptCheckBox->setChecked(true);
    enhanceLayout->addWidget(enhancePromptLabel);
    enhanceLayout->addWidget(enhancePromptCheckBox);

    QVBoxLayout *upsampleLayout = new QVBoxLayout();
    enableUpsampleLabel = new QLabel("超分辨率");
    enableUpsampleLabel->setStyleSheet("font-size: 14px;");
    enableUpsampleCheckBox = new QCheckBox("enable_upsample");
    enableUpsampleCheckBox->setStyleSheet("color: white; font-size: 12px;");
    enableUpsampleCheckBox->setChecked(true);
    upsampleLayout->addWidget(enableUpsampleLabel);
    upsampleLayout->addWidget(enableUpsampleCheckBox);

    enhancePromptLabel->setVisible(false);
    enhancePromptCheckBox->setVisible(false);
    enableUpsampleLabel->setVisible(false);
    enableUpsampleCheckBox->setVisible(false);

    paramsLayout->addLayout(resLayout);
    paramsLayout->addLayout(durLayout);
    paramsLayout->addLayout(sizeLayout);
    paramsLayout->addLayout(watermarkLayout);
    paramsLayout->addLayout(enhanceLayout);
    paramsLayout->addLayout(upsampleLayout);
    paramsLayout->addStretch();

    contentLayout->addLayout(paramsLayout);

    // resolutionCombo 已在 paramsLayout 中创建，现在加入 wanDurationLayout
    wanDurationLayout->insertWidget(3, resolutionCombo);

    // WAN 参数容器（默认隐藏）
    wanParamsWidget = new QWidget();
    QVBoxLayout *wanParamsLayout = new QVBoxLayout(wanParamsWidget);
    wanParamsLayout->setContentsMargins(0, 0, 0, 0);
    wanParamsLayout->setSpacing(10);

    // WAN 反向提示词
    QHBoxLayout *wanNegPromptLayout = new QHBoxLayout();
    wanNegativePromptLabel = new QLabel("反向提示词:");
    wanNegativePromptLabel->setStyleSheet("font-size: 14px;");
    wanNegativePromptInput = new QTextEdit();
    wanNegativePromptInput->setPlaceholderText("输入不希望出现的元素...");
    wanNegativePromptInput->setMaximumHeight(60);
    wanNegativePromptInput->setStyleSheet("font-size: 13px;");
    wanNegPromptLayout->addWidget(wanNegativePromptLabel);
    wanNegPromptLayout->addWidget(wanNegativePromptInput, 1);
    wanParamsLayout->addLayout(wanNegPromptLayout);

    // WAN 特效模板
    QHBoxLayout *wanTemplateLayout = new QHBoxLayout();
    wanTemplateLabel = new QLabel("特效模板:");
    wanTemplateLabel->setStyleSheet("font-size: 14px;");
    wanTemplateCombo = new QComboBox();
    wanTemplateCombo->addItem("无", "");
    wanTemplateCombo->addItem("squish（解压捏捏）", "squish");
    wanTemplateCombo->addItem("rotation（转圈圈）", "rotation");
    wanTemplateCombo->addItem("poke（戳戳乐）", "poke");
    wanTemplateCombo->addItem("inflate（气球膨胀）", "inflate");
    wanTemplateCombo->addItem("dissolve（分子扩散）", "dissolve");
    wanTemplateCombo->addItem("melt（热浪融化）", "melt");
    wanTemplateCombo->addItem("icecream（冰淇淋星球）", "icecream");
    wanTemplateCombo->addItem("carousel（时光木马）", "carousel");
    wanTemplateCombo->addItem("singleheart（爱你哟）", "singleheart");
    wanTemplateCombo->addItem("dance1（摇摆时刻）", "dance1");
    wanTemplateCombo->addItem("dance2（头号甩舞）", "dance2");
    wanTemplateCombo->addItem("dance3（星摇时刻）", "dance3");
    wanTemplateCombo->addItem("dance4（指感节奏）", "dance4");
    wanTemplateCombo->addItem("dance5（舞动开关）", "dance5");
    wanTemplateCombo->addItem("mermaid（人鱼觉醒）", "mermaid");
    wanTemplateCombo->addItem("graduation（学术加冕）", "graduation");
    wanTemplateCombo->addItem("dragon（巨兽追袭）", "dragon");
    wanTemplateCombo->addItem("money（财从天降）", "money");
    wanTemplateCombo->addItem("jellyfish（水母之约）", "jellyfish");
    wanTemplateCombo->addItem("pupil（瞳孔穿越）", "pupil");
    wanTemplateCombo->addItem("flying（魔法悬浮）", "flying");
    wanTemplateCombo->addItem("rose（赠人玫瑰）", "rose");
    wanTemplateCombo->addItem("crystalrose（闪亮玫瑰）", "crystalrose");
    wanTemplateCombo->addItem("hug（爱的抱抱）", "hug");
    wanTemplateCombo->addItem("frenchkiss（唇齿相依）", "frenchkiss");
    wanTemplateCombo->addItem("coupleheart（双倍心动）", "coupleheart");
    wanTemplateCombo->addItem("hanfu-1（唐韵翩然）", "hanfu-1");
    wanTemplateCombo->addItem("solaron（机甲变身）", "solaron");
    wanTemplateCombo->addItem("magazine（闪耀封面）", "magazine");
    wanTemplateCombo->addItem("mech1（机械觉醒）", "mech1");
    wanTemplateCombo->addItem("mech2（赛博登场）", "mech2");
    wanTemplateLayout->addWidget(wanTemplateLabel);
    wanTemplateLayout->addWidget(wanTemplateCombo, 1);
    wanParamsLayout->addLayout(wanTemplateLayout);

    // WAN Prompt智能改写 + 随机种子
    QHBoxLayout *wanOptionLayout = new QHBoxLayout();
    wanPromptExtendCheckBox = new QCheckBox("Prompt智能改写");
    wanPromptExtendCheckBox->setStyleSheet("color: white; font-size: 13px;");
    wanPromptExtendCheckBox->setChecked(true);
    wanSeedLabel = new QLabel("随机种子:");
    wanSeedLabel->setStyleSheet("font-size: 14px;");
    wanSeedInput = new QSpinBox();
    wanSeedInput->setRange(0, 999999999);
    wanSeedInput->setValue(0);
    wanSeedInput->setMaximumWidth(150);
    wanSeedInput->setStyleSheet(
        "QSpinBox { color: white; background-color: #2a2a2a; border: 1px solid #555; border-radius: 4px; padding: 2px 4px; font-size: 13px; } "
        "QSpinBox::up-button, QSpinBox::down-button { background-color: #444; width: 16px; } "
        "QSpinBox::up-arrow, QSpinBox::down-arrow { border-color: white; }");
    wanOptionLayout->addWidget(wanPromptExtendCheckBox);
    wanOptionLayout->addSpacing(20);
    wanOptionLayout->addWidget(wanSeedLabel);
    wanOptionLayout->addWidget(wanSeedInput);
    wanOptionLayout->addStretch();
    wanParamsLayout->addLayout(wanOptionLayout);

    // WAN 音频上传（仅 wan2.5 支持）
    wanAudioUploadWidget = new QWidget();
    wanAudioUploadWidget->setMinimumHeight(36);
    QHBoxLayout *wanAudioLayout = new QHBoxLayout(wanAudioUploadWidget);
    wanAudioLayout->setContentsMargins(0, 0, 0, 0);
    wanAudioLayout->setSpacing(10);
    wanAudioCheckBox = new QCheckBox("为视频添加音频（仅wan2.5）");
    wanAudioCheckBox->setStyleSheet("color: white; font-size: 13px;");
    wanAudioCheckBox->setChecked(false);
    wanAudioUploadButton = new QPushButton("🎵 上传音频");
    wanAudioUploadButton->setFixedWidth(120);
    connect(wanAudioUploadButton, &QPushButton::clicked, this, &VideoSingleTab::uploadWanAudio);
    wanAudioFileLabel = new QLabel("未选择音频文件");
    wanAudioFileLabel->setStyleSheet("font-size: 12px; color: #888;");
    wanAudioFileLabel->setWordWrap(true);
    clearWanAudioButton = new QPushButton("🗑️ 清空");
    clearWanAudioButton->setFixedWidth(80);
    clearWanAudioButton->setVisible(false);
    connect(clearWanAudioButton, &QPushButton::clicked, this, &VideoSingleTab::clearWanAudio);
    wanAudioLayout->addWidget(wanAudioCheckBox);
    wanAudioLayout->addWidget(wanAudioUploadButton);
    wanAudioLayout->addWidget(wanAudioFileLabel, 1);
    wanAudioLayout->addWidget(clearWanAudioButton);
    wanParamsLayout->addWidget(wanAudioUploadWidget);
    wanAudioUploadWidget->setVisible(false);

    // WAN 保留水印
    QHBoxLayout *wanWatermarkLayout = new QHBoxLayout();
    wanWatermarkCheckBox = new QCheckBox("保留水印");
    wanWatermarkCheckBox->setStyleSheet("color: white; font-size: 13px;");
    wanWatermarkCheckBox->setChecked(false);
    wanWatermarkLayout->addWidget(wanWatermarkCheckBox);
    wanWatermarkLayout->addStretch();
    wanParamsLayout->addLayout(wanWatermarkLayout);

    // wanParamsWidget 和 wanPromptLabel 插入到 promptHeaderLayout 之前（WAN 时显示）
    int phIdx = contentLayout->indexOf(promptHeaderLayout);
    contentLayout->insertWidget(phIdx, wanParamsWidget);   // WAN 参数（negative_prompt、template 等）
    contentLayout->insertWidget(phIdx, wanPromptLabel);   // "正向提示词:" 标签
    wanParamsWidget->setVisible(false);

    // 预览区域
    previewLabel = new QLabel();
    previewLabel->setObjectName("videoPreviewLabel");
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setText("💡 生成结果将在【生成历史记录】");
    previewLabel->setMinimumHeight(150);
    contentLayout->addWidget(previewLabel);

    // 按钮布局（生成按钮和重置按钮并排）
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);

    generateButton = new QPushButton("🚀 生成视频");
    generateButton->setCursor(Qt::PointingHandCursor);
    connect(generateButton, &QPushButton::clicked, this, &VideoSingleTab::generateVideo);
    buttonLayout->addWidget(generateButton);

    resetButton = new QPushButton("🔄 一键重置");
    resetButton->setCursor(Qt::PointingHandCursor);
    connect(resetButton, &QPushButton::clicked, this, &VideoSingleTab::resetForm);
    buttonLayout->addWidget(resetButton);

    contentLayout->addLayout(buttonLayout);

    // 设置滚动区域
    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);

    // 初始化UI状态（默认 VEO3 Variant 1）
    variantTypeWidget->setVisible(true);
    onVariantTypeChanged();
}

void VideoSingleTab::connectSignals()
{
    // 连接参数变化信号 - 用于重复提交检测
    connect(promptInput, &QTextEdit::textChanged, this, &VideoSingleTab::onAnyParameterChanged);
    connect(modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::onAnyParameterChanged);
    connect(modelVariantCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::onAnyParameterChanged);
    connect(resolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::onAnyParameterChanged);
    connect(watermarkCheckBox, &QCheckBox::checkStateChanged, this, &VideoSingleTab::onAnyParameterChanged);
    connect(durationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::onAnyParameterChanged);

    // 连接参数变化信号 - 自动保存设置
    connect(promptInput, &QTextEdit::textChanged, this, &VideoSingleTab::queueSaveSettings);
    connect(modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::queueSaveSettings);
    connect(modelVariantCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::queueSaveSettings);
    connect(apiKeyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::queueSaveSettings);
    connect(serverCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::queueSaveSettings);
    connect(resolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::queueSaveSettings);
    connect(watermarkCheckBox, &QCheckBox::checkStateChanged, this, &VideoSingleTab::queueSaveSettings);
    connect(durationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::queueSaveSettings);
    connect(enhancePromptCheckBox, &QCheckBox::checkStateChanged, this, &VideoSingleTab::queueSaveSettings);
    connect(enableUpsampleCheckBox, &QCheckBox::checkStateChanged, this, &VideoSingleTab::queueSaveSettings);

    // 单向同步：生成页当前 key 值 -> 历史页查询 key
    connect(apiKeyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        QString selectedApiKeyValue;
        int keyId = apiKeyCombo->currentData().toInt();
        if (keyId > 0) {
            const ApiKey key = DBManager::instance()->getApiKey(keyId);
            selectedApiKeyValue = key.apiKey;
        }

        emit apiKeySelectionChanged(selectedApiKeyValue);
    });

    // 单选按钮切换
    connect(variantType1Radio, &QRadioButton::toggled, this, &VideoSingleTab::onVariantTypeChanged);

    // WAN 参数信号连接
    connect(wanNegativePromptInput, &QTextEdit::textChanged, this, &VideoSingleTab::queueSaveSettings);
    connect(wanTemplateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::queueSaveSettings);
    connect(wanPromptExtendCheckBox, &QCheckBox::checkStateChanged, this, &VideoSingleTab::queueSaveSettings);
    connect(wanSeedInput, &QSpinBox::valueChanged, this, &VideoSingleTab::queueSaveSettings);
    connect(wanAudioCheckBox, &QCheckBox::checkStateChanged, this, &VideoSingleTab::queueSaveSettings);
    connect(wanDurationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::queueSaveSettings);
    connect(wanResolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::queueSaveSettings);
    connect(wanWatermarkCheckBox, &QCheckBox::checkStateChanged, this, &VideoSingleTab::queueSaveSettings);
}

void VideoSingleTab::loadApiKeys()
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

void VideoSingleTab::refreshApiKeys()
{
    loadApiKeys();
}

void VideoSingleTab::resetForm()
{
    // 清空提示词
    promptInput->clear();

    // 清空首帧图片
    uploadedImagePaths.clear();
    imagePreviewLabel->clear();
    imagePreviewLabel->setText("📷 未上传图片");
    imagePreviewLabel->setAlignment(Qt::AlignCenter);

    // 清空尾帧图片
    uploadedEndFrameImagePath.clear();
    if (endFramePreviewLabel) {
        endFramePreviewLabel->clear();
        endFramePreviewLabel->setText("📷 未上传尾帧图片");
        endFramePreviewLabel->setAlignment(Qt::AlignCenter);
    }

    // 清空预览区域
    previewLabel->setText("💡 生成结果将在【生成历史记录】");

    // 标记参数已修改
    parametersModified = true;
}

void VideoSingleTab::onModelChanged(int index)
{
    Q_UNUSED(index);
    suppressAutoSave = true;
    QString modelName = modelCombo->currentText();

    // 根据模型类型切换UI
    if (modelName.contains("Grok", Qt::CaseInsensitive)) {
        // Grok模型：切换到Grok参数
        // 1. 更新模型变体下拉列表（block信号避免触发onModelVariantChanged时data为空）
        modelVariantCombo->blockSignals(true);
        modelVariantCombo->clear();
        modelVariantCombo->addItem("grok-video-3-15s（15秒）", "grok-video-3-15s");
        modelVariantCombo->addItem("grok-video-3-10s（10秒）", "grok-video-3-10s");
        modelVariantCombo->addItem("grok-video-3（6秒）", "grok-video-3");
        modelVariantCombo->blockSignals(false);

        // 2. 隐藏VEO3专用参数
        durationCombo->setVisible(false);
        durationLabel->setVisible(false);
        watermarkCheckBox->setVisible(false);
        watermarkLabel->setVisible(false);
        variantTypeWidget->setVisible(false);
        enhancePromptCheckBox->setVisible(false);
        enhancePromptLabel->setVisible(false);
        enableUpsampleCheckBox->setVisible(false);
        enableUpsampleLabel->setVisible(false);

        // 3. 隐藏 WAN 参数
        wanParamsWidget->setVisible(false);
        wanDurationLabel->setVisible(false);
        wanDurationCombo->setVisible(false);
        wanResolutionLabel->setVisible(false);
        wanResolutionCombo->setVisible(false);
        wanPromptLabel->setVisible(false);
        promptLabel->setVisible(true);

        // 4. 修改分辨率标签和选项为宽高比
        resolutionLabel->setText("宽高比");
        resolutionCombo->clear();
        resolutionCombo->addItem("2:3（竖屏）", "2:3");
        resolutionCombo->addItem("3:2（横屏）", "3:2");
        resolutionCombo->addItem("1:1（方形）", "1:1");

        // 5. 显示Grok专用的size参数
        sizeCombo->setVisible(true);
        sizeLabel->setVisible(true);

    } else if (modelName.contains("VEO3", Qt::CaseInsensitive)) {
        // VEO3模型：显示单选按钮，默认 Variant 1
        variantTypeWidget->setVisible(true);
        variantType1Radio->setChecked(true);

        // 隐藏Grok专用参数
        sizeCombo->setVisible(false);
        sizeLabel->setVisible(false);

        // 隐藏 WAN 参数
        wanParamsWidget->setVisible(false);
        wanDurationLabel->setVisible(false);
        wanDurationCombo->setVisible(false);
        wanResolutionLabel->setVisible(false);
        wanResolutionCombo->setVisible(false);
        wanPromptLabel->setVisible(false);
        promptLabel->setVisible(true);

        // 触发 onVariantTypeChanged 更新变体列表和控件
        onVariantTypeChanged();

    } else if (modelName.contains("wan", Qt::CaseInsensitive)) {
        // WAN 模型：显示 WAN 参数
        modelVariantCombo->blockSignals(true);
        modelVariantCombo->clear();
        modelVariantCombo->addItem("wan2.5-i2v-preview（预览版）", "wan2.5-i2v-preview");
        modelVariantCombo->addItem("wan2.6-i2v-flash（快速版）", "wan2.6-i2v-flash");
        modelVariantCombo->addItem("wan2.6-i2v（正式版）", "wan2.6-i2v");
        modelVariantCombo->blockSignals(false);

        // 显示 WAN 参数控件
        wanParamsWidget->setVisible(true);
        wanDurationLabel->setVisible(true);
        wanDurationCombo->setVisible(true);
        wanResolutionLabel->setVisible(true);
        wanResolutionCombo->setVisible(true);
        resolutionCombo->setVisible(false);  // VEO3/Grok 的 combo 隐藏
        wanPromptLabel->setVisible(true);
        promptLabel->setVisible(false);

        // 隐藏其他模型参数
        durationCombo->setVisible(false);
        durationLabel->setVisible(false);
        resolutionLabel->setVisible(false);
        watermarkCheckBox->setVisible(false);
        watermarkLabel->setVisible(false);
        variantTypeWidget->setVisible(false);
        enhancePromptCheckBox->setVisible(false);
        enhancePromptLabel->setVisible(false);
        enableUpsampleCheckBox->setVisible(false);
        enableUpsampleLabel->setVisible(false);
        sizeCombo->setVisible(false);
        sizeLabel->setVisible(false);

        // 根据变体更新音频控件可见性
        onModelVariantChanged(0);

    } else {
        // 其他模型：恢复VEO3默认参数
        modelVariantCombo->blockSignals(true);
        modelVariantCombo->clear();
        modelVariantCombo->addItem("veo_3_1-fast", "veo_3_1-fast");
        modelVariantCombo->addItem("veo_3_1", "veo_3_1");
        modelVariantCombo->addItem("veo_3_1-fast-4K", "veo_3_1-fast-4K");
        modelVariantCombo->addItem("veo_3_1-fast-components-4K", "veo_3_1-fast-components-4K");
        modelVariantCombo->blockSignals(false);

        durationCombo->setVisible(true);
        durationLabel->setVisible(true);
        watermarkCheckBox->setVisible(true);
        watermarkLabel->setVisible(true);
        variantTypeWidget->setVisible(false);
        enhancePromptCheckBox->setVisible(false);
        enhancePromptLabel->setVisible(false);
        enableUpsampleCheckBox->setVisible(false);
        enableUpsampleLabel->setVisible(false);

        // 隐藏 WAN 参数
        wanParamsWidget->setVisible(false);
        wanDurationLabel->setVisible(false);
        wanDurationCombo->setVisible(false);
        wanResolutionLabel->setVisible(false);
        wanResolutionCombo->setVisible(false);
        wanPromptLabel->setVisible(false);
        promptLabel->setVisible(true);

        // resolutionCombo 在 resLayout 中，不需要显式 show
        resolutionLabel->setText("分辨率");
        resolutionCombo->clear();
        resolutionCombo->addItem("1280x720 (横屏)", "1280x720");
        resolutionCombo->addItem("720x1280 (竖屏)", "720x1280");

        sizeCombo->setVisible(false);
        sizeLabel->setVisible(false);
    }

    // 模型切换时重新加载对应的密钥
    loadApiKeys();

    // 更新图片上传UI
    QString currentVariant = modelVariantCombo->currentData().toString();
    updateImageUploadUI(currentVariant);

    suppressAutoSave = false;
    if (!modelCombo->signalsBlocked()) {
        queueSaveSettings();
    }
}

void VideoSingleTab::onVariantTypeChanged()
{
    bool isVariant1 = variantType1Radio->isChecked();

    modelVariantCombo->blockSignals(true);
    modelVariantCombo->clear();

    if (isVariant1) {
        // Variant 1: OpenAI格式
        modelVariantCombo->addItem("veo_3_1-fast", "veo_3_1-fast");
        modelVariantCombo->addItem("veo_3_1", "veo_3_1");
        modelVariantCombo->addItem("veo_3_1-fast-4K", "veo_3_1-fast-4K");
        modelVariantCombo->addItem("veo_3_1-fast-components-4K", "veo_3_1-fast-components-4K");

        durationCombo->setVisible(true);
        durationLabel->setVisible(true);
        watermarkCheckBox->setVisible(true);
        watermarkLabel->setVisible(true);
        enhancePromptCheckBox->setVisible(false);
        enhancePromptLabel->setVisible(false);
        enableUpsampleCheckBox->setVisible(false);
        enableUpsampleLabel->setVisible(false);
        resolutionLabel->setText("分辨率");
    } else {
        // Variant 2: 统一格式
        modelVariantCombo->addItem("veo3.1-fast", "veo3.1-fast");
        modelVariantCombo->addItem("veo3.1", "veo3.1");
        modelVariantCombo->addItem("veo3.1-fast-components", "veo3.1-fast-components");
        modelVariantCombo->addItem("veo3.1-components", "veo3.1-components");
        modelVariantCombo->addItem("veo3.1-4k", "veo3.1-4k");
        modelVariantCombo->addItem("veo3.1-components-4k", "veo3.1-components-4k");
        modelVariantCombo->addItem("veo3-pro-frames", "veo3-pro-frames");
        modelVariantCombo->addItem("veo3.1-pro", "veo3.1-pro");
        modelVariantCombo->addItem("veo3.1-pro-4k", "veo3.1-pro-4k");

        durationCombo->setVisible(false);
        durationLabel->setVisible(false);
        watermarkCheckBox->setVisible(false);
        watermarkLabel->setVisible(false);
        enhancePromptCheckBox->setVisible(true);
        enhancePromptLabel->setVisible(true);
        enableUpsampleCheckBox->setVisible(true);
        enableUpsampleLabel->setVisible(true);
        resolutionLabel->setText("宽高比");
    }

    modelVariantCombo->blockSignals(false);

    // 触发变体变化以更新图片上传UI和分辨率
    onModelVariantChanged(0);
}

QStringList VideoSingleTab::filterValidImagePaths() const
{
    QStringList validPaths;
    for (const QString& path : uploadedImagePaths) {
        if (!path.isEmpty()) {
            validPaths.append(path);
        }
    }
    if (!uploadedMiddleFrameImagePath.isEmpty()) {
        validPaths.append(uploadedMiddleFrameImagePath);
    }
    if (!uploadedEndFrameImagePath.isEmpty()) {
        validPaths.append(uploadedEndFrameImagePath);
    }
    return validPaths;
}

bool VideoSingleTab::validateImgbbKey(QString &errorMsg) const
{
    ImgbbKey activeImgbbKey = DBManager::instance()->getActiveImgbbKey();
    if (activeImgbbKey.apiKey.isEmpty()) {
        errorMsg = "请先到设置页应用临时图床密钥";
        return false;
    }
    return true;
}

VideoSingleTab::ModelType VideoSingleTab::detectModelType(const QString &modelName) const
{
    if (modelName.contains("wan", Qt::CaseInsensitive)) {
        return ModelType::Wan;
    } else if (modelName.contains("grok", Qt::CaseInsensitive)) {
        return ModelType::Grok;
    }
    return ModelType::VEO3;
}

void VideoSingleTab::uploadMiddleFrameImage()
{
    // 如果已有图片，弹出确认对话框
    if (!uploadedMiddleFrameImagePath.isEmpty()) {
        int ret = QMessageBox::question(this, "重新选择图片",
            "当前已有图片，是否重新选择？",
            QMessageBox::Yes | QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
    }

    QString fileName = selectAndValidateImageFile("选择图片2（中间帧）", uploadedMiddleFrameImagePath.isEmpty());
    if (fileName.isEmpty()) {
        return;
    }

    QPixmap pixmap(fileName);
    uploadedMiddleFrameImagePath = fileName;
    QPixmap scaledPixmap = pixmap.scaled(200, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    middleFramePreviewLabel->setPixmap(scaledPixmap);
    middleFramePreviewLabel->setText("");
    middleFramePreviewLabel->setProperty("hasImage", true);
    middleFramePreviewLabel->style()->unpolish(middleFramePreviewLabel);
    middleFramePreviewLabel->style()->polish(middleFramePreviewLabel);

    queueSaveSettings();
}


void VideoSingleTab::uploadImage()
{
    QString modelName = modelVariantCombo->currentData().toString();
    bool isComponents = modelName.contains("components");
    bool isFrames = modelName.contains("frames");
    bool isGrok = modelName.contains("grok", Qt::CaseInsensitive);

    // Grok模型：直接替换图片1
    if (isGrok) {
        QString fileName = selectAndValidateImageFile("选择图片1", false);
        if (fileName.isEmpty()) return;

        QPixmap pixmap(fileName);

        // 确保 uploadedImagePaths 有足够空间
        while (uploadedImagePaths.size() < 3) {
            uploadedImagePaths.append(QString());
        }
        uploadedImagePaths[0] = fileName;

        QPixmap scaledPixmap = pixmap.scaled(200, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        imagePreviewLabel->setPixmap(scaledPixmap);
        imagePreviewLabel->setText("");
        imagePreviewLabel->setProperty("hasImage", true);
        imagePreviewLabel->style()->unpolish(imagePreviewLabel);
        imagePreviewLabel->style()->polish(imagePreviewLabel);

        queueSaveSettings();
        return;
    }

    // 其他模型：原有逻辑
    // 已有图片时确认替换后清空
    if (!uploadedImagePaths.isEmpty()) {
        int ret = QMessageBox::question(this, "重新选择图片",
            isFrames ? "当前已有首帧图片，是否重新选择？" : "当前已有图片，是否重新选择？",
            QMessageBox::Yes | QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
        uploadedImagePaths.clear();
    }

    QString fileName = selectAndValidateImageFile("选择图片1", uploadedImagePaths.isEmpty());
    if (fileName.isEmpty()) {
        return;
    }

    uploadedImagePaths.append(fileName);
    updateImagePreview();
    queueSaveSettings();
}

void VideoSingleTab::removeImage(int index)
{
    if (index >= 0 && index < uploadedImagePaths.size()) {
        uploadedImagePaths.removeAt(index);
        updateImagePreview();
        queueSaveSettings();  // 保存更改
    }
}

void VideoSingleTab::uploadGrokImage2()
{
    QString fileName = selectAndValidateImageFile("选择图片2", false);
    if (fileName.isEmpty()) return;

    QPixmap pixmap(fileName);

    // 确保 uploadedImagePaths 有足够空间
    while (uploadedImagePaths.size() < 3) {
        uploadedImagePaths.append(QString());
    }
    uploadedImagePaths[1] = fileName;

    QPixmap scaledPixmap = pixmap.scaled(200, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    grokImage2PreviewLabel->setPixmap(scaledPixmap);
    grokImage2PreviewLabel->setText("");
    grokImage2PreviewLabel->setProperty("hasImage", true);
    grokImage2PreviewLabel->style()->unpolish(grokImage2PreviewLabel);
    grokImage2PreviewLabel->style()->polish(grokImage2PreviewLabel);

    queueSaveSettings();
}

void VideoSingleTab::uploadGrokImage3()
{
    QString fileName = selectAndValidateImageFile("选择图片3", false);
    if (fileName.isEmpty()) return;

    QPixmap pixmap(fileName);

    // 确保 uploadedImagePaths 有足够空间
    while (uploadedImagePaths.size() < 3) {
        uploadedImagePaths.append(QString());
    }
    uploadedImagePaths[2] = fileName;

    QPixmap scaledPixmap = pixmap.scaled(200, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    grokImage3PreviewLabel->setPixmap(scaledPixmap);
    grokImage3PreviewLabel->setText("");
    grokImage3PreviewLabel->setProperty("hasImage", true);
    grokImage3PreviewLabel->style()->unpolish(grokImage3PreviewLabel);
    grokImage3PreviewLabel->style()->polish(grokImage3PreviewLabel);

    queueSaveSettings();
}

void VideoSingleTab::clearGrokImage(int index)
{
    if (index < 0 || index >= uploadedImagePaths.size()) return;

    uploadedImagePaths[index] = QString();

    QLabel *previewLabel = nullptr;
    if (index == 0) {
        previewLabel = imagePreviewLabel;
    } else if (index == 1) {
        previewLabel = grokImage2PreviewLabel;
    } else if (index == 2) {
        previewLabel = grokImage3PreviewLabel;
    }

    if (previewLabel) {
        previewLabel->setText("未选择图片\n点击此处上传");
        previewLabel->setPixmap(QPixmap());
        previewLabel->setProperty("hasImage", false);
        previewLabel->style()->unpolish(previewLabel);
        previewLabel->style()->polish(previewLabel);
    }

    queueSaveSettings();
}

void VideoSingleTab::uploadWanAudio()
{
    if (wanAudioUploading) return;

    QString filePath = QFileDialog::getOpenFileName(this, "选择音频文件",
        QString(), "音频文件 (*.mp3 *.wav *.m4a *.aac *.ogg);;所有文件 (*.*)");
    if (filePath.isEmpty()) return;

    QFileInfo fi(filePath);
    if (fi.size() > 100 * 1024 * 1024) {
        QMessageBox::warning(this, "提示", "音频文件不能超过100MB");
        return;
    }

    uploadedWanAudioPath = filePath;
    wanAudioFileLabel->setText("✓ " + fi.fileName());
    wanAudioFileLabel->setStyleSheet("font-size: 12px; color: #4CAF50;");
    clearWanAudioButton->setVisible(true);

    // 上传到 tmpfile.link
    wanAudioUploading = true;
    ImageUploader *uploader = veo3API->getImageUploader();
    connect(uploader, &ImageUploader::audioUploadSuccess, this, [this, uploader](const QString &url) {
        uploadedWanAudioUrl = url;
        wanAudioUploading = false;
        qDebug() << "[WAN] Audio uploaded:" << url;
        disconnect(uploader, &ImageUploader::audioUploadSuccess, this, nullptr);
    });
    connect(uploader, &ImageUploader::uploadError, this, [this, uploader](const QString &error) {
        wanAudioUploading = false;
        QMessageBox::warning(this, "上传失败", error);
        disconnect(uploader, &ImageUploader::uploadError, this, nullptr);
    });
    uploader->uploadAudioToTmpfile(filePath);

    queueSaveSettings();
}

void VideoSingleTab::clearWanAudio()
{
    uploadedWanAudioPath.clear();
    uploadedWanAudioUrl.clear();
    wanAudioUploading = false;
    wanAudioFileLabel->setText("未选择音频文件");
    wanAudioFileLabel->setStyleSheet("font-size: 12px; color: #888;");
    clearWanAudioButton->setVisible(false);
    queueSaveSettings();
}

void VideoSingleTab::updateImagePreview()
{
    if (uploadedImagePaths.isEmpty()) {
        imagePreviewLabel->setText("未选择图片\n点击此处上传");
        imagePreviewLabel->setPixmap(QPixmap());
        imagePreviewLabel->setProperty("hasImage", false);
    } else {
        // 显示第一张图片的缩略图
        QPixmap pixmap(uploadedImagePaths[0]);
        if (!pixmap.isNull()) {
            QPixmap scaledPixmap = pixmap.scaled(200, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            imagePreviewLabel->setPixmap(scaledPixmap);
            imagePreviewLabel->setText("");
        } else {
            imagePreviewLabel->setPixmap(QPixmap());
            QString text = QString("✓ 已选择 %1 张图片\n").arg(uploadedImagePaths.size());
            for (int i = 0; i < uploadedImagePaths.size(); ++i) {
                QFileInfo fileInfo(uploadedImagePaths[i]);
                text += QString("%1. %2\n").arg(i + 1).arg(fileInfo.fileName());
            }
            imagePreviewLabel->setText(text.trimmed());
        }
        imagePreviewLabel->setProperty("hasImage", true);
    }
    // 强制刷新样式
    imagePreviewLabel->style()->unpolish(imagePreviewLabel);
    imagePreviewLabel->style()->polish(imagePreviewLabel);

    // 更新图2预览
    if (uploadedImagePaths.size() >= 2 && !uploadedImagePaths[1].isEmpty()) {
        QPixmap pixmap2(uploadedImagePaths[1]);
        if (!pixmap2.isNull()) {
            QPixmap scaledPixmap = pixmap2.scaled(200, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            grokImage2PreviewLabel->setPixmap(scaledPixmap);
            grokImage2PreviewLabel->setText("");
        } else {
            grokImage2PreviewLabel->setPixmap(QPixmap());
            QFileInfo fileInfo(uploadedImagePaths[1]);
            grokImage2PreviewLabel->setText("✓ " + fileInfo.fileName());
        }
        grokImage2PreviewLabel->setProperty("hasImage", true);
        grokImage2PreviewLabel->style()->unpolish(grokImage2PreviewLabel);
        grokImage2PreviewLabel->style()->polish(grokImage2PreviewLabel);
    } else {
        grokImage2PreviewLabel->setText("未选择图片\n点击此处上传");
        grokImage2PreviewLabel->setPixmap(QPixmap());
        grokImage2PreviewLabel->setProperty("hasImage", false);
        grokImage2PreviewLabel->style()->unpolish(grokImage2PreviewLabel);
        grokImage2PreviewLabel->style()->polish(grokImage2PreviewLabel);
    }

    // 更新图3预览
    if (uploadedImagePaths.size() >= 3 && !uploadedImagePaths[2].isEmpty()) {
        QPixmap pixmap3(uploadedImagePaths[2]);
        if (!pixmap3.isNull()) {
            QPixmap scaledPixmap = pixmap3.scaled(200, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            grokImage3PreviewLabel->setPixmap(scaledPixmap);
            grokImage3PreviewLabel->setText("");
        } else {
            grokImage3PreviewLabel->setPixmap(QPixmap());
            QFileInfo fileInfo(uploadedImagePaths[2]);
            grokImage3PreviewLabel->setText("✓ " + fileInfo.fileName());
        }
        grokImage3PreviewLabel->setProperty("hasImage", true);
        grokImage3PreviewLabel->style()->unpolish(grokImage3PreviewLabel);
        grokImage3PreviewLabel->style()->polish(grokImage3PreviewLabel);
    } else {
        grokImage3PreviewLabel->setText("未选择图片\n点击此处上传");
        grokImage3PreviewLabel->setPixmap(QPixmap());
        grokImage3PreviewLabel->setProperty("hasImage", false);
        grokImage3PreviewLabel->style()->unpolish(grokImage3PreviewLabel);
        grokImage3PreviewLabel->style()->polish(grokImage3PreviewLabel);
    }
}

void VideoSingleTab::uploadEndFrameImage()
{
    // 如果已有图片，弹出确认对话框
    if (!uploadedEndFrameImagePath.isEmpty()) {
        int ret = QMessageBox::question(this, "重新选择图片",
            "当前已有尾帧图片，是否重新选择？",
            QMessageBox::Yes | QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
    }

    QString fileName = selectAndValidateImageFile("选择尾帧图片", uploadedEndFrameImagePath.isEmpty());
    if (fileName.isEmpty()) {
        return;
    }

    QPixmap pixmap(fileName);
    uploadedEndFrameImagePath = fileName;
    QPixmap scaledPixmap = pixmap.scaled(200, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    endFramePreviewLabel->setPixmap(scaledPixmap);
    endFramePreviewLabel->setText("");
    endFramePreviewLabel->setProperty("hasImage", true);
    endFramePreviewLabel->style()->unpolish(endFramePreviewLabel);
    endFramePreviewLabel->style()->polish(endFramePreviewLabel);

    queueSaveSettings();
}

void VideoSingleTab::onModelVariantChanged(int index)
{
    QString modelVariant = modelVariantCombo->currentData().toString();
    QString fullModelName = modelCombo->currentText();
    updateImageUploadUI(modelVariant);
    
    bool isGrok = modelVariant.contains("grok", Qt::CaseInsensitive);
    bool isWan = fullModelName.contains("wan", Qt::CaseInsensitive);
    
    if (isGrok) {
        grokImage2Widget->setVisible(true);
        grokImage3Widget->setVisible(true);
        imageUploadHintLabel->setVisible(true);
        wanParamsWidget->setVisible(false);
    } else if (isWan) {
        wanParamsWidget->setVisible(true);
        updateWanAudioWidgetVisibility(modelVariant);
    } else {
        wanParamsWidget->setVisible(false);
    }
    
    if (!isGrok) {
        bool is4K = modelVariant.contains("4K") || modelVariant.contains("4k");
        bool isVariant2 = variantType2Radio && variantType2Radio->isChecked();
        updateResolutionOptions(is4K, isVariant2);
    }
}

void VideoSingleTab::updateWanAudioWidgetVisibility(const QString &modelVariant)
{
    bool isWan25 = modelVariant.contains("wan2.5", Qt::CaseInsensitive);
    wanAudioUploadWidget->setVisible(isWan25);
    if (!isWan25) {
        clearWanAudio();
    }
    // macOS Qt: setVisible 不会自动触发父布局重新计算，需显式 invalidate + activate
    if (QLayout *l = wanParamsWidget->layout()) {
        l->invalidate();
        l->activate();
    }
}

void VideoSingleTab::updateImageUploadUI(const QString &modelName)
{
    bool isComponents = modelName.contains("components");
    bool isFrames = modelName.contains("frames");
    bool isGrok = modelName.contains("grok", Qt::CaseInsensitive);

    if (isGrok) {
        // Grok模型：显示3个独立上传组件
        imageLabel->setText("图片1（可选）:");
        uploadImageButton->setText("📁 选择图片1");
        clearImageButton->setVisible(true);
        endFrameWidget->setVisible(false);
        middleFrameWidget->setVisible(false);
        grokImage2Widget->setVisible(true);
        grokImage3Widget->setVisible(true);
        imageUploadHintLabel->setVisible(true);
    } else if (isComponents) {
        // components 变体：3 个独立上传控件，各传 1 张
        // 布局物理顺序：imageLabel → endFrame → middleFrame
        imageLabel->setText("图片1（可选）:");
        uploadImageButton->setText("📁 选择图片1");
        clearImageButton->setVisible(false);
        endFrameWidget->setVisible(true);
        endFrameLabel->setText("图片2（可选）:");
        uploadEndFrameButton->setText("📁 选择图片2");
        middleFrameWidget->setVisible(true);
        middleFrameLabel->setText("图片3（可选）:");
        uploadMiddleFrameButton->setText("📁 选择图片3");
        grokImage2Widget->setVisible(false);
        grokImage3Widget->setVisible(false);
        imageUploadHintLabel->setVisible(false);
    } else if (isFrames) {
        // 仅支持单张首帧
        imageLabel->setText("首帧图片（单张）:");
        uploadImageButton->setText("📁 选择首帧图片");
        clearImageButton->setVisible(false);
        endFrameWidget->setVisible(false);
        middleFrameWidget->setVisible(false);
        grokImage2Widget->setVisible(false);
        grokImage3Widget->setVisible(false);
        imageUploadHintLabel->setVisible(false);
    } else {
        // 支持首尾帧
        imageLabel->setText("首帧图片:");
        uploadImageButton->setText("📁 选择首帧图片");
        clearImageButton->setVisible(false);
        endFrameWidget->setVisible(true);
        endFrameLabel->setText("尾帧图片（可选）:");
        uploadEndFrameButton->setText("📁 选择尾帧图片");
        middleFrameWidget->setVisible(false);
        grokImage2Widget->setVisible(false);
        grokImage3Widget->setVisible(false);
        imageUploadHintLabel->setVisible(false);
    }
}

void VideoSingleTab::updateResolutionOptions(bool is4K, bool isVariant2)
{
    resolutionCombo->clear();
    if (isVariant2) {
        // Variant 2: 宽高比模式
        if (is4K) {
            resolutionCombo->addItem("横屏 16:9 (3840x2160)", "16:9");
            resolutionCombo->addItem("竖屏 9:16 (2160x3840)", "9:16");
        } else {
            resolutionCombo->addItem("横屏 16:9 (1280x720)", "16:9");
            resolutionCombo->addItem("竖屏 9:16 (720x1280)", "9:16");
        }
    } else {
        // Variant 1 / Grok: 像素值模式
        if (is4K) {
            resolutionCombo->addItem("横屏 16:9 (3840x2160)", "3840x2160");
            resolutionCombo->addItem("竖屏 9:16 (2160x3840)", "2160x3840");
        } else {
            resolutionCombo->addItem("横屏 16:9 (1280x720)", "1280x720");
            resolutionCombo->addItem("竖屏 9:16 (720x1280)", "720x1280");
        }
    }
}

void VideoSingleTab::generateVideo()
{
    QString prompt = promptInput->toPlainText().trimmed();
    if (prompt.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入视频生成提示词");
        return;
    }

    // 规范化图片引用：@图1 → @图0, @图片2 → @图片1
    prompt = normalizeImageReferences(prompt);

    // 检查是否有有效图片（过滤空字符串）
    bool hasValidImage = false;
    for (const QString& path : uploadedImagePaths) {
        if (!path.isEmpty()) {
            hasValidImage = true;
            break;
        }
    }
    if (!hasValidImage && uploadedMiddleFrameImagePath.isEmpty() && uploadedEndFrameImagePath.isEmpty()) {
        QString model = modelCombo->currentText();
        if (model.contains("Grok", Qt::CaseInsensitive)) {
            int ret = QMessageBox::question(this, "提示",
                "未选择图片，将为你进行文生视频",
                QMessageBox::Yes | QMessageBox::No);
            if (ret != QMessageBox::Yes) return;
        }
    }

    if (apiKeyCombo->count() == 0 || !apiKeyCombo->isEnabled()) {
        QMessageBox::warning(this, "提示", "请先添加 API 密钥");
        return;
    }

    // 检测重复提交
    if (!checkDuplicateSubmission()) {
        return;
    }

    QString model = modelCombo->currentText();
    QString modelVariant = modelVariantCombo->currentData().toString();
    QString modelVariantText = modelVariantCombo->currentText();
    QString server = serverCombo->currentData().toString();
    QString resolution = resolutionCombo->currentData().toString();
    QString duration = durationCombo->currentData().toString();
    bool watermark = watermarkCheckBox->isChecked();  // "保留水印"勾选=加水印
    int keyId = apiKeyCombo->currentData().toInt();

    // 获取 API Key
    ApiKey apiKeyData = DBManager::instance()->getApiKey(keyId);
    if (apiKeyData.apiKey.isEmpty()) {
        QMessageBox::warning(this, "错误", "无法获取 API 密钥");
        return;
    }

    // 生成临时 taskId
    QString tempTaskId = QString("temp_%1_%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(QRandomGenerator::global()->bounded(10000));

    // 构建任务对象
    VideoTask task;
    task.taskId = tempTaskId;
    task.taskType = "video_single";
    task.prompt = prompt;
    task.modelVariant = modelVariantText;
    task.modelName = model;
    task.apiKeyName = apiKeyCombo->currentText();
    task.serverUrl = server;
    task.status = "submitting";
    task.progress = 0;

    // 填充参数
    task.resolution = resolution;
    task.duration = durationCombo->currentText().remove("秒").toInt();
    task.watermark = watermark;

    // Grok专用参数
    if (model.contains("Grok", Qt::CaseInsensitive)) {
        task.aspectRatio = resolution;
        task.size = sizeCombo->currentData().toString();
    }

    // 序列化图片路径（过滤空字符串）
    QJsonArray imageArray;
    for (const QString& path : uploadedImagePaths) {
        if (!path.isEmpty()) {
            imageArray.append(path);
        }
    }
    task.imagePaths = QString::fromUtf8(QJsonDocument(imageArray).toJson(QJsonDocument::Compact));
    task.endFrameImagePath = uploadedEndFrameImagePath;

    // 插入数据库
    int dbId = DBManager::instance()->insertVideoTask(task);
    if (dbId < 0) {
        QMessageBox::critical(this, "错误", "无法保存任务记录");
        return;
    }

    currentTaskId = tempTaskId;

    // 非阻塞提示：API 调用中
    previewLabel->setText("⏳ 正在提交视频生成任务...");

    // 根据模型类型调用不同的API
    if (model.contains("Grok", Qt::CaseInsensitive)) {
        // Grok模型：校验imgbb密钥
        ImgbbKey activeImgbbKey = DBManager::instance()->getActiveImgbbKey();
        if (activeImgbbKey.apiKey.isEmpty()) {
            QMessageBox::warning(this, "提示", "请先到设置页应用临时图床密钥");
            return;
        }

        // Grok模型：传递aspectRatio和size参数
        QString aspectRatio = resolution; // Grok使用resolution字段存储aspectRatio
        QString size = sizeCombo->currentData().toString();

        // 从variant名提取duration：grok-video-3-15s→15, grok-video-3-10s→10, grok-video-3→6
        int grokDuration = 6;
        if (modelVariant.contains("15s")) grokDuration = 15;
        else if (modelVariant.contains("10s")) grokDuration = 10;

        // 过滤空字符串
        QStringList validImagePaths;
        for (const QString& path : uploadedImagePaths) {
            if (!path.isEmpty()) {
                validImagePaths.append(path);
            }
        }

        veo3API->createVideo(
            apiKeyData.apiKey,
            server,
            model,  // modelName
            modelVariant,
            prompt,
            validImagePaths,
            size,
            QString::number(grokDuration),  // seconds → duration
            false,  // watermark (Grok不需要)
            aspectRatio,  // aspectRatio
            activeImgbbKey.apiKey  // imgbbApiKey
        );
    } else if (model.contains("wan", Qt::CaseInsensitive)) {
        // WAN 模型：使用专用 API
        QString errorMsg;
        if (!validateImgbbKey(errorMsg)) {
            QMessageBox::warning(this, "提示", errorMsg);
            return;
        }

        QStringList validImagePaths = filterValidImagePaths();
        if (validImagePaths.isEmpty()) {
            QMessageBox::warning(this, "提示", "WAN 模型需要上传图片");
            return;
        }

        ImgbbKey activeImgbbKey = DBManager::instance()->getActiveImgbbKey();
        VideoAPI::WanVideoParams params;
        params.apiKey = apiKeyData.apiKey;
        params.baseUrl = server;
        params.model = modelVariant;
        params.prompt = prompt;
        params.negativePrompt = wanNegativePromptInput->toPlainText().trimmed();
        params.localImagePaths = validImagePaths;
        params.audioUrl = (wanAudioCheckBox->isChecked() && !uploadedWanAudioUrl.isEmpty())
                         ? uploadedWanAudioUrl : QString();
        params.templateName = wanTemplateCombo->currentData().toString();
        params.resolution = resolution;
        params.duration = wanDurationCombo->currentData().toInt();
        params.promptExtend = wanPromptExtendCheckBox->isChecked();
        params.watermark = wanWatermarkCheckBox->isChecked();
        params.seed = QString::number(wanSeedInput->value());

        veo3API->prepareWanRequest(params);
        veo3API->uploadImageToImgbb(validImagePaths.first(), activeImgbbKey.apiKey);
    } else if (variantType2Radio && variantType2Radio->isChecked()) {
        // VEO3 Variant 2 统一格式：校验imgbb密钥，上传图片后 JSON POST
        ImgbbKey activeImgbbKey = DBManager::instance()->getActiveImgbbKey();
        if (activeImgbbKey.apiKey.isEmpty()) {
            QMessageBox::warning(this, "提示", "请先到设置页应用临时图床密钥");
            return;
        }

        // 合并所有图片路径：首帧 + 中间帧 + 尾帧（过滤空字符串）
        QStringList allImagePaths;
        for (const QString& path : uploadedImagePaths) {
            if (!path.isEmpty()) {
                allImagePaths.append(path);
            }
        }
        if (!uploadedMiddleFrameImagePath.isEmpty()) {
            allImagePaths.append(uploadedMiddleFrameImagePath);
        }
        if (!uploadedEndFrameImagePath.isEmpty()) {
            allImagePaths.append(uploadedEndFrameImagePath);
        }

        bool enhancePrompt = enhancePromptCheckBox->isChecked();
        bool enableUpsample = enableUpsampleCheckBox->isChecked();

        veo3API->createVideo(
            apiKeyData.apiKey,
            server,
            model,  // modelName (不含 "Grok"，不以 "veo_" 开头)
            modelVariant,
            prompt,
            allImagePaths,
            "",  // size (Variant 2 不需要)
            "",  // seconds (Variant 2 不需要)
            false,  // watermark (Variant 2 不需要)
            resolution,  // aspectRatio
            activeImgbbKey.apiKey,
            enhancePrompt,
            enableUpsample
        );
    } else {
        // VEO3 Variant 1 OpenAI格式：合并所有图片路径（过滤空字符串）
        QStringList allImagePaths;
        for (const QString& path : uploadedImagePaths) {
            if (!path.isEmpty()) {
                allImagePaths.append(path);
            }
        }
        if (!uploadedMiddleFrameImagePath.isEmpty()) {
            allImagePaths.append(uploadedMiddleFrameImagePath);
        }
        if (!uploadedEndFrameImagePath.isEmpty()) {
            allImagePaths.append(uploadedEndFrameImagePath);
        }
        veo3API->createVideo(
            apiKeyData.apiKey,
            server,
            model,  // modelName
            modelVariant,
            prompt,
            allImagePaths,
            resolution,
            duration,
            watermark,
            ""  // aspectRatio (VEO3 Variant 1 不需要)
        );
    }

    // 注意：任务创建成功后会触发 onVideoCreated 回调
    // 在那里更新 taskId 并启动轮询
}

void VideoSingleTab::onVideoCreated(const QString &taskId, const QString &status)
{
    // 更新 taskId（从临时ID更新为真实ID）
    if (!currentTaskId.isEmpty() && currentTaskId != taskId) {
        DBManager::instance()->updateTaskId(currentTaskId, taskId);
    }

    currentTaskId = taskId;

    // 更新状态为 pending
    DBManager::instance()->updateTaskStatus(taskId, "pending", 0, "");

    // 获取当前参数
    int keyId = apiKeyCombo->currentData().toInt();
    ApiKey apiKeyData = DBManager::instance()->getApiKey(keyId);
    QString server = serverCombo->currentData().toString();

    // 启动轮询
    TaskPollManager::getInstance()->startPolling(taskId, "video_single", apiKeyData.apiKey, server,
                                                  modelCombo->currentText());

    // 保存当前参数哈希和设置
    lastSubmittedParamsHash = calculateParamsHash();
    parametersModified = false;
    saveSettings();

    // API 成功回调后弹窗，逻辑一致
    QMessageBox::information(this, "任务已提交",
        "视频生成任务已提交！\n\n"
        "请前往【生成历史记录】标签页查看任务状态。\n"
        "系统将自动轮询任务状态并下载完成的视频。");
    previewLabel->setText("💡 生成结果将在【生成历史记录】");

    // 不清空输入，保留参数供用户继续使用
    // 参数已通过 saveSettings() 自动保存
}

QString VideoSingleTab::normalizeImageReferences(const QString &prompt) const
{
    QString result = prompt;
    QRegularExpression re(R"(@(图片?)(\d+))");
    QRegularExpressionMatchIterator it = re.globalMatch(result);

    QList<QPair<int, int>> replacements;
    QStringList newTexts;

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        int num = match.captured(2).toInt();
        if (num >= 1 && num <= 3) {
            int newNum = num - 1;
            QString prefix = match.captured(1);
            QString newText = QString("@%1%2").arg(prefix).arg(newNum);
            replacements.prepend(qMakePair(match.capturedStart(), match.capturedLength()));
            newTexts.prepend(newText);
        }
    }

    for (int i = 0; i < replacements.size(); ++i) {
        result.replace(replacements[i].first, replacements[i].second, newTexts[i]);
    }

    return result;
}

bool VideoSingleTab::validateImageFile(const QString &filePath, QString &errorMsg) const
{
    QFileInfo fileInfo(filePath);

    if (!fileInfo.exists()) {
        errorMsg = "文件不存在";
        return false;
    }

    qint64 fileSize = fileInfo.size();
    if (fileSize > 10 * 1024 * 1024) {
        errorMsg = QString("文件过大（%1 MB），请选择小于 10MB 的图片")
                   .arg(fileSize / 1024.0 / 1024.0, 0, 'f', 2);
        return false;
    }

    QImageReader reader(filePath);
    QString format = reader.format().toLower();
    if (format != "jpg" && format != "jpeg" && format != "png" && format != "gif" && format != "webp") {
        errorMsg = QString("不支持的格式（%1），仅支持 JPG/PNG/GIF/WEBP").arg(format);
        return false;
    }

    return true;
}

QString VideoSingleTab::selectAndValidateImageFile(const QString &dialogTitle, bool warnIfEmpty)
{
    QSettings settings("ChickenAI", "VideoGen");
    QString lastDir = settings.value("lastImageUploadDir",
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)).toString();
    if (!QDir(lastDir).exists()) {
        lastDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }

    QString fileName = QFileDialog::getOpenFileName(
        this, dialogTitle, lastDir,
        "图片文件 (*.png *.jpg *.jpeg *.gif *.webp)"
    );

    if (fileName.isEmpty()) {
        if (warnIfEmpty) {
            QMessageBox::warning(this, "提示", "没有选择图片");
        }
        return QString();
    }

    QString errorMsg;
    if (!validateImageFile(fileName, errorMsg)) {
        QMessageBox::warning(this, "图片校验失败", errorMsg);
        return QString();
    }

    QPixmap pixmap(fileName);
    if (pixmap.isNull()) {
        QMessageBox::warning(this, "提示", "选择的文件不是图片格式");
        return QString();
    }

    QFileInfo fileInfo(fileName);
    settings.setValue("lastImageUploadDir", fileInfo.absolutePath());
    return fileName;
}

void VideoSingleTab::onTaskStatusUpdated(const QString &taskId, const QString &status, const QString &videoUrl, int progress)
{
    // 不再需要在这里显示状态，由历史记录页面负责
    // 保留此方法以兼容 Veo3API 的信号
}

void VideoSingleTab::onApiError(const QString &error)
{
    // 更新状态为 failed
    if (!currentTaskId.isEmpty()) {
        DBManager::instance()->updateTaskStatus(currentTaskId, "failed", 0, "");
        DBManager::instance()->updateTaskErrorMessage(currentTaskId, error);
    }

    QMessageBox::critical(this, "错误", QString("API 调用失败:\n%1").arg(error));

    previewLabel->setText("💡 生成结果将在【生成历史记录】");
}

void VideoSingleTab::queueSaveSettings()
{
    if (suppressAutoSave) {
        return;
    }

    if (pendingSaveSettings) {
        return;
    }

    pendingSaveSettings = true;
    QTimer::singleShot(0, this, [this]() {
        pendingSaveSettings = false;
        saveSettings();
    });
}

QString VideoSingleTab::buildSettingsSnapshot() const
{
    QString selectedApiKeyValue;
    int keyId = apiKeyCombo->currentData().toInt();
    if (keyId > 0) {
        const ApiKey key = DBManager::instance()->getApiKey(keyId);
        selectedApiKeyValue = key.apiKey;
    }

    const QStringList snapshotParts = {
        modelCombo->currentText(),
        modelVariantCombo->currentData().toString(),
        apiKeyCombo->currentData().toString(),
        selectedApiKeyValue,
        serverCombo->currentData().toString(),
        promptInput->toPlainText(),
        resolutionCombo->currentData().toString(),
        durationCombo->currentData().toString(),
        sizeCombo->currentData().toString(),
        QString::number(watermarkCheckBox->isChecked()),
        QString::number(variantType1Radio->isChecked() ? 1 : 2),
        QString::number(enhancePromptCheckBox->isChecked()),
        QString::number(enableUpsampleCheckBox->isChecked()),
        uploadedImagePaths.join("\n"),
        uploadedEndFrameImagePath,
        uploadedMiddleFrameImagePath
    };

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(snapshotParts.join("\x1F").toUtf8());
    return QString(hash.result().toHex());
}

void VideoSingleTab::saveSettings()
{
    const QString snapshot = buildSettingsSnapshot();
    if (!lastSavedSettingsSnapshot.isEmpty() && snapshot == lastSavedSettingsSnapshot) {
        qDebug() << "[VideoSingleTab] Skip save, snapshot unchanged:" << snapshot.left(8);
        return;
    }

    QSettings settings("ChickenAI", "VideoGen");
    settings.beginGroup("VideoSingleTab");

    settings.setValue("prompt", promptInput->toPlainText());

    // 保存模型类型（文本而非索引）
    QString modelType = modelCombo->currentText();
    settings.setValue("modelType", modelType);
    settings.setValue("modelVariant", modelVariantCombo->currentData().toString());
    settings.setValue("apiKey", apiKeyCombo->currentIndex());
    settings.setValue("server", serverCombo->currentIndex());

    QString selectedApiKeyValue;
    int keyId = apiKeyCombo->currentData().toInt();
    if (keyId > 0) {
        const ApiKey key = DBManager::instance()->getApiKey(keyId);
        selectedApiKeyValue = key.apiKey;
    }
    settings.setValue("selectedApiKeyValue", selectedApiKeyValue);

    // 根据模型类型保存对应参数
    if (modelType.contains("Grok", Qt::CaseInsensitive)) {
        settings.setValue("grok_aspectRatio", resolutionCombo->currentData().toString());
        settings.setValue("grok_size", sizeCombo->currentData().toString());
    } else if (modelType.contains("wan", Qt::CaseInsensitive)) {
        settings.setValue("wan_resolution", wanResolutionCombo->currentData().toString());
        settings.setValue("wan_duration", durationCombo->currentData().toString());
    } else {
        settings.setValue("veo_resolution", resolutionCombo->currentData().toString());
        settings.setValue("duration", durationCombo->currentData().toString());
    }

    settings.setValue("watermark", watermarkCheckBox->isChecked());
    settings.setValue("imagePaths", uploadedImagePaths);
    settings.setValue("endFrameImagePath", uploadedEndFrameImagePath);
    settings.setValue("middleFrameImagePath", uploadedMiddleFrameImagePath);
    settings.setValue("lastSubmittedHash", lastSubmittedParamsHash);

    // 保存 Variant 类型和 Variant 2 专用参数
    settings.setValue("variantType", variantType1Radio->isChecked() ? 1 : 2);
    settings.setValue("enhancePrompt", enhancePromptCheckBox->isChecked());
    settings.setValue("enableUpsample", enableUpsampleCheckBox->isChecked());

    // WAN 参数
    settings.setValue("wan_negativePrompt", wanNegativePromptInput->toPlainText());
    settings.setValue("wan_template", wanTemplateCombo->currentData().toString());
    settings.setValue("wan_promptExtend", wanPromptExtendCheckBox->isChecked());
    settings.setValue("wan_seed", wanSeedInput->value());
    settings.setValue("wan_duration", wanDurationCombo->currentData().toInt());
    settings.setValue("wan_watermark", wanWatermarkCheckBox->isChecked());
    settings.setValue("wan_audioEnabled", wanAudioCheckBox->isChecked());
    settings.setValue("wan_audioPath", uploadedWanAudioPath);
    settings.setValue("wan_audioUrl", uploadedWanAudioUrl);

    settings.endGroup();
    settings.sync();  // 强制立即写入磁盘
    lastSavedSettingsSnapshot = snapshot;

    // 调试日志
    qDebug() << "[VideoSingleTab] Settings saved:"
             << "snapshot=" << snapshot.left(8)
             << "prompt=" << promptInput->toPlainText().left(20)
             << "modelType=" << modelType
             << "resolution=" << resolutionCombo->currentData().toString()
             << "watermark=" << watermarkCheckBox->isChecked();
}

void VideoSingleTab::loadFromTask(const VideoTask& task)
{
    // 阻止信号触发，避免触发参数保存
    blockSignals(true);

    // 1. 设置提示词
    promptInput->setPlainText(task.prompt);

    // 2. 设置模型（通过文本匹配）
    for (int i = 0; i < modelCombo->count(); ++i) {
        if (modelCombo->itemText(i) == task.modelName) {
            modelCombo->setCurrentIndex(i);
            break;
        }
    }

    // 3. 设置模型变体（通过文本匹配）
    for (int i = 0; i < modelVariantCombo->count(); ++i) {
        if (modelVariantCombo->itemText(i) == task.modelVariant) {
            modelVariantCombo->setCurrentIndex(i);
            break;
        }
    }

    // 4. 设置API密钥（通过名称匹配，如果不存在则用第一个）
    bool apiKeyFound = false;
    for (int i = 0; i < apiKeyCombo->count(); ++i) {
        if (apiKeyCombo->itemText(i) == task.apiKeyName) {
            apiKeyCombo->setCurrentIndex(i);
            apiKeyFound = true;
            break;
        }
    }
    if (!apiKeyFound && apiKeyCombo->count() > 0) {
        apiKeyCombo->setCurrentIndex(0);
    }

    // 5. 设置服务器URL（通过data匹配）
    for (int i = 0; i < serverCombo->count(); ++i) {
        if (serverCombo->itemData(i).toString() == task.serverUrl) {
            serverCombo->setCurrentIndex(i);
            break;
        }
    }

    // 6. 设置分辨率（通过data匹配）
    for (int i = 0; i < resolutionCombo->count(); ++i) {
        if (resolutionCombo->itemData(i).toString() == task.resolution) {
            resolutionCombo->setCurrentIndex(i);
            break;
        }
    }

    // 7. 设置时长（通过文本匹配）
    QString durationText = QString::number(task.duration) + "秒";
    for (int i = 0; i < durationCombo->count(); ++i) {
        if (durationCombo->itemText(i) == durationText) {
            durationCombo->setCurrentIndex(i);
            break;
        }
    }

    // 8. 设置水印
    watermarkCheckBox->setChecked(task.watermark);

    // 8.5. 设置Grok专用参数（如果是Grok模型）
    if (task.modelName.contains("Grok", Qt::CaseInsensitive)) {
        // 设置aspectRatio（存储在resolution字段中）
        if (!task.aspectRatio.isEmpty()) {
            for (int i = 0; i < resolutionCombo->count(); ++i) {
                if (resolutionCombo->itemData(i).toString() == task.aspectRatio) {
                    resolutionCombo->setCurrentIndex(i);
                    break;
                }
            }
        }
        // 设置size
        if (!task.size.isEmpty()) {
            for (int i = 0; i < sizeCombo->count(); ++i) {
                if (sizeCombo->itemData(i).toString() == task.size) {
                    sizeCombo->setCurrentIndex(i);
                    break;
                }
            }
        }
    }

    // 9. 解析并设置图片路径
    uploadedImagePaths.clear();
    if (!task.imagePaths.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(task.imagePaths.toUtf8());
        if (doc.isArray()) {
            QJsonArray imageArray = doc.array();
            for (const QJsonValue& value : imageArray) {
                QString path = value.toString();
                if (QFile::exists(path)) {
                    uploadedImagePaths.append(path);
                }
            }
        }
    }

    // 10. 设置尾帧图片
    uploadedEndFrameImagePath = task.endFrameImagePath;
    if (!uploadedEndFrameImagePath.isEmpty() && !QFile::exists(uploadedEndFrameImagePath)) {
        uploadedEndFrameImagePath.clear();
    }

    // 11. 更新图片预览
    updateImagePreview();

    // 12. 重置参数修改标记和哈希
    parametersModified = false;
    lastSubmittedParamsHash.clear();

    // 恢复信号
    blockSignals(false);

    qDebug() << "[VideoSingleTab] Loaded task parameters:" << task.taskId;
}

void VideoSingleTab::loadSettings()
{
    QSettings settings("ChickenAI", "VideoGen");
    settings.beginGroup("VideoSingleTab");

    // 阻止信号发射，避免加载时触发保存
    promptInput->blockSignals(true);
    modelCombo->blockSignals(true);
    modelVariantCombo->blockSignals(true);
    apiKeyCombo->blockSignals(true);
    serverCombo->blockSignals(true);
    resolutionCombo->blockSignals(true);
    watermarkCheckBox->blockSignals(true);
    durationCombo->blockSignals(true);
    sizeCombo->blockSignals(true);
    variantType1Radio->blockSignals(true);
    variantType2Radio->blockSignals(true);
    enhancePromptCheckBox->blockSignals(true);
    enableUpsampleCheckBox->blockSignals(true);

    // 加载提示词
    QString loadedPrompt = settings.value("prompt", "").toString();
    promptInput->setPlainText(loadedPrompt);

    // 恢复模型类型（文本匹配）
    QString savedModelType = settings.value("modelType", "VEO3视频").toString();
    for (int i = 0; i < modelCombo->count(); ++i) {
        if (modelCombo->itemText(i) == savedModelType) {
            modelCombo->setCurrentIndex(i);
            break;
        }
    }

    // 手动触发 onModelChanged 以更新 UI 状态（因为信号被阻塞）
    onModelChanged(modelCombo->currentIndex());

    // 恢复 Variant 类型（VEO3模型时）
    if (savedModelType.contains("VEO3", Qt::CaseInsensitive)) {
        int variantType = settings.value("variantType", 1).toInt();
        if (variantType == 2) {
            variantType2Radio->setChecked(true);
        } else {
            variantType1Radio->setChecked(true);
        }
        // 手动触发以更新变体列表
        onVariantTypeChanged();
    }

    // 恢复 enhance_prompt / enable_upsample
    enhancePromptCheckBox->setChecked(settings.value("enhancePrompt", true).toBool());
    enableUpsampleCheckBox->setChecked(settings.value("enableUpsample", true).toBool());

    // 恢复模型变体（值匹配）
    QString savedVariant = settings.value("modelVariant", "").toString();
    if (!savedVariant.isEmpty()) {
        for (int i = 0; i < modelVariantCombo->count(); ++i) {
            if (modelVariantCombo->itemData(i).toString() == savedVariant) {
                modelVariantCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    // 恢复 API Key 和 Server（索引）
    int apiKeyIndex = settings.value("apiKey", 0).toInt();
    if (apiKeyIndex >= 0 && apiKeyIndex < apiKeyCombo->count()) {
        apiKeyCombo->setCurrentIndex(apiKeyIndex);
    }

    int serverIndex = settings.value("server", 0).toInt();
    if (serverIndex >= 0 && serverIndex < serverCombo->count()) {
        serverCombo->setCurrentIndex(serverIndex);
    }

    // 根据模型类型恢复对应参数
    if (savedModelType.contains("Grok", Qt::CaseInsensitive)) {
        // Grok 模型：恢复 aspectRatio 和 size
        QString aspectRatio = settings.value("grok_aspectRatio", "1:1").toString();
        for (int i = 0; i < resolutionCombo->count(); ++i) {
            if (resolutionCombo->itemData(i).toString() == aspectRatio) {
                resolutionCombo->setCurrentIndex(i);
                break;
            }
        }

        QString size = settings.value("grok_size", "720P").toString();
        for (int i = 0; i < sizeCombo->count(); ++i) {
            if (sizeCombo->itemData(i).toString() == size) {
                sizeCombo->setCurrentIndex(i);
                break;
            }
        }
    } else if (savedModelType.contains("wan", Qt::CaseInsensitive)) {
        // WAN 模型：恢复分辨率
        wanResolutionCombo->setCurrentIndex(
            wanResolutionCombo->findData(settings.value("wan_resolution", "720P").toString()));

        // 恢复 WAN 参数
        wanNegativePromptInput->setPlainText(settings.value("wan_negativePrompt", "").toString());
        QString wanTemplate = settings.value("wan_template", "").toString();
        for (int i = 0; i < wanTemplateCombo->count(); ++i) {
            if (wanTemplateCombo->itemData(i).toString() == wanTemplate) {
                wanTemplateCombo->setCurrentIndex(i);
                break;
            }
        }
        wanPromptExtendCheckBox->setChecked(settings.value("wan_promptExtend", true).toBool());
        wanSeedInput->setValue(settings.value("wan_seed", 0).toInt());
        wanDurationCombo->setCurrentIndex(
            wanDurationCombo->findData(settings.value("wan_duration", 5).toInt()));
        wanWatermarkCheckBox->setChecked(settings.value("wan_watermark", false).toBool());
        wanAudioCheckBox->setChecked(settings.value("wan_audioEnabled", false).toBool());
        uploadedWanAudioPath = settings.value("wan_audioPath", "").toString();
        uploadedWanAudioUrl = settings.value("wan_audioUrl", "").toString();
        if (!uploadedWanAudioPath.isEmpty() && QFile::exists(uploadedWanAudioPath)) {
            QFileInfo fi(uploadedWanAudioPath);
            wanAudioFileLabel->setText("✓ " + fi.fileName());
            wanAudioFileLabel->setStyleSheet("font-size: 12px; color: #4CAF50;");
            clearWanAudioButton->setVisible(true);
        }
    } else {
        // VEO3 模型：恢复 resolution 和 duration
        QString resolution = settings.value("veo_resolution", "1080x1920").toString();
        for (int i = 0; i < resolutionCombo->count(); ++i) {
            if (resolutionCombo->itemData(i).toString() == resolution) {
                resolutionCombo->setCurrentIndex(i);
                break;
            }
        }

        QString duration = settings.value("duration", "5s").toString();
        for (int i = 0; i < durationCombo->count(); ++i) {
            if (durationCombo->itemData(i).toString() == duration) {
                durationCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    watermarkCheckBox->setChecked(settings.value("watermark", false).toBool());

    qDebug() << "[VideoSingleTab] Loading settings:"
             << "prompt=" << loadedPrompt.left(30)
             << "modelType=" << savedModelType
             << "resolution=" << resolutionCombo->currentData().toString()
             << "watermark=" << watermarkCheckBox->isChecked();

    // 加载图片路径，验证文件是否存在
    QStringList imagePaths = settings.value("imagePaths").toStringList();
    uploadedImagePaths.clear();
    for (const QString &path : imagePaths) {
        if (QFile::exists(path)) {
            uploadedImagePaths.append(path);
        }
    }
    if (!uploadedImagePaths.isEmpty()) {
        updateImagePreview();
    }

    // 加载尾帧图片路径
    QString endFramePath = settings.value("endFrameImagePath").toString();
    if (!endFramePath.isEmpty() && QFile::exists(endFramePath)) {
        uploadedEndFrameImagePath = endFramePath;

        // 显示尾帧缩略图
        QPixmap pixmap(endFramePath);
        if (!pixmap.isNull()) {
            QPixmap scaledPixmap = pixmap.scaled(200, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            endFramePreviewLabel->setPixmap(scaledPixmap);
            endFramePreviewLabel->setText("");
        } else {
            endFramePreviewLabel->setPixmap(QPixmap());
            QFileInfo fileInfo(endFramePath);
            endFramePreviewLabel->setText("✓ " + fileInfo.fileName());
        }
        endFramePreviewLabel->setProperty("hasImage", true);
        endFramePreviewLabel->style()->unpolish(endFramePreviewLabel);
        endFramePreviewLabel->style()->polish(endFramePreviewLabel);
    }

    // 加载中间帧图片路径
    QString middleFramePath = settings.value("middleFrameImagePath").toString();
    if (!middleFramePath.isEmpty() && QFile::exists(middleFramePath)) {
        uploadedMiddleFrameImagePath = middleFramePath;

        QPixmap pixmap(middleFramePath);
        if (!pixmap.isNull()) {
            QPixmap scaledPixmap = pixmap.scaled(200, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            middleFramePreviewLabel->setPixmap(scaledPixmap);
            middleFramePreviewLabel->setText("");
        } else {
            middleFramePreviewLabel->setPixmap(QPixmap());
            QFileInfo fileInfo(middleFramePath);
            middleFramePreviewLabel->setText("✓ " + fileInfo.fileName());
        }
        middleFramePreviewLabel->setProperty("hasImage", true);
        middleFramePreviewLabel->style()->unpolish(middleFramePreviewLabel);
        middleFramePreviewLabel->style()->polish(middleFramePreviewLabel);
    }

    lastSubmittedParamsHash = settings.value("lastSubmittedHash", "").toString();

    settings.endGroup();

    // 读取完成后记录当前快照，避免无变更重复写盘
    lastSavedSettingsSnapshot = buildSettingsSnapshot();

    // 恢复信号发射
    promptInput->blockSignals(false);
    modelCombo->blockSignals(false);
    modelVariantCombo->blockSignals(false);
    apiKeyCombo->blockSignals(false);
    serverCombo->blockSignals(false);
    resolutionCombo->blockSignals(false);
    watermarkCheckBox->blockSignals(false);
    durationCombo->blockSignals(false);
    variantType1Radio->blockSignals(false);
    variantType2Radio->blockSignals(false);
    enhancePromptCheckBox->blockSignals(false);
    enableUpsampleCheckBox->blockSignals(false);

    // 调试日志 - 验证UI状态
    qDebug() << "[VideoSingleTab] After loading, UI state:"
             << "promptInput=" << promptInput->toPlainText().left(30)
             << "modelCombo=" << modelCombo->currentIndex()
             << "resolutionCombo=" << resolutionCombo->currentIndex()
             << "watermarkCheckBox=" << watermarkCheckBox->isChecked();

    // 加载后标记为未修改
    parametersModified = false;
}

QString VideoSingleTab::calculateParamsHash() const
{
    QCryptographicHash hash(QCryptographicHash::Sha256);

    hash.addData(promptInput->toPlainText().toUtf8());
    hash.addData(QString::number(modelCombo->currentIndex()).toUtf8());
    hash.addData(QString::number(modelVariantCombo->currentIndex()).toUtf8());
    hash.addData(QString::number(resolutionCombo->currentIndex()).toUtf8());
    hash.addData(QString::number(watermarkCheckBox->isChecked()).toUtf8());
    hash.addData(QString::number(durationCombo->currentIndex()).toUtf8());

    for (const QString &path : uploadedImagePaths) {
        hash.addData(path.toUtf8());
    }

    if (!uploadedEndFrameImagePath.isEmpty()) {
        hash.addData(uploadedEndFrameImagePath.toUtf8());
    }

    return QString(hash.result().toHex());
}

bool VideoSingleTab::checkDuplicateSubmission()
{
    QString currentHash = calculateParamsHash();

    // 如果参数未修改且与上次提交相同，且未抑制警告
    if (!parametersModified && currentHash == lastSubmittedParamsHash && !suppressDuplicateWarning) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("重复提交提醒");
        msgBox.setText("刚刚已经提交这次任务，是否继续？");
        msgBox.setIcon(QMessageBox::Question);

        QPushButton *continueBtn = msgBox.addButton("继续", QMessageBox::AcceptRole);
        QPushButton *cancelBtn = msgBox.addButton("停止", QMessageBox::RejectRole);

        QCheckBox *suppressCheckBox = new QCheckBox("不再显示此提示");
        msgBox.setCheckBox(suppressCheckBox);

        msgBox.exec();

        if (msgBox.clickedButton() == cancelBtn) {
            return false;
        }

        if (suppressCheckBox->isChecked()) {
            suppressDuplicateWarning = true;
        }
    }

    return true;
}

void VideoSingleTab::onAnyParameterChanged()
{
    parametersModified = true;
    suppressDuplicateWarning = false;  // 参数修改后重置抑制标志
}

QString VideoSingleTab::copyImagesToPersistentStorage(const QString &taskId)
{
    // 获取当前模型名称
    QString modelName = modelCombo->currentText();
    QString modelDirName;

    if (modelName.contains("sora", Qt::CaseInsensitive)) {
        modelDirName = "sora_one_by_one";
    } else if (modelName.contains("veo", Qt::CaseInsensitive)) {
        modelDirName = "veo3_one_by_one";
    } else if (modelName.contains("grok", Qt::CaseInsensitive)) {
        modelDirName = "grok_one_by_one";
    } else if (modelName.contains("wan", Qt::CaseInsensitive)) {
        modelDirName = "wan_one_by_one";
    } else {
        modelDirName = "unknown_one_by_one";
    }

    // 构建目录路径: inputs/model_name/YYYYMMDD/taskid/
    // 使用应用可执行文件所在目录的父目录（项目根目录）
    QString dateStr = QDateTime::currentDateTime().toString("yyyyMMdd");
    QString appPath = QCoreApplication::applicationDirPath();

    // 如果是 macOS .app 包，需要向上3级到项目根目录
    QDir appDir(appPath);
    if (appPath.contains(".app/Contents/MacOS")) {
        appDir.cdUp();  // Contents
        appDir.cdUp();  // ChickenAI.app
        appDir.cdUp();  // build
    }

    QString basePath = appDir.absolutePath();
    QString targetDir = QString("%1/inputs/%2/%3/%4")
        .arg(basePath)
        .arg(modelDirName)
        .arg(dateStr)
        .arg(taskId);

    // 创建目录
    QDir dir;
    if (!dir.mkpath(targetDir)) {
        qWarning() << "Failed to create directory:" << targetDir;
        return "";
    }

    // 复制首帧图片
    for (int i = 0; i < uploadedImagePaths.size(); ++i) {
        QString sourcePath = uploadedImagePaths[i];
        QFileInfo fileInfo(sourcePath);
        QString targetPath = QString("%1/frame_start_%2.%3")
            .arg(targetDir)
            .arg(i)
            .arg(fileInfo.suffix());

        if (!QFile::copy(sourcePath, targetPath)) {
            qWarning() << "Failed to copy image:" << sourcePath << "to" << targetPath;
        }
    }

    // 复制尾帧图片（如果有）
    if (!uploadedEndFrameImagePath.isEmpty()) {
        QFileInfo fileInfo(uploadedEndFrameImagePath);
        QString targetPath = QString("%1/frame_end.%2")
            .arg(targetDir)
            .arg(fileInfo.suffix());

        if (!QFile::copy(uploadedEndFrameImagePath, targetPath)) {
            qWarning() << "Failed to copy end frame image:" << uploadedEndFrameImagePath;
        }
    }

    return targetDir;
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
