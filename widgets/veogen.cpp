#include "veogen.h"
#include "../network/videoapi.h"
#include "../network/taskpollmanager.h"
#include "../database/dbmanager.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QSettings>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QStandardPaths>
#include <QImageReader>
#include <QDateTime>
#include <QRandomGenerator>
#include <QSpinBox>
#include <QRegularExpression>

// VeoGenPage 实现
VeoGenPage::VeoGenPage(QWidget *parent)
    : QWidget(parent),
      suppressDuplicateWarning(false),
      parametersModified(false),
      pendingSaveSettings(false),
      suppressAutoSave(false)
{
    api = new VideoAPI(this);
    connect(api, &VideoAPI::videoCreated, this, &VeoGenPage::onVideoCreated);
    connect(api, &VideoAPI::taskStatusUpdated, this, &VeoGenPage::onTaskStatusUpdated);
    connect(api, &VideoAPI::errorOccurred, this, &VeoGenPage::onApiError);

    setupUI();
    loadApiKeys();
    loadSettings();

    // 加载配置后再连接信号
    connectSignals();
}

void VeoGenPage::setupUI()
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

    // ========== 变体类型单选按钮 ==========
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

    // ========== 模型变体选择 ==========
    QHBoxLayout *variantLayout = new QHBoxLayout();
    QLabel *variantLabel = new QLabel("模型变体:");
    variantLabel->setStyleSheet("font-size: 14px;");
    modelVariantCombo = new QComboBox();
    modelVariantCombo->addItem("veo_3_1-fast", "veo_3_1-fast");
    modelVariantCombo->addItem("veo_3_1", "veo_3_1");
    modelVariantCombo->addItem("veo_3_1-fast-4K", "veo_3_1-fast-4K");
    modelVariantCombo->addItem("veo_3_1-fast-components-4K", "veo_3_1-fast-components-4K");
    connect(modelVariantCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VeoGenPage::onModelVariantChanged);
    variantLayout->addWidget(variantLabel);
    variantLayout->addWidget(modelVariantCombo, 1);
    contentLayout->addLayout(variantLayout);

    // ========== API Key 选择 ==========
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

    // ========== 服务器选择 ==========
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

    // ========== 提示词输入 ==========
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
    contentLayout->addLayout(promptHeaderLayout);

    promptInput = new QTextEdit();
    promptInput->setPlaceholderText("输入视频生成提示词...\n例如：一只可爱的猫咪在花园里玩耍，阳光明媚，电影级画质");
    promptInput->setMinimumHeight(240);
    promptInput->setStyleSheet("font-size: 15px;");
    contentLayout->addWidget(promptInput);

    // ========== 图片上传区域（首帧）==========
    QHBoxLayout *imageLabelLayout = new QHBoxLayout();
    imageLabel = new QLabel("首帧图片:");
    imageLabel->setStyleSheet("font-size: 14px;");
    imageUploadHintLabel = new QLabel("💡提示：垫图后，视频尺寸跟垫图的图片尺寸保持一致，下面\"宽高比\"参数会自动忽略处理");
    imageUploadHintLabel->setStyleSheet("font-size: 12px; color: #888888;");
    imageUploadHintLabel->setWordWrap(false);
    imageUploadHintLabel->setVisible(false);
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
    imagePreviewLabel->installEventFilter(this);

    uploadImageButton = new QPushButton("📁 选择首帧图片");
    uploadImageButton->setFixedWidth(150);
    connect(uploadImageButton, &QPushButton::clicked, this, &VeoGenPage::uploadImage);

    clearImageButton = new QPushButton("🗑️ 清空");
    clearImageButton->setFixedWidth(80);
    connect(clearImageButton, &QPushButton::clicked, [this]() { clearImage(0); });

    imageLayout->addWidget(imagePreviewLabel, 1);
    imageLayout->addWidget(uploadImageButton);
    imageLayout->addWidget(clearImageButton);
    contentLayout->addLayout(imageLayout);

    // ========== 尾帧图片上传 ==========
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
    endFramePreviewLabel->installEventFilter(this);
    uploadEndFrameButton = new QPushButton("📁 选择尾帧图片");
    uploadEndFrameButton->setFixedWidth(150);
    connect(uploadEndFrameButton, &QPushButton::clicked, this, &VeoGenPage::uploadEndFrameImage);
    endFrameImageLayout->addWidget(endFramePreviewLabel, 1);
    endFrameImageLayout->addWidget(uploadEndFrameButton);
    endFrameLayout->addLayout(endFrameImageLayout);
    contentLayout->addWidget(endFrameWidget);

    // ========== 中间帧图片上传（components 模型）==========
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
    connect(uploadMiddleFrameButton, &QPushButton::clicked, this, &VeoGenPage::uploadMiddleFrameImage);
    middleFrameImageLayout->addWidget(middleFramePreviewLabel, 1);
    middleFrameImageLayout->addWidget(uploadMiddleFrameButton);
    middleFrameLayout->addLayout(middleFrameImageLayout);
    contentLayout->addWidget(middleFrameWidget);
    middleFrameWidget->setVisible(false);

    // ========== 参数设置 ==========
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
    durationCombo->setEnabled(false);
    durLayout->addWidget(durationLabel);
    durLayout->addWidget(durationCombo);

    QVBoxLayout *watermarkLayout = new QVBoxLayout();
    watermarkLabel = new QLabel("水印");
    watermarkLabel->setStyleSheet("font-size: 14px;");
    watermarkCheckBox = new QCheckBox("保留水印");
    watermarkCheckBox->setStyleSheet("color: white; font-size: 12px;");
    watermarkCheckBox->setChecked(false);
    watermarkLayout->addWidget(watermarkLabel);
    watermarkLayout->addWidget(watermarkCheckBox);

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
    paramsLayout->addLayout(watermarkLayout);
    paramsLayout->addLayout(enhanceLayout);
    paramsLayout->addLayout(upsampleLayout);
    paramsLayout->addStretch();
    contentLayout->addLayout(paramsLayout);

    // ========== 预览区域 ==========
    previewLabel = new QLabel();
    previewLabel->setObjectName("videoPreviewLabel");
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setText("💡 生成结果将在【生成历史记录】");
    previewLabel->setMinimumHeight(150);
    contentLayout->addWidget(previewLabel);

    // ========== 按钮 ==========
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    generateButton = new QPushButton("🚀 生成视频");
    generateButton->setCursor(Qt::PointingHandCursor);
    connect(generateButton, &QPushButton::clicked, this, &VeoGenPage::generateVideo);
    buttonLayout->addWidget(generateButton);
    resetButton = new QPushButton("🔄 一键重置");
    resetButton->setCursor(Qt::PointingHandCursor);
    connect(resetButton, &QPushButton::clicked, this, &VeoGenPage::resetForm);
    buttonLayout->addWidget(resetButton);
    contentLayout->addLayout(buttonLayout);

    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);

    // 初始化UI状态（默认 VEO3 Variant 1）
    onVariantTypeChanged();
}

