#include "wangen.h"
#include "../network/videoapi.h"
#include "../network/imageuploader.h"
#include "../network/taskpollmanager.h"
#include "../database/dbmanager.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QSettings>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QStandardPaths>
#include <QImageReader>
#include <QDateTime>
#include <QRandomGenerator>

namespace {

void applyAudioFileLabelState(QLabel *label, bool hasAudio, const QString &fileName = QString())
{
    if (!label) {
        return;
    }

    if (hasAudio) {
        label->setText("✓ " + fileName);
        label->setStyleSheet("font-size: 12px; color: palette(highlight);");
        return;
    }

    label->setText("未选择音频文件");
    label->setStyleSheet("font-size: 12px; color: palette(mid);");
}

} // namespace

// WanGenPage 实现
WanGenPage::WanGenPage(QWidget *parent)
    : QWidget(parent),
      suppressDuplicateWarning(false),
      parametersModified(false),
      pendingSaveSettings(false),
      suppressAutoSave(false),
      audioUploading(false)
{
    api = new VideoAPI(this);
    connect(api, &VideoAPI::videoCreated, this, &WanGenPage::onVideoCreated);
    connect(api, &VideoAPI::taskStatusUpdated, this, &WanGenPage::onTaskStatusUpdated);
    connect(api, &VideoAPI::errorOccurred, this, &WanGenPage::onApiError);

    setupUI();
    loadApiKeys();
    loadSettings();
    connectSignals();
}

