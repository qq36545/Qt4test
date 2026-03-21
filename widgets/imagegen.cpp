#include "imagegen.h"
#include "../database/dbmanager.h"
#include "../network/imageapi.h"

#include <QMessageBox>
#include <QResizeEvent>
#include <QTabBar>
#include <QHeaderView>
#include <QScrollArea>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDesktopServices>
#include <QUrl>
#include <QPixmap>
#include <QCoreApplication>
#include <QProcess>

namespace {

QString extensionFromMime(const QString &mimeType)
{
    const QString mime = mimeType.toLower();
    if (mime.contains("png")) return "png";
    if (mime.contains("jpeg") || mime.contains("jpg")) return "jpg";
    if (mime.contains("webp")) return "webp";
    return "png";
}

QString modeLabel(const QString &generationMode)
{
    return generationMode == "image_to_image" ? "图生图" : "文生图";
}

const char *kGenerateButtonText = "生成图片";
const char *kGeneratingButtonText = "生成中...";
const char *kResetButtonText = "一键重置";
const char *kRefreshButtonText = "刷新";
const char *kOpenFolderButtonText = "打开所在目录";

QString openFolderPathByFile(const QString &filePath)
{
    QFileInfo info(filePath);
    if (!info.exists()) return QString();
    return info.absolutePath();
}

void openFolderInSystem(const QString &folderPath)
{
    if (folderPath.isEmpty() || !QDir(folderPath).exists()) {
        return;
    }
#ifdef Q_OS_MAC
    QProcess::execute("open", QStringList() << folderPath);
#elif defined(Q_OS_WIN)
    QProcess::execute("explorer", QStringList() << QDir::toNativeSeparators(folderPath));
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
#endif
}

} // namespace

// ImageGenWidget 实现
ImageGenWidget::ImageGenWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void ImageGenWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(40, 40, 40, 40);
    mainLayout->setSpacing(20);

    QLabel *titleLabel = new QLabel("AI 图片生成");
    titleLabel->setStyleSheet("font-size: 28px; font-weight: bold; color: #F8FAFC;");
    mainLayout->addWidget(titleLabel);

    tabWidget = new QTabWidget();
    singleTab = new ImageSingleTab();
    batchTab = new ImageBatchTab();
    historyTab = new ImageHistoryTab();

    tabWidget->addTab(singleTab, "AI图片生成-单个");
    tabWidget->addTab(batchTab, "AI图片生成-批量");
    tabWidget->addTab(historyTab, "生成历史记录");

    connect(singleTab, &ImageSingleTab::imageGeneratedSuccessfully,
            historyTab, &ImageHistoryTab::refreshHistory);

    mainLayout->addWidget(tabWidget);

    updateTabWidths();
}

void ImageGenWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateTabWidths();
}

void ImageGenWidget::updateTabWidths()
{
    if (!tabWidget) return;

    int windowWidth = width();
    int totalTabWidth = windowWidth * 0.55;

    int tabCount = tabWidget->count();
    if (tabCount == 0) return;

    int tabWidth = totalTabWidth / tabCount;

    QString tabStyle = QString(
        "QTabBar::tab {"
        "    width: %1px;"
        "    text-align: center;"
        "}"
    ).arg(tabWidth);

    tabWidget->tabBar()->setStyleSheet(tabStyle);
}

// GeminiImagePage 实现
GeminiImagePage::GeminiImagePage(QWidget *parent)
    : QWidget(parent),
      imageApi(new ImageAPI(this)),
      currentHistoryId(-1)
{
    setupUI();
    loadApiKeys();

    connect(imageApi, &ImageAPI::imageGenerated,
            this, &GeminiImagePage::onApiImageGenerated);
    connect(imageApi, &ImageAPI::errorOccurred,
            this, &GeminiImagePage::onApiError);
}