void VeoGenPage::connectSignals()
{
    connect(promptInput, &QTextEdit::textChanged, this, &VeoGenPage::onAnyParameterChanged);
    connect(modelVariantCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VeoGenPage::onAnyParameterChanged);
    connect(resolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VeoGenPage::onAnyParameterChanged);
    connect(watermarkCheckBox, &QCheckBox::checkStateChanged, this, &VeoGenPage::onAnyParameterChanged);
    connect(durationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VeoGenPage::onAnyParameterChanged);

    connect(promptInput, &QTextEdit::textChanged, this, &VeoGenPage::queueSaveSettings);
    connect(modelVariantCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VeoGenPage::queueSaveSettings);
    connect(apiKeyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VeoGenPage::queueSaveSettings);
    connect(serverCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VeoGenPage::queueSaveSettings);
    connect(resolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VeoGenPage::queueSaveSettings);
    connect(watermarkCheckBox, &QCheckBox::checkStateChanged, this, &VeoGenPage::queueSaveSettings);
    connect(durationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VeoGenPage::queueSaveSettings);
    connect(enhancePromptCheckBox, &QCheckBox::checkStateChanged, this, &VeoGenPage::queueSaveSettings);
    connect(enableUpsampleCheckBox, &QCheckBox::checkStateChanged, this, &VeoGenPage::queueSaveSettings);

    connect(apiKeyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        QString selectedApiKeyValue;
        int keyId = apiKeyCombo->currentData().toInt();
        if (keyId > 0) {
            const ApiKey key = DBManager::instance()->getApiKey(keyId);
            selectedApiKeyValue = key.apiKey;
        }
        emit apiKeySelectionChanged(selectedApiKeyValue);
    });

    connect(variantType1Radio, &QRadioButton::toggled, this, &VeoGenPage::onVariantTypeChanged);

    // eventFilter 处理点击上传
    connect(imagePreviewLabel, &QLabel::linkActivated, this, &VeoGenPage::uploadImage);
    connect(endFramePreviewLabel, &QLabel::linkActivated, this, &VeoGenPage::uploadEndFrameImage);
    connect(middleFramePreviewLabel, &QLabel::linkActivated, this, &VeoGenPage::uploadMiddleFrameImage);
}

