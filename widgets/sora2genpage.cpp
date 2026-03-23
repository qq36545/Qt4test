#include "sora2genpage.h"

#include "../database/dbmanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QListWidget>
#include <QStackedWidget>
#include <QGroupBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QImageReader>
#include <QScrollArea>
#include <QFrame>
#include <QPalette>
#include <QColor>
#include <QFont>
#include <QShowEvent>
#include <QObject>

Sora2GenPage::Sora2GenPage(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void Sora2GenPage::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *contentWidget = new QWidget();
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(20, 20, 20, 20);
    contentLayout->setSpacing(12);

    auto *apiFormatLayout = new QHBoxLayout();
    apiFormatLabel = new QLabel("API格式:");
    apiFormatLayout->addWidget(apiFormatLabel);
    unifiedFormatRadio = new QRadioButton("统一格式");
    openaiFormatRadio = new QRadioButton("OpenAI格式");
    apiFormatGroup = new QButtonGroup(this);
    apiFormatGroup->setExclusive(true);
    apiFormatGroup->addButton(unifiedFormatRadio);
    apiFormatGroup->addButton(openaiFormatRadio);
    unifiedFormatRadio->setChecked(true);
    apiFormatLayout->addWidget(unifiedFormatRadio);
    apiFormatLayout->addWidget(openaiFormatRadio);
    apiFormatLayout->addStretch();
    contentLayout->addLayout(apiFormatLayout);

    auto *variantLayout = new QHBoxLayout();
    variantLayout->addWidget(new QLabel("模型变体:"));
    variantCombo = new QComboBox();
    variantCombo->addItems({
        "sora-2-all",
        "sora-2",
        "sora-2-vip-all",
        "sora-2-pro",
        "sora-2-pro-all"
    });
    variantLayout->addWidget(variantCombo, 1);
    contentLayout->addLayout(variantLayout);

    paramsStack = new QStackedWidget();

    auto *unifiedWidget = new QWidget();
    auto *unifiedRow = new QHBoxLayout(unifiedWidget);
    unifiedRow->setContentsMargins(0, 0, 0, 0);
    unifiedRow->setSpacing(12);

    auto *unifiedSizeLayout = new QVBoxLayout();
    auto *unifiedSizeLabel = new QLabel("分辨率");
    unifiedSizeLabel->setStyleSheet("font-size: 14px;");
    unifiedSizeCombo = new QComboBox();
    unifiedSizeCombo->addItem("small(720p)", "small");
    unifiedSizeCombo->addItem("large(1080p)", "large");
    unifiedSizeLayout->addWidget(unifiedSizeLabel);
    unifiedSizeLayout->addWidget(unifiedSizeCombo);

    auto *unifiedOrientationLayout = new QVBoxLayout();
    auto *unifiedOrientationLabel = new QLabel("方向");
    unifiedOrientationLabel->setStyleSheet("font-size: 14px;");
    unifiedOrientationCombo = new QComboBox();
    unifiedOrientationCombo->addItem("竖屏", "portrait");
    unifiedOrientationCombo->addItem("横屏", "landscape");
    unifiedOrientationLayout->addWidget(unifiedOrientationLabel);
    unifiedOrientationLayout->addWidget(unifiedOrientationCombo);

    auto *unifiedDurationLayout = new QVBoxLayout();
    auto *unifiedDurationLabel = new QLabel("时长");
    unifiedDurationLabel->setStyleSheet("font-size: 14px;");
    unifiedDurationCombo = new QComboBox();
    unifiedDurationCombo->addItem("5秒", "5");
    unifiedDurationCombo->addItem("10秒", "10");
    unifiedDurationLayout->addWidget(unifiedDurationLabel);
    unifiedDurationLayout->addWidget(unifiedDurationCombo);

    auto *unifiedWatermarkLayout = new QVBoxLayout();
    auto *unifiedWatermarkLabel = new QLabel("watermark");
    unifiedWatermarkLabel->setStyleSheet("font-size: 14px;");
    watermarkCheckBox = new QCheckBox("开启");
    watermarkCheckBox->setChecked(false);
    unifiedWatermarkLayout->addWidget(unifiedWatermarkLabel);
    unifiedWatermarkLayout->addWidget(watermarkCheckBox);

    auto *unifiedPrivateLayout = new QVBoxLayout();
    auto *unifiedPrivateLabel = new QLabel("private");
    unifiedPrivateLabel->setStyleSheet("font-size: 14px;");
    privateCheckBox = new QCheckBox("开启");
    privateCheckBox->setChecked(false);
    unifiedPrivateLayout->addWidget(unifiedPrivateLabel);
    unifiedPrivateLayout->addWidget(privateCheckBox);

    unifiedRow->addLayout(unifiedSizeLayout);
    unifiedRow->addLayout(unifiedOrientationLayout);
    unifiedRow->addLayout(unifiedDurationLayout);
    unifiedRow->addLayout(unifiedWatermarkLayout);
    unifiedRow->addLayout(unifiedPrivateLayout);
    unifiedRow->addStretch();
    paramsStack->addWidget(unifiedWidget);

    auto *openaiWidget = new QWidget();
    auto *openaiRow = new QHBoxLayout(openaiWidget);
    openaiRow->setContentsMargins(0, 0, 0, 0);
    openaiRow->setSpacing(12);

    auto *openaiSizeLayout = new QVBoxLayout();
    auto *openaiSizeLabel = new QLabel("分辨率");
    openaiSizeLabel->setStyleSheet("font-size: 14px;");
    openaiSizeCombo = new QComboBox();
    openaiSizeCombo->addItem("720x1280", "720x1280");
    openaiSizeCombo->addItem("1280x720", "1280x720");
    openaiSizeCombo->addItem("1024x1792", "1024x1792");
    openaiSizeCombo->addItem("1792x1024", "1792x1024");
    openaiSizeLayout->addWidget(openaiSizeLabel);
    openaiSizeLayout->addWidget(openaiSizeCombo);

    auto *openaiDurationLayout = new QVBoxLayout();
    auto *openaiDurationLabel = new QLabel("时长");
    openaiDurationLabel->setStyleSheet("font-size: 14px;");
    openaiSecondsCombo = new QComboBox();
    openaiSecondsCombo->addItem("4秒", "4");
    openaiSecondsCombo->addItem("8秒", "8");
    openaiSecondsCombo->addItem("12秒", "12");
    openaiDurationLayout->addWidget(openaiDurationLabel);
    openaiDurationLayout->addWidget(openaiSecondsCombo);

    auto *openaiStyleLayout = new QVBoxLayout();
    auto *openaiStyleLabel = new QLabel("风格");
    openaiStyleLabel->setStyleSheet("font-size: 14px;");
    openaiStyleCombo = new QComboBox();
    openaiStyleCombo->addItem("默认(空)", "");
    openaiStyleCombo->addItem("感恩节", "thanksgiving");
    openaiStyleCombo->addItem("漫画", "comic");
    openaiStyleCombo->addItem("新闻", "news");
    openaiStyleCombo->addItem("自拍", "selfie");
    openaiStyleCombo->addItem("怀旧", "nostalgic");
    openaiStyleCombo->addItem("动漫", "anime");
    openaiStyleLayout->addWidget(openaiStyleLabel);
    openaiStyleLayout->addWidget(openaiStyleCombo);

    auto *openaiWatermarkLayout = new QVBoxLayout();
    auto *openaiWatermarkLabel = new QLabel("watermark");
    openaiWatermarkLabel->setStyleSheet("font-size: 14px;");
    auto *openaiWatermarkCheckBox = new QCheckBox("开启");
    openaiWatermarkCheckBox->setChecked(false);
    openaiWatermarkLayout->addWidget(openaiWatermarkLabel);
    openaiWatermarkLayout->addWidget(openaiWatermarkCheckBox);

    auto *openaiPrivateLayout = new QVBoxLayout();
    auto *openaiPrivateLabel = new QLabel("private");
    openaiPrivateLabel->setStyleSheet("font-size: 14px;");
    auto *openaiPrivateCheckBox = new QCheckBox("开启");
    openaiPrivateCheckBox->setChecked(false);
    openaiPrivateLayout->addWidget(openaiPrivateLabel);
    openaiPrivateLayout->addWidget(openaiPrivateCheckBox);

    connect(openaiWatermarkCheckBox, &QCheckBox::toggled, watermarkCheckBox, &QCheckBox::setChecked);
    connect(watermarkCheckBox, &QCheckBox::toggled, openaiWatermarkCheckBox, &QCheckBox::setChecked);
    connect(openaiPrivateCheckBox, &QCheckBox::toggled, privateCheckBox, &QCheckBox::setChecked);
    connect(privateCheckBox, &QCheckBox::toggled, openaiPrivateCheckBox, &QCheckBox::setChecked);

    openaiRow->addLayout(openaiSizeLayout);
    openaiRow->addLayout(openaiDurationLayout);
    openaiRow->addLayout(openaiStyleLayout);
    openaiRow->addLayout(openaiWatermarkLayout);
    openaiRow->addLayout(openaiPrivateLayout);
    openaiRow->addStretch();
    paramsStack->addWidget(openaiWidget);

    auto *paramsGroup = new QGroupBox("参数设置");
    auto *paramsLayout = new QVBoxLayout(paramsGroup);
    paramsLayout->addWidget(paramsStack);
    contentLayout->addWidget(paramsGroup);

    auto *promptHeaderLayout = new QHBoxLayout();
    auto *promptLabel = new QLabel("提示词:");
    promptLabel->setStyleSheet("font-size: 14px;");
    auto *clearPromptButton = new QPushButton("清空文本");
    clearPromptButton->setCursor(Qt::PointingHandCursor);
    connect(clearPromptButton, &QPushButton::clicked, this, [this]() {
        promptInput->clear();
    });
    promptHeaderLayout->addWidget(promptLabel);
    promptHeaderLayout->addStretch();
    promptHeaderLayout->addWidget(clearPromptButton);
    contentLayout->addLayout(promptHeaderLayout);

    promptInput = new QTextEdit();
    promptInput->setMinimumHeight(240);
    promptInput->setPlaceholderText("输入视频生成提示词...\n例如：一只可爱的猫咪在花园里玩耍，阳光明媚，电影级画质");
    promptInput->setStyleSheet("font-size: 15px;");
    contentLayout->addWidget(promptInput);

    auto *imagesTitleLayout = new QHBoxLayout();
    auto *imagesLabel = new QLabel("参考图:");
    imagesLabel->setStyleSheet("font-size: 14px;");
    imagesTitleLayout->addWidget(imagesLabel);
    imagesTitleLayout->addStretch();
    contentLayout->addLayout(imagesTitleLayout);

    auto *imagesLayout = new QHBoxLayout();
    imageList = new QListWidget();
    imageList->setMinimumHeight(150);
    imageList->setStyleSheet("font-size: 14px;");

    auto *imagesButtonsLayout = new QVBoxLayout();
    uploadImagesButton = new QPushButton("选择参考图");
    uploadImagesButton->setFixedWidth(150);
    uploadImagesButton->setCursor(Qt::PointingHandCursor);

    auto *clearImagesButton = new QPushButton("清除");
    clearImagesButton->setFixedWidth(80);
    clearImagesButton->setCursor(Qt::PointingHandCursor);
    connect(clearImagesButton, &QPushButton::clicked, this, [this]() {
        uploadedImagePaths.clear();
        refreshImageList();
    });

    imagesButtonsLayout->addWidget(uploadImagesButton);
    imagesButtonsLayout->addWidget(clearImagesButton);
    imagesButtonsLayout->addStretch();

    imagesLayout->addWidget(imageList, 1);
    imagesLayout->addLayout(imagesButtonsLayout);
    contentLayout->addLayout(imagesLayout);

    submitButton = new QPushButton("开始生成");
    contentLayout->addWidget(submitButton);

    connect(submitButton, &QPushButton::clicked, this, &Sora2GenPage::onSubmitClicked);
    connect(unifiedFormatRadio, &QRadioButton::toggled, this, &Sora2GenPage::onApiFormatChanged);
    connect(openaiFormatRadio, &QRadioButton::toggled, this, &Sora2GenPage::onApiFormatChanged);
    connect(uploadImagesButton, &QPushButton::clicked, this, &Sora2GenPage::onUploadImagesClicked);

    contentLayout->addStretch();
    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);

    onApiFormatChanged();
    applyApiFormatRadioStyle();
}

