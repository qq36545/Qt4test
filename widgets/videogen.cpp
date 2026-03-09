#include "videogen.h"
#include "../database/dbmanager.h"
#include "../network/veo3api.h"
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
#include <QResizeEvent>
#include <QTabBar>
#include <QPixmap>
#include <QEvent>
#include <QMouseEvent>
#include <QScrollArea>
#include <QStackedWidget>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <QDir>
#include <QSettings>
#include <QCryptographicHash>
#include <QCheckBox>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QMenu>
#include <QClipboard>
#include <QApplication>

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
      parametersModified(false)
{
    veo3API = new Veo3API(this);
    connect(veo3API, &Veo3API::videoCreated, this, &VideoSingleTab::onVideoCreated);
    connect(veo3API, &Veo3API::taskStatusUpdated, this, &VideoSingleTab::onTaskStatusUpdated);
    connect(veo3API, &Veo3API::errorOccurred, this, &VideoSingleTab::onApiError);

    setupUI();
    loadApiKeys();
    loadSettings();
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
    modelCombo->addItems({"sora2视频", "VEO3视频", "Grok3视频", "wan视频"});
    modelCombo->setCurrentIndex(1); // 默认选择 VEO3
    connect(modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::onModelChanged);
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
    connect(modelVariantCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::onModelVariantChanged);
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

    // 提示词输入
    QLabel *promptLabel = new QLabel("提示词:");
    promptLabel->setStyleSheet("font-size: 14px;");
    promptInput = new QTextEdit();
    promptInput->setPlaceholderText("输入视频生成提示词...\n例如：一只可爱的猫咪在花园里玩耍，阳光明媚，电影级画质");
    promptInput->setMinimumHeight(120);  // 支持多行输入和滚动
    contentLayout->addWidget(promptLabel);
    contentLayout->addWidget(promptInput);

    // 图片上传区域（首帧）
    imageLabel = new QLabel("首帧图片:");
    imageLabel->setStyleSheet("font-size: 14px;");
    contentLayout->addWidget(imageLabel);

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
    imageLayout->addWidget(imagePreviewLabel, 1);
    imageLayout->addWidget(uploadImageButton);
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
    watermarkCheckBox->setStyleSheet("color: white; font-size: 12px;");
    watermarkLayout->addWidget(watermarkLabel);
    watermarkLayout->addWidget(watermarkCheckBox);

    paramsLayout->addLayout(resLayout);
    paramsLayout->addLayout(durLayout);
    paramsLayout->addLayout(watermarkLayout);
    paramsLayout->addStretch();

    contentLayout->addLayout(paramsLayout);

    // 预览区域
    previewLabel = new QLabel();
    previewLabel->setObjectName("videoPreviewLabel");
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setText("💡 生成结果将显示在这里");
    previewLabel->setMinimumHeight(150);
    contentLayout->addWidget(previewLabel);

    // 生成按钮
    generateButton = new QPushButton("🚀 生成视频");
    generateButton->setCursor(Qt::PointingHandCursor);
    connect(generateButton, &QPushButton::clicked, this, &VideoSingleTab::generateVideo);
    contentLayout->addWidget(generateButton);

    // 设置滚动区域
    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);

    // 初始化UI状态
    onModelVariantChanged(0);

    // 连接参数变化信号 - 用于重复提交检测
    connect(promptInput, &QTextEdit::textChanged, this, &VideoSingleTab::onAnyParameterChanged);
    connect(modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::onAnyParameterChanged);
    connect(modelVariantCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::onAnyParameterChanged);
    connect(resolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::onAnyParameterChanged);
    connect(watermarkCheckBox, &QCheckBox::checkStateChanged, this, &VideoSingleTab::onAnyParameterChanged);

    // 连接参数变化信号 - 自动保存设置
    connect(promptInput, &QTextEdit::textChanged, this, &VideoSingleTab::saveSettings);
    connect(modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::saveSettings);
    connect(modelVariantCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::saveSettings);
    connect(apiKeyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::saveSettings);
    connect(serverCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::saveSettings);
    connect(resolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::saveSettings);
    connect(watermarkCheckBox, &QCheckBox::checkStateChanged, this, &VideoSingleTab::saveSettings);
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

