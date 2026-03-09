#include "videogen.h"
#include "../database/dbmanager.h"
#include "../network/veo3api.h"
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
    titleLabel->setStyleSheet("font-size: 28px; font-weight: bold; color: #F8FAFC;");
    mainLayout->addWidget(titleLabel);

    // Tab Widget
    tabWidget = new QTabWidget();
    tabWidget->setObjectName("videoTabWidget");

    singleTab = new VideoSingleTab();
    batchTab = new VideoBatchTab();
    historyTab = new VideoHistoryTab();

    tabWidget->addTab(singleTab, "AI视频生成-单个");
    tabWidget->addTab(batchTab, "AI视频生成-批量");
    tabWidget->addTab(historyTab, "生成历史记录");

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
    : QWidget(parent)
{
    veo3API = new Veo3API(this);
    connect(veo3API, &Veo3API::videoCreated, this, &VideoSingleTab::onVideoCreated);
    connect(veo3API, &Veo3API::taskStatusUpdated, this, &VideoSingleTab::onTaskStatusUpdated);
    connect(veo3API, &Veo3API::errorOccurred, this, &VideoSingleTab::onApiError);

    setupUI();
    loadApiKeys();
}

void VideoSingleTab::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    // 模型选择
    QHBoxLayout *modelLayout = new QHBoxLayout();
    QLabel *modelLabel = new QLabel("视频模型:");
    modelLabel->setStyleSheet("color: #F8FAFC; font-size: 14px;");
    modelCombo = new QComboBox();
    modelCombo->addItems({"sora2视频", "VEO3视频", "Grok3视频", "wan视频"});
    modelCombo->setCurrentIndex(1); // 默认选择 VEO3
    connect(modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoSingleTab::onModelChanged);
    modelLayout->addWidget(modelLabel);
    modelLayout->addWidget(modelCombo, 1);
    mainLayout->addLayout(modelLayout);

    // VEO3 模型变体选择
    QHBoxLayout *variantLayout = new QHBoxLayout();
    QLabel *variantLabel = new QLabel("模型变体:");
    variantLabel->setStyleSheet("color: #F8FAFC; font-size: 14px;");
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
    mainLayout->addLayout(variantLayout);

    // API Key 选择
    QHBoxLayout *keyLayout = new QHBoxLayout();
    QLabel *keyLabel = new QLabel("API 密钥:");
    keyLabel->setStyleSheet("color: #F8FAFC; font-size: 14px;");
    apiKeyCombo = new QComboBox();
    addKeyButton = new QPushButton("➕ 添加密钥");
    connect(addKeyButton, &QPushButton::clicked, []() {
        QMessageBox::information(nullptr, "提示", "请前往配置页面添加密钥");
    });
    keyLayout->addWidget(keyLabel);
    keyLayout->addWidget(apiKeyCombo, 1);
    keyLayout->addWidget(addKeyButton);
    mainLayout->addLayout(keyLayout);

    // 请求服务器选择
    QHBoxLayout *serverLayout = new QHBoxLayout();
    QLabel *serverLabel = new QLabel("请求服务器:");
    serverLabel->setStyleSheet("color: #F8FAFC; font-size: 14px;");
    serverCombo = new QComboBox();
    serverCombo->addItem("【主站】https://api.kegeai.top", "https://api.kegeai.top");
    serverCombo->addItem("【备用】https://api.kuai.host", "https://api.kuai.host");
    serverCombo->setCurrentIndex(0); // 默认选择主站
    serverLayout->addWidget(serverLabel);
    serverLayout->addWidget(serverCombo, 1);
    mainLayout->addLayout(serverLayout);

    // 提示词输入
    QLabel *promptLabel = new QLabel("提示词:");
    promptLabel->setStyleSheet("color: #F8FAFC; font-size: 14px;");
    promptInput = new QTextEdit();
    promptInput->setPlaceholderText("输入视频生成提示词...\n例如：一只可爱的猫咪在花园里玩耍，阳光明媚，电影级画质");
    promptInput->setMinimumHeight(100);
    mainLayout->addWidget(promptLabel);
    mainLayout->addWidget(promptInput);

    // 图片上传区域（首帧）
    imageLabel = new QLabel("首帧图片:");
    imageLabel->setStyleSheet("color: #F8FAFC; font-size: 14px;");
    mainLayout->addWidget(imageLabel);

    QHBoxLayout *imageLayout = new QHBoxLayout();
    imagePreviewLabel = new QLabel("未选择图片");
    imagePreviewLabel->setStyleSheet(
        "background: rgba(30, 27, 75, 0.5);"
        "border: 1px solid rgba(248, 250, 252, 0.1);"
        "border-radius: 8px;"
        "color: #64748B;"
        "padding: 10px;"
        "min-height: 60px;"
    );
    imagePreviewLabel->setAlignment(Qt::AlignCenter);
    uploadImageButton = new QPushButton("📁 选择首帧图片");
    uploadImageButton->setFixedWidth(150);
    connect(uploadImageButton, &QPushButton::clicked, this, &VideoSingleTab::uploadImage);
    imageLayout->addWidget(imagePreviewLabel, 1);
    imageLayout->addWidget(uploadImageButton);
    mainLayout->addLayout(imageLayout);

    // 尾帧图片上传区域（默认隐藏，根据模型动态显示）
    endFrameWidget = new QWidget();
    QVBoxLayout *endFrameLayout = new QVBoxLayout(endFrameWidget);
    endFrameLayout->setContentsMargins(0, 0, 0, 0);
    endFrameLayout->setSpacing(10);

    endFrameLabel = new QLabel("尾帧图片（可选）:");
    endFrameLabel->setStyleSheet("color: #F8FAFC; font-size: 14px;");
    endFrameLayout->addWidget(endFrameLabel);

    QHBoxLayout *endFrameImageLayout = new QHBoxLayout();
    endFramePreviewLabel = new QLabel("未选择图片");
    endFramePreviewLabel->setStyleSheet(
        "background: rgba(30, 27, 75, 0.5);"
        "border: 1px solid rgba(248, 250, 252, 0.1);"
        "border-radius: 8px;"
        "color: #64748B;"
        "padding: 10px;"
        "min-height: 60px;"
    );
    endFramePreviewLabel->setAlignment(Qt::AlignCenter);
    uploadEndFrameButton = new QPushButton("📁 选择尾帧图片");
    uploadEndFrameButton->setFixedWidth(150);
    connect(uploadEndFrameButton, &QPushButton::clicked, this, &VideoSingleTab::uploadEndFrameImage);
    endFrameImageLayout->addWidget(endFramePreviewLabel, 1);
    endFrameImageLayout->addWidget(uploadEndFrameButton);
    endFrameLayout->addLayout(endFrameImageLayout);

    mainLayout->addWidget(endFrameWidget);
    endFrameWidget->setVisible(true); // 默认显示，支持首尾帧

    // 参数设置
    QHBoxLayout *paramsLayout = new QHBoxLayout();

    QVBoxLayout *resLayout = new QVBoxLayout();
    QLabel *resLabel = new QLabel("分辨率");
    resLabel->setStyleSheet("color: #94A3B8; font-size: 14px;");
    resolutionCombo = new QComboBox();
    resolutionCombo->addItem("横屏 16:9 (1280x720)", "1280x720");
    resolutionCombo->addItem("竖屏 9:16 (720x1280)", "720x1280");
    resLayout->addWidget(resLabel);
    resLayout->addWidget(resolutionCombo);

    QVBoxLayout *durLayout = new QVBoxLayout();
    QLabel *durLabel = new QLabel("时长（秒）");
    durLabel->setStyleSheet("color: #94A3B8; font-size: 14px;");
    durationCombo = new QComboBox();
    durationCombo->addItem("8秒（固定）", "8");
    durationCombo->setEnabled(false); // VEO3 固定 8 秒
    durLayout->addWidget(durLabel);
    durLayout->addWidget(durationCombo);

    QVBoxLayout *watermarkLayout = new QVBoxLayout();
    QLabel *watermarkLabel = new QLabel("水印");
    watermarkLabel->setStyleSheet("color: #94A3B8; font-size: 14px;");
    watermarkCheckBox = new QCheckBox("添加水印");
    watermarkCheckBox->setStyleSheet("color: #F8FAFC;");
    watermarkLayout->addWidget(watermarkLabel);
    watermarkLayout->addWidget(watermarkCheckBox);

    paramsLayout->addLayout(resLayout);
    paramsLayout->addLayout(durLayout);
    paramsLayout->addLayout(watermarkLayout);
    paramsLayout->addStretch();

    mainLayout->addLayout(paramsLayout);

    // 预览区域
    previewLabel = new QLabel();
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setText("生成结果将显示在这里");
    previewLabel->setMinimumHeight(300);
    previewLabel->setStyleSheet(
        "background: rgba(30, 27, 75, 0.5);"
        "border: 1px solid rgba(248, 250, 252, 0.1);"
        "border-radius: 12px;"
        "color: #64748B;"
        "font-size: 16px;"
    );
    mainLayout->addWidget(previewLabel);

    // 生成按钮
    generateButton = new QPushButton("🚀 生成视频");
    generateButton->setCursor(Qt::PointingHandCursor);
    connect(generateButton, &QPushButton::clicked, this, &VideoSingleTab::generateVideo);
    mainLayout->addWidget(generateButton);

    // 初始化UI状态
    onModelVariantChanged(0);
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

    QString fileName = QFileDialog::getOpenFileName(
        this,
        "选择首帧图片",
        "",
        "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif)"
    );

    if (!fileName.isEmpty()) {
        uploadedImagePaths.append(fileName);
        updateImagePreview();
    }
}