void Sora2GenPage::applyApiFormatRadioStyle()
{
    const QFont labelFont = apiFormatLabel->font();
    unifiedFormatRadio->setFont(labelFont);
    openaiFormatRadio->setFont(labelFont);

    const QString appTheme = resolveAppTheme();
    QString textColor;
    if (appTheme == "dark") {
        textColor = "#FFFFFF";
    } else if (appTheme == "light") {
        textColor = "#000000";
    } else {
        const QColor windowColor = palette().color(QPalette::Window);
        const int brightness = (windowColor.red() * 299 + windowColor.green() * 587 + windowColor.blue() * 114) / 1000;
        textColor = (brightness < 128) ? "#FFFFFF" : "#000000";
    }

    const int fontSize = labelFont.pointSize() > 0 ? labelFont.pointSize() : font().pointSize();
    const QString style = QString(
        "QRadioButton { color: %1; font-size: %2pt; }"
        "QRadioButton::indicator:disabled { border: 1px solid %1; }"
    ).arg(textColor).arg(fontSize);

    unifiedFormatRadio->setStyleSheet(style);
    openaiFormatRadio->setStyleSheet(style);
    unifiedFormatRadio->update();
    openaiFormatRadio->update();
}

QString Sora2GenPage::resolveAppTheme() const
{
    const QObject *obj = this;
    while (obj) {
        const QVariant theme = obj->property("appTheme");
        if (theme.isValid()) {
            const QString value = theme.toString().trimmed().toLower();
            if (value == "dark" || value == "light") {
                return value;
            }
        }
        obj = obj->parent();
    }
    return QString();
}