void VideoSingleTab::onModelChanged(int index)
{
    // 模型切换时可以重新加载对应的密钥
    loadApiKeys();
}

void VideoSingleTab::uploadImage()
{
    QString modelName = modelVariantCombo->currentData().toString();
    bool isComponents = modelName.contains("components");

    if (isComponents && uploadedImagePaths.size() >= 3) {
        QMessageBox::warning(this, "提示", "components 模型最多支持 3 张图片");
        return;
    }

    bool isFrames = modelName.contains("frames");
    if (isFrames && uploadedImagePaths.size() >= 1) {
        QMessageBox::warning(this, "提示", "frames 模型只支持单张图片");
        return;
    }

    if (!isComponents && !isFrames && uploadedImagePaths.size() >= 1) {
        QMessageBox::warning(this, "提示", "当前模型只支持单张首帧图片");
        return;
    }

    // 读取上次图片上传路径
    QSettings settings("ChickenAI", "VideoGen");
    QString lastDir = settings.value("lastImageUploadDir", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)).toString();

    // 验证路径是否存在
    if (!QDir(lastDir).exists()) {
        lastDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }

    QString fileName = QFileDialog::getOpenFileName(
        this,
        "选择首帧图片",
        lastDir,
        "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif)"
    );

    if (!fileName.isEmpty()) {
        uploadedImagePaths.append(fileName);
        updateImagePreview();

        // 保存图片所在目录
        QFileInfo fileInfo(fileName);
        settings.setValue("lastImageUploadDir", fileInfo.absolutePath());

        // 保存图片路径到设置
        saveSettings();
    }
}

void VideoSingleTab::removeImage(int index)
{
    if (index >= 0 && index < uploadedImagePaths.size()) {
        uploadedImagePaths.removeAt(index);
        updateImagePreview();
        saveSettings();  // 保存更改
    }
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
}

void VideoSingleTab::uploadEndFrameImage()
{
    // 读取上次图片上传路径（与首帧共享）
    QSettings settings("ChickenAI", "VideoGen");
    QString lastDir = settings.value("lastImageUploadDir", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)).toString();

    // 验证路径是否存在
    if (!QDir(lastDir).exists()) {
        lastDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }

    QString fileName = QFileDialog::getOpenFileName(
        this,
        "选择尾帧图片",
        lastDir,
        "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif)"
    );

    if (!fileName.isEmpty()) {
        uploadedEndFrameImagePath = fileName;

        // 显示缩略图
        QPixmap pixmap(fileName);
        if (!pixmap.isNull()) {
            QPixmap scaledPixmap = pixmap.scaled(200, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            endFramePreviewLabel->setPixmap(scaledPixmap);
            endFramePreviewLabel->setText("");
        } else {
            endFramePreviewLabel->setPixmap(QPixmap());
            QFileInfo fileInfo(fileName);
            endFramePreviewLabel->setText("✓ " + fileInfo.fileName());
        }

        endFramePreviewLabel->setProperty("hasImage", true);
        // 强制刷新样式
        endFramePreviewLabel->style()->unpolish(endFramePreviewLabel);
        endFramePreviewLabel->style()->polish(endFramePreviewLabel);

        // 保存图片所在目录
        QFileInfo fileInfo(fileName);
        settings.setValue("lastImageUploadDir", fileInfo.absolutePath());

        // 保存图片路径到设置
        saveSettings();
    }
}

void VideoSingleTab::onModelVariantChanged(int index)
{
    QString modelName = modelVariantCombo->currentData().toString();
    updateImageUploadUI(modelName);
    updateResolutionOptions(modelName.contains("4K") || modelName.contains("4k"));
}