void GeminiImagePage::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(12);

    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget *contentWidget = new QWidget();
    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(12);

    QHBoxLayout *variantLayout = new QHBoxLayout();
    QLabel *variantLabel = new QLabel("模型变体:");
    variantCombo = new QComboBox();
    variantCombo->addItem("香蕉2 + gemini-3.1-flash-image-preview", "gemini-3.1-flash-image-preview");
    variantCombo->addItem("香蕉Pro + gemini-3-pro-image-preview", "gemini-3-pro-image-preview");
    variantCombo->addItem("香蕉1 + gemini-2.5-flash-image", "gemini-2.5-flash-image");
    connect(variantCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GeminiImagePage::onVariantChanged);
    variantLayout->addWidget(variantLabel);
    variantLayout->addWidget(variantCombo, 1);
    contentLayout->addLayout(variantLayout);

    QHBoxLayout *keyLayout = new QHBoxLayout();
    QLabel *keyLabel = new QLabel("API 密钥:");
    apiKeyCombo = new QComboBox();
    addKeyButton = new QPushButton("添加密钥");
    connect(addKeyButton, &QPushButton::clicked, []() {
        QMessageBox::information(nullptr, "提示", "请前往配置页面添加密钥");
    });
    keyLayout->addWidget(keyLabel);
    keyLayout->addWidget(apiKeyCombo, 1);
    keyLayout->addWidget(addKeyButton);
    contentLayout->addLayout(keyLayout);

    QHBoxLayout *serverLayout = new QHBoxLayout();
    QLabel *serverLabel = new QLabel("请求服务器:");
    serverCombo = new QComboBox();
    serverCombo->addItem("【主站】https://ai.kegeai.top", "https://ai.kegeai.top");
    serverCombo->addItem("【备用】https://api.kuai.host", "https://api.kuai.host");
    serverCombo->setCurrentIndex(0);
    serverLayout->addWidget(serverLabel);
    serverLayout->addWidget(serverCombo, 1);
    contentLayout->addLayout(serverLayout);

    QHBoxLayout *paramsLayout = new QHBoxLayout();

    imageSizeWidget = new QWidget();
    QVBoxLayout *imageSizeLayout = new QVBoxLayout(imageSizeWidget);
    imageSizeLayout->setContentsMargins(0, 0, 0, 0);
    imageSizeLabel = new QLabel("清晰度 imageSize");
    imageSizeCombo = new QComboBox();
    imageSizeCombo->addItem("1K", "1K");
    imageSizeCombo->addItem("2K", "2K");
    imageSizeCombo->addItem("4K", "4K");
    imageSizeLayout->addWidget(imageSizeLabel);
    imageSizeLayout->addWidget(imageSizeCombo);

    QVBoxLayout *aspectLayout = new QVBoxLayout();
    QLabel *aspectLabel = new QLabel("宽高比 aspectRatio");
    aspectRatioCombo = new QComboBox();
    aspectLayout->addWidget(aspectLabel);
    aspectLayout->addWidget(aspectRatioCombo);

    paramsLayout->addWidget(imageSizeWidget);
    paramsLayout->addLayout(aspectLayout);
    paramsLayout->addStretch();
    contentLayout->addLayout(paramsLayout);

    QHBoxLayout *promptHeader = new QHBoxLayout();
    QLabel *promptLabel = new QLabel("提示词:");
    QPushButton *clearPromptButton = new QPushButton("清空文本");
    connect(clearPromptButton, &QPushButton::clicked, this, &GeminiImagePage::clearPrompt);
    promptHeader->addWidget(promptLabel);
    promptHeader->addStretch();
    promptHeader->addWidget(clearPromptButton);
    contentLayout->addLayout(promptHeader);

    promptInput = new QTextEdit();
    promptInput->setPlaceholderText("输入图片生成提示词...");
    promptInput->setMinimumHeight(180);
    contentLayout->addWidget(promptInput);

    QLabel *referenceLabel = new QLabel("参考图（可选）:");
    contentLayout->addWidget(referenceLabel);

    QHBoxLayout *referenceLayout = new QHBoxLayout();
    referenceImagePreviewLabel = new QLabel("未选择图片");
    referenceImagePreviewLabel->setAlignment(Qt::AlignCenter);
    referenceImagePreviewLabel->setMinimumHeight(120);
    uploadReferenceButton = new QPushButton("上传参考图");
    clearReferenceButton = new QPushButton("清理参考图");
    connect(uploadReferenceButton, &QPushButton::clicked, this, &GeminiImagePage::uploadReferenceImage);
    connect(clearReferenceButton, &QPushButton::clicked, this, &GeminiImagePage::clearReferenceImage);
    referenceLayout->addWidget(referenceImagePreviewLabel, 1);
    referenceLayout->addWidget(uploadReferenceButton);
    referenceLayout->addWidget(clearReferenceButton);
    contentLayout->addLayout(referenceLayout);

    QLabel *resultLabel = new QLabel("当前结果预览:");
    contentLayout->addWidget(resultLabel);

    resultPreviewLabel = new QLabel("生成完成后在此预览");
    resultPreviewLabel->setAlignment(Qt::AlignCenter);
    resultPreviewLabel->setMinimumHeight(240);
    contentLayout->addWidget(resultPreviewLabel);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    generateButton = new QPushButton(kGenerateButtonText);
    resetButton = new QPushButton(kResetButtonText);
    connect(generateButton, &QPushButton::clicked, this, &GeminiImagePage::generateImage);
    connect(resetButton, &QPushButton::clicked, this, &GeminiImagePage::resetForm);
    buttonLayout->addWidget(generateButton);
    buttonLayout->addWidget(resetButton);
    contentLayout->addLayout(buttonLayout);

    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);

    onVariantChanged(0);
}