bool VeoGenPage::eventFilter(QObject *obj, QEvent *event)
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
        }
    }
    return QWidget::eventFilter(obj, event);
}

void VeoGenPage::loadApiKeys()
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

void VeoGenPage::refreshApiKeys()
{
    loadApiKeys();
}

void VeoGenPage::onVariantTypeChanged()
{
    bool isVariant1 = variantType1Radio->isChecked();

    modelVariantCombo->blockSignals(true);
    modelVariantCombo->clear();

    if (isVariant1) {
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
    onModelVariantChanged(0);
}

void VeoGenPage::onModelVariantChanged(int index)
{
    Q_UNUSED(index);
    QString modelVariant = modelVariantCombo->currentData().toString();
    updateImageUploadUI(modelVariant);

    bool is4K = modelVariant.contains("4K") || modelVariant.contains("4k");
    bool isVariant2 = variantType2Radio && variantType2Radio->isChecked();
    updateResolutionOptions(is4K, isVariant2);
}

void VeoGenPage::updateImageUploadUI(const QString &modelName)
{
    bool isComponents = modelName.contains("components");
    bool isFrames = modelName.contains("frames");

    if (isComponents) {
        imageLabel->setText("图片1（可选）:");
        uploadImageButton->setText("📁 选择图片1");
        clearImageButton->setVisible(false);
        endFrameWidget->setVisible(true);
        endFrameLabel->setText("图片2（可选）:");
        uploadEndFrameButton->setText("📁 选择图片2");
        middleFrameWidget->setVisible(true);
        middleFrameLabel->setText("图片3（可选）:");
        uploadMiddleFrameButton->setText("📁 选择图片3");
        imageUploadHintLabel->setVisible(false);
    } else if (isFrames) {
        imageLabel->setText("首帧图片（单张）:");
        uploadImageButton->setText("📁 选择首帧图片");
        clearImageButton->setVisible(false);
        endFrameWidget->setVisible(false);
        middleFrameWidget->setVisible(false);
        imageUploadHintLabel->setVisible(false);
    } else {
        imageLabel->setText("首帧图片:");
        uploadImageButton->setText("📁 选择首帧图片");
        clearImageButton->setVisible(false);
        endFrameWidget->setVisible(true);
        endFrameLabel->setText("尾帧图片（可选）:");
        uploadEndFrameButton->setText("📁 选择尾帧图片");
        middleFrameWidget->setVisible(false);
        imageUploadHintLabel->setVisible(false);
    }
}

void VeoGenPage::updateResolutionOptions(bool is4K, bool isVariant2)
{
    resolutionCombo->clear();
    if (isVariant2) {
        if (is4K) {
            resolutionCombo->addItem("横屏 16:9 (3840x2160)", "16:9");
            resolutionCombo->addItem("竖屏 9:16 (2160x3840)", "9:16");
        } else {
            resolutionCombo->addItem("横屏 16:9 (1280x720)", "16:9");
            resolutionCombo->addItem("竖屏 9:16 (720x1280)", "9:16");
        }
    } else {
        if (is4K) {
            resolutionCombo->addItem("横屏 16:9 (3840x2160)", "3840x2160");
            resolutionCombo->addItem("竖屏 9:16 (2160x3840)", "2160x3840");
        } else {
            resolutionCombo->addItem("横屏 16:9 (1280x720)", "1280x720");
            resolutionCombo->addItem("竖屏 9:16 (720x1280)", "720x1280");
        }
    }
}

void VeoGenPage::uploadImage()
{
    QString modelVariant = modelVariantCombo->currentData().toString();
    bool isComponents = modelVariant.contains("components");
    bool isFrames = modelVariant.contains("frames");

    if (!uploadedImagePaths.isEmpty()) {
        int ret = QMessageBox::question(this, "重新选择图片",
            isFrames ? "当前已有首帧图片，是否重新选择？" : "当前已有图片，是否重新选择？",
            QMessageBox::Yes | QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
        uploadedImagePaths.clear();
    }

    QString fileName = selectAndValidateImageFile("选择图片1", uploadedImagePaths.isEmpty());
    if (fileName.isEmpty()) return;

    uploadedImagePaths.append(fileName);
    updateImagePreview();
    queueSaveSettings();
}

void VeoGenPage::uploadEndFrameImage()
{
    if (!uploadedEndFrameImagePath.isEmpty()) {
        int ret = QMessageBox::question(this, "重新选择图片",
            "当前已有尾帧图片，是否重新选择？",
            QMessageBox::Yes | QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
    }

    QString fileName = selectAndValidateImageFile("选择尾帧图片", uploadedEndFrameImagePath.isEmpty());
    if (fileName.isEmpty()) return;

    uploadedEndFrameImagePath = fileName;
    QPixmap pixmap(fileName);
    QPixmap scaledPixmap = pixmap.scaled(200, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    endFramePreviewLabel->setPixmap(scaledPixmap);
    endFramePreviewLabel->setText("");
    endFramePreviewLabel->setProperty("hasImage", true);
    endFramePreviewLabel->style()->unpolish(endFramePreviewLabel);
    endFramePreviewLabel->style()->polish(endFramePreviewLabel);
    queueSaveSettings();
}

void VeoGenPage::uploadMiddleFrameImage()
{
    if (!uploadedMiddleFrameImagePath.isEmpty()) {
        int ret = QMessageBox::question(this, "重新选择图片",
            "当前已有图片，是否重新选择？",
            QMessageBox::Yes | QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
    }

    QString fileName = selectAndValidateImageFile("选择图片2（中间帧）", uploadedMiddleFrameImagePath.isEmpty());
    if (fileName.isEmpty()) return;

    uploadedMiddleFrameImagePath = fileName;
    QPixmap pixmap(fileName);
    QPixmap scaledPixmap = pixmap.scaled(200, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    middleFramePreviewLabel->setPixmap(scaledPixmap);
    middleFramePreviewLabel->setText("");
    middleFramePreviewLabel->setProperty("hasImage", true);
    middleFramePreviewLabel->style()->unpolish(middleFramePreviewLabel);
    middleFramePreviewLabel->style()->polish(middleFramePreviewLabel);
    queueSaveSettings();
}

void VeoGenPage::clearImage(int index)
{
    if (index < 0 || index >= uploadedImagePaths.size()) return;
    uploadedImagePaths[index] = QString();
    updateImagePreview();
    queueSaveSettings();
}

void VeoGenPage::updateImagePreview()
{
    if (uploadedImagePaths.isEmpty()) {
        imagePreviewLabel->setText("未选择图片\n点击此处上传");
        imagePreviewLabel->setPixmap(QPixmap());
        imagePreviewLabel->setProperty("hasImage", false);
    } else {
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
    imagePreviewLabel->style()->unpolish(imagePreviewLabel);
    imagePreviewLabel->style()->polish(imagePreviewLabel);

    // 尾帧预览
    if (!uploadedEndFrameImagePath.isEmpty()) {
        QPixmap pixmap(uploadedEndFrameImagePath);
        if (!pixmap.isNull()) {
            QPixmap scaledPixmap = pixmap.scaled(200, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            endFramePreviewLabel->setPixmap(scaledPixmap);
            endFramePreviewLabel->setText("");
        } else {
            endFramePreviewLabel->setPixmap(QPixmap());
            QFileInfo fileInfo(uploadedEndFrameImagePath);
            endFramePreviewLabel->setText("✓ " + fileInfo.fileName());
        }
        endFramePreviewLabel->setProperty("hasImage", true);
        endFramePreviewLabel->style()->unpolish(endFramePreviewLabel);
        endFramePreviewLabel->style()->polish(endFramePreviewLabel);
    } else {
        endFramePreviewLabel->setText("未选择图片\n点击此处上传");
        endFramePreviewLabel->setPixmap(QPixmap());
        endFramePreviewLabel->setProperty("hasImage", false);
        endFramePreviewLabel->style()->unpolish(endFramePreviewLabel);
        endFramePreviewLabel->style()->polish(endFramePreviewLabel);
    }
}

QString VeoGenPage::selectAndValidateImageFile(const QString &dialogTitle, bool warnIfEmpty)
{
    QSettings settings("ChickenAI", "VideoGen");
    QString lastDir = settings.value("lastImageUploadDir",
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)).toString();
    if (!QDir(lastDir).exists()) {
        lastDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }

    QString fileName = QFileDialog::getOpenFileName(this, dialogTitle, lastDir,
        "图片文件 (*.png *.jpg *.jpeg *.gif *.webp)");
    if (fileName.isEmpty()) {
        if (warnIfEmpty) QMessageBox::warning(this, "提示", "没有选择图片");
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

bool VeoGenPage::validateImageFile(const QString &filePath, QString &errorMsg) const
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

bool VeoGenPage::validateImgbbKey(QString &errorMsg) const
{
    ImgbbKey activeImgbbKey = DBManager::instance()->getActiveImgbbKey();
    if (activeImgbbKey.apiKey.isEmpty()) {
        errorMsg = "请先到设置页应用临时图床密钥";
        return false;
    }
    return true;
}

QString VeoGenPage::normalizeImageReferences(const QString &prompt) const
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

void VeoGenPage::resetForm()
{
    promptInput->clear();
    uploadedImagePaths.clear();
    imagePreviewLabel->clear();
    imagePreviewLabel->setText("📷 未上传图片");
    imagePreviewLabel->setAlignment(Qt::AlignCenter);
    uploadedEndFrameImagePath.clear();
    if (endFramePreviewLabel) {
        endFramePreviewLabel->clear();
        endFramePreviewLabel->setText("📷 未上传尾帧图片");
        endFramePreviewLabel->setAlignment(Qt::AlignCenter);
    }
    uploadedMiddleFrameImagePath.clear();
    previewLabel->setText("💡 生成结果将在【生成历史记录】");
    parametersModified = true;
}

void VeoGenPage::generateVideo()
{
    QString prompt = promptInput->toPlainText().trimmed();
    if (prompt.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入视频生成提示词");
        return;
    }

    prompt = normalizeImageReferences(prompt);

    if (apiKeyCombo->count() == 0 || !apiKeyCombo->isEnabled()) {
        QMessageBox::warning(this, "提示", "请先添加 API 密钥");
        return;
    }

    if (!checkDuplicateSubmission()) return;

    QString modelVariant = modelVariantCombo->currentData().toString();
    QString modelVariantText = modelVariantCombo->currentText();
    QString server = serverCombo->currentData().toString();
    QString resolution = resolutionCombo->currentData().toString();
    QString duration = durationCombo->currentData().toString();
    bool watermark = watermarkCheckBox->isChecked();
    int keyId = apiKeyCombo->currentData().toInt();

    ApiKey apiKeyData = DBManager::instance()->getApiKey(keyId);
    if (apiKeyData.apiKey.isEmpty()) {
        QMessageBox::warning(this, "错误", "无法获取 API 密钥");
        return;
    }

    QString tempTaskId = QString("temp_%1_%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(QRandomGenerator::global()->bounded(10000));

    VideoTask task;
    task.taskId = tempTaskId;
    task.taskType = "video_single";
    task.prompt = prompt;
    task.modelVariant = modelVariantText;
    task.modelName = "VEO3视频";
    task.apiKeyName = apiKeyCombo->currentText();
    task.serverUrl = server;
    task.status = "submitting";
    task.progress = 0;
    task.resolution = resolution;
    task.duration = durationCombo->currentText().remove("秒").toInt();
    task.watermark = watermark;

    QJsonArray imageArray;
    for (const QString& path : uploadedImagePaths) {
        if (!path.isEmpty()) imageArray.append(path);
    }
    task.imagePaths = QString::fromUtf8(QJsonDocument(imageArray).toJson(QJsonDocument::Compact));
    task.endFrameImagePath = uploadedEndFrameImagePath;

    int dbId = DBManager::instance()->insertVideoTask(task);
    if (dbId < 0) {
        QMessageBox::critical(this, "错误", "无法保存任务记录");
        return;
    }

    currentTaskId = tempTaskId;
    previewLabel->setText("⏳ 正在提交视频生成任务...");

    // 合并所有图片路径
    QStringList allImagePaths;
    for (const QString& path : uploadedImagePaths) {
        if (!path.isEmpty()) allImagePaths.append(path);
    }
    if (!uploadedMiddleFrameImagePath.isEmpty()) allImagePaths.append(uploadedMiddleFrameImagePath);
    if (!uploadedEndFrameImagePath.isEmpty()) allImagePaths.append(uploadedEndFrameImagePath);

    if (variantType2Radio && variantType2Radio->isChecked()) {
        // VEO3 Variant 2 统一格式
        ImgbbKey activeImgbbKey = DBManager::instance()->getActiveImgbbKey();
        if (activeImgbbKey.apiKey.isEmpty()) {
            QMessageBox::warning(this, "提示", "请先到设置页应用临时图床密钥");
            return;
        }
        bool enhancePrompt = enhancePromptCheckBox->isChecked();
        bool enableUpsample = enableUpsampleCheckBox->isChecked();

        api->createVideo(
            apiKeyData.apiKey,
            server,
            "VEO3视频",
            modelVariant,
            prompt,
            allImagePaths,
            "",
            "",
            false,
            resolution,
            activeImgbbKey.apiKey,
            enhancePrompt,
            enableUpsample
        );
    } else {
        // VEO3 Variant 1 OpenAI格式
        api->createVideo(
            apiKeyData.apiKey,
            server,
            "VEO3视频",
            modelVariant,
            prompt,
            allImagePaths,
            resolution,
            duration,
            watermark,
            ""
        );
    }
}

void VeoGenPage::onVideoCreated(const QString &taskId, const QString &status)
{
    Q_UNUSED(status);
    if (!currentTaskId.isEmpty() && currentTaskId != taskId) {
        DBManager::instance()->updateTaskId(currentTaskId, taskId);
    }
    currentTaskId = taskId;
    DBManager::instance()->updateTaskStatus(taskId, "pending", 0, "");

    int keyId = apiKeyCombo->currentData().toInt();
    ApiKey apiKeyData = DBManager::instance()->getApiKey(keyId);
    QString server = serverCombo->currentData().toString();

    TaskPollManager *pollMgr = TaskPollManager::getInstance();
    pollMgr->startPolling(taskId, "video_single", apiKeyData.apiKey, server, "VEO3视频");

    lastSubmittedParamsHash = calculateParamsHash();
    parametersModified = false;
    saveSettings();

    QMessageBox::information(this, "任务已提交",
        "视频生成任务已提交！\n\n请前往【生成历史记录】标签页查看任务状态。\n系统将自动轮询任务状态并下载完成的视频。");
    previewLabel->setText("💡 生成结果将在【生成历史记录】");
}

void VeoGenPage::onTaskStatusUpdated(const QString &taskId, const QString &status, const QString &videoUrl, int progress)
{
    Q_UNUSED(taskId);
    Q_UNUSED(status);
    Q_UNUSED(videoUrl);
    Q_UNUSED(progress);
}

void VeoGenPage::onApiError(const QString &error)
{
    if (!currentTaskId.isEmpty()) {
        DBManager::instance()->updateTaskStatus(currentTaskId, "failed", 0, "");
        DBManager::instance()->updateTaskErrorMessage(currentTaskId, error);
    }
    QMessageBox::critical(this, "错误", QString("API 调用失败:\n%1").arg(error));
    previewLabel->setText("💡 生成结果将在【生成历史记录】");
}

void VeoGenPage::queueSaveSettings()
{
    if (suppressAutoSave) return;
    if (pendingSaveSettings) return;
    pendingSaveSettings = true;
    QTimer::singleShot(0, this, [this]() {
        pendingSaveSettings = false;
        saveSettings();
    });
}

QString VeoGenPage::buildSettingsSnapshot() const
{
    QString selectedApiKeyValue;
    int keyId = apiKeyCombo->currentData().toInt();
    if (keyId > 0) {
        const ApiKey key = DBManager::instance()->getApiKey(keyId);
        selectedApiKeyValue = key.apiKey;
    }

    const QStringList snapshotParts = {
        modelVariantCombo->currentData().toString(),
        apiKeyCombo->currentData().toString(),
        selectedApiKeyValue,
        serverCombo->currentData().toString(),
        promptInput->toPlainText(),
        resolutionCombo->currentData().toString(),
        durationCombo->currentData().toString(),
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

void VeoGenPage::saveSettings()
{
    const QString snapshot = buildSettingsSnapshot();
    if (!lastSavedSettingsSnapshot.isEmpty() && snapshot == lastSavedSettingsSnapshot) {
        return;
    }

    QSettings settings("ChickenAI", "VideoGen");
    settings.beginGroup("VideoSingleTab");

    settings.setValue("prompt", promptInput->toPlainText());
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
    settings.setValue("veo_resolution", resolutionCombo->currentData().toString());
    settings.setValue("duration", durationCombo->currentData().toString());
    settings.setValue("watermark", watermarkCheckBox->isChecked());
    settings.setValue("imagePaths", uploadedImagePaths);
    settings.setValue("endFrameImagePath", uploadedEndFrameImagePath);
    settings.setValue("middleFrameImagePath", uploadedMiddleFrameImagePath);
    settings.setValue("lastSubmittedHash", lastSubmittedParamsHash);
    settings.setValue("variantType", variantType1Radio->isChecked() ? 1 : 2);
    settings.setValue("enhancePrompt", enhancePromptCheckBox->isChecked());
    settings.setValue("enableUpsample", enableUpsampleCheckBox->isChecked());

    settings.endGroup();
    settings.sync();
    lastSavedSettingsSnapshot = snapshot;
}

void VeoGenPage::loadSettings()
{
    QSettings settings("ChickenAI", "VideoGen");
    settings.beginGroup("VideoSingleTab");

    promptInput->blockSignals(true);
    modelVariantCombo->blockSignals(true);
    apiKeyCombo->blockSignals(true);
    serverCombo->blockSignals(true);
    resolutionCombo->blockSignals(true);
    watermarkCheckBox->blockSignals(true);
    durationCombo->blockSignals(true);
    variantType1Radio->blockSignals(true);
    variantType2Radio->blockSignals(true);
    enhancePromptCheckBox->blockSignals(true);
    enableUpsampleCheckBox->blockSignals(true);

    promptInput->setPlainText(settings.value("prompt", "").toString());

    int variantType = settings.value("variantType", 1).toInt();
    if (variantType == 2) {
        variantType2Radio->setChecked(true);
    } else {
        variantType1Radio->setChecked(true);
    }
    onVariantTypeChanged();

    enhancePromptCheckBox->setChecked(settings.value("enhancePrompt", true).toBool());
    enableUpsampleCheckBox->setChecked(settings.value("enableUpsample", true).toBool());

    QString savedVariant = settings.value("modelVariant", "").toString();
    if (!savedVariant.isEmpty()) {
        for (int i = 0; i < modelVariantCombo->count(); ++i) {
            if (modelVariantCombo->itemData(i).toString() == savedVariant) {
                modelVariantCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    // 恢复 API 密钥：优先使用索引，备选使用密钥值匹配
    int apiKeyIndex = settings.value("apiKey", 0).toInt();
    if (apiKeyIndex >= 0 && apiKeyIndex < apiKeyCombo->count()) {
        apiKeyCombo->setCurrentIndex(apiKeyIndex);
    } else {
        // 索引无效时，通过保存的密钥值匹配
        QString savedApiKeyValue = settings.value("selectedApiKeyValue", "").toString();
        if (!savedApiKeyValue.isEmpty()) {
            for (int i = 0; i < apiKeyCombo->count(); ++i) {
                int keyId = apiKeyCombo->itemData(i).toInt();
                if (keyId > 0) {
                    ApiKey key = DBManager::instance()->getApiKey(keyId);
                    if (key.apiKey == savedApiKeyValue) {
                        apiKeyCombo->setCurrentIndex(i);
                        break;
                    }
                }
            }
        }
    }

    int serverIndex = settings.value("server", 0).toInt();
    if (serverIndex >= 0 && serverIndex < serverCombo->count()) {
        serverCombo->setCurrentIndex(serverIndex);
    }

    QString resolution = settings.value("veo_resolution", "1280x720").toString();
    for (int i = 0; i < resolutionCombo->count(); ++i) {
        if (resolutionCombo->itemData(i).toString() == resolution) {
            resolutionCombo->setCurrentIndex(i);
            break;
        }
    }

    QString duration = settings.value("duration", "8").toString();
    for (int i = 0; i < durationCombo->count(); ++i) {
        if (durationCombo->itemData(i).toString() == duration) {
            durationCombo->setCurrentIndex(i);
            break;
        }
    }

    watermarkCheckBox->setChecked(settings.value("watermark", false).toBool());

    QStringList imagePaths = settings.value("imagePaths").toStringList();
    uploadedImagePaths.clear();
    for (const QString &path : imagePaths) {
        if (QFile::exists(path)) uploadedImagePaths.append(path);
    }
    if (!uploadedImagePaths.isEmpty()) updateImagePreview();

    QString endFramePath = settings.value("endFrameImagePath").toString();
    if (!endFramePath.isEmpty() && QFile::exists(endFramePath)) {
        uploadedEndFrameImagePath = endFramePath;
        QPixmap pixmap(endFramePath);
        if (!pixmap.isNull()) {
            QPixmap scaledPixmap = pixmap.scaled(200, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            endFramePreviewLabel->setPixmap(scaledPixmap);
            endFramePreviewLabel->setText("");
        }
        endFramePreviewLabel->setProperty("hasImage", true);
        endFramePreviewLabel->style()->unpolish(endFramePreviewLabel);
        endFramePreviewLabel->style()->polish(endFramePreviewLabel);
    }

    QString middleFramePath = settings.value("middleFrameImagePath").toString();
    if (!middleFramePath.isEmpty() && QFile::exists(middleFramePath)) {
        uploadedMiddleFrameImagePath = middleFramePath;
        QPixmap pixmap(middleFramePath);
        if (!pixmap.isNull()) {
            QPixmap scaledPixmap = pixmap.scaled(200, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            middleFramePreviewLabel->setPixmap(scaledPixmap);
            middleFramePreviewLabel->setText("");
        }
        middleFramePreviewLabel->setProperty("hasImage", true);
        middleFramePreviewLabel->style()->unpolish(middleFramePreviewLabel);
        middleFramePreviewLabel->style()->polish(middleFramePreviewLabel);
    }

    lastSubmittedParamsHash = settings.value("lastSubmittedHash", "").toString();

    settings.endGroup();
    lastSavedSettingsSnapshot = buildSettingsSnapshot();

    promptInput->blockSignals(false);
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

    parametersModified = false;
}

QString VeoGenPage::calculateParamsHash() const
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(promptInput->toPlainText().toUtf8());
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

bool VeoGenPage::checkDuplicateSubmission()
{
    QString currentHash = calculateParamsHash();
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
        if (msgBox.clickedButton() == cancelBtn) return false;
        if (suppressCheckBox->isChecked()) suppressDuplicateWarning = true;
    }
    return true;
}

void VeoGenPage::onAnyParameterChanged()
{
    parametersModified = true;
    suppressDuplicateWarning = false;
}

void VeoGenPage::loadFromTask(const VideoTask& task)
{
    blockSignals(true);
    promptInput->setPlainText(task.prompt);

    // 设置模型变体（通过文本匹配）
    for (int i = 0; i < modelVariantCombo->count(); ++i) {
        if (modelVariantCombo->itemText(i) == task.modelVariant) {
            modelVariantCombo->setCurrentIndex(i);
            break;
        }
    }

    // 设置API密钥
    bool apiKeyFound = false;
    for (int i = 0; i < apiKeyCombo->count(); ++i) {
        if (apiKeyCombo->itemText(i) == task.apiKeyName) {
            apiKeyCombo->setCurrentIndex(i);
            apiKeyFound = true;
            break;
        }
    }
    if (!apiKeyFound && apiKeyCombo->count() > 0) apiKeyCombo->setCurrentIndex(0);

    // 设置服务器
    for (int i = 0; i < serverCombo->count(); ++i) {
        if (serverCombo->itemData(i).toString() == task.serverUrl) {
            serverCombo->setCurrentIndex(i);
            break;
        }
    }

    // 设置分辨率
    for (int i = 0; i < resolutionCombo->count(); ++i) {
        if (resolutionCombo->itemData(i).toString() == task.resolution) {
            resolutionCombo->setCurrentIndex(i);
            break;
        }
    }

    // 设置时长
    QString durationText = QString::number(task.duration) + "秒";
    for (int i = 0; i < durationCombo->count(); ++i) {
        if (durationCombo->itemText(i) == durationText) {
            durationCombo->setCurrentIndex(i);
            break;
        }
    }

    // 设置水印
    watermarkCheckBox->setChecked(task.watermark);

    // 解析图片路径
    uploadedImagePaths.clear();
    if (!task.imagePaths.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(task.imagePaths.toUtf8());
        if (doc.isArray()) {
            QJsonArray imageArray = doc.array();
            for (const QJsonValue& value : imageArray) {
                QString path = value.toString();
                if (QFile::exists(path)) uploadedImagePaths.append(path);
            }
        }
    }

    uploadedEndFrameImagePath = task.endFrameImagePath;
    if (!uploadedEndFrameImagePath.isEmpty() && !QFile::exists(uploadedEndFrameImagePath)) {
        uploadedEndFrameImagePath.clear();
    }

    updateImagePreview();
    parametersModified = false;
    lastSubmittedParamsHash.clear();
    blockSignals(false);
}
