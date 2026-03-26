#include "grokgen.h"
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
#include <QFile>
#include <QStandardPaths>
#include <QImageReader>
#include <QDateTime>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QDir>
#include <QMouseEvent>

GrokGenPage::GrokGenPage(QWidget *parent)
    : QWidget(parent),
      suppressDuplicateWarning(false),
      parametersModified(false),
      pendingSaveSettings(false),
      suppressAutoSave(false)
{
    api = new VideoAPI(this);
    connect(api, &VideoAPI::videoCreated, this, &GrokGenPage::onVideoCreated);
    connect(api, &VideoAPI::taskStatusUpdated, this, &GrokGenPage::onTaskStatusUpdated);
    connect(api, &VideoAPI::errorOccurred, this, &GrokGenPage::onApiError);

    setupUI();
    loadApiKeys();
    loadSettings();
    connectSignals();
}

void GrokGenPage::setupUI()
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

    QHBoxLayout *variantLayout = new QHBoxLayout();
    QLabel *variantLabel = new QLabel("模型变体:");
    variantLabel->setStyleSheet("font-size: 14px;");
    modelVariantCombo = new QComboBox();
    modelVariantCombo->addItem("grok-video-3-15s（15秒）", "grok-video-3-15s");
    modelVariantCombo->addItem("grok-video-3-10s（10秒）", "grok-video-3-10s");
    modelVariantCombo->addItem("grok-video-3（6秒）", "grok-video-3");
    connect(modelVariantCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GrokGenPage::onModelVariantChanged);
    variantLayout->addWidget(variantLabel);
    variantLayout->addWidget(modelVariantCombo, 1);
    contentLayout->addLayout(variantLayout);

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

    QHBoxLayout *serverLayout = new QHBoxLayout();
    QLabel *serverLabel = new QLabel("请求服务器:");
    serverLabel->setStyleSheet("font-size: 14px;");
    serverCombo = new QComboBox();
    serverCombo->addItem("【主站】https://ai.kegeai.top", "https://ai.kegeai.top");
    serverCombo->addItem("【备用】https://api.kuai.host", "https://api.kuai.host");
    serverCombo->addItem("【备用】https://api.kegeai.top", "https://api.kegeai.top");
    serverCombo->setCurrentIndex(0);
    serverLayout->addWidget(serverLabel);
    serverLayout->addWidget(serverCombo, 1);
    contentLayout->addLayout(serverLayout);

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

    QHBoxLayout *imageHeaderLayout = new QHBoxLayout();
    imageLabel = new QLabel("参考图（可选）:");
    imageLabel->setStyleSheet("font-size: 14px;");
    imageUploadHintLabel = new QLabel("💡提示：垫图后，视频尺寸跟垫图的图片尺寸保持一致");
    imageUploadHintLabel->setStyleSheet("font-size: 12px; color: #888888;");
    imageUploadHintLabel->setWordWrap(false);
    referenceCountLabel = new QLabel(QString("已选 0/%1 张").arg(maxReferenceImages()));
    referenceCountLabel->setStyleSheet("font-size: 12px; color: #64748b;");
    clearImagesButton = new QPushButton("🗑️ 清除");
    clearImagesButton->setCursor(Qt::PointingHandCursor);
    connect(clearImagesButton, &QPushButton::clicked, this, [this]() {
        if (uploadedImagePaths.isEmpty()) {
            return;
        }
        uploadedImagePaths.clear();
        updateThumbnailGrid();
        queueSaveSettings();
    });

    imageHeaderLayout->setContentsMargins(0, 0, 0, 0);
    imageHeaderLayout->setSpacing(8);
    imageHeaderLayout->addWidget(imageLabel);
    imageHeaderLayout->addWidget(imageUploadHintLabel);
    imageHeaderLayout->addStretch();
    imageHeaderLayout->addWidget(referenceCountLabel);
    imageHeaderLayout->addWidget(clearImagesButton);
    contentLayout->addLayout(imageHeaderLayout);

    thumbnailContainer = new QWidget();
    thumbnailContainer->setObjectName("grokThumbnailContainer");
    thumbnailContainer->setStyleSheet("background-color: transparent;");
    thumbnailContainer->setMinimumHeight(90);
    thumbnailContainer->setCursor(Qt::PointingHandCursor);
    thumbnailContainer->installEventFilter(this);

    thumbnailLayout = new QGridLayout(thumbnailContainer);
    thumbnailLayout->setContentsMargins(8, 8, 8, 8);
    thumbnailLayout->setHorizontalSpacing(8);
    thumbnailLayout->setVerticalSpacing(8);
    contentLayout->addWidget(thumbnailContainer);

    QHBoxLayout *paramsLayout = new QHBoxLayout();

    QVBoxLayout *resLayout = new QVBoxLayout();
    resolutionLabel = new QLabel("宽高比");
    resolutionLabel->setStyleSheet("font-size: 14px;");
    resolutionCombo = new QComboBox();
    resolutionCombo->addItem("2:3（竖屏）", "2:3");
    resolutionCombo->addItem("3:2（横屏）", "3:2");
    resolutionCombo->addItem("1:1（方形）", "1:1");
    resLayout->addWidget(resolutionLabel);
    resLayout->addWidget(resolutionCombo);

    QVBoxLayout *sizeLayout = new QVBoxLayout();
    sizeLabel = new QLabel("分辨率规格");
    sizeLabel->setStyleSheet("font-size: 14px;");
    sizeCombo = new QComboBox();
    sizeCombo->addItem("720P", "720P");
    sizeCombo->addItem("1080P", "1080P");
    sizeLayout->addWidget(sizeLabel);
    sizeLayout->addWidget(sizeCombo);

    paramsLayout->addLayout(resLayout);
    paramsLayout->addLayout(sizeLayout);
    paramsLayout->addStretch();
    contentLayout->addLayout(paramsLayout);

    previewLabel = new QLabel();
    previewLabel->setObjectName("videoPreviewLabel");
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setText("💡 生成结果将在【生成历史记录】");
    previewLabel->setMinimumHeight(150);
    contentLayout->addWidget(previewLabel);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    generateButton = new QPushButton("🚀 生成视频");
    generateButton->setCursor(Qt::PointingHandCursor);
    connect(generateButton, &QPushButton::clicked, this, &GrokGenPage::generateVideo);
    buttonLayout->addWidget(generateButton);
    resetButton = new QPushButton("🔄 一键重置");
    resetButton->setCursor(Qt::PointingHandCursor);
    connect(resetButton, &QPushButton::clicked, this, &GrokGenPage::resetForm);
    buttonLayout->addWidget(resetButton);
    contentLayout->addLayout(buttonLayout);

    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);

    updateThumbnailGrid();
}