void Sora2GenPage::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange || event->type() == QEvent::ThemeChange) {
        applyApiFormatRadioStyle();
    }
}

void Sora2GenPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    applyApiFormatRadioStyle();
}

void Sora2GenPage::refreshImageList()
{
    imageList->clear();
    for (const QString &path : uploadedImagePaths) {
        imageList->addItem(QFileInfo(path).fileName());
    }
}

QString Sora2GenPage::currentApiFormat() const
{
    return unifiedFormatRadio->isChecked() ? "unified" : "openai";
}

QString Sora2GenPage::currentVariant() const
{
    return variantCombo->currentText();
}

QStringList Sora2GenPage::currentImagePaths() const
{
    return uploadedImagePaths;
}

bool Sora2GenPage::isImageToVideo() const
{
    return !uploadedImagePaths.isEmpty();
}

QVariantMap Sora2GenPage::buildUnifiedPayload() const
{
    QVariantMap payload;
    payload["api_format"] = "unified";
    payload["model"] = currentVariant();
    payload["prompt"] = promptInput->toPlainText().trimmed();
    payload["images"] = currentImagePaths();
    payload["size"] = unifiedSizeCombo->currentData().toString();
    payload["orientation"] = unifiedOrientationCombo->currentData().toString();
    payload["duration"] = unifiedDurationCombo->currentData().toString();
    payload["watermark"] = watermarkCheckBox->isChecked();
    payload["private"] = privateCheckBox->isChecked();
    payload["is_image_to_video"] = isImageToVideo();
    return payload;
}

