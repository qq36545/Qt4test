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
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QImageReader>

Sora2GenPage::Sora2GenPage(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void Sora2GenPage::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(12);

    auto *apiFormatLayout = new QHBoxLayout();
    apiFormatLayout->addWidget(new QLabel("API格式:"));
    apiFormatCombo = new QComboBox();
    apiFormatCombo->addItem("统一格式", "unified");
    apiFormatCombo->addItem("OpenAI格式", "openai");
    apiFormatLayout->addWidget(apiFormatCombo, 1);
    mainLayout->addLayout(apiFormatLayout);

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
    mainLayout->addLayout(variantLayout);

    mainLayout->addWidget(new QLabel("提示词:"));
    promptInput = new QTextEdit();
    promptInput->setMinimumHeight(180);
    promptInput->setPlaceholderText("请输入视频生成提示词...");
    mainLayout->addWidget(promptInput);

    auto *imagesTitleLayout = new QHBoxLayout();
    imagesTitleLayout->addWidget(new QLabel("参考图:"));
    imagesTitleLayout->addStretch();
    uploadImagesButton = new QPushButton("上传参考图");
    imagesTitleLayout->addWidget(uploadImagesButton);
    mainLayout->addLayout(imagesTitleLayout);

    imageList = new QListWidget();
    imageList->setMinimumHeight(96);
    mainLayout->addWidget(imageList);

    paramsStack = new QStackedWidget();

    auto *unifiedWidget = new QWidget();
    auto *unifiedForm = new QFormLayout(unifiedWidget);
    unifiedSizeCombo = new QComboBox();
    unifiedSizeCombo->addItem("small(720p)", "small");
    unifiedSizeCombo->addItem("large(1080p)", "large");
    unifiedOrientationCombo = new QComboBox();
    unifiedOrientationCombo->addItem("竖屏", "portrait");
    unifiedOrientationCombo->addItem("横屏", "landscape");
    unifiedDurationCombo = new QComboBox();
    unifiedDurationCombo->addItem("5秒", "5");
    unifiedDurationCombo->addItem("10秒", "10");
    unifiedForm->addRow("分辨率", unifiedSizeCombo);
    unifiedForm->addRow("方向", unifiedOrientationCombo);
    unifiedForm->addRow("时长", unifiedDurationCombo);
    paramsStack->addWidget(unifiedWidget);

    auto *openaiWidget = new QWidget();
    auto *openaiForm = new QFormLayout(openaiWidget);
    openaiSecondsCombo = new QComboBox();
    openaiSecondsCombo->addItem("4秒", "4");
    openaiSecondsCombo->addItem("8秒", "8");
    openaiSecondsCombo->addItem("12秒", "12");
    openaiSizeCombo = new QComboBox();
    openaiSizeCombo->addItem("720x1280", "720x1280");
    openaiSizeCombo->addItem("1280x720", "1280x720");
    openaiSizeCombo->addItem("1024x1792", "1024x1792");
    openaiSizeCombo->addItem("1792x1024", "1792x1024");
    openaiStyleCombo = new QComboBox();
    openaiStyleCombo->addItem("默认(空)", "");
    openaiStyleCombo->addItem("感恩节", "thanksgiving");
    openaiStyleCombo->addItem("漫画", "comic");
    openaiStyleCombo->addItem("新闻", "news");
    openaiStyleCombo->addItem("自拍", "selfie");
    openaiStyleCombo->addItem("怀旧", "nostalgic");
    openaiStyleCombo->addItem("动漫", "anime");
    openaiForm->addRow("时长", openaiSecondsCombo);
    openaiForm->addRow("分辨率", openaiSizeCombo);
    openaiForm->addRow("风格", openaiStyleCombo);
    paramsStack->addWidget(openaiWidget);

    auto *paramsGroup = new QGroupBox("参数设置");
    auto *paramsLayout = new QVBoxLayout(paramsGroup);
    paramsLayout->addWidget(paramsStack);
    mainLayout->addWidget(paramsGroup);

    auto *flagsLayout = new QHBoxLayout();
    watermarkCheckBox = new QCheckBox("watermark");
    privateCheckBox = new QCheckBox("private");
    watermarkCheckBox->setChecked(false);
    privateCheckBox->setChecked(false);
    flagsLayout->addWidget(watermarkCheckBox);
    flagsLayout->addWidget(privateCheckBox);
    flagsLayout->addStretch();
    mainLayout->addLayout(flagsLayout);

    submitButton = new QPushButton("开始生成");
    mainLayout->addWidget(submitButton);

    connect(submitButton, &QPushButton::clicked, this, &Sora2GenPage::onSubmitClicked);
    connect(apiFormatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Sora2GenPage::onApiFormatChanged);
    connect(uploadImagesButton, &QPushButton::clicked, this, &Sora2GenPage::onUploadImagesClicked);

    onApiFormatChanged(apiFormatCombo->currentIndex());
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
    return apiFormatCombo->currentData().toString();
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

void Sora2GenPage::onApiFormatChanged(int index)
{
    paramsStack->setCurrentIndex(index == 0 ? 0 : 1);
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