void VideoSingleTab::removeImage(int index)
{
    if (index >= 0 && index < uploadedImagePaths.size()) {
        uploadedImagePaths.removeAt(index);
        updateImagePreview();
    }
}

void VideoSingleTab::updateImagePreview()
{
    if (uploadedImagePaths.isEmpty()) {
        imagePreviewLabel->setText("未选择图片");
        imagePreviewLabel->setStyleSheet(
            "background: rgba(30, 27, 75, 0.5);"
            "border: 1px solid rgba(248, 250, 252, 0.1);"
            "border-radius: 8px;"
            "color: #64748B;"
            "padding: 10px;"
            "min-height: 60px;"
        );
    } else {
        QString text = QString("✓ 已选择 %1 张图片\n").arg(uploadedImagePaths.size());
        for (int i = 0; i < uploadedImagePaths.size(); ++i) {
            QFileInfo fileInfo(uploadedImagePaths[i]);
            text += QString("%1. %2\n").arg(i + 1).arg(fileInfo.fileName());
        }
        imagePreviewLabel->setText(text.trimmed());
        imagePreviewLabel->setStyleSheet(
            "background: rgba(34, 197, 94, 0.1);"
            "border: 1px solid rgba(34, 197, 94, 0.3);"
            "border-radius: 8px;"
            "color: #22C55E;"
            "padding: 10px;"
            "min-height: 60px;"
        );
    }
}