void WanGenPage::setupUI()
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

    // ========== 模型变体选择 ==========
    QHBoxLayout *variantLayout = new QHBoxLayout();
    QLabel *variantLabel = new QLabel("模型变体:");
    variantLabel->setStyleSheet("font-size: 14px;");
    modelVariantCombo = new QComboBox();
    modelVariantCombo->addItem("wan2.5-i2v-preview（预览版）", "wan2.5-i2v-preview");
    modelVariantCombo->addItem("wan2.6-i2v-flash（快速版）", "wan2.6-i2v-flash");
    modelVariantCombo->addItem("wan2.6-i2v（正式版）", "wan2.6-i2v");
    connect(modelVariantCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WanGenPage::onModelVariantChanged);
    variantLayout->addWidget(variantLabel);
    variantLayout->addWidget(modelVariantCombo, 1);
    contentLayout->addLayout(variantLayout);

    // ========== 参数设置 ==========
    QHBoxLayout *paramsRow1 = new QHBoxLayout();
    paramsRow1->setSpacing(20);

    // 时长
    QVBoxLayout *durLayout = new QVBoxLayout();
    durationLabel = new QLabel("视频时长:");
    durationLabel->setStyleSheet("font-size: 14px;");
    durationCombo = new QComboBox();
    durationCombo->addItem("5秒", 5);
    durationCombo->addItem("10秒", 10);
    durLayout->addWidget(durationLabel);
    durLayout->addWidget(durationCombo);

    // 分辨率
    QVBoxLayout *resLayout = new QVBoxLayout();
    resolutionLabel = new QLabel("视频分辨率:");
    resolutionLabel->setStyleSheet("font-size: 14px;");
    resolutionCombo = new QComboBox();
    resolutionCombo->addItem("480P", "480P");
    resolutionCombo->addItem("720P", "720P");
    resolutionCombo->addItem("1080P", "1080P");
    resLayout->addWidget(resolutionLabel);
    resLayout->addWidget(resolutionCombo);

    paramsRow1->addLayout(durLayout);
    paramsRow1->addLayout(resLayout);
    paramsRow1->addStretch();
    contentLayout->addLayout(paramsRow1);

    // ========== 特效模板 ==========
    QHBoxLayout *templateLayout = new QHBoxLayout();
    templateLabel = new QLabel("特效模板:");
    templateLabel->setStyleSheet("font-size: 14px;");
    templateCombo = new QComboBox();
    templateCombo->addItem("无", "");
    templateCombo->addItem("squish（解压捏捏）", "squish");
    templateCombo->addItem("rotation（转圈圈）", "rotation");
    templateCombo->addItem("poke（戳戳乐）", "poke");
    templateCombo->addItem("inflate（气球膨胀）", "inflate");
    templateCombo->addItem("dissolve（分子扩散）", "dissolve");
    templateCombo->addItem("melt（热浪融化）", "melt");
    templateCombo->addItem("icecream（冰淇淋星球）", "icecream");
    templateCombo->addItem("carousel（时光木马）", "carousel");
    templateCombo->addItem("singleheart（爱你哟）", "singleheart");
    templateCombo->addItem("dance1（摇摆时刻）", "dance1");
    templateCombo->addItem("dance2（头号甩舞）", "dance2");
    templateCombo->addItem("dance3（星摇时刻）", "dance3");
    templateCombo->addItem("dance4（指感节奏）", "dance4");
    templateCombo->addItem("dance5（舞动开关）", "dance5");
    templateCombo->addItem("mermaid（人鱼觉醒）", "mermaid");
    templateCombo->addItem("graduation（学术加冕）", "graduation");
    templateCombo->addItem("dragon（巨兽追袭）", "dragon");
    templateCombo->addItem("money（财从天降）", "money");
    templateCombo->addItem("jellyfish（水母之约）", "jellyfish");
    templateCombo->addItem("pupil（瞳孔穿越）", "pupil");
    templateCombo->addItem("flying（魔法悬浮）", "flying");
    templateCombo->addItem("rose（赠人玫瑰）", "rose");
    templateCombo->addItem("crystalrose（闪亮玫瑰）", "crystalrose");
    templateCombo->addItem("hug（爱的抱抱）", "hug");
    templateCombo->addItem("frenchkiss（唇齿相依）", "frenchkiss");
    templateCombo->addItem("coupleheart（双倍心动）", "coupleheart");
    templateCombo->addItem("hanfu-1（唐韵翩然）", "hanfu-1");
    templateCombo->addItem("solaron（机甲变身）", "solaron");
    templateCombo->addItem("magazine（闪耀封面）", "magazine");
    templateCombo->addItem("mech1（机械觉醒）", "mech1");
    templateCombo->addItem("mech2（赛博登场）", "mech2");
    templateLayout->addWidget(templateLabel);
    templateLayout->addWidget(templateCombo, 1);
    contentLayout->addLayout(templateLayout);

    // ========== Prompt智能改写 + 随机种子 ==========
    QHBoxLayout *optionLayout = new QHBoxLayout();
    promptExtendCheckBox = new QCheckBox("Prompt智能改写");
    promptExtendCheckBox->setStyleSheet("font-size: 13px;");
    promptExtendCheckBox->setChecked(true);
    seedLabel = new QLabel("随机种子:");
    seedLabel->setStyleSheet("font-size: 14px;");
    seedInput = new QSpinBox();
    seedInput->setRange(0, 999999999);
    seedInput->setValue(0);
    seedInput->setMaximumWidth(150);
    seedInput->setStyleSheet(
        "QSpinBox { color: palette(text); background-color: palette(base); border: 1px solid palette(mid); border-radius: 4px; padding: 2px 4px; font-size: 13px; } "
        "QSpinBox::up-button, QSpinBox::down-button { background-color: palette(button); width: 16px; } "
        "QSpinBox::up-arrow, QSpinBox::down-arrow { border-color: palette(text); }");
    optionLayout->addWidget(promptExtendCheckBox);
    optionLayout->addSpacing(20);
    optionLayout->addWidget(seedLabel);
    optionLayout->addWidget(seedInput);
    optionLayout->addStretch();
    contentLayout->addLayout(optionLayout);

    // ========== 音频上传（仅 wan2.5）==========
    audioUploadWidget = new QWidget();
    audioUploadWidget->setMinimumHeight(36);
    QHBoxLayout *audioLayout = new QHBoxLayout(audioUploadWidget);
    audioLayout->setContentsMargins(0, 0, 0, 0);
    audioLayout->setSpacing(10);
    audioCheckBox = new QCheckBox("为视频添加音频（仅wan2.5）");
    audioCheckBox->setStyleSheet("font-size: 13px;");
    audioCheckBox->setChecked(false);
    audioUploadButton = new QPushButton("🎵 上传音频");
    audioUploadButton->setFixedWidth(120);
    connect(audioUploadButton, &QPushButton::clicked, this, &WanGenPage::uploadAudio);
    audioFileLabel = new QLabel("未选择音频文件");
    applyAudioFileLabelState(audioFileLabel, false);
    audioFileLabel->setWordWrap(true);
    clearAudioButton = new QPushButton("🗑️ 清空");
    clearAudioButton->setFixedWidth(80);
    clearAudioButton->setVisible(false);
    connect(clearAudioButton, &QPushButton::clicked, this, &WanGenPage::clearAudio);
    audioLayout->addWidget(audioCheckBox);
    audioLayout->addWidget(audioUploadButton);
    audioLayout->addWidget(audioFileLabel, 1);
    audioLayout->addWidget(clearAudioButton);
    contentLayout->addWidget(audioUploadWidget);
    audioUploadWidget->setVisible(false);

    // ========== 保留水印 ==========
    QHBoxLayout *watermarkLayout = new QHBoxLayout();
    watermarkCheckBox = new QCheckBox("保留水印");
    watermarkCheckBox->setStyleSheet("font-size: 13px;");
    watermarkCheckBox->setChecked(false);
    watermarkLayout->addWidget(watermarkCheckBox);
    watermarkLayout->addStretch();
    contentLayout->addLayout(watermarkLayout);

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

    // ========== 正向提示词 ==========
    QHBoxLayout *promptHeaderLayout = new QHBoxLayout();
    QLabel *promptLabel = new QLabel("正向提示词:");
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
    promptInput->setMinimumHeight(200);
    promptInput->setStyleSheet("font-size: 15px;");
    contentLayout->addWidget(promptInput);

    // ========== 反向提示词（竖排：标题在上，输入框在下） ==========
    QVBoxLayout *negativePromptLayout = new QVBoxLayout();
    negativePromptLayout->setContentsMargins(0, 0, 0, 0);
    negativePromptLayout->setSpacing(6);

    QLabel *negPromptLabel = new QLabel("反向提示词:");
    negPromptLabel->setStyleSheet("font-size: 14px;");
    negativePromptLayout->addWidget(negPromptLabel);

    negativePromptInput = new QTextEdit();
    negativePromptInput->setPlaceholderText("输入不希望出现的元素...");
    negativePromptInput->setMaximumHeight(60);
    negativePromptInput->setStyleSheet("font-size: 13px;");
    negativePromptLayout->addWidget(negativePromptInput);
    contentLayout->addLayout(negativePromptLayout);

    // ========== 图片上传 ==========
    QHBoxLayout *imageLabelLayout = new QHBoxLayout();
    imageLabel = new QLabel("首帧图片:");
    imageLabel->setStyleSheet("font-size: 14px;");
    imageLabelLayout->setContentsMargins(0, 0, 0, 0);
    imageLabelLayout->setSpacing(8);
    imageLabelLayout->addWidget(imageLabel);
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
    connect(uploadImageButton, &QPushButton::clicked, this, &WanGenPage::uploadImage);

    clearImageButton = new QPushButton("🗑️ 清空");
    clearImageButton->setFixedWidth(80);
    connect(clearImageButton, &QPushButton::clicked, [this]() {
        uploadedImagePath.clear();
        imagePreviewLabel->setText("未选择图片\n点击此处上传");
        imagePreviewLabel->setPixmap(QPixmap());
        imagePreviewLabel->setProperty("hasImage", false);
        imagePreviewLabel->style()->unpolish(imagePreviewLabel);
        imagePreviewLabel->style()->polish(imagePreviewLabel);
        queueSaveSettings();
    });

    imageLayout->addWidget(imagePreviewLabel, 1);
    imageLayout->addWidget(uploadImageButton);
    imageLayout->addWidget(clearImageButton);
    contentLayout->addLayout(imageLayout);

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
    connect(generateButton, &QPushButton::clicked, this, &WanGenPage::generateVideo);
    buttonLayout->addWidget(generateButton);
    resetButton = new QPushButton("🔄 一键重置");
    resetButton->setCursor(Qt::PointingHandCursor);
    connect(resetButton, &QPushButton::clicked, this, &WanGenPage::resetForm);
    buttonLayout->addWidget(resetButton);
    contentLayout->addLayout(buttonLayout);

    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);

    // 初始化音频控件可见性
    onModelVariantChanged(0);
}