void GeminiImagePage::loadApiKeys()
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

void GeminiImagePage::refreshApiKeys()
{
    loadApiKeys();
}

void GeminiImagePage::onVariantChanged(int)
{
    updateImageSizeVisibility();
    rebuildAspectRatioOptions();
}

void GeminiImagePage::updateImageSizeVisibility()
{
    const QString model = currentModelValue();
    const bool showImageSize = (model == "gemini-3.1-flash-image-preview" || model == "gemini-3-pro-image-preview");
    imageSizeWidget->setVisible(showImageSize);
}

void GeminiImagePage::rebuildAspectRatioOptions()
{
    const QString model = currentModelValue();
    aspectRatioCombo->blockSignals(true);
    aspectRatioCombo->clear();

    if (model == "gemini-2.5-flash-image") {
        aspectRatioCombo->addItem("1:1", "1:1");
        aspectRatioCombo->addItem("16:9", "16:9");
        aspectRatioCombo->addItem("9:16", "9:16");
        aspectRatioCombo->addItem("4:3", "4:3");
        aspectRatioCombo->addItem("3:4", "3:4");
    } else {
        aspectRatioCombo->addItem("1:1", "1:1");
        aspectRatioCombo->addItem("16:9", "16:9");
        aspectRatioCombo->addItem("9:16", "9:16");
        aspectRatioCombo->addItem("4:3", "4:3");
        aspectRatioCombo->addItem("3:4", "3:4");
        aspectRatioCombo->addItem("2:3", "2:3");
        aspectRatioCombo->addItem("3:2", "3:2");
    }

    aspectRatioCombo->setCurrentIndex(0);
    aspectRatioCombo->blockSignals(false);
}

void GeminiImagePage::uploadReferenceImage()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "选择参考图",
        QString(),
        "图片文件 (*.png *.jpg *.jpeg *.webp)"
    );

    if (filePath.isEmpty()) return;

    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        QMessageBox::warning(this, "错误", "参考图不存在或不可读");
        return;
    }

    referenceImagePath = filePath;
    updateReferenceImagePreview();
}

void GeminiImagePage::clearReferenceImage()
{
    referenceImagePath.clear();
    updateReferenceImagePreview();
}