void VideoSingleTab::updateImageUploadUI(const QString &modelName)
{
    bool isComponents = modelName.contains("components");
    bool isFrames = modelName.contains("frames");

    if (isComponents) {
        // 支持1-3张首帧图片
        imageLabel->setText("首帧图片（1-3张）:");
        uploadImageButton->setText("📁 选择首帧图片");
        endFrameWidget->setVisible(false);
    } else if (isFrames) {
        // 仅支持单张首帧
        imageLabel->setText("首帧图片（单张）:");
        uploadImageButton->setText("📁 选择首帧图片");
        endFrameWidget->setVisible(false);
    } else {
        // 支持首尾帧
        imageLabel->setText("首帧图片:");
        uploadImageButton->setText("📁 选择首帧图片");
        endFrameWidget->setVisible(true);
    }
}

void VideoSingleTab::updateResolutionOptions(bool is4K)
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

void VideoSingleTab::generateVideo()
{
    QString prompt = promptInput->toPlainText().trimmed();
    if (prompt.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入视频生成提示词");
        return;
    }

    if (uploadedImagePaths.isEmpty()) {
        QMessageBox::warning(this, "提示", "请上传首帧图片");
        return;
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
    QString server = serverCombo->currentData().toString();
    QString resolution = resolutionCombo->currentData().toString();
    QString duration = durationCombo->currentData().toString();
    bool watermark = watermarkCheckBox->isChecked();
    int keyId = apiKeyCombo->currentData().toInt();

    // 获取 API Key
    ApiKey apiKeyData = DBManager::instance()->getApiKey(keyId);
    if (apiKeyData.apiKey.isEmpty()) {
        QMessageBox::warning(this, "错误", "无法获取 API 密钥");
        return;
    }

    // 调用 API 创建任务
    veo3API->createVideo(
        apiKeyData.apiKey,
        server,
        modelVariant,
        prompt,
        uploadedImagePaths,
        resolution,
        duration,
        watermark
    );

    // 注意：任务创建成功后会触发 onVideoCreated 回调
    // 在那里插入数据库并启动轮询
}

void VideoSingleTab::onVideoCreated(const QString &taskId, const QString &status)
{
    currentTaskId = taskId;

    // 复制图片到持久化存储
    QString persistentImagePath = copyImagesToPersistentStorage(taskId);

    // 获取当前参数
    QString prompt = promptInput->toPlainText().trimmed();
    QString modelVariantText = modelVariantCombo->currentText();  // 获取模型变体显示文本
    int keyId = apiKeyCombo->currentData().toInt();
    ApiKey apiKeyData = DBManager::instance()->getApiKey(keyId);
    QString server = serverCombo->currentData().toString();

    // 插入数据库
    VideoTask task;
    task.taskId = taskId;
    task.taskType = "video_single";
    task.prompt = prompt;
    task.modelVariant = modelVariantText;  // 保存模型变体
    task.status = "pending";
    task.progress = 0;

    DBManager::instance()->insertVideoTask(task);

    // 启动轮询
    TaskPollManager::getInstance()->startPolling(taskId, "video_single", apiKeyData.apiKey, server);

    // 保存当前参数哈希和设置
    lastSubmittedParamsHash = calculateParamsHash();
    parametersModified = false;
    saveSettings();

    // 显示提示
    QMessageBox::information(this, "任务已创建",
        QString("视频生成任务已创建！\n\n"
                "任务 ID: %1\n\n"
                "生成结果请在【生成历史记录】标签页查看。\n"
                "系统将自动轮询任务状态并下载完成的视频。")
        .arg(taskId));

    // 不清空输入，保留参数供用户继续使用
    // 参数已通过 saveSettings() 自动保存
}

void VideoSingleTab::onTaskStatusUpdated(const QString &taskId, const QString &status, const QString &videoUrl, int progress)
{
    // 不再需要在这里显示状态，由历史记录页面负责
    // 保留此方法以兼容 Veo3API 的信号
}

void VideoSingleTab::onApiError(const QString &error)
{
    QMessageBox::critical(this, "错误", QString("API 调用失败:\n%1").arg(error));
}

void VideoSingleTab::saveSettings()
{
    QSettings settings("ChickenAI", "VideoGen");
    settings.beginGroup("VideoSingleTab");

    settings.setValue("prompt", promptInput->toPlainText());
    settings.setValue("model", modelCombo->currentIndex());
    settings.setValue("modelVariant", modelVariantCombo->currentIndex());
    settings.setValue("apiKey", apiKeyCombo->currentIndex());
    settings.setValue("server", serverCombo->currentIndex());
    settings.setValue("resolution", resolutionCombo->currentIndex());
    settings.setValue("watermark", watermarkCheckBox->isChecked());
    settings.setValue("imagePaths", uploadedImagePaths);
    settings.setValue("endFrameImagePath", uploadedEndFrameImagePath);
    settings.setValue("lastSubmittedHash", lastSubmittedParamsHash);

    settings.endGroup();
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

    promptInput->setPlainText(settings.value("prompt", "").toString());

    int modelIndex = settings.value("model", 1).toInt();  // 默认 VEO3
    if (modelIndex >= 0 && modelIndex < modelCombo->count()) {
        modelCombo->setCurrentIndex(modelIndex);
    }

    int variantIndex = settings.value("modelVariant", 0).toInt();
    if (variantIndex >= 0 && variantIndex < modelVariantCombo->count()) {
        modelVariantCombo->setCurrentIndex(variantIndex);
    }

    int apiKeyIndex = settings.value("apiKey", 0).toInt();
    if (apiKeyIndex >= 0 && apiKeyIndex < apiKeyCombo->count()) {
        apiKeyCombo->setCurrentIndex(apiKeyIndex);
    }

    int serverIndex = settings.value("server", 0).toInt();
    if (serverIndex >= 0 && serverIndex < serverCombo->count()) {
        serverCombo->setCurrentIndex(serverIndex);
    }

    int resIndex = settings.value("resolution", 1).toInt();  // 默认竖屏
    if (resIndex >= 0 && resIndex < resolutionCombo->count()) {
        resolutionCombo->setCurrentIndex(resIndex);
    }

    watermarkCheckBox->setChecked(settings.value("watermark", false).toBool());

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

    lastSubmittedParamsHash = settings.value("lastSubmittedHash", "").toString();

    settings.endGroup();

    // 恢复信号发射
    promptInput->blockSignals(false);
    modelCombo->blockSignals(false);
    modelVariantCombo->blockSignals(false);
    apiKeyCombo->blockSignals(false);
    serverCombo->blockSignals(false);
    resolutionCombo->blockSignals(false);
    watermarkCheckBox->blockSignals(false);

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
    modelCombo->addItems({"sora2视频", "VEO3视频", "Grok3视频", "wan视频"});
    modelCombo->setCurrentIndex(1); // 默认选择 VEO3
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
    QLabel *promptLabel = new QLabel("批量提示词（每行一个）:");
    promptLabel->setStyleSheet("font-size: 14px;");
    promptInput = new QTextEdit();
    promptInput->setPlaceholderText("输入多个提示词，每行一个...\n例如：\n一只猫在花园玩耍\n日落时的海滩\n城市夜景");
    promptInput->setMinimumHeight(120);  // 支持多行输入和滚动
    contentLayout->addWidget(promptLabel);
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
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    // 创建 Tab Widget
    tabWidget = new QTabWidget();

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
    : QWidget(parent), isListView(true), currentOffset(0)
{
    setupUI();
    loadHistory();
}

void VideoSingleHistoryTab::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 20, 0, 20);
    mainLayout->setSpacing(15);

    // 标题
    QLabel *titleLabel = new QLabel("📜 AI生成历史记录");
    titleLabel->setStyleSheet("font-size: 24px; font-weight: bold;");
    mainLayout->addWidget(titleLabel);

    // 顶部工具栏
    QHBoxLayout *toolbarLayout = new QHBoxLayout();

    switchViewButton = new QPushButton("📷 缩略图视图");
    connect(switchViewButton, &QPushButton::clicked, this, &VideoSingleHistoryTab::switchView);

    refreshButton = new QPushButton("🔄 刷新");
    connect(refreshButton, &QPushButton::clicked, this, &VideoSingleHistoryTab::refreshHistory);

    toolbarLayout->addWidget(switchViewButton);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(refreshButton);

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

void VideoSingleHistoryTab::setupListView()
{
    listViewWidget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(listViewWidget);
    layout->setContentsMargins(0, 0, 0, 0);

    historyTable = new QTableWidget();
    historyTable->setColumnCount(8);
    historyTable->setHorizontalHeaderLabels({
        "序号", "任务ID", "提示词", "状态", "进度", "创建时间", "视频类型", "操作"
    });

    // 序号列 - 可调整
    historyTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    historyTable->setColumnWidth(0, 50);

    // 任务ID列 - 可调整
    historyTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    historyTable->setColumnWidth(1, 100);

    // 提示词列 - 可调整
    historyTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    historyTable->setColumnWidth(2, 200);

    // 状态列 - 可调整
    historyTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    historyTable->setColumnWidth(3, 80);

    // 进度列 - 可调整
    historyTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);
    historyTable->setColumnWidth(4, 80);

    // 创建时间列 - 可调整
    historyTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Interactive);
    historyTable->setColumnWidth(5, 150);

    // 视频类型列 - 可调整
    historyTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Interactive);
    historyTable->setColumnWidth(6, 120);

    // 操作列 - 扩展宽度以容纳新按钮
    historyTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Stretch);

    historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    historyTable->verticalHeader()->setVisible(false);
    historyTable->verticalHeader()->setDefaultSectionSize(90);  // 增加行高

    // 启用右键菜单
    historyTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(historyTable, &QTableWidget::customContextMenuRequested, this, &VideoSingleHistoryTab::showContextMenu);

    layout->addWidget(historyTable);
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
        QList<VideoTask> tasks = DBManager::instance()->getTasksByType("video_single", offset, limit);

        int row = 0;
        for (const VideoTask& task : tasks) {
            historyTable->insertRow(row);

            // 序号
            historyTable->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));

            // 任务ID - 显示完整ID，让Qt自动省略
            QTableWidgetItem *taskIdItem = new QTableWidgetItem(task.taskId);
            taskIdItem->setToolTip(task.taskId);  // 鼠标悬停显示完整ID
            historyTable->setItem(row, 1, taskIdItem);

            // 提示词 - 显示完整提示词，让Qt自动省略
            QTableWidgetItem *promptItem = new QTableWidgetItem(task.prompt);
            promptItem->setToolTip(task.prompt);  // 鼠标悬停显示完整提示词
            historyTable->setItem(row, 2, promptItem);

            // 状态
            QString statusText;
            if (task.status == "pending") statusText = "⏳ 等待中";
            else if (task.status == "processing") statusText = "🔄 处理中";
            else if (task.status == "completed") statusText = "✅ 已完成";
            else if (task.status == "failed") statusText = "❌ 失败";
            else if (task.status == "timeout") statusText = "⏱️ 超时";
            historyTable->setItem(row, 3, new QTableWidgetItem(statusText));

            // 进度
            historyTable->setItem(row, 4, new QTableWidgetItem(QString::number(task.progress) + "%"));

            // 创建时间
            historyTable->setItem(row, 5, new QTableWidgetItem(task.createdAt.toString("yyyy-MM-dd HH:mm:ss")));

            // 视频类型（模型变体）
            QString videoType = task.modelVariant.isEmpty() ? "-" : task.modelVariant;
            historyTable->setItem(row, 6, new QTableWidgetItem(videoType));

            // 操作按钮
            QWidget *btnWidget = new QWidget();
            QHBoxLayout *btnLayout = new QHBoxLayout(btnWidget);
            btnLayout->setContentsMargins(5, 2, 5, 2);
            btnLayout->setSpacing(5);

            QPushButton *viewBtn = new QPushButton("查看");
            QPushButton *browseBtn = new QPushButton("浏览");
            QPushButton *refreshBtn = new QPushButton("刷新");
            QPushButton *regenerateBtn = new QPushButton("重新生成");

            viewBtn->setMaximumWidth(60);
            browseBtn->setMaximumWidth(60);
            refreshBtn->setMaximumWidth(60);
            regenerateBtn->setMaximumWidth(80);

            connect(viewBtn, &QPushButton::clicked, [this, task]() {
                onViewVideo(task.taskId);
            });
            connect(browseBtn, &QPushButton::clicked, [this, task]() {
                onBrowseFile(task.taskId);
            });
            connect(refreshBtn, &QPushButton::clicked, [this, task]() {
                onRetryQuery(task.taskId);
            });
            connect(regenerateBtn, &QPushButton::clicked, [this, task]() {
                onRegenerate(task.taskId);
            });

            btnLayout->addWidget(viewBtn);
            btnLayout->addWidget(browseBtn);
            btnLayout->addWidget(refreshBtn);
            btnLayout->addWidget(regenerateBtn);

            historyTable->setCellWidget(row, 7, btnWidget);

            row++;
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
            QLabel* statusLabel = new QLabel();
            if (task.status == "completed") statusLabel->setText("✅");
            else if (task.status == "processing") statusLabel->setText("🔄");
            else if (task.status == "pending") statusLabel->setText("⏳");
            else if (task.status == "failed") statusLabel->setText("❌");
            else if (task.status == "timeout") statusLabel->setText("⏱️");
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
            viewBtn->setMaximumWidth(70);
            browseBtn->setMaximumWidth(70);

            connect(viewBtn, &QPushButton::clicked, [this, task]() {
                onViewVideo(task.taskId);
            });
            connect(browseBtn, &QPushButton::clicked, [this, task]() {
                onBrowseFile(task.taskId);
            });

            btnLayout->addWidget(viewBtn);
            btnLayout->addWidget(browseBtn);

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

    // 检查视频是否已下载
    if (task.videoPath.isEmpty() || !QFile::exists(task.videoPath)) {
        // 视频未下载，尝试下载
        if (task.videoUrl.isEmpty()) {
            QMessageBox::warning(this, "提示", "视频尚未生成完成，请稍后再试");
            return;
        }

        QMessageBox::StandardButton reply = QMessageBox::question(this, "下载视频",
            "视频尚未下载，是否立即下载？",
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            // TODO: 触发下载
            QMessageBox::information(this, "提示", "下载功能待实现");
        }
        return;
    }

    // 打开视频文件
    QDesktopServices::openUrl(QUrl::fromLocalFile(task.videoPath));
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