void WanGenPage::connectSignals()
{
    connect(promptInput, &QTextEdit::textChanged, this, &WanGenPage::onAnyParameterChanged);
    connect(modelVariantCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WanGenPage::onAnyParameterChanged);

    connect(promptInput, &QTextEdit::textChanged, this, &WanGenPage::queueSaveSettings);
    connect(negativePromptInput, &QTextEdit::textChanged, this, &WanGenPage::queueSaveSettings);
    connect(modelVariantCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WanGenPage::queueSaveSettings);
    connect(apiKeyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WanGenPage::queueSaveSettings);
    connect(serverCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WanGenPage::queueSaveSettings);
    connect(durationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WanGenPage::queueSaveSettings);
    connect(resolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WanGenPage::queueSaveSettings);
    connect(templateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WanGenPage::queueSaveSettings);
    connect(promptExtendCheckBox, &QCheckBox::checkStateChanged, this, &WanGenPage::queueSaveSettings);
    connect(seedInput, &QSpinBox::valueChanged, this, &WanGenPage::queueSaveSettings);
    connect(audioCheckBox, &QCheckBox::checkStateChanged, this, &WanGenPage::queueSaveSettings);
    connect(watermarkCheckBox, &QCheckBox::checkStateChanged, this, &WanGenPage::queueSaveSettings);

    connect(apiKeyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        QString selectedApiKeyValue;
        int keyId = apiKeyCombo->currentData().toInt();
        if (keyId > 0) {
            const ApiKey key = DBManager::instance()->getApiKey(keyId);
            selectedApiKeyValue = key.apiKey;
        }
        emit apiKeySelectionChanged(selectedApiKeyValue);
    });
}