QVariantMap Sora2GenPage::buildOpenAIPayload() const
{
    QVariantMap payload;
    payload["api_format"] = "openai";
    payload["model"] = currentVariant();
    payload["prompt"] = promptInput->toPlainText().trimmed();
    payload["seconds"] = openaiSecondsCombo->currentData().toString();
    payload["size"] = openaiSizeCombo->currentData().toString();
    payload["style"] = openaiStyleCombo->currentData().toString();
    payload["images"] = currentImagePaths();
    payload["watermark"] = watermarkCheckBox->isChecked();
    payload["private"] = privateCheckBox->isChecked();
    payload["is_image_to_video"] = isImageToVideo();
    return payload;
}

void Sora2GenPage::onSubmitClicked()
{
    const QString prompt = promptInput->toPlainText().trimmed();
    if (prompt.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入提示词");
        return;
    }

    if (currentApiFormat() == "unified") {
        emit createTaskRequested(buildUnifiedPayload());
    } else {
        emit createTaskRequested(buildOpenAIPayload());
    }
}

void Sora2GenPage::onApiFormatChanged()
{
    paramsStack->setCurrentIndex(unifiedFormatRadio->isChecked() ? 0 : 1);
}

void Sora2GenPage::onUploadImagesClicked()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this,
        "选择参考图",
        QString(),
        "图片文件 (*.png *.jpg *.jpeg *.webp *.bmp)"
    );

    if (files.isEmpty()) {
        return;
    }

    QStringList validFiles;
    for (const QString &file : files) {
        QImageReader reader(file);
        if (reader.canRead()) {
            validFiles.append(file);
        }
    }

    if (validFiles.isEmpty()) {
        QMessageBox::warning(this, "提示", "未选择有效图片文件");
        return;
    }

    uploadedImagePaths = validFiles;
    refreshImageList();
}

void Sora2GenPage::loadFromTask(const VideoTask &task)
{
    promptInput->setText(task.prompt);

    const int variantIndex = variantCombo->findText(task.modelVariant);
    if (variantIndex >= 0) {
        variantCombo->setCurrentIndex(variantIndex);
    }

    uploadedImagePaths.clear();
    if (!task.imagePaths.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(task.imagePaths.toUtf8());
        if (doc.isArray()) {
            const QJsonArray arr = doc.array();
            for (const QJsonValue &v : arr) {
                if (v.isString()) {
                    uploadedImagePaths.append(v.toString());
                }
            }
        }
    }
    refreshImageList();
}