void GrokGenPage::connectSignals()
{
    connect(promptInput, &QTextEdit::textChanged, this, &GrokGenPage::onAnyParameterChanged);
    connect(modelVariantCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GrokGenPage::onAnyParameterChanged);
    connect(resolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GrokGenPage::onAnyParameterChanged);
    connect(sizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GrokGenPage::onAnyParameterChanged);

    connect(promptInput, &QTextEdit::textChanged, this, &GrokGenPage::queueSaveSettings);
    connect(modelVariantCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GrokGenPage::queueSaveSettings);
    connect(apiKeyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GrokGenPage::queueSaveSettings);
    connect(serverCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GrokGenPage::queueSaveSettings);
    connect(resolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GrokGenPage::queueSaveSettings);
    connect(sizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GrokGenPage::queueSaveSettings);

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

bool GrokGenPage::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        if (isSubmitting) {
            return true;
        }
        if (obj == thumbnailContainer) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            const QPoint pos = mouseEvent->pos();
            for (int i = 0; i < thumbnailLayout->count(); ++i) {
                QWidget *widget = thumbnailLayout->itemAt(i)->widget();
                if (widget && widget->geometry().contains(pos)) {
                    return QWidget::eventFilter(obj, event);
                }
            }
            onUploadImagesClicked();
            return true;
        }

        const QWidget *clickedWidget = qobject_cast<QWidget *>(obj);
        if (clickedWidget && clickedWidget->property("uploadSlot").toBool()) {
            onUploadImagesClicked();
            return true;
        }

        bool ok = false;
        const int imageIndex = clickedWidget ? clickedWidget->property("imageIndex").toInt(&ok) : -1;
        if (ok) {
            removeReferenceImage(imageIndex);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void GrokGenPage::loadApiKeys()
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

void GrokGenPage::refreshApiKeys()
{
    loadApiKeys();
}

void GrokGenPage::onModelVariantChanged(int index)
{
    Q_UNUSED(index);
}

void GrokGenPage::setSubmitting(bool submitting)
{
    isSubmitting = submitting;

    if (generateButton) generateButton->setEnabled(!submitting);
    if (resetButton) resetButton->setEnabled(!submitting);
    if (clearImagesButton) clearImagesButton->setEnabled(!submitting);
    if (modelVariantCombo) modelVariantCombo->setEnabled(!submitting);
    if (resolutionCombo) resolutionCombo->setEnabled(!submitting);
    if (sizeCombo) sizeCombo->setEnabled(!submitting);
    if (promptInput) promptInput->setReadOnly(submitting);
}

int GrokGenPage::maxReferenceImages() const
{
    return 10;
}

void GrokGenPage::updateThumbnailGrid()
{
    QLayoutItem *item = nullptr;
    while ((item = thumbnailLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    const int count = uploadedImagePaths.size();
    const int maxImages = maxReferenceImages();
    referenceCountLabel->setText(QString("已选 %1/%2 张").arg(count).arg(maxImages));

    const int slotCount = count < maxImages ? (count + 1) : count;
    const int rows = qMax(1, (slotCount + 4) / 5);
    thumbnailContainer->setMinimumHeight(rows * 74 + 16);

    for (int i = 0; i < count; ++i) {
        auto *thumb = new QLabel();
        thumb->setAlignment(Qt::AlignCenter);
        thumb->setCursor(Qt::PointingHandCursor);
        thumb->setProperty("imageIndex", i);
        thumb->setToolTip(QString("第%1张，点击替换/删除").arg(i + 1));
        thumb->setStyleSheet("border: 1px solid #475569; border-radius: 4px; background-color: #0f172a;");

        const QPixmap pix(uploadedImagePaths.at(i));
        if (!pix.isNull()) {
            thumb->setPixmap(pix.scaled(60, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            thumb->setText("加载失败");
        }

        thumb->installEventFilter(this);
        thumbnailLayout->addWidget(thumb, i / 5, i % 5);
    }

    if (count < maxImages) {
        auto *slot = new QLabel("+\n点击上传");
        slot->setAlignment(Qt::AlignCenter);
        slot->setCursor(Qt::PointingHandCursor);
        slot->setProperty("uploadSlot", true);
        slot->setStyleSheet("border: 1px dashed #64748b; border-radius: 4px; color: #94a3b8; background-color: #0f172a;");
        slot->installEventFilter(this);
        thumbnailLayout->addWidget(slot, count / 5, count % 5);
    }
}

void GrokGenPage::onUploadImagesClicked()
{
    if (isSubmitting) {
        return;
    }
    const int maxImages = maxReferenceImages();
    if (uploadedImagePaths.size() >= maxImages) {
        QMessageBox::information(this, "提示", QString("最多上传 %1 张参考图").arg(maxImages));
        return;
    }

    QSettings settings("ChickenAI", "VideoGen");
    QString lastDir = settings.value("lastImageUploadDir",
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)).toString();
    if (!QDir(lastDir).exists()) {
        lastDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }

    const QStringList selectedFiles = QFileDialog::getOpenFileNames(
        this,
        "选择参考图",
        lastDir,
        "图片文件 (*.png *.jpg *.jpeg *.webp *.bmp)"
    );

    if (selectedFiles.isEmpty()) {
        return;
    }

    const int remaining = maxImages - uploadedImagePaths.size();
    if (selectedFiles.size() > remaining) {
        QMessageBox::warning(this, "提示", QString("最多还能上传 %1 张参考图").arg(remaining));
        return;
    }

    bool added = false;
    for (const QString &filePath : selectedFiles) {
        QString errorMsg;
        if (!validateImageFile(filePath, errorMsg)) {
            QMessageBox::warning(this, "图片校验失败", errorMsg);
            continue;
        }
        if (uploadedImagePaths.contains(filePath)) {
            continue;
        }
        uploadedImagePaths.append(filePath);
        added = true;
        if (uploadedImagePaths.size() >= maxImages) {
            break;
        }
    }

    if (!selectedFiles.isEmpty()) {
        QFileInfo fileInfo(selectedFiles.last());
        settings.setValue("lastImageUploadDir", fileInfo.absolutePath());
    }

    if (added) {
        updateThumbnailGrid();
        queueSaveSettings();
    }
}

void GrokGenPage::removeReferenceImage(int index)
{
    if (index < 0 || index >= uploadedImagePaths.size()) {
        return;
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("参考图操作");
    msgBox.setText(QString("已选 %1 张参考图，请选择操作").arg(uploadedImagePaths.size()));
    QPushButton *replaceBtn = msgBox.addButton("替换", QMessageBox::AcceptRole);
    QPushButton *deleteBtn = msgBox.addButton("删除", QMessageBox::DestructiveRole);
    msgBox.addButton("取消", QMessageBox::RejectRole);

    msgBox.exec();

    if (msgBox.clickedButton() == replaceBtn) {
        replaceReferenceImage(index);
    } else if (msgBox.clickedButton() == deleteBtn) {
        uploadedImagePaths.removeAt(index);
        updateThumbnailGrid();
        queueSaveSettings();
    }
}

void GrokGenPage::replaceReferenceImage(int index)
{
    if (index < 0 || index >= uploadedImagePaths.size()) {
        return;
    }

    const QString filePath = QFileDialog::getOpenFileName(
        this,
        "选择替换图片",
        QString(),
        "图片文件 (*.png *.jpg *.jpeg *.webp *.bmp)"
    );

    if (filePath.isEmpty()) {
        return;
    }

    QString errorMsg;
    if (!validateImageFile(filePath, errorMsg)) {
        QMessageBox::warning(this, "图片校验失败", errorMsg);
        return;
    }

    uploadedImagePaths[index] = filePath;
    updateThumbnailGrid();
    queueSaveSettings();
}

bool GrokGenPage::validateImageFile(const QString &filePath, QString &errorMsg) const
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) { errorMsg = "文件不存在"; return false; }
    if (fileInfo.size() > 10 * 1024 * 1024) {
        errorMsg = QString("文件过大（%1 MB），请选择小于 10MB 的图片").arg(fileInfo.size() / 1024.0 / 1024.0, 0, 'f', 2);
        return false;
    }
    QImageReader reader(filePath);
    QString format = reader.format().toLower();
    if (format != "jpg" && format != "jpeg" && format != "png" && format != "webp" && format != "bmp") {
        errorMsg = QString("不支持的格式（%1），仅支持 JPG/PNG/WEBP/BMP").arg(format);
        return false;
    }
    return true;
}

void GrokGenPage::resetForm()
{
    promptInput->clear();
    uploadedImagePaths.clear();
    updateThumbnailGrid();
    previewLabel->setText("💡 生成结果将在【生成历史记录】");
    parametersModified = true;
}

void GrokGenPage::generateVideo()
{
    if (isSubmitting) {
        return;
    }
    QString prompt = promptInput->toPlainText().trimmed();
    if (prompt.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入视频生成提示词");
        return;
    }

    QStringList validImagePaths;
    for (const QString& path : uploadedImagePaths) {
        if (!path.isEmpty()) validImagePaths.append(path);
    }

    if (validImagePaths.isEmpty()) {
        int ret = QMessageBox::question(this, "提示", "未选择图片，将为你进行文生视频",
            QMessageBox::Yes | QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
    }

    if (apiKeyCombo->count() == 0 || !apiKeyCombo->isEnabled()) {
        QMessageBox::warning(this, "提示", "请先添加 API 密钥");
        return;
    }

    ImgbbKey activeImgbbKey = DBManager::instance()->getActiveImgbbKey();
    if (activeImgbbKey.apiKey.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先到设置页应用临时图床密钥");
        return;
    }

    if (!checkDuplicateSubmission()) return;

    QString modelVariant = modelVariantCombo->currentData().toString();
    QString modelVariantText = modelVariantCombo->currentText();
    QString server = serverCombo->currentData().toString();
    QString aspectRatio = resolutionCombo->currentData().toString();
    QString size = sizeCombo->currentData().toString();
    int keyId = apiKeyCombo->currentData().toInt();

    ApiKey apiKeyData = DBManager::instance()->getApiKey(keyId);
    if (apiKeyData.apiKey.isEmpty()) {
        QMessageBox::warning(this, "错误", "无法获取 API 密钥");
        return;
    }

    int grokDuration = 6;
    if (modelVariant.contains("15s")) grokDuration = 15;
    else if (modelVariant.contains("10s")) grokDuration = 10;

    QString tempTaskId = QString("temp_%1_%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(QRandomGenerator::global()->bounded(10000));

    VideoTask task;
    task.taskId = tempTaskId;
    task.taskType = "video_single";
    task.prompt = prompt;
    task.modelVariant = modelVariantText;
    task.modelName = "Grok3视频";
    task.apiKeyName = apiKeyCombo->currentText();
    task.serverUrl = server;
    task.status = "submitting";
    task.progress = 0;
    task.aspectRatio = aspectRatio;
    task.size = size;

    QJsonArray imageArray;
    for (const QString& path : validImagePaths) {
        imageArray.append(path);
    }
    task.imagePaths = QString::fromUtf8(QJsonDocument(imageArray).toJson(QJsonDocument::Compact));

    int dbId = DBManager::instance()->insertVideoTask(task);
    if (dbId < 0) {
        QMessageBox::critical(this, "错误", "无法保存任务记录");
        return;
    }

    currentTaskId = tempTaskId;
    previewLabel->setText("⏳ 正在提交视频生成任务...");
    setSubmitting(true);

    api->createVideo(
        apiKeyData.apiKey,
        server,
        "Grok3视频",
        modelVariant,
        prompt,
        validImagePaths,
        size,
        QString::number(grokDuration),
        false,
        aspectRatio,
        activeImgbbKey.apiKey
    );
}

void GrokGenPage::onVideoCreated(const QString &taskId, const QString &status)
{
    Q_UNUSED(status);
    setSubmitting(false);
    if (!currentTaskId.isEmpty() && currentTaskId != taskId) {
        DBManager::instance()->updateTaskId(currentTaskId, taskId);
    }
    currentTaskId = taskId;
    DBManager::instance()->updateTaskStatus(taskId, "pending", 0, "");

    int keyId = apiKeyCombo->currentData().toInt();
    ApiKey apiKeyData = DBManager::instance()->getApiKey(keyId);
    QString server = serverCombo->currentData().toString();

    TaskPollManager *pollMgr = TaskPollManager::getInstance();
    pollMgr->startPolling(taskId, "video_single", apiKeyData.apiKey, server, "Grok3视频");

    lastSubmittedParamsHash = calculateParamsHash();
    parametersModified = false;
    saveSettings();

    QMessageBox::information(this, "任务已提交",
        "视频生成任务已提交！\n\n请前往【生成历史记录】标签页查看任务状态。\n系统将自动轮询任务状态并下载完成的视频。");
    previewLabel->setText("💡 生成结果将在【生成历史记录】");
}

void GrokGenPage::onTaskStatusUpdated(const QString &taskId, const QString &status, const QString &videoUrl, int progress)
{
    Q_UNUSED(taskId);
    Q_UNUSED(status);
    Q_UNUSED(videoUrl);
    Q_UNUSED(progress);
}

void GrokGenPage::onApiError(const QString &error)
{
    setSubmitting(false);
    if (!currentTaskId.isEmpty()) {
        DBManager::instance()->updateTaskStatus(currentTaskId, "failed", 0, "");
        DBManager::instance()->updateTaskErrorMessage(currentTaskId, error);
    }
    QMessageBox::critical(this, "错误", QString("API 调用失败:\n%1").arg(error));
    previewLabel->setText("💡 生成结果将在【生成历史记录】");
}

void GrokGenPage::queueSaveSettings()
{
    if (suppressAutoSave) return;
    if (pendingSaveSettings) return;
    pendingSaveSettings = true;
    QTimer::singleShot(0, this, [this]() {
        pendingSaveSettings = false;
        saveSettings();
    });
}

QString GrokGenPage::buildSettingsSnapshot() const
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
        sizeCombo->currentData().toString(),
        uploadedImagePaths.join("\n")
    };

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(snapshotParts.join("\x1F").toUtf8());
    return QString(hash.result().toHex());
}

void GrokGenPage::saveSettings()
{
    const QString snapshot = buildSettingsSnapshot();
    if (!lastSavedSettingsSnapshot.isEmpty() && snapshot == lastSavedSettingsSnapshot) return;

    QSettings settings("ChickenAI", "VideoGen");
    settings.beginGroup("VideoSingleTab");

    settings.setValue("grok_prompt", promptInput->toPlainText());
    settings.setValue("grok_modelVariant", modelVariantCombo->currentData().toString());
    settings.setValue("grok_apiKey", apiKeyCombo->currentIndex());
    settings.setValue("grok_server", serverCombo->currentIndex());
    settings.setValue("grok_aspectRatio", resolutionCombo->currentData().toString());
    settings.setValue("grok_size", sizeCombo->currentData().toString());
    settings.setValue("grok_imagePaths", uploadedImagePaths);
    settings.setValue("grok_lastSubmittedHash", lastSubmittedParamsHash);

    settings.endGroup();
    settings.sync();
    lastSavedSettingsSnapshot = snapshot;
}

void GrokGenPage::loadSettings()
{
    QSettings settings("ChickenAI", "VideoGen");
    settings.beginGroup("VideoSingleTab");

    promptInput->blockSignals(true);
    modelVariantCombo->blockSignals(true);
    apiKeyCombo->blockSignals(true);
    serverCombo->blockSignals(true);
    resolutionCombo->blockSignals(true);
    sizeCombo->blockSignals(true);

    promptInput->setPlainText(settings.value("grok_prompt", "").toString());

    QString savedVariant = settings.value("grok_modelVariant", "").toString();
    for (int i = 0; i < modelVariantCombo->count(); ++i) {
        if (modelVariantCombo->itemData(i).toString() == savedVariant) {
            modelVariantCombo->setCurrentIndex(i);
            break;
        }
    }

    int apiKeyIndex = settings.value("grok_apiKey", 0).toInt();
    if (apiKeyIndex >= 0 && apiKeyIndex < apiKeyCombo->count()) {
        apiKeyCombo->setCurrentIndex(apiKeyIndex);
    } else {
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

    int serverIndex = settings.value("grok_server", 0).toInt();
    if (serverIndex >= 0 && serverIndex < serverCombo->count()) {
        serverCombo->setCurrentIndex(serverIndex);
    }

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

    QStringList imagePaths = settings.value("grok_imagePaths").toStringList();
    uploadedImagePaths.clear();
    for (const QString &path : imagePaths) {
        if (path.isEmpty() || !QFile::exists(path) || uploadedImagePaths.contains(path)) {
            continue;
        }
        uploadedImagePaths.append(path);
        if (uploadedImagePaths.size() >= maxReferenceImages()) {
            break;
        }
    }
    updateThumbnailGrid();

    lastSubmittedParamsHash = settings.value("grok_lastSubmittedHash", "").toString();

    settings.endGroup();
    lastSavedSettingsSnapshot = buildSettingsSnapshot();

    promptInput->blockSignals(false);
    modelVariantCombo->blockSignals(false);
    apiKeyCombo->blockSignals(false);
    serverCombo->blockSignals(false);
    resolutionCombo->blockSignals(false);
    sizeCombo->blockSignals(false);

    parametersModified = false;
}

QString GrokGenPage::calculateParamsHash() const
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(promptInput->toPlainText().toUtf8());
    hash.addData(QString::number(modelVariantCombo->currentIndex()).toUtf8());
    hash.addData(QString::number(resolutionCombo->currentIndex()).toUtf8());
    hash.addData(QString::number(sizeCombo->currentIndex()).toUtf8());
    for (const QString &path : uploadedImagePaths) {
        hash.addData(path.toUtf8());
    }
    return QString(hash.result().toHex());
}

bool GrokGenPage::checkDuplicateSubmission()
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

void GrokGenPage::onAnyParameterChanged()
{
    parametersModified = true;
    suppressDuplicateWarning = false;
}

void GrokGenPage::loadFromTask(const VideoTask& task)
{
    blockSignals(true);
    promptInput->setPlainText(task.prompt);

    for (int i = 0; i < modelVariantCombo->count(); ++i) {
        if (modelVariantCombo->itemText(i) == task.modelVariant) {
            modelVariantCombo->setCurrentIndex(i);
            break;
        }
    }

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

    if (!task.aspectRatio.isEmpty()) {
        for (int i = 0; i < resolutionCombo->count(); ++i) {
            if (resolutionCombo->itemData(i).toString() == task.aspectRatio) {
                resolutionCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    if (!task.size.isEmpty()) {
        for (int i = 0; i < sizeCombo->count(); ++i) {
            if (sizeCombo->itemData(i).toString() == task.size) {
                sizeCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    uploadedImagePaths.clear();
    if (!task.imagePaths.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(task.imagePaths.toUtf8());
        if (doc.isArray()) {
            QJsonArray imageArray = doc.array();
            for (int i = 0; i < imageArray.size(); ++i) {
                QString path = imageArray[i].toString();
                if (path.isEmpty() || !QFile::exists(path) || uploadedImagePaths.contains(path)) {
                    continue;
                }
                uploadedImagePaths.append(path);
                if (uploadedImagePaths.size() >= maxReferenceImages()) {
                    break;
                }
            }
        }
    }
    updateThumbnailGrid();

    parametersModified = false;
    lastSubmittedParamsHash.clear();
    blockSignals(false);
}