bool WanGenPage::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        if (obj == imagePreviewLabel) {
            uploadImage();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void WanGenPage::loadApiKeys()
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

void WanGenPage::refreshApiKeys()
{
    loadApiKeys();
}

void WanGenPage::onModelVariantChanged(int index)
{
    Q_UNUSED(index);
    QString modelVariant = modelVariantCombo->currentData().toString();

    const bool needsTallNegativePrompt =
        modelVariant == "wan2.5-i2v-preview" ||
        modelVariant == "wan2.6-i2v-flash" ||
        modelVariant == "wan2.6-i2v";
    if (needsTallNegativePrompt) {
        negativePromptInput->setMinimumHeight(120);
        negativePromptInput->setMaximumHeight(120);
    } else {
        negativePromptInput->setMinimumHeight(0);
        negativePromptInput->setMaximumHeight(60);
    }

    updateAudioWidgetVisibility(modelVariant);
}

void WanGenPage::updateAudioWidgetVisibility(const QString &modelVariant)
{
    bool isWan25 = modelVariant.contains("wan2.5", Qt::CaseInsensitive);
    audioUploadWidget->setVisible(isWan25);
    if (!isWan25) {
        clearAudio();
    }
    if (QLayout *l = audioUploadWidget->layout()) {
        l->invalidate();
        l->activate();
    }
}

void WanGenPage::uploadImage()
{
    if (!uploadedImagePath.isEmpty()) {
        int ret = QMessageBox::question(this, "重新选择图片",
            "当前已有图片，是否重新选择？",
            QMessageBox::Yes | QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
    }

    QString fileName = selectAndValidateImageFile("选择首帧图片");
    if (fileName.isEmpty()) return;

    uploadedImagePath = fileName;
    updateImagePreview();
    queueSaveSettings();
}

void WanGenPage::updateImagePreview()
{
    if (uploadedImagePath.isEmpty()) {
        imagePreviewLabel->setText("未选择图片\n点击此处上传");
        imagePreviewLabel->setPixmap(QPixmap());
        imagePreviewLabel->setProperty("hasImage", false);
    } else {
        QPixmap pixmap(uploadedImagePath);
        if (!pixmap.isNull()) {
            QPixmap scaledPixmap = pixmap.scaled(200, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            imagePreviewLabel->setPixmap(scaledPixmap);
            imagePreviewLabel->setText("");
        } else {
            QFileInfo fi(uploadedImagePath);
            imagePreviewLabel->setText("✓ " + fi.fileName());
        }
        imagePreviewLabel->setProperty("hasImage", true);
    }
    imagePreviewLabel->style()->unpolish(imagePreviewLabel);
    imagePreviewLabel->style()->polish(imagePreviewLabel);
}

QString WanGenPage::selectAndValidateImageFile(const QString &dialogTitle)
{
    QSettings settings("ChickenAI", "VideoGen");
    QString lastDir = settings.value("lastImageUploadDir",
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)).toString();
    if (!QDir(lastDir).exists()) {
        lastDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }

    QString fileName = QFileDialog::getOpenFileName(this, dialogTitle, lastDir,
        "图片文件 (*.png *.jpg *.jpeg *.gif *.webp)");
    if (fileName.isEmpty()) return QString();

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

bool WanGenPage::validateImageFile(const QString &filePath, QString &errorMsg) const
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) { errorMsg = "文件不存在"; return false; }
    if (fileInfo.size() > 10 * 1024 * 1024) {
        errorMsg = QString("文件过大（%1 MB），请选择小于 10MB 的图片").arg(fileInfo.size() / 1024.0 / 1024.0, 0, 'f', 2);
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

bool WanGenPage::validateImgbbKey(QString &errorMsg) const
{
    ImgbbKey activeImgbbKey = DBManager::instance()->getActiveImgbbKey();
    if (activeImgbbKey.apiKey.isEmpty()) {
        errorMsg = "请先到设置页应用临时图床密钥";
        return false;
    }
    return true;
}

void WanGenPage::uploadAudio()
{
    if (audioUploading) return;

    QString filePath = QFileDialog::getOpenFileName(this, "选择音频文件",
        QString(), "音频文件 (*.mp3 *.wav *.m4a *.aac *.ogg);;所有文件 (*.*)");
    if (filePath.isEmpty()) return;

    QFileInfo fi(filePath);
    if (fi.size() > 100 * 1024 * 1024) {
        QMessageBox::warning(this, "提示", "音频文件不能超过100MB");
        return;
    }

    uploadedAudioPath = filePath;
    applyAudioFileLabelState(audioFileLabel, true, fi.fileName());
    clearAudioButton->setVisible(true);

    audioUploading = true;
    ImageUploader *uploader = api->getImageUploader();
    connect(uploader, &ImageUploader::audioUploadSuccess, this, [this, uploader](const QString &url) {
        uploadedAudioUrl = url;
        audioUploading = false;
        qDebug() << "[WAN] Audio uploaded:" << url;
        disconnect(uploader, &ImageUploader::audioUploadSuccess, this, nullptr);
    });
    connect(uploader, &ImageUploader::uploadError, this, [this, uploader](const QString &error) {
        audioUploading = false;
        QMessageBox::warning(this, "上传失败", error);
        disconnect(uploader, &ImageUploader::uploadError, this, nullptr);
    });
    uploader->uploadAudioToTmpfile(filePath);

    queueSaveSettings();
}

void WanGenPage::clearAudio()
{
    uploadedAudioPath.clear();
    uploadedAudioUrl.clear();
    audioUploading = false;
    applyAudioFileLabelState(audioFileLabel, false);
    clearAudioButton->setVisible(false);
    queueSaveSettings();
}

void WanGenPage::resetForm()
{
    promptInput->clear();
    negativePromptInput->clear();
    uploadedImagePath.clear();
    imagePreviewLabel->setText("未选择图片\n点击此处上传");
    imagePreviewLabel->setPixmap(QPixmap());
    imagePreviewLabel->setProperty("hasImage", false);
    imagePreviewLabel->style()->unpolish(imagePreviewLabel);
    imagePreviewLabel->style()->polish(imagePreviewLabel);
    clearAudio();
    previewLabel->setText("💡 生成结果将在【生成历史记录】");
    parametersModified = true;
}

void WanGenPage::generateVideo()
{
    QString prompt = promptInput->toPlainText().trimmed();
    if (prompt.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入视频生成提示词");
        return;
    }

    if (apiKeyCombo->count() == 0 || !apiKeyCombo->isEnabled()) {
        QMessageBox::warning(this, "提示", "请先添加 API 密钥");
        return;
    }

    QString errorMsg;
    if (!validateImgbbKey(errorMsg)) {
        QMessageBox::warning(this, "提示", errorMsg);
        return;
    }

    if (uploadedImagePath.isEmpty()) {
        QMessageBox::warning(this, "提示", "WAN 模型需要上传图片");
        return;
    }

    if (!checkDuplicateSubmission()) return;

    QString modelVariant = modelVariantCombo->currentData().toString();
    QString modelVariantText = modelVariantCombo->currentText();
    QString server = serverCombo->currentData().toString();
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
    task.modelName = "WAN视频";
    task.apiKeyName = apiKeyCombo->currentText();
    task.serverUrl = server;
    task.status = "submitting";
    task.progress = 0;
    task.resolution = resolutionCombo->currentData().toString();
    task.duration = durationCombo->currentData().toInt();
    task.watermark = watermarkCheckBox->isChecked();

    QJsonArray imageArray;
    imageArray.append(uploadedImagePath);
    task.imagePaths = QString::fromUtf8(QJsonDocument(imageArray).toJson(QJsonDocument::Compact));

    int dbId = DBManager::instance()->insertVideoTask(task);
    if (dbId < 0) {
        QMessageBox::critical(this, "错误", "无法保存任务记录");
        return;
    }

    currentTaskId = tempTaskId;
    previewLabel->setText("⏳ 正在提交视频生成任务...");

    ImgbbKey activeImgbbKey = DBManager::instance()->getActiveImgbbKey();

    VideoAPI::WanVideoParams params;
    params.apiKey = apiKeyData.apiKey;
    params.baseUrl = server;
    params.model = modelVariant;
    params.prompt = prompt;
    params.negativePrompt = negativePromptInput->toPlainText().trimmed();
    params.localImagePaths.append(uploadedImagePath);
    params.audioUrl = (audioCheckBox->isChecked() && !uploadedAudioUrl.isEmpty())
                     ? uploadedAudioUrl : QString();
    params.templateName = templateCombo->currentData().toString();
    params.resolution = resolutionCombo->currentData().toString();
    params.duration = durationCombo->currentData().toInt();
    params.promptExtend = promptExtendCheckBox->isChecked();
    params.watermark = watermarkCheckBox->isChecked();
    params.seed = QString::number(seedInput->value());

    api->prepareWanRequest(params);
    api->uploadImageToImgbb(uploadedImagePath, activeImgbbKey.apiKey);
}

void WanGenPage::onVideoCreated(const QString &taskId, const QString &status)
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
    pollMgr->startPolling(taskId, "video_single", apiKeyData.apiKey, server, "WAN视频");

    lastSubmittedParamsHash = calculateParamsHash();
    parametersModified = false;
    saveSettings();

    QMessageBox::information(this, "任务已提交",
        "视频生成任务已提交！\n\n请前往【生成历史记录】标签页查看任务状态。\n系统将自动轮询任务状态并下载完成的视频。");
    previewLabel->setText("💡 生成结果将在【生成历史记录】");
}

void WanGenPage::onTaskStatusUpdated(const QString &taskId, const QString &status, const QString &videoUrl, int progress)
{
    Q_UNUSED(taskId);
    Q_UNUSED(status);
    Q_UNUSED(videoUrl);
    Q_UNUSED(progress);
}

void WanGenPage::onApiError(const QString &error)
{
    if (!currentTaskId.isEmpty()) {
        DBManager::instance()->updateTaskStatus(currentTaskId, "failed", 0, "");
        DBManager::instance()->updateTaskErrorMessage(currentTaskId, error);
    }
    QMessageBox::critical(this, "错误", QString("API 调用失败:\n%1").arg(error));
    previewLabel->setText("💡 生成结果将在【生成历史记录】");
}

void WanGenPage::queueSaveSettings()
{
    if (suppressAutoSave) return;
    if (pendingSaveSettings) return;
    pendingSaveSettings = true;
    QTimer::singleShot(0, this, [this]() {
        pendingSaveSettings = false;
        saveSettings();
    });
}

QString WanGenPage::buildSettingsSnapshot() const
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
        negativePromptInput->toPlainText(),
        resolutionCombo->currentData().toString(),
        durationCombo->currentData().toString(),
        templateCombo->currentData().toString(),
        QString::number(promptExtendCheckBox->isChecked()),
        QString::number(seedInput->value()),
        QString::number(audioCheckBox->isChecked()),
        uploadedAudioPath,
        QString::number(watermarkCheckBox->isChecked()),
        uploadedImagePath
    };

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(snapshotParts.join("\x1F").toUtf8());
    return QString(hash.result().toHex());
}