void GeminiImagePage::updateReferenceImagePreview()
{
    if (referenceImagePath.isEmpty()) {
        referenceImagePreviewLabel->setPixmap(QPixmap());
        referenceImagePreviewLabel->setText("未选择图片");
        return;
    }

    QPixmap pixmap(referenceImagePath);
    if (pixmap.isNull()) {
        referenceImagePreviewLabel->setPixmap(QPixmap());
        referenceImagePreviewLabel->setText("参考图预览失败");
        return;
    }

    referenceImagePreviewLabel->setText("");
    referenceImagePreviewLabel->setPixmap(pixmap.scaled(260, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void GeminiImagePage::clearPrompt()
{
    promptInput->clear();
}

void GeminiImagePage::resetForm()
{
    variantCombo->setCurrentIndex(0);
    serverCombo->setCurrentIndex(0);
    imageSizeCombo->setCurrentIndex(0);
    promptInput->clear();
    clearReferenceImage();
    resultPreviewLabel->setPixmap(QPixmap());
    resultPreviewLabel->setText("生成完成后在此预览");
}

QString GeminiImagePage::currentModelValue() const
{
    return variantCombo->currentData().toString();
}

QString GeminiImagePage::currentVariantLabel() const
{
    return variantCombo->currentText();
}

void GeminiImagePage::generateImage()
{
    const QString prompt = promptInput->toPlainText().trimmed();
    if (prompt.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入图片生成提示词");
        return;
    }

    if (!apiKeyCombo->isEnabled() || apiKeyCombo->count() == 0) {
        QMessageBox::warning(this, "提示", "请先添加 API 密钥");
        return;
    }

    if (!referenceImagePath.isEmpty() && !QFile::exists(referenceImagePath)) {
        QMessageBox::warning(this, "提示", "参考图不存在或不可读");
        return;
    }

    int keyId = apiKeyCombo->currentData().toInt();
    ApiKey keyData = DBManager::instance()->getApiKey(keyId);
    if (keyData.apiKey.isEmpty()) {
        QMessageBox::warning(this, "提示", "无法获取 API 密钥");
        return;
    }

    const QString model = currentModelValue();
    const QString aspectRatio = aspectRatioCombo->currentData().toString();
    const QString imageSize = imageSizeWidget->isVisible() ? imageSizeCombo->currentData().toString() : QString();
    const QString serverUrl = serverCombo->currentData().toString();
    const QString generationMode = referenceImagePath.isEmpty() ? "text_to_image" : "image_to_image";

    QJsonObject params;
    params["model"] = model;
    params["variantLabel"] = currentVariantLabel();
    params["aspectRatio"] = aspectRatio;
    params["imageSize"] = imageSize;
    params["serverUrl"] = serverUrl;
    params["generationMode"] = generationMode;

    GenerationHistory history;
    history.date = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    history.type = "single";
    history.modelType = "image";
    history.modelName = "Gemini（香蕉）";
    history.prompt = prompt;
    history.imagePath = referenceImagePath;
    history.parameters = QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact));
    history.status = "generating";
    history.resultPath.clear();

    currentHistoryId = DBManager::instance()->addGenerationHistory(history);

    generateButton->setEnabled(false);
    generateButton->setText(kGeneratingButtonText);
    resultPreviewLabel->setPixmap(QPixmap());
    resultPreviewLabel->setText("请求已发送，正在生成...");

    imageApi->generateImage(
        keyData.apiKey,
        serverUrl,
        model,
        prompt,
        aspectRatio,
        imageSize,
        referenceImagePath
    );
}

QString GeminiImagePage::saveGeneratedImage(const QByteArray &imageBytes, const QString &mimeType, QString &error)
{
    const QString model = currentModelValue();
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString dirPath = appDir + "/outputs/gemini-img/" + model;

    QDir dir;
    if (!dir.mkpath(dirPath)) {
        error = "创建输出目录失败";
        return QString();
    }

    const QString ext = extensionFromMime(mimeType);
    const QString fileName = QString("%1-%2.%3")
        .arg(model)
        .arg(QDateTime::currentDateTime().toString("yyyyMMddHHmmss"))
        .arg(ext);
    const QString filePath = dirPath + "/" + fileName;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        error = "保存图片失败";
        return QString();
    }

    if (file.write(imageBytes) <= 0) {
        file.close();
        error = "写入图片失败";
        return QString();
    }

    file.close();
    return filePath;
}