void VideoSingleTab::uploadEndFrameImage()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "选择尾帧图片",
        "",
        "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif)"
    );

    if (!fileName.isEmpty()) {
        uploadedEndFrameImagePath = fileName;
        QFileInfo fileInfo(fileName);
        endFramePreviewLabel->setText("✓ " + fileInfo.fileName());
        endFramePreviewLabel->setStyleSheet(
            "background: rgba(34, 197, 94, 0.1);"
            "border: 1px solid rgba(34, 197, 94, 0.3);"
            "border-radius: 8px;"
            "color: #22C55E;"
            "padding: 10px;"
            "min-height: 60px;"
        );
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

    QString model = modelCombo->currentText();
    QString modelVariant = modelVariantCombo->currentData().toString();
    QString server = serverCombo->currentData().toString();
    QString resolution = resolutionCombo->currentData().toString();
    QString duration = durationCombo->currentData().toString();
    bool watermark = watermarkCheckBox->isChecked();
    int keyId = apiKeyCombo->currentData().toInt();

    // 构建参数字符串
    QString imageInfo;
    if (uploadedImagePaths.size() == 1) {
        imageInfo = QString("首帧:%1").arg(QFileInfo(uploadedImagePaths[0]).fileName());
    } else {
        imageInfo = QString("首帧(%1张):").arg(uploadedImagePaths.size());
        for (int i = 0; i < uploadedImagePaths.size(); ++i) {
            imageInfo += QString("%1").arg(QFileInfo(uploadedImagePaths[i]).fileName());
            if (i < uploadedImagePaths.size() - 1) imageInfo += ", ";
        }
    }

    QString params = QString("服务器:%1, 分辨率:%2, 时长:%3秒, 水印:%4, %5")
        .arg(server)
        .arg(resolution)
        .arg(duration)
        .arg(watermark ? "是" : "否")
        .arg(imageInfo);

    // 如果有尾帧图片
    if (!uploadedEndFrameImagePath.isEmpty()) {
        params += QString(", 尾帧:%1").arg(QFileInfo(uploadedEndFrameImagePath).fileName());
    }

    // 保存到历史记录
    GenerationHistory history;
    history.date = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    history.type = "single";
    history.modelType = "video";
    history.modelName = model + " (" + modelVariant + ")";
    history.prompt = prompt;
    history.parameters = params;
    history.status = "pending";

    int historyId = DBManager::instance()->addGenerationHistory(history);

    QString infoMsg = QString("正在生成视频...\n\n"
                "模型: %1\n"
                "模型变体: %2\n"
                "服务器: %3\n"
                "提示词: %4\n"
                "分辨率: %5\n"
                "时长: %6秒\n"
                "水印: %7\n"
                "%8\n")
        .arg(model)
        .arg(modelVariant)
        .arg(server)
        .arg(prompt)
        .arg(resolution)
        .arg(duration)
        .arg(watermark ? "是" : "否")
        .arg(imageInfo);

    if (!uploadedEndFrameImagePath.isEmpty()) {
        infoMsg += QString("尾帧: %1\n").arg(QFileInfo(uploadedEndFrameImagePath).fileName());
    }

    infoMsg += QString("\n正在调用 API 生成视频...\n历史记录 #%1").arg(historyId);

    QMessageBox::information(this, "生成中", infoMsg);

    // 获取 API Key
    ApiKey apiKeyData = DBManager::instance()->getApiKey(keyId);
    if (apiKeyData.apiKey.isEmpty()) {
        QMessageBox::warning(this, "错误", "无法获取 API 密钥");
        return;
    }

    // 调用 API
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
}