void WanGenPage::saveSettings()
{
    const QString snapshot = buildSettingsSnapshot();
    if (!lastSavedSettingsSnapshot.isEmpty() && snapshot == lastSavedSettingsSnapshot) return;

    QSettings settings("ChickenAI", "VideoGen");
    settings.beginGroup("VideoSingleTab");

    settings.setValue("wan_prompt", promptInput->toPlainText());
    settings.setValue("wan_negativePrompt", negativePromptInput->toPlainText());
    settings.setValue("wan_modelVariant", modelVariantCombo->currentData().toString());
    settings.setValue("wan_apiKey", apiKeyCombo->currentIndex());
    settings.setValue("wan_server", serverCombo->currentIndex());
    settings.setValue("wan_resolution", resolutionCombo->currentData().toString());
    settings.setValue("wan_duration", durationCombo->currentData().toInt());
    settings.setValue("wan_template", templateCombo->currentData().toString());
    settings.setValue("wan_promptExtend", promptExtendCheckBox->isChecked());
    settings.setValue("wan_seed", seedInput->value());
    settings.setValue("wan_audioEnabled", audioCheckBox->isChecked());
    settings.setValue("wan_audioPath", uploadedAudioPath);
    settings.setValue("wan_audioUrl", uploadedAudioUrl);
    settings.setValue("wan_watermark", watermarkCheckBox->isChecked());
    settings.setValue("wan_imagePath", uploadedImagePath);
    settings.setValue("wan_lastSubmittedHash", lastSubmittedParamsHash);

    settings.endGroup();
    settings.sync();
    lastSavedSettingsSnapshot = snapshot;
}