void VideoSingleHistoryTab::onRetryQuery(const QString& taskId)
{
    // 查询任务信息
    VideoTask task = DBManager::instance()->getTaskById(taskId);

    if (task.taskId.isEmpty()) {
        QMessageBox::warning(this, "错误", "未找到任务记录");
        return;
    }

    // 重新启动轮询（需要 API Key 和 baseUrl）
    // TODO: 从数据库或配置中获取这些信息
    QMessageBox::information(this, "提示",
        "重新查询功能需要存储 API Key 和服务器信息\n"
        "当前版本暂不支持，请重新生成任务");
}

void VideoSingleHistoryTab::onRegenerate(const QString& taskId)
{
    // 获取任务信息
    QList<VideoTask> tasks = DBManager::instance()->getTasksByType("video_single");
    VideoTask targetTask;
    bool found = false;

    for (const VideoTask& task : tasks) {
        if (task.taskId == taskId) {
            targetTask = task;
            found = true;
            break;
        }
    }

    if (!found) {
        QMessageBox::warning(this, "错误", "未找到任务信息");
        return;
    }

    // 确认对话框
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "确认重新生成",
        "确定要使用相同的提示词重新生成视频吗？",
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        // TODO: 调用 API 重新生成视频
        // 这里需要根据实际的 API 调用逻辑来实现
        QMessageBox::information(this, "提示", "重新生成功能待实现");
    }
}

void VideoSingleHistoryTab::showContextMenu(const QPoint &pos)
{
    // 获取点击的单元格
    QTableWidgetItem *item = historyTable->itemAt(pos);
    if (!item) {
        return;
    }

    // 创建右键菜单
    QMenu contextMenu(this);
    contextMenu.setStyleSheet("QMenu { font-size: 14px; } QMenu::item { padding: 5px 20px; }");
    QAction *copyAction = contextMenu.addAction("📋 复制");

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
    }
}