void VideoSingleTab::onVideoCreated(const QString &taskId, const QString &status)
{
    currentTaskId = taskId;
    previewLabel->setText(QString("视频任务已创建\n任务 ID: %1\n状态: %2").arg(taskId).arg(status));
    QMessageBox::information(this, "成功", QString("视频生成任务已创建\n任务 ID: %1\n状态: %2").arg(taskId).arg(status));
}

void VideoSingleTab::onTaskStatusUpdated(const QString &taskId, const QString &status, const QString &videoUrl, int progress)
{
    previewLabel->setText(QString("任务 ID: %1\n状态: %2\n进度: %3%\n视频 URL: %4")
                          .arg(taskId).arg(status).arg(progress).arg(videoUrl));

    if (status == "completed" && !videoUrl.isEmpty()) {
        QMessageBox::information(this, "完成", QString("视频生成完成！\n\n视频 URL:\n%1").arg(videoUrl));
    }
}

void VideoSingleTab::onApiError(const QString &error)
{
    QMessageBox::critical(this, "错误", QString("API 调用失败:\n%1").arg(error));
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
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    // 模型选择
    QHBoxLayout *modelLayout = new QHBoxLayout();
    QLabel *modelLabel = new QLabel("视频模型:");
    modelLabel->setStyleSheet("color: #F8FAFC; font-size: 14px;");
    modelCombo = new QComboBox();
    modelCombo->addItems({"sora2视频", "VEO3视频", "Grok3视频", "wan视频"});
    modelCombo->setCurrentIndex(1); // 默认选择 VEO3
    connect(modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoBatchTab::onModelChanged);
    modelLayout->addWidget(modelLabel);
    modelLayout->addWidget(modelCombo, 1);
    mainLayout->addLayout(modelLayout);

    // VEO3 模型变体选择
    QHBoxLayout *variantLayout = new QHBoxLayout();
    QLabel *variantLabel = new QLabel("模型变体:");
    variantLabel->setStyleSheet("color: #F8FAFC; font-size: 14px;");
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
    mainLayout->addLayout(variantLayout);

    // API Key 选择
    QHBoxLayout *keyLayout = new QHBoxLayout();
    QLabel *keyLabel = new QLabel("API 密钥:");
    keyLabel->setStyleSheet("color: #F8FAFC; font-size: 14px;");
    apiKeyCombo = new QComboBox();
    addKeyButton = new QPushButton("➕ 添加密钥");
    connect(addKeyButton, &QPushButton::clicked, []() {
        QMessageBox::information(nullptr, "提示", "请前往配置页面添加密钥");
    });
    keyLayout->addWidget(keyLabel);
    keyLayout->addWidget(apiKeyCombo, 1);
    keyLayout->addWidget(addKeyButton);
    mainLayout->addLayout(keyLayout);

    // 请求服务器选择
    QHBoxLayout *serverLayout = new QHBoxLayout();
    QLabel *serverLabel = new QLabel("请求服务器:");
    serverLabel->setStyleSheet("color: #F8FAFC; font-size: 14px;");
    serverCombo = new QComboBox();
    serverCombo->addItem("【主站】https://api.kegeai.top", "https://api.kegeai.top");
    serverCombo->addItem("【备用】https://api.kuai.host", "https://api.kuai.host");
    serverCombo->setCurrentIndex(0); // 默认选择主站
    serverLayout->addWidget(serverLabel);
    serverLayout->addWidget(serverCombo, 1);
    mainLayout->addLayout(serverLayout);

    // 提示词输入（多行）
    QLabel *promptLabel = new QLabel("批量提示词（每行一个）:");
    promptLabel->setStyleSheet("color: #F8FAFC; font-size: 14px;");
    promptInput = new QTextEdit();
    promptInput->setPlaceholderText("输入多个提示词，每行一个...\n例如：\n一只猫在花园玩耍\n日落时的海滩\n城市夜景");
    promptInput->setMinimumHeight(150);
    mainLayout->addWidget(promptLabel);
    mainLayout->addWidget(promptInput);

    // 图片拖放区域
    imageLabel = new QLabel("图片（可选，拖放图片到下方区域）:");
    imageLabel->setStyleSheet("color: #F8FAFC; font-size: 14px;");
    imageDropArea = new QLabel();
    imageDropArea->setAlignment(Qt::AlignCenter);
    imageDropArea->setText("📁 拖放图片到这里\n（图生视频）");
    imageDropArea->setMinimumHeight(100);
    imageDropArea->setStyleSheet(
        "background: rgba(30, 27, 75, 0.5);"
        "border: 2px dashed rgba(248, 250, 252, 0.3);"
        "border-radius: 12px;"
        "color: #94A3B8;"
        "font-size: 14px;"
    );
    imageDropArea->setAcceptDrops(true);
    mainLayout->addWidget(imageLabel);
    mainLayout->addWidget(imageDropArea);

    // 图片列表
    imageList = new QListWidget();
    imageList->setMaximumHeight(100);
    mainLayout->addWidget(imageList);

    // 尾帧提示（根据模型动态显示）
    endFrameWidget = new QWidget();
    QVBoxLayout *endFrameLayout = new QVBoxLayout(endFrameWidget);
    endFrameLayout->setContentsMargins(0, 0, 0, 0);
    endFrameLabel = new QLabel("提示：当前模型支持首尾帧垫图");
    endFrameLabel->setStyleSheet("color: #94A3B8; font-size: 12px; font-style: italic;");
    endFrameLayout->addWidget(endFrameLabel);
    mainLayout->addWidget(endFrameWidget);
    endFrameWidget->setVisible(true);

    // 参数设置
    QHBoxLayout *paramsLayout = new QHBoxLayout();

    QVBoxLayout *resLayout = new QVBoxLayout();
    QLabel *resLabel = new QLabel("分辨率");
    resLabel->setStyleSheet("color: #94A3B8; font-size: 14px;");
    resolutionCombo = new QComboBox();
    resolutionCombo->addItem("横屏 16:9 (1280x720)", "1280x720");
    resolutionCombo->addItem("竖屏 9:16 (720x1280)", "720x1280");
    resLayout->addWidget(resLabel);
    resLayout->addWidget(resolutionCombo);

    QVBoxLayout *durLayout = new QVBoxLayout();
    QLabel *durLabel = new QLabel("时长（秒）");
    durLabel->setStyleSheet("color: #94A3B8; font-size: 14px;");
    durationCombo = new QComboBox();
    durationCombo->addItem("8秒（固定）", "8");
    durationCombo->setEnabled(false); // VEO3 固定 8 秒
    durLayout->addWidget(durLabel);
    durLayout->addWidget(durationCombo);

    QVBoxLayout *watermarkLayout = new QVBoxLayout();
    QLabel *watermarkLabel = new QLabel("水印");
    watermarkLabel->setStyleSheet("color: #94A3B8; font-size: 14px;");
    watermarkCheckBox = new QCheckBox("添加水印");
    watermarkCheckBox->setStyleSheet("color: #F8FAFC;");
    watermarkLayout->addWidget(watermarkLabel);
    watermarkLayout->addWidget(watermarkCheckBox);

    paramsLayout->addLayout(resLayout);
    paramsLayout->addLayout(durLayout);
    paramsLayout->addLayout(watermarkLayout);
    paramsLayout->addStretch();

    mainLayout->addLayout(paramsLayout);

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
    mainLayout->addLayout(buttonLayout);

    // 生成按钮
    generateButton = new QPushButton("🚀 批量生成视频");
    generateButton->setCursor(Qt::PointingHandCursor);
    connect(generateButton, &QPushButton::clicked, this, &VideoBatchTab::generateBatch);
    mainLayout->addWidget(generateButton);

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
    QString fileName = QFileDialog::getOpenFileName(this, "导入 CSV", "", "CSV Files (*.csv)");
    if (!fileName.isEmpty()) {
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