void WanGenPage::loadSettings()
{
    QSettings settings("ChickenAI", "VideoGen");
    settings.beginGroup("VideoSingleTab");

    promptInput->blockSignals(true);
    negativePromptInput->blockSignals(true);
    modelVariantCombo->blockSignals(true);
    apiKeyCombo->blockSignals(true);
    serverCombo->blockSignals(true);
    durationCombo->blockSignals(true);
    resolutionCombo->blockSignals(true);
    templateCombo->blockSignals(true);
    promptExtendCheckBox->blockSignals(true);
    seedInput->blockSignals(true);
    audioCheckBox->blockSignals(true);
    watermarkCheckBox->blockSignals(true);

    promptInput->setPlainText(settings.value("wan_prompt", "").toString());
    negativePromptInput->setPlainText(settings.value("wan_negativePrompt", "").toString());

    QString savedVariant = settings.value("wan_modelVariant", "").toString();
    for (int i = 0; i < modelVariantCombo->count(); ++i) {
        if (modelVariantCombo->itemData(i).toString() == savedVariant) {
            modelVariantCombo->setCurrentIndex(i);
            break;
        }
    }
    onModelVariantChanged(0);

    // 恢复 API 密钥：优先使用索引，备选使用密钥值匹配
    int apiKeyIndex = settings.value("wan_apiKey", 0).toInt();
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

    int serverIndex = settings.value("wan_server", 0).toInt();
    if (serverIndex >= 0 && serverIndex < serverCombo->count()) {
        serverCombo->setCurrentIndex(serverIndex);
    }

    resolutionCombo->setCurrentIndex(
        resolutionCombo->findData(settings.value("wan_resolution", "720P").toString()));
    durationCombo->setCurrentIndex(
        durationCombo->findData(settings.value("wan_duration", 5).toInt()));
    templateCombo->setCurrentIndex(
        templateCombo->findData(settings.value("wan_template", "").toString()));
    promptExtendCheckBox->setChecked(settings.value("wan_promptExtend", true).toBool());
    seedInput->setValue(settings.value("wan_seed", 0).toInt());
    audioCheckBox->setChecked(settings.value("wan_audioEnabled", false).toBool());
    uploadedAudioPath = settings.value("wan_audioPath", "").toString();
    uploadedAudioUrl = settings.value("wan_audioUrl", "").toString();
    if (!uploadedAudioPath.isEmpty() && QFile::exists(uploadedAudioPath)) {
        QFileInfo fi(uploadedAudioPath);
        applyAudioFileLabelState(audioFileLabel, true, fi.fileName());
        clearAudioButton->setVisible(true);
    }
    watermarkCheckBox->setChecked(settings.value("wan_watermark", false).toBool());

    uploadedImagePath = settings.value("wan_imagePath", "").toString();
    if (!uploadedImagePath.isEmpty() && QFile::exists(uploadedImagePath)) {
        updateImagePreview();
    }

    lastSubmittedParamsHash = settings.value("wan_lastSubmittedHash", "").toString();

    settings.endGroup();
    lastSavedSettingsSnapshot = buildSettingsSnapshot();

    promptInput->blockSignals(false);
    negativePromptInput->blockSignals(false);
    modelVariantCombo->blockSignals(false);
    apiKeyCombo->blockSignals(false);
    serverCombo->blockSignals(false);
    durationCombo->blockSignals(false);
    resolutionCombo->blockSignals(false);
    templateCombo->blockSignals(false);
    promptExtendCheckBox->blockSignals(false);
    seedInput->blockSignals(false);
    audioCheckBox->blockSignals(false);
    watermarkCheckBox->blockSignals(false);

    parametersModified = false;
}