void GeminiImagePage::onApiImageGenerated(const QByteArray &imageBytes, const QString &mimeType)
{
    generateButton->setEnabled(true);
    generateButton->setText(kGenerateButtonText);

    if (imageBytes.isEmpty()) {
        onApiError("响应中未包含可用图片");
        return;
    }

    QString saveError;
    const QString savedPath = saveGeneratedImage(imageBytes, mimeType, saveError);
    if (savedPath.isEmpty()) {
        if (currentHistoryId > 0) {
            DBManager::instance()->updateGenerationHistory(currentHistoryId, "failed", "");
        }
        resultPreviewLabel->setPixmap(QPixmap());
        resultPreviewLabel->setText("保存失败");
        QMessageBox::critical(this, "错误", saveError);
        return;
    }

    if (currentHistoryId > 0) {
        DBManager::instance()->updateGenerationHistory(currentHistoryId, "success", savedPath);
    }

    QPixmap pixmap;
    if (!pixmap.loadFromData(imageBytes)) {
        resultPreviewLabel->setPixmap(QPixmap());
        resultPreviewLabel->setText("已生成并保存，但预览失败\n" + savedPath);
    } else {
        resultPreviewLabel->setText("");
        resultPreviewLabel->setPixmap(pixmap.scaled(720, 360, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    emit imageGeneratedSuccessfully();
    QMessageBox::information(this, "成功", "图片生成成功并已保存到本地");
}

void GeminiImagePage::onApiError(const QString &error)
{
    generateButton->setEnabled(true);
    generateButton->setText(kGenerateButtonText);

    if (currentHistoryId > 0) {
        DBManager::instance()->updateGenerationHistory(currentHistoryId, "failed", "");
    }

    resultPreviewLabel->setPixmap(QPixmap());
    resultPreviewLabel->setText("生成失败");
    QMessageBox::critical(this, "生成失败", error);
}

// ImageSingleTab 实现（路由容器）
ImageSingleTab::ImageSingleTab(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void ImageSingleTab::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    QHBoxLayout *modelLayout = new QHBoxLayout();
    modelLayout->setContentsMargins(20, 15, 20, 5);

    QLabel *modelLabel = new QLabel("图片模型:");
    modelCombo = new QComboBox();
    modelCombo->addItem("Gemini（香蕉）");
    modelCombo->setCurrentIndex(0);
    connect(modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImageSingleTab::onModelChanged);

    modelLayout->addWidget(modelLabel);
    modelLayout->addWidget(modelCombo, 1);
    mainLayout->addLayout(modelLayout);

    stack = new QStackedWidget();
    geminiPage = new GeminiImagePage();
    stack->addWidget(geminiPage);

    connect(geminiPage, &GeminiImagePage::imageGeneratedSuccessfully,
            this, &ImageSingleTab::imageGeneratedSuccessfully);

    mainLayout->addWidget(stack);
}

void ImageSingleTab::onModelChanged(int index)
{
    stack->setCurrentIndex(index);
    if (index == 0) {
        geminiPage->refreshApiKeys();
    }
}

void ImageSingleTab::refreshApiKeys()
{
    geminiPage->refreshApiKeys();
}

// ImageBatchTab 实现（占位）
ImageBatchTab::ImageBatchTab(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void ImageBatchTab::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QLabel *label = new QLabel("批量图片生成功能待实现");
    mainLayout->addWidget(label);
}

void ImageBatchTab::loadApiKeys() {}
void ImageBatchTab::generateBatch() {}
void ImageBatchTab::importCSV() {}
void ImageBatchTab::deleteAll() {}

// ImageHistoryTab 实现
ImageHistoryTab::ImageHistoryTab(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    loadHistory();
}

void ImageHistoryTab::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(12);

    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    refreshButton = new QPushButton(kRefreshButtonText);
    openFolderButton = new QPushButton(kOpenFolderButtonText);

    connect(refreshButton, &QPushButton::clicked, this, &ImageHistoryTab::refreshHistory);
    connect(openFolderButton, &QPushButton::clicked, this, &ImageHistoryTab::openSelectedFolder);

    toolbarLayout->addStretch();
    toolbarLayout->addWidget(refreshButton);
    toolbarLayout->addWidget(openFolderButton);
    mainLayout->addLayout(toolbarLayout);

    historyTable = new QTableWidget();
    historyTable->setColumnCount(7);
    historyTable->setHorizontalHeaderLabels({"序号", "时间", "模型", "模式", "提示词摘要", "状态", "结果路径"});
    historyTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    historyTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    historyTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    historyTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    historyTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    historyTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    historyTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    historyTable->setColumnWidth(0, 80);
    historyTable->setColumnWidth(1, 160);
    historyTable->setColumnWidth(2, 180);
    historyTable->setColumnWidth(3, 90);
    historyTable->setColumnWidth(5, 100);
    historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    historyTable->verticalHeader()->setVisible(false);

    connect(historyTable, &QTableWidget::cellDoubleClicked,
            this, &ImageHistoryTab::onRowDoubleClicked);

    mainLayout->addWidget(historyTable);
}

void ImageHistoryTab::loadHistory()
{
    historyTable->setRowCount(0);
    QList<GenerationHistory> histories = DBManager::instance()->getGenerationHistoryByModelAndType("image", "single");

    int row = 0;
    for (const GenerationHistory &history : histories) {

        QString generationMode = "text_to_image";
        QJsonParseError parseError;
        QJsonDocument paramsDoc = QJsonDocument::fromJson(history.parameters.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError && paramsDoc.isObject()) {
            generationMode = paramsDoc.object().value("generationMode").toString("text_to_image");
        }

        historyTable->insertRow(row);
        historyTable->setItem(row, 0, new QTableWidgetItem(QString::number(history.id)));
        historyTable->setItem(row, 1, new QTableWidgetItem(history.date));
        historyTable->setItem(row, 2, new QTableWidgetItem(history.modelName));
        historyTable->setItem(row, 3, new QTableWidgetItem(modeLabel(generationMode)));

        QString promptSummary = history.prompt;
        if (promptSummary.length() > 60) {
            promptSummary = promptSummary.left(60) + "...";
        }
        QTableWidgetItem *promptItem = new QTableWidgetItem(promptSummary);
        promptItem->setToolTip(history.prompt);
        historyTable->setItem(row, 4, promptItem);

        historyTable->setItem(row, 5, new QTableWidgetItem(history.status));

        QTableWidgetItem *pathItem = new QTableWidgetItem(history.resultPath);
        pathItem->setToolTip(history.resultPath);
        historyTable->setItem(row, 6, pathItem);

        historyTable->item(row, 0)->setData(Qt::UserRole, history.id);
        row++;
    }
}

void ImageHistoryTab::refreshHistory()
{
    loadHistory();
}

void ImageHistoryTab::onRowDoubleClicked(int row, int)
{
    if (row < 0 || row >= historyTable->rowCount()) {
        return;
    }

    const int historyId = historyTable->item(row, 0)->data(Qt::UserRole).toInt();
    GenerationHistory history = DBManager::instance()->getGenerationHistory(historyId);

    if (history.resultPath.isEmpty()) {
        QMessageBox::warning(this, "提示", "该记录暂无生成图片路径");
        return;
    }

    QFileInfo fileInfo(history.resultPath);
    if (fileInfo.exists()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(history.resultPath));
        return;
    }

    const QString folderPath = openFolderPathByFile(history.resultPath);
    if (!folderPath.isEmpty()) {
        openFolderInSystem(folderPath);
        return;
    }

    QMessageBox::warning(this, "提示", "图片文件不存在，且无法打开所在目录");
}

void ImageHistoryTab::openSelectedFolder()
{
    const int row = historyTable->currentRow();
    if (row < 0 || row >= historyTable->rowCount()) {
        QMessageBox::information(this, "提示", "请先选择一条记录");
        return;
    }

    const int historyId = historyTable->item(row, 0)->data(Qt::UserRole).toInt();
    GenerationHistory history = DBManager::instance()->getGenerationHistory(historyId);

    if (history.resultPath.isEmpty()) {
        QMessageBox::warning(this, "提示", "该记录暂无结果路径");
        return;
    }

    QString folderPath = openFolderPathByFile(history.resultPath);
    if (folderPath.isEmpty()) {
        QMessageBox::warning(this, "提示", "目录不存在");
        return;
    }

    openFolderInSystem(folderPath);
}
