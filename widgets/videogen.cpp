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
#include <QColor>
#include <QPalette>

namespace {

bool hasLocalVideoFile(const VideoTask& task)
{
    return !task.videoPath.isEmpty() && QFile::exists(task.videoPath);
}

bool isTempTaskId(const QString& taskId)
{
    return taskId.startsWith("temp_") || taskId.startsWith("temp-");
}

bool isRecoverableSubmitError(const QString& error)
{
    const QString normalized = error.trimmed();
    return normalized.contains("可能已创建")
           || normalized.contains("timeout", Qt::CaseInsensitive)
           || normalized.contains("超时");
}

void showSora2RecoveryHint(QWidget* parent, const QString& taskId, const QString& reason)
{
    QMessageBox::warning(parent, "提示",
        QString("任务已创建（%1），但%2，未自动启动轮询。\n"
                "请稍后在历史记录中刷新状态，或重启应用后自动恢复。")
            .arg(taskId, reason));
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

    const QString rawStatus = task.status.trimmed();
    const QString normalizedStatus = rawStatus.toLower();
    const QString errorDetail = task.errorMessage.trimmed();

    if (displayState.hasLocalFile) {
        displayState.resolvedState = VideoResolvedState::CompletedLocal;
        displayState.statusText = "✅ 已完成";
        displayState.statusIcon = "✅";
    } else if (normalizedStatus == "failed") {
        displayState.resolvedState = VideoResolvedState::Failed;
        displayState.statusText = errorDetail.isEmpty()
            ? "生成失败"
            : QString("生成失败，原因是：%1").arg(errorDetail);
        displayState.statusIcon = "❌";
    } else if (normalizedStatus == "timeout") {
        displayState.resolvedState = VideoResolvedState::Timeout;
        const QString timeoutReason = errorDetail.isEmpty() ? "任务超时" : errorDetail;
        displayState.statusText = QString("生成失败，原因是：%1").arg(timeoutReason);
        displayState.statusIcon = "❌";
    } else if (task.downloadStatus == "downloading") {
        displayState.resolvedState = VideoResolvedState::Downloading;
        displayState.statusText = "下载中";
        displayState.statusIcon = "⬇️";
    } else if (normalizedStatus == "queued" || normalizedStatus == "pending") {
        displayState.resolvedState = VideoResolvedState::Waiting;
        displayState.statusText = "排队中";
        displayState.statusIcon = "⏳";
    } else if (normalizedStatus == "processing" || normalizedStatus == "video_generating") {
        displayState.resolvedState = VideoResolvedState::Processing;
        displayState.statusText = "处理中";
        displayState.statusIcon = "🔄";
    } else if (normalizedStatus == "completed") {
        if (task.downloadStatus == "failed") {
            displayState.resolvedState = VideoResolvedState::DownloadFailed;
            displayState.statusText = "下载失败";
            displayState.statusIcon = "⚠️";
            displayState.canDownload = !task.videoUrl.isEmpty();
        } else if (!task.videoUrl.isEmpty()) {
            displayState.resolvedState = VideoResolvedState::PendingDownload;
            displayState.statusText = "已生成待下载";
            displayState.statusIcon = "📥";
            displayState.canDownload = true;
        } else {
            displayState.resolvedState = VideoResolvedState::Processing;
            displayState.statusText = "处理中";
            displayState.statusIcon = "🔄";
        }
    } else {
        displayState.resolvedState = VideoResolvedState::Unknown;
        displayState.statusText = rawStatus.isEmpty() ? "未知状态" : rawStatus;
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
#include "sora2genpage.h"

// VideoSingleTab 实现（调度器）
VideoSingleTab::VideoSingleTab(QWidget *parent)
    : QWidget(parent)
    , sora2Api(new VideoAPI(this))
{
    setupUI();

    // 连接子页的 apiKeySelectionChanged 信号
    connect(veoPage, &VeoGenPage::apiKeySelectionChanged, this, &VideoSingleTab::apiKeySelectionChanged);
    connect(grokPage, &GrokGenPage::apiKeySelectionChanged, this, &VideoSingleTab::apiKeySelectionChanged);
    connect(wanPage, &WanGenPage::apiKeySelectionChanged, this, &VideoSingleTab::apiKeySelectionChanged);
    connect(sora2Page, &Sora2GenPage::apiKeySelectionChanged, this, &VideoSingleTab::apiKeySelectionChanged);

    connect(veoPage, &VeoGenPage::submitSucceeded, this, &VideoSingleTab::onVeoSubmitSucceeded);
    connect(veoPage, &VeoGenPage::submitFailed, this, &VideoSingleTab::onVeoSubmitFailed);
    connect(grokPage, &GrokGenPage::submitSucceeded, this, &VideoSingleTab::onGrokSubmitSucceeded);
    connect(grokPage, &GrokGenPage::submitFailed, this, &VideoSingleTab::onGrokSubmitFailed);
    connect(wanPage, &WanGenPage::submitSucceeded, this, &VideoSingleTab::onWanSubmitSucceeded);
    connect(wanPage, &WanGenPage::submitFailed, this, &VideoSingleTab::onWanSubmitFailed);

    if (auto *veoApi = veoPage->findChild<VideoAPI*>()) {
        connect(veoApi, &VideoAPI::imageUploadProgress,
                this, &VideoSingleTab::onVeoImageUploadProgress);
    }
    if (auto *grokApi = grokPage->findChild<VideoAPI*>()) {
        connect(grokApi, &VideoAPI::imageUploadProgress,
                this, &VideoSingleTab::onGrokImageUploadProgress);
    }
    if (auto *wanApi = wanPage->findChild<VideoAPI*>()) {
        connect(wanApi, &VideoAPI::imageUploadProgress,
                this, &VideoSingleTab::onWanImageUploadProgress);
    }

    auto bindGeneralSubmitDialogTrigger = [this](QWidget *page) {
        const auto buttons = page->findChildren<QPushButton*>();
        for (QPushButton *button : buttons) {
            if (button && button->text().contains("生成视频")) {
                connect(button, &QPushButton::clicked, this, [this, button]() {
                    QTimer::singleShot(0, this, [this, button]() {
                        if (!button || !button->isEnabled()) {
                            showGeneralSubmittingDialog();
                        }
                    });
                });
                break;
            }
        }
    };
    bindGeneralSubmitDialogTrigger(veoPage);
    bindGeneralSubmitDialogTrigger(grokPage);
    bindGeneralSubmitDialogTrigger(wanPage);

    connect(sora2Page, &Sora2GenPage::createTaskRequested,
            this, &VideoSingleTab::onSora2CreateTaskRequested);
    connect(sora2Api, &VideoAPI::videoCreated,
            this, &VideoSingleTab::onSora2VideoCreated);
    connect(sora2Api, &VideoAPI::imageUploadProgress,
            this, &VideoSingleTab::onSora2ImageUploadProgress);
    connect(sora2Api, &VideoAPI::errorOccurred,
            this, &VideoSingleTab::onSora2ApiError);
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
    modelCombo->addItems({"VEO3视频", "Grok3视频", "WAN视频", "Sora2视频"});
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
    sora2Page = new Sora2GenPage();
    stack->addWidget(veoPage);   // index 0: VEO3
    stack->addWidget(grokPage);  // index 1: Grok
    stack->addWidget(wanPage);    // index 2: WAN
    stack->addWidget(sora2Page);  // index 3: Sora2

    mainLayout->addWidget(stack);
}

void VideoSingleTab::onModelChanged(int index)
{
    closeGeneralSubmittingDialog();
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
    case 3:
        sora2Page->refreshApiKeys();
        sora2Page->restoreDraftSettings();
        break;
    }
}

void VideoSingleTab::refreshApiKeys()
{
    veoPage->refreshApiKeys();
    grokPage->refreshApiKeys();
    wanPage->refreshApiKeys();
    sora2Page->refreshApiKeys();
}

void VideoSingleTab::setSora2Submitting(bool submitting)
{
    sora2Submitting = submitting;
    if (sora2Page) {
        sora2Page->setSubmitting(submitting);
    }
}

VideoSingleTab::SubmitDialogTheme VideoSingleTab::resolveSubmitDialogTheme() const
{
    QString themeName;
    const QWidget *cursor = this;
    while (cursor) {
        const QVariant themeValue = cursor->property("appTheme");
        if (themeValue.isValid()) {
            themeName = themeValue.toString().trimmed().toLower();
            if (!themeName.isEmpty()) {
                break;
            }
        }
        cursor = cursor->parentWidget();
    }

    bool isDark = false;
    if (themeName == "dark") {
        isDark = true;
    } else if (themeName == "light") {
        isDark = false;
    } else {
        const QColor windowColor = palette().color(QPalette::Window);
        isDark = windowColor.lightness() < 128;
    }

    SubmitDialogTheme theme;
    theme.isDark = isDark;
    theme.textColor = isDark ? "#FFFFFF" : "#000000";
    theme.bgColor = isDark ? "#1B1D22" : "#FFFFFF";
    theme.borderColor = isDark ? "#2D3138" : "#D0D5DD";
    theme.accentBgColor = isDark ? "rgba(99, 102, 241, 0.22)" : "#EEF2FF";
    theme.accentTextColor = isDark ? "#C7D2FE" : "#3730A3";
    theme.buttonBgColor = isDark ? "#2A2F37" : "#F2F4F7";
    theme.buttonHoverColor = isDark ? "#343A44" : "#E4E7EC";
    theme.buttonTextColor = theme.textColor;
    return theme;
}

void VideoSingleTab::showGeneralSubmittingDialog(const QString& message)
{
    if (generalSubmittingDialog && generalSubmittingLabel) {
        generalSubmittingLabel->setText(message.isEmpty() ? QStringLiteral("正在提交...") : message);
        generalSubmittingDialog->show();
        generalSubmittingDialog->raise();
        generalSubmittingDialog->activateWindow();
        return;
    }

    const SubmitDialogTheme theme = resolveSubmitDialogTheme();

    generalSubmittingDialog = new QDialog(this);
    generalSubmittingDialog->setObjectName("generalSubmittingDialog");
    generalSubmittingDialog->setWindowTitle("正在提交任务");
    generalSubmittingDialog->setModal(true);
    generalSubmittingDialog->setAttribute(Qt::WA_DeleteOnClose, false);
    generalSubmittingDialog->setAttribute(Qt::WA_StyledBackground, true);
    generalSubmittingDialog->setMinimumWidth(420);

    auto *layout = new QVBoxLayout(generalSubmittingDialog);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(12);

    auto *titleLabel = new QLabel("正在提交任务", generalSubmittingDialog);
    titleLabel->setStyleSheet(QString("font-size: 14px; font-weight: 700; color: %1;").arg(theme.textColor));
    layout->addWidget(titleLabel);

    generalSubmittingLabel = new QLabel(message.isEmpty() ? QStringLiteral("正在提交...") : message, generalSubmittingDialog);
    generalSubmittingLabel->setWordWrap(true);
    generalSubmittingLabel->setStyleSheet(QString("font-size: 14px; color: %1;").arg(theme.textColor));
    layout->addWidget(generalSubmittingLabel);

    generalSubmittingDialog->setStyleSheet(QString(
        "QDialog#generalSubmittingDialog {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 12px;"
        "}"
    ).arg(theme.bgColor, theme.borderColor));

    generalSubmittingDialog->show();
}

void VideoSingleTab::updateGeneralSubmittingDialog(int current, int total)
{
    if (current <= 0 || total <= 0) {
        return;
    }
    showGeneralSubmittingDialog(QString("正在提交 第%1张图片/共%2张图片").arg(current).arg(total));
}

void VideoSingleTab::closeGeneralSubmittingDialog()
{
    if (!generalSubmittingDialog) {
        return;
    }
    generalSubmittingDialog->hide();
    if (generalSubmittingLabel) {
        generalSubmittingLabel->setText(QStringLiteral("正在提交..."));
    }
}

void VideoSingleTab::showSubmitSuccessToast()
{
    QLabel *toast = new QLabel("✅视频生成任务已经提交，请往【生成历史记录】查看任务状态", this);
    toast->setStyleSheet(
        "QLabel {"
        "    background-color: #12B76A;"
        "    color: #FFFFFF;"
        "    padding: 10px 16px;"
        "    border-radius: 8px;"
        "    font-size: 13px;"
        "    font-weight: 600;"
        "}"
    );
    toast->setAlignment(Qt::AlignCenter);
    toast->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    toast->setAttribute(Qt::WA_TranslucentBackground);
    toast->setWordWrap(true);
    toast->setMaximumWidth(420);
    toast->adjustSize();

    const QPoint globalPos = mapToGlobal(rect().center());
    toast->move(globalPos.x() - (toast->width() / 2), globalPos.y() - 110);
    toast->show();

    QTimer::singleShot(3000, toast, &QLabel::deleteLater);
}

void VideoSingleTab::showUnifiedSubmitResultDialog(bool success,
                                                    const QString& title,
                                                    const QString& message,
                                                    const QString& detail)
{
    const SubmitDialogTheme theme = resolveSubmitDialogTheme();
    const bool isFailure = !success;
    const QString statusText = success ? "提交成功" : "提交失败";
    const QString accentColor = success ? "#12B76A" : "#F04438";
    const QString primaryTextColor = isFailure ? "#FFFFFF" : theme.textColor;
    const QString frameBorderColor = isFailure ? theme.borderColor : accentColor;

    QDialog dialog(this);
    dialog.setObjectName("submitResultDialog");
    dialog.setWindowTitle(title);
    dialog.setModal(true);
    dialog.setAttribute(Qt::WA_StyledBackground, true);
    dialog.setMinimumWidth(460);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(14);

    auto *titleLabel = new QLabel(title, &dialog);
    titleLabel->setWordWrap(true);
    titleLabel->setStyleSheet(QString("font-size: 14px; font-weight: 700; color: %1;").arg(primaryTextColor));
    layout->addWidget(titleLabel);

    auto *messageLabel = new QLabel(message, &dialog);
    messageLabel->setWordWrap(true);
    messageLabel->setStyleSheet(QString("font-size: 14px; line-height: 1.45; color: %1;").arg(primaryTextColor));
    layout->addWidget(messageLabel);

    auto *statusFrame = new QFrame(&dialog);
    statusFrame->setStyleSheet(QString(
        "QFrame {"
        "  border-radius: 10px;"
        "  border: 1px solid %1;"
        "  background: %2;"
        "}"
    ).arg(frameBorderColor, theme.accentBgColor));
    auto *statusLayout = new QVBoxLayout(statusFrame);
    statusLayout->setContentsMargins(12, 10, 12, 10);
    statusLayout->setSpacing(6);

    auto *statusLabel = new QLabel(statusText, statusFrame);
    statusLabel->setStyleSheet(QString("font-size: 14px; font-weight: 700; color: %1;").arg(primaryTextColor));
    statusLayout->addWidget(statusLabel);

    if (!detail.trimmed().isEmpty()) {
        auto *detailLabel = new QLabel(detail, statusFrame);
        detailLabel->setWordWrap(true);
        detailLabel->setStyleSheet(QString("font-size: 14px; color: %1;").arg(primaryTextColor));
        statusLayout->addWidget(detailLabel);
    }

    layout->addWidget(statusFrame);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->addStretch();
    auto *okButton = new QPushButton("我知道了", &dialog);
    okButton->setMinimumHeight(32);
    okButton->setCursor(Qt::PointingHandCursor);
    okButton->setStyleSheet(QString(
        "QPushButton {"
        "  min-width: 96px;"
        "  padding: 0 14px;"
        "  border-radius: 8px;"
        "  border: 1px solid %1;"
        "  background: %2;"
        "  color: %3;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover { background: %4; }"
        "QPushButton:pressed { background: %4; }"
    ).arg(theme.borderColor, theme.buttonBgColor, theme.buttonTextColor, theme.buttonHoverColor));
    buttonRow->addWidget(okButton);
    buttonRow->addStretch();
    layout->addLayout(buttonRow);

    dialog.setStyleSheet(QString(
        "QDialog#submitResultDialog {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 12px;"
        "}"
    ).arg(theme.bgColor, theme.borderColor));

    connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();
}

void VideoSingleTab::onVeoSubmitSucceeded(const QString& modelName)
{
    Q_UNUSED(modelName);
    closeGeneralSubmittingDialog();
    showSubmitSuccessToast();
}

void VideoSingleTab::onVeoSubmitFailed(const QString& modelName, const QString& error)
{
    closeGeneralSubmittingDialog();
    showUnifiedSubmitResultDialog(
        false,
        QString("%1提交失败").arg(modelName),
        "接口调用失败，请检查配置后重试。",
        error
    );
}

void VideoSingleTab::onGrokSubmitSucceeded(const QString& modelName)
{
    onVeoSubmitSucceeded(modelName);
}

void VideoSingleTab::onGrokSubmitFailed(const QString& modelName, const QString& error)
{
    onVeoSubmitFailed(modelName, error);
}

void VideoSingleTab::onWanSubmitSucceeded(const QString& modelName)
{
    onVeoSubmitSucceeded(modelName);
}

void VideoSingleTab::onWanSubmitFailed(const QString& modelName, const QString& error)
{
    onVeoSubmitFailed(modelName, error);
}

void VideoSingleTab::onVeoImageUploadProgress(int current, int total)
{
    updateGeneralSubmittingDialog(current, total);
}

void VideoSingleTab::onGrokImageUploadProgress(int current, int total)
{
    updateGeneralSubmittingDialog(current, total);
}

void VideoSingleTab::onWanImageUploadProgress(int current, int total)
{
    updateGeneralSubmittingDialog(current, total);
}

void VideoSingleTab::onSora2CreateTaskRequested(const QVariantMap& payload)
{
    closeGeneralSubmittingDialog();
    if (sora2Submitting) {
        return;
    }

    const QString prompt = payload.value("prompt").toString().trimmed();
    if (prompt.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入提示词");
        return;
    }

    const QString apiFormat = payload.value("api_format", "unified").toString();
    const QString model = payload.value("model").toString();
    QStringList imagePaths = payload.value("images").toStringList();
    if (imagePaths.isEmpty()) {
        const QVariantList imageList = payload.value("images").toList();
        for (const QVariant &value : imageList) {
            const QString path = value.toString();
            if (!path.isEmpty()) {
                imagePaths.append(path);
            }
        }
    }

    const QString size = payload.value("size").toString();
    const QString seconds = payload.value("seconds").toString();
    const QString style = payload.value("style").toString();
    const QString orientation = payload.value("orientation").toString();
    const QString duration = payload.value("duration").toString();
    const bool watermark = payload.value("watermark").toBool();
    const bool privateMode = payload.value("private").toBool();
    const bool isImageToVideo = payload.value("is_image_to_video").toBool() || !imagePaths.isEmpty();

    QList<ApiKey> keys = DBManager::instance()->getAllApiKeys();
    if (keys.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先添加 API 密钥");
        return;
    }

    QSettings settings("ChickenAI", "VideoGen");
    settings.beginGroup("VideoSingleTab");
    const QString selectedApiKeyValue = settings.value("selectedApiKeyValue").toString().trimmed();
    const int serverIndex = settings.value("server", 0).toInt();
    settings.endGroup();

    const QString payloadApiKeyValue = payload.value("api_key_value").toString().trimmed();
    const QString effectiveApiKeyValue = !payloadApiKeyValue.isEmpty() ? payloadApiKeyValue : selectedApiKeyValue;

    ApiKey selectedKey = keys.first();
    bool foundSelectedKey = false;
    if (!effectiveApiKeyValue.isEmpty()) {
        for (const ApiKey &key : keys) {
            if (key.apiKey == effectiveApiKeyValue) {
                selectedKey = key;
                foundSelectedKey = true;
                break;
            }
        }
    }

    if (!foundSelectedKey && !effectiveApiKeyValue.isEmpty()) {
        QMessageBox::warning(this, "提示", "所选 API 密钥无效，请重新选择");
        return;
    }

    const QStringList serverUrls = {
        "https://ai.kegeai.top",
        "https://api.kuai.host",
        "https://api.kegeai.top"
    };
    const QString payloadServerUrl = payload.value("server_url").toString().trimmed();
    QString baseUrl = payloadServerUrl;
    if (baseUrl.isEmpty()) {
        baseUrl = (serverIndex >= 0 && serverIndex < serverUrls.size())
                ? serverUrls.at(serverIndex)
                : serverUrls.first();
    }

    QString tempTaskId = QString("temp_%1_%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(QRandomGenerator::global()->bounded(10000));

    VideoTask task;
    task.taskId = tempTaskId;
    task.taskType = "video_single";
    task.prompt = prompt;
    task.modelVariant = model;
    task.modelName = "Sora2视频";
    task.apiKeyName = selectedKey.name;
    task.serverUrl = baseUrl;
    task.status = "submitting";
    task.progress = 0;
    task.resolution = size;
    task.duration = apiFormat == "openai" ? seconds.toInt() : duration.toInt();
    task.watermark = watermark;

    QJsonArray imageArray;
    for (const QString &path : imagePaths) {
        if (!path.isEmpty()) {
            imageArray.append(path);
        }
    }
    task.imagePaths = QString::fromUtf8(QJsonDocument(imageArray).toJson(QJsonDocument::Compact));

    int dbId = DBManager::instance()->insertVideoTask(task);
    if (dbId < 0) {
        QMessageBox::critical(this, "错误", "无法保存任务记录");
        return;
    }

    sora2PendingTasks.append({tempTaskId, selectedKey.apiKey, baseUrl});
    setSora2Submitting(true);
    if (sora2Page) {
        sora2Page->setStatusHint("⏳ 正在提交视频生成任务...");
    }

    if (imagePaths.isEmpty()) {
        showGeneralSubmittingDialog(QStringLiteral("正在提交..."));
    } else {
        showGeneralSubmittingDialog(QString("正在上传第1张/共%1张").arg(imagePaths.size()));
    }

    sora2Api->createVideo(
        selectedKey.apiKey,
        baseUrl,
        "Sora2视频",
        model,
        prompt,
        imagePaths,
        size,
        seconds,
        watermark,
        "",
        selectedKey.apiKey,
        true,
        true,
        apiFormat,
        style,
        privateMode,
        orientation,
        duration
    );
}

void VideoSingleTab::onSora2VideoCreated(const QString& taskId, const QString& status)
{
    setSora2Submitting(false);
    closeGeneralSubmittingDialog();
    if (sora2Page) {
        sora2Page->clearStatusHint();
    }

    QString resolvedStatus = status.trimmed();
    if (resolvedStatus.isEmpty()) {
        resolvedStatus = "pending";
    }

    if (sora2PendingTasks.isEmpty()) {
        qWarning() << "[Sora2] Missing pending context for created task:" << taskId;
        DBManager::instance()->updateTaskStatus(taskId, resolvedStatus, 0, "");
        showSora2RecoveryHint(this, taskId, "本地上下文丢失");
        return;
    }

    const auto context = sora2PendingTasks.takeFirst();

    if (context.tempTaskId != taskId) {
        DBManager::instance()->updateTaskId(context.tempTaskId, taskId);
    }

    DBManager::instance()->updateTaskStatus(taskId, resolvedStatus, 0, "");

    if (sora2Page) {
        sora2Page->onSubmitSuccess(taskId);
    }

    if (!context.apiKey.isEmpty() && !context.baseUrl.isEmpty()) {
        TaskPollManager::getInstance()->startPolling(taskId, "video_single", context.apiKey, context.baseUrl, "Sora2视频");
    } else {
        qWarning() << "[Sora2] Missing API context for polling, task:" << taskId;
        showSora2RecoveryHint(this, taskId, "缺少轮询所需上下文");
    }

    showSubmitSuccessToast();
}

void VideoSingleTab::onSora2ImageUploadProgress(int current, int total)
{
    if (sora2Page) {
        sora2Page->setStatusHint(QString("⏳ 正在上传第%1张/共%2张").arg(current).arg(total));
    }
    updateGeneralSubmittingDialog(current, total);
    if (!sora2PendingTasks.isEmpty()) {
        const auto &context = sora2PendingTasks.first();
        if (!context.tempTaskId.isEmpty()) {
            DBManager::instance()->updateTaskStatus(
                context.tempTaskId,
                QString("uploading_images:%1/%2").arg(current).arg(total),
                0,
                "");
        }
    }
}

void VideoSingleTab::onSora2ApiError(const QString& error)
{
    const QString userFacingError = VideoAPI::normalizeUserFacingError(error);
    setSora2Submitting(false);
    closeGeneralSubmittingDialog();
    if (sora2Page) {
        sora2Page->clearStatusHint();
    }

    if (!sora2PendingTasks.isEmpty()) {
        const auto context = sora2PendingTasks.takeFirst();
        if (isRecoverableSubmitError(error)) {
            DBManager::instance()->updateTaskStatus(context.tempTaskId, "pending", 0, "");
            DBManager::instance()->updateTaskErrorMessage(context.tempTaskId, userFacingError);
        } else {
            DBManager::instance()->updateTaskStatus(context.tempTaskId, "failed", 0, "");
            DBManager::instance()->updateTaskErrorMessage(context.tempTaskId, userFacingError);
        }
    }

    if (sora2Page) {
        sora2Page->onSubmitError(userFacingError);
    }

    showUnifiedSubmitResultDialog(
        false,
        "Sora2视频提交失败",
        "接口调用失败，请检查配置后重试。",
        userFacingError
    );
}

void VideoSingleTab::loadFromTask(const VideoTask& task)
{
    const QString modelName = task.modelName;
    const QString modelVariant = task.modelVariant;

    // 根据 task.modelName / modelVariant 切换到对应子页
    if (modelName.contains("Grok", Qt::CaseInsensitive)) {
        modelCombo->setCurrentIndex(1);
    } else if (modelName.contains("wan", Qt::CaseInsensitive)) {
        modelCombo->setCurrentIndex(2);
    } else if (modelName.contains("sora", Qt::CaseInsensitive)
               || modelVariant.contains("sora", Qt::CaseInsensitive)) {
        modelCombo->setCurrentIndex(3);
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
    } else if (currentPage == sora2Page) {
        sora2Page->loadFromTask(task);
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

    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget *contentWidget = new QWidget();
    contentWidget->setObjectName("scrollContentWidget");
    contentWidget->setAutoFillBackground(false);
    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(20, 20, 20, 20);
    contentLayout->setSpacing(15);

    // API Key 选择
    QHBoxLayout *keyLayout = new QHBoxLayout();
    QLabel *keyLabel = new QLabel("API 密钥:");
    keyLabel->setStyleSheet("font-size: 14px;");
    apiKeyCombo = new QComboBox();
    addKeyButton = nullptr;
    keyLayout->addWidget(keyLabel);
    keyLayout->addWidget(apiKeyCombo, 1);
    contentLayout->addLayout(keyLayout);

    // 请求服务器选择
    QHBoxLayout *serverLayout = new QHBoxLayout();
    QLabel *serverLabel = new QLabel("请求服务器:");
    serverLabel->setStyleSheet("font-size: 14px;");
    serverCombo = new QComboBox();
    serverCombo->addItem("【主站】https://ai.kegeai.top", "https://ai.kegeai.top");
    serverCombo->addItem("【备用】https://api.kuai.host", "https://api.kuai.host");
    serverCombo->setCurrentIndex(0);
    serverLayout->addWidget(serverLabel);
    serverLayout->addWidget(serverCombo, 1);
    contentLayout->addLayout(serverLayout);

    // 生成按钮
    generateButton = new QPushButton("🚀 批量生成视频");
    generateButton->setCursor(Qt::PointingHandCursor);
    connect(generateButton, &QPushButton::clicked, this, &VideoBatchTab::generateBatch);
    contentLayout->addWidget(generateButton);

    // 开发状态提示
    QLabel *comingSoonLabel = new QLabel("批量功能待开发中");
    comingSoonLabel->setAlignment(Qt::AlignCenter);
    comingSoonLabel->setStyleSheet("font-size: 28px;");
    contentLayout->addWidget(comingSoonLabel);

    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);
}

void VideoBatchTab::loadApiKeys()
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

    if (addKeyButton) {
        addKeyButton->setVisible(keys.isEmpty());
    }
}

void VideoBatchTab::refreshApiKeys()
{
    loadApiKeys();
}

void VideoBatchTab::generateBatch()
{
    if (apiKeyCombo->count() == 0 || !apiKeyCombo->isEnabled()) {
        QMessageBox::warning(this, "提示", "请先添加 API 密钥");
        return;
    }

    QMessageBox::information(this, "提示", "批量功能待开发中");
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

// VideoHistoryWidget 实现（新的 2 tab 容器）
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

    mainLayout->addWidget(tabWidget);
}

// VideoSingleHistoryTab 实现
VideoSingleHistoryTab::VideoSingleHistoryTab(QWidget *parent)
    : QWidget(parent), isListView(true), currentOffset(0)
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
    connect(TaskPollManager::getInstance(), &TaskPollManager::taskTimeout,
            this, &VideoSingleHistoryTab::onTaskTimeout);
    connect(TaskPollManager::getInstance(), &TaskPollManager::taskFailed,
            this, &VideoSingleHistoryTab::onTaskFailed);

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
    serverCombo->addItem("【备用】https://api.kegeai.top", "https://api.kegeai.top");
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
            QTableWidgetItem *statusItem = new QTableWidgetItem(displayState.statusText);
            if (!task.errorMessage.isEmpty()) {
                statusItem->setToolTip(task.errorMessage);
            }
            historyTable->setItem(row, 4, statusItem);

            // 如果是temp-ID任务，设置行背景色高亮
            if (isTempTaskId(task.taskId)) {
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

    const VideoTask task = DBManager::instance()->getTaskById(taskId);
    QAction *copyErrorAction = nullptr;
    if (!task.errorMessage.trimmed().isEmpty()) {
        copyErrorAction = contextMenu.addAction("📋 复制错误详情");
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
    } else if (copyErrorAction && selectedAction == copyErrorAction) {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(task.errorMessage.trimmed());
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
    bool rowMatched = false;
    for (int row = 0; row < historyTable->rowCount(); ++row) {
        QTableWidgetItem *taskIdItem = historyTable->item(row, 2);
        if (!taskIdItem || taskIdItem->text() != taskId) {
            continue;
        }

        rowMatched = true;

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

    if (!rowMatched) {
        refreshHistory();
    }
}

void VideoSingleHistoryTab::onTaskTimeout(const QString& taskId)
{
    onTaskStatusUpdated(taskId, "timeout", 0);

    if (!isVisible() || notifiedTaskFailures.contains(taskId)) {
        return;
    }

    notifiedTaskFailures.insert(taskId);
    QMessageBox::warning(this, "任务超时",
        QString("任务 %1 已超时，已按失败处理，请重试。")
            .arg(taskId));
}

void VideoSingleHistoryTab::onTaskFailed(const QString& taskId, const QString& error)
{
    VideoTask task = DBManager::instance()->getTaskById(taskId);
    const QString failedStatus = task.status.trimmed().isEmpty() ? "failed" : task.status;
    onTaskStatusUpdated(taskId, failedStatus, task.progress);

    if (!isVisible() || notifiedTaskFailures.contains(taskId)) {
        return;
    }

    notifiedTaskFailures.insert(taskId);
    const QString message = error.trimmed().isEmpty()
        ? QString("任务 %1 失败，请重试。").arg(taskId)
        : QString("任务 %1 失败，请重试。\n\n原因：%2").arg(taskId, error);
    QMessageBox::warning(this, "任务失败", message);
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