QString WanGenPage::calculateParamsHash() const
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(promptInput->toPlainText().toUtf8());
    hash.addData(negativePromptInput->toPlainText().toUtf8());
    hash.addData(QString::number(modelVariantCombo->currentIndex()).toUtf8());
    hash.addData(QString::number(durationCombo->currentIndex()).toUtf8());
    hash.addData(QString::number(resolutionCombo->currentIndex()).toUtf8());
    hash.addData(QString::number(promptExtendCheckBox->isChecked()).toUtf8());
    hash.addData(QString::number(seedInput->value()).toUtf8());
    hash.addData(QString::number(audioCheckBox->isChecked()).toUtf8());
    hash.addData(QString::number(watermarkCheckBox->isChecked()).toUtf8());
    hash.addData(uploadedImagePath.toUtf8());
    return QString(hash.result().toHex());
}

bool WanGenPage::checkDuplicateSubmission()
{
    QString currentHash = calculateParamsHash();
    if (!parametersModified && currentHash == lastSubmittedParamsHash && !suppressDuplicateWarning) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("重复提交提醒");
        msgBox.setText("刚刚已经提交这次任务，是否继续？");
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

void WanGenPage::onAnyParameterChanged()
{
    parametersModified = true;
    suppressDuplicateWarning = false;
}

void WanGenPage::loadFromTask(const VideoTask& task)
{
    blockSignals(true);
    promptInput->setPlainText(task.prompt);
    negativePromptInput->setPlainText("");

    for (int i = 0; i < modelVariantCombo->count(); ++i) {
        if (modelVariantCombo->itemText(i) == task.modelVariant) {
            modelVariantCombo->setCurrentIndex(i);
            break;
        }
    }
    onModelVariantChanged(0);

    bool apiKeyFound = false;
    for (int i = 0; i < apiKeyCombo->count(); ++i) {
        if (apiKeyCombo->itemText(i) == task.apiKeyName) {
            apiKeyCombo->setCurrentIndex(i);
            apiKeyFound = true;
            break;
        }
    }
    if (!apiKeyFound && apiKeyCombo->count() > 0) apiKeyCombo->setCurrentIndex(0);

    for (int i = 0; i < serverCombo->count(); ++i) {
        if (serverCombo->itemData(i).toString() == task.serverUrl) {
            serverCombo->setCurrentIndex(i);
            break;
        }
    }

    if (!task.imagePaths.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(task.imagePaths.toUtf8());
        if (doc.isArray()) {
            QJsonArray imageArray = doc.array();
            if (!imageArray.isEmpty()) {
                QString path = imageArray[0].toString();
                if (QFile::exists(path)) {
                    uploadedImagePath = path;
                    updateImagePreview();
                }
            }
        }
    }

    parametersModified = false;
    lastSubmittedParamsHash.clear();
    blockSignals(false);
}
