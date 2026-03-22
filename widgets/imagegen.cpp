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
#include <QMouseEvent>
#include <QHideEvent>
#include <QDebug>

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
    // 连接 API 密钥选择变化信号，用于同步历史页密钥（如需扩展）
    connect(singleTab, &ImageSingleTab::apiKeySelectionChanged,
            historyTab, &ImageHistoryTab::refreshApiKeys);

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
    // 初始化草稿保存定时器
    m_comboSaveTimer = new QTimer(this);
    m_comboSaveTimer->setSingleShot(true);
    m_comboSaveTimer->setInterval(1000);  // 1秒延迟

    m_promptSaveTimer = new QTimer(this);
    m_promptSaveTimer->setSingleShot(true);
    m_promptSaveTimer->setInterval(3000);  // 3秒延迟

    setupUI();
    loadApiKeys();
    restoreLastModelVariant();

    connect(imageApi, &ImageAPI::imageGenerated,
            this, &GeminiImagePage::onApiImageGenerated);
    connect(imageApi, &ImageAPI::errorOccurred,
            this, &GeminiImagePage::onApiError);

    // 连接草稿保存定时器
    connect(m_comboSaveTimer, &QTimer::timeout, this, &GeminiImagePage::saveDraft);
    connect(m_promptSaveTimer, &QTimer::timeout, this, &GeminiImagePage::saveDraft);
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
    // 连接 API 密钥选择变化信号
    connect(apiKeyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        int keyId = apiKeyCombo->currentData().toInt();
        if (keyId > 0) {
            const ApiKey key = DBManager::instance()->getApiKey(keyId);
            emit apiKeySelectionChanged(key.apiKey);
        }
        // API密钥变化时触发延迟保存
        if (!m_initializing) m_comboSaveTimer->start();
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
    // 服务器变化时触发延迟保存
    connect(serverCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { if (!m_initializing) m_comboSaveTimer->start(); });
    serverLayout->addWidget(serverLabel);
    serverLayout->addWidget(serverCombo, 1);
    contentLayout->addLayout(serverLayout);

    QHBoxLayout *paramsLayout = new QHBoxLayout();

    imageSizeWidget = new QWidget();
    QVBoxLayout *imageSizeLayout = new QVBoxLayout(imageSizeWidget);
    imageSizeLayout->setContentsMargins(0, 0, 0, 0);
    imageSizeLabel = new QLabel("清晰度");
    imageSizeCombo = new QComboBox();
    imageSizeCombo->addItem("0.5k", "512");
    imageSizeCombo->addItem("1K", "1K");
    imageSizeCombo->addItem("2K", "2K");
    imageSizeCombo->addItem("4K", "4K");
    // 清晰度变化时立即保存（blockSignals 防止 rebuild 时误触发）
    imageSizeCombo->blockSignals(true);
    connect(imageSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        // combo 为空时跳过（rebuildImageSizeOptions 中 clear+addItem 会触发多次信号）
        if (!m_initializing && imageSizeCombo->count() > 0) {
            savePreferences(variantCombo->currentIndex());
        }
    });
    imageSizeCombo->blockSignals(false);
    imageSizeLayout->addWidget(imageSizeLabel);
    imageSizeLayout->addWidget(imageSizeCombo);

    QVBoxLayout *aspectLayout = new QVBoxLayout();
    QLabel *aspectLabel = new QLabel("宽高比");
    aspectRatioCombo = new QComboBox();
    // 宽高比变化时触发延迟保存
    connect(aspectRatioCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (!m_initializing) m_comboSaveTimer->start();
    });
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
    // 提示词变化时触发3秒延迟保存
    connect(promptInput, &QTextEdit::textChanged, this, [this]() {
        if (!m_initializing) {
            m_promptSaveTimer->start();
        }
    });
    // 为 promptInput 安装事件过滤器，处理焦点离开
    promptInput->installEventFilter(this);
    contentLayout->addWidget(promptInput);

    QLabel *referenceLabel = new QLabel("参考图（可选）:");
    contentLayout->addWidget(referenceLabel);

    referenceHintLabel = new QLabel();
    referenceHintLabel->setStyleSheet("color: #94a3b8; font-size: 12px;");
    referenceHintLabel->setVisible(false);
    referenceHintLabel->setWordWrap(true);
    contentLayout->addWidget(referenceHintLabel);

    referenceCountLabel = new QLabel("已选 0/10 张");
    referenceCountLabel->setStyleSheet("color: #94a3b8; font-size: 12px;");
    referenceCountLabel->setVisible(false);
    contentLayout->addWidget(referenceCountLabel);

    thumbnailContainer = new QWidget();
    thumbnailContainer->setMinimumHeight(140);
    thumbnailContainer->setStyleSheet("background-color: #1e293b; border-radius: 8px; border: 1px solid #334155;");
    thumbnailLayout = new QGridLayout(thumbnailContainer);
    thumbnailLayout->setSpacing(8);
    thumbnailLayout->setContentsMargins(8, 8, 8, 8);
    for (int i = 0; i < 10; ++i) {
        thumbnailLayout->setRowStretch(i / 5, 1);
        thumbnailLayout->setColumnStretch(i % 5, 1);
    }
    contentLayout->addWidget(thumbnailContainer);
    thumbnailContainer->installEventFilter(this);

    QHBoxLayout *referenceLayout = new QHBoxLayout();
    uploadReferenceButton = new QPushButton("上传参考图");
    clearReferenceButton = new QPushButton("清理参考图");
    connect(uploadReferenceButton, &QPushButton::clicked, this, &GeminiImagePage::uploadReferenceImage);
    connect(clearReferenceButton, &QPushButton::clicked, this, &GeminiImagePage::clearReferenceImage);
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
        // 密钥为空时不显示警告，静默处理
        // 让 generateImage() 统一处理提示
        return;
    }

    apiKeyCombo->setEnabled(true);
    addKeyButton->setVisible(false);
    for (const ApiKey& key : keys) {
        apiKeyCombo->addItem(key.name, key.id);
    }

    // 自动选择第一个
    apiKeyCombo->setCurrentIndex(0);
    emit apiKeySelectionChanged(keys.first().apiKey);
}

void GeminiImagePage::refreshApiKeys()
{
    loadApiKeys();
}

void GeminiImagePage::onVariantChanged(int newIndex)
{
    // 初始化阶段跳过保存
    if (!m_initializing) {
        // 保存切换前模型的参数到草稿
        saveDraft();
        // 保存当前选择的模型到全局设置
        DBManager::instance()->saveAppPreference("last_image_model_variant", currentModelValue());
    }
    m_previousVariantIndex = newIndex;
    updateImageSizeVisibility();
    rebuildImageSizeOptions();
    rebuildAspectRatioOptions();

    // 切换模型时，尝试从草稿恢复该模型的参数
    const QString newModel = currentModelValue();
    ImageDraft draft = DBManager::instance()->loadImageDraft(newModel);

    if (!draft.serverUrl.isEmpty() || !draft.prompt.isEmpty()) {
        // 有草稿数据，恢复参数
        int serverIndex = serverCombo->findData(draft.serverUrl);
        if (serverIndex >= 0) serverCombo->setCurrentIndex(serverIndex);

        if (!draft.apiKeyId.isEmpty() && draft.apiKeyId != "0") {
            int keyIndex = apiKeyCombo->findData(draft.apiKeyId.toInt());
            if (keyIndex >= 0) apiKeyCombo->setCurrentIndex(keyIndex);
        }

        int aspectIndex = aspectRatioCombo->findData(draft.aspectRatio);
        if (aspectIndex >= 0) aspectRatioCombo->setCurrentIndex(aspectIndex);

        if (m_imageSizeVisible && !draft.imageSize.isEmpty()) {
            int sizeIndex = imageSizeCombo->findData(draft.imageSize);
            if (sizeIndex >= 0) imageSizeCombo->setCurrentIndex(sizeIndex);
        }

        if (!draft.prompt.isEmpty()) {
            promptInput->setPlainText(draft.prompt);
        }

        // 恢复参考图
        referenceImagePaths.clear();
        if (!draft.referenceImagePaths.isEmpty()) {
            QStringList paths = draft.referenceImagePaths.split("|");
            for (const QString &path : paths) {
                if (QFile::exists(path)) {
                    referenceImagePaths.append(path);
                }
            }
        }
        updateThumbnailGrid();
    }

    const QString model = currentModelValue();
    const int maxImages = maxReferenceImages();

    if (model == "gemini-3.1-flash-image-preview") {
        // 香蕉2：最多14张
        referenceHintLabel->setVisible(true);
        referenceHintLabel->setText(
            "当前选择的变体模型「香蕉2」：\n"
            "• 最多 10 张与最终图片高度一致的对象图片\n"
            "• 最多 4 张角色图片，以保持角色一致性"
        );
        referenceCountLabel->setVisible(true);
    } else if (model == "gemini-3-pro-image-preview") {
        // 香蕉Pro：最多11张
        referenceHintLabel->setVisible(true);
        referenceHintLabel->setText(
            "当前选择的变体模型「香蕉Pro」：\n"
            "• 最多 6 张高保真对象图片，用于包含在最终图片中\n"
            "• 最多 5 张角色图片，以保持角色一致性"
        );
        referenceCountLabel->setVisible(true);
    } else if (model == "gemini-2.5-flash-image") {
        // 香蕉1：最多3张
        referenceHintLabel->setVisible(true);
        referenceHintLabel->setText(
            "当前选择的变体模型「香蕉1」支持上传 5 张参考图"
        );
        referenceCountLabel->setVisible(true);
    } else {
        referenceHintLabel->setVisible(false);
        referenceCountLabel->setVisible(false);
        if (!referenceImagePaths.isEmpty()) {
            referenceImagePaths.clear();
        }
    }
    updateThumbnailGrid();
    // 注意：不在此处保存 last_image_model_variant，避免覆盖用户关闭时的选择
}

void GeminiImagePage::updateImageSizeVisibility()
{
    const QString model = currentModelValue();
    m_imageSizeVisible = (model == "gemini-3.1-flash-image-preview" || model == "gemini-3-pro-image-preview");
    imageSizeWidget->setVisible(m_imageSizeVisible);
}

void GeminiImagePage::rebuildImageSizeOptions()
{
    const QString model = currentModelValue();
    imageSizeCombo->blockSignals(true);
    imageSizeCombo->clear();

    if (model == "gemini-3.1-flash-image-preview") {
        // 香蕉2: 512, 1K, 2K, 4K
        imageSizeCombo->addItem("0.5k", "512");
        imageSizeCombo->addItem("1K", "1K");
        imageSizeCombo->addItem("2K", "2K");
        imageSizeCombo->addItem("4K", "4K");
    } else if (model == "gemini-3-pro-image-preview") {
        // 香蕉Pro: 1K, 2K, 4K
        imageSizeCombo->addItem("1K", "1K");
        imageSizeCombo->addItem("2K", "2K");
        imageSizeCombo->addItem("4K", "4K");
    }
    // 香蕉1 无 imageSize 参数，不添加选项

    imageSizeCombo->setCurrentIndex(0);
    imageSizeCombo->blockSignals(false);
}

void GeminiImagePage::rebuildAspectRatioOptions()
{
    const QString model = currentModelValue();
    aspectRatioCombo->blockSignals(true);
    aspectRatioCombo->clear();

    if (model == "gemini-3.1-flash-image-preview") {
        // 香蕉2: 14种宽高比
        aspectRatioCombo->addItem("1:1", "1:1");
        aspectRatioCombo->addItem("1:4", "1:4");
        aspectRatioCombo->addItem("1:8", "1:8");
        aspectRatioCombo->addItem("2:3", "2:3");
        aspectRatioCombo->addItem("3:2", "3:2");
        aspectRatioCombo->addItem("3:4", "3:4");
        aspectRatioCombo->addItem("4:1", "4:1");
        aspectRatioCombo->addItem("4:3", "4:3");
        aspectRatioCombo->addItem("4:5", "4:5");
        aspectRatioCombo->addItem("5:4", "5:4");
        aspectRatioCombo->addItem("8:1", "8:1");
        aspectRatioCombo->addItem("9:16", "9:16");
        aspectRatioCombo->addItem("16:9", "16:9");
        aspectRatioCombo->addItem("21:9", "21:9");
    } else if (model == "gemini-3-pro-image-preview") {
        // 香蕉Pro: 10种宽高比
        aspectRatioCombo->addItem("1:1", "1:1");
        aspectRatioCombo->addItem("2:3", "2:3");
        aspectRatioCombo->addItem("3:2", "3:2");
        aspectRatioCombo->addItem("3:4", "3:4");
        aspectRatioCombo->addItem("4:3", "4:3");
        aspectRatioCombo->addItem("4:5", "4:5");
        aspectRatioCombo->addItem("5:4", "5:4");
        aspectRatioCombo->addItem("9:16", "9:16");
        aspectRatioCombo->addItem("16:9", "16:9");
        aspectRatioCombo->addItem("21:9", "21:9");
    } else {
        // 香蕉1: 10种宽高比
        aspectRatioCombo->addItem("1:1", "1:1");
        aspectRatioCombo->addItem("2:3", "2:3");
        aspectRatioCombo->addItem("3:2", "3:2");
        aspectRatioCombo->addItem("3:4", "3:4");
        aspectRatioCombo->addItem("4:3", "4:3");
        aspectRatioCombo->addItem("4:5", "4:5");
        aspectRatioCombo->addItem("5:4", "5:4");
        aspectRatioCombo->addItem("9:16", "9:16");
        aspectRatioCombo->addItem("16:9", "16:9");
        aspectRatioCombo->addItem("21:9", "21:9");
    }

    aspectRatioCombo->setCurrentIndex(0);
    aspectRatioCombo->blockSignals(false);
}

void GeminiImagePage::uploadReferenceImage()
{
    const int maxImages = maxReferenceImages();
    if (referenceImagePaths.size() >= maxImages) {
        QMessageBox::warning(this, "提示", QString("只支持最多%1张图片").arg(maxImages));
        return;
    }
    // 多图：追加模式
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "选择参考图",
        QString(),
        "图片文件 (*.png *.jpg *.jpeg *.webp *.bmp)"
    );

    if (filePath.isEmpty()) return;

    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        QMessageBox::warning(this, "错误", "参考图不存在或不可读");
        return;
    }

    referenceImagePaths.append(filePath);
    updateThumbnailGrid();
}

void GeminiImagePage::clearReferenceImage()
{
    referenceImagePaths.clear();
    updateThumbnailGrid();
}

void GeminiImagePage::clearPrompt()
{
    promptInput->clear();
}

bool GeminiImagePage::isMultiImageMode() const
{
    return maxReferenceImages() > 0;
}

int GeminiImagePage::maxReferenceImages() const
{
    const QString model = currentModelValue();
    if (model == "gemini-3.1-flash-image-preview") {
        return 14; // 香蕉2：最多14张
    }
    if (model == "gemini-3-pro-image-preview") {
        return 11; // 香蕉Pro：最多11张
    }
    if (model == "gemini-2.5-flash-image") {
        return 5; // 香蕉1：最多5张
    }
    return 0; // 默认不支持
}

void GeminiImagePage::updateThumbnailGrid()
{
    // 清空现有缩略图
    QLayoutItem *item;
    while ((item = thumbnailLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    int count = referenceImagePaths.size();
    int maxImages = maxReferenceImages();
    referenceCountLabel->setText(QString("已选 %1/%2 张").arg(count).arg(maxImages));

    // 设置容器高度（2排=140px，3排=210px）
    int rows = (count <= 5) ? 2 : 3;
    thumbnailContainer->setMinimumHeight(rows * 70);

    for (int i = 0; i < count; ++i) {
        QLabel *thumb = new QLabel();
        QPixmap pix(referenceImagePaths[i]);
        if (!pix.isNull()) {
            thumb->setPixmap(pix.scaled(60, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            thumb->setText("加载失败");
        }
        thumb->setAlignment(Qt::AlignCenter);
        thumb->setStyleSheet("border: 1px solid #475569; border-radius: 4px; background-color: #0f172a;");
        thumb->setCursor(Qt::PointingHandCursor);
        thumb->setToolTip(QString("点击替换/删除（第%1张）").arg(i + 1));

        // 安装事件过滤器来处理点击
        thumb->installEventFilter(this);

        // 行 = i/5, 列 = i%5
        thumbnailLayout->addWidget(thumb, i / 5, i % 5);
    }
}

bool GeminiImagePage::eventFilter(QObject *obj, QEvent *event)
{
    // 处理 promptInput 的焦点离开事件，立即保存
    if (obj == promptInput && event->type() == QEvent::FocusOut) {
        m_promptSaveTimer->stop();  // 停止延迟保存定时器
        saveDraft();                 // 立即保存
        return true;
    }

    if (event->type() == QEvent::MouseButtonPress) {
        // 点击的是缩略图容器空白区域
        if (obj == thumbnailContainer) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            QPoint pos = mouseEvent->pos();

            // 检查点击位置是否在任何缩略图之内
            for (int i = 0; i < thumbnailLayout->count(); ++i) {
                QWidget *widget = thumbnailLayout->itemAt(i)->widget();
                if (widget && widget->geometry().contains(pos)) {
                    // 点击的是缩略图，交给下面的逻辑处理
                    return QWidget::eventFilter(obj, event);
                }
            }
            // 点击空白区域，触发上传
            uploadReferenceImage();
            return true;
        }

        // 找到点击的缩略图索引
        for (int i = 0; i < thumbnailLayout->count(); ++i) {
            if (thumbnailLayout->itemAt(i)->widget() == obj) {
                removeReferenceImage(i);
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void GeminiImagePage::hideEvent(QHideEvent *event)
{
    // 页面隐藏时保存当前参数
    saveCurrentModelPreferences();
    QWidget::hideEvent(event);
}

void GeminiImagePage::removeReferenceImage(int index)
{
    if (index < 0 || index >= referenceImagePaths.size()) {
        return;
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("参考图操作");
    msgBox.setText(QString("已选 %1 张参考图，请选择操作").arg(referenceImagePaths.size()));
    QPushButton *replaceBtn = msgBox.addButton("替换", QMessageBox::AcceptRole);
    QPushButton *deleteBtn = msgBox.addButton("删除", QMessageBox::DestructiveRole);
    QPushButton *cancelBtn = msgBox.addButton("取消", QMessageBox::RejectRole);

    msgBox.exec();

    if (msgBox.clickedButton() == replaceBtn) {
        replaceReferenceImage(index);
    } else if (msgBox.clickedButton() == deleteBtn) {
        referenceImagePaths.removeAt(index);
        updateThumbnailGrid();
    }
}

void GeminiImagePage::replaceReferenceImage(int index)
{
    if (index < 0 || index >= referenceImagePaths.size()) {
        return;
    }

    QString filePath = QFileDialog::getOpenFileName(
        this,
        "选择替换图片",
        QString(),
        "图片文件 (*.png *.jpg *.jpeg *.webp *.bmp)"
    );

    if (!filePath.isEmpty()) {
        referenceImagePaths[index] = filePath;
        updateThumbnailGrid();
    }
}

void GeminiImagePage::restorePreferences()
{
    const QString model = currentModelValue();
    ImagePreferences prefs = DBManager::instance()->loadImagePreferences(model);
    FILE *f = fopen("/tmp/chickenai_debug.txt", "a");
    if (f) { fprintf(f, "restorePreferences: model=%s imageSize=%s m_imageSizeVisible=%d\n", model.toUtf8().constData(), prefs.imageSize.toUtf8().constData(), m_imageSizeVisible); fclose(f); }

    // 恢复 aspectRatio
    int aspectIndex = aspectRatioCombo->findData(prefs.aspectRatio);
    if (aspectIndex >= 0) {
        aspectRatioCombo->setCurrentIndex(aspectIndex);
    }

    // 恢复 imageSize
    if (m_imageSizeVisible) {
        int sizeIndex = imageSizeCombo->findData(prefs.imageSize);
        if (sizeIndex >= 0) {
            imageSizeCombo->setCurrentIndex(sizeIndex);
        }
    }

    // 恢复 serverUrl
    int serverIndex = serverCombo->findData(prefs.serverUrl);
    if (serverIndex >= 0) {
        serverCombo->setCurrentIndex(serverIndex);
    }

    // 恢复 API Key
    if (prefs.apiKeyId > 0) {
        int keyIndex = apiKeyCombo->findData(prefs.apiKeyId);
        if (keyIndex >= 0) {
            apiKeyCombo->setCurrentIndex(keyIndex);
        }
        // 如果找不到对应的 keyId，不弹出警告，静默处理
    }

    // 恢复 prompt
    if (!prefs.prompt.isEmpty()) {
        promptInput->setPlainText(prefs.prompt);
    }

    // 恢复 referenceImagePaths
    referenceImagePaths.clear();
    if (!prefs.referenceImagePaths.isEmpty()) {
        QStringList paths = prefs.referenceImagePaths.split("|");
        for (const QString &path : paths) {
            if (QFile::exists(path)) {
                referenceImagePaths.append(path);
            }
            // 不存在的文件静默忽略
        }
    }
    updateThumbnailGrid();
}

void GeminiImagePage::savePreferences(int modelIndex)
{
    // modelIndex 是切换到的模型索引（来自 onVariantChanged 参数）
    // 用于在 setupUI 场景下正确保存：restoreLastModelVariant 设置了 combo，
    // 但 onVariantChanged(0) 被触发时，combo 已经在用户上次选择的模型上了
    QString model;
    if (modelIndex >= 0 && modelIndex < variantCombo->count()) {
        model = variantCombo->itemData(modelIndex).toString();
    } else {
        model = currentModelValue();
    }

    ImagePreferences prefs;
    prefs.modelVariant = model;
    prefs.aspectRatio = aspectRatioCombo->currentData().toString();
    prefs.imageSize = imageSizeWidget->isVisible() ? imageSizeCombo->currentData().toString() : "";
    prefs.serverUrl = serverCombo->currentData().toString();
    prefs.apiKeyId = apiKeyCombo->currentData().toInt();
    prefs.prompt = promptInput->toPlainText();
    prefs.referenceImagePaths = referenceImagePaths.join("|");
    DBManager::instance()->saveImagePreferences(prefs);
}

void GeminiImagePage::saveCurrentModelPreferences()
{
    // 保存当前模型的参数（保存当前 combo 选中的模型，即切换前的模型）
    const int currentIdx = variantCombo->currentIndex();
    savePreferences(currentIdx);
}

void GeminiImagePage::saveDraft()
{
    // 保存当前草稿到数据库
    ImageDraft draft;
    draft.modelVariant = currentModelValue();
    draft.apiKeyId = QString::number(apiKeyCombo->currentData().toInt());
    draft.serverUrl = serverCombo->currentData().toString();
    draft.imageSize = imageSizeWidget->isVisible() ? imageSizeCombo->currentData().toString() : "";
    draft.aspectRatio = aspectRatioCombo->currentData().toString();
    draft.prompt = promptInput->toPlainText();
    draft.referenceImagePaths = referenceImagePaths.join("|");
    DBManager::instance()->saveImageDraft(draft);
}

void GeminiImagePage::saveDraftOnClose()
{
    // 关闭时保存草稿（立即保存，不使用延迟）
    m_promptSaveTimer->stop();  // 停止延迟保存定时器
    m_comboSaveTimer->stop();   // 停止下拉框延迟保存定时器

    // blockSignals 避免 onVariantChanged 被触发
    variantCombo->blockSignals(true);
    apiKeyCombo->blockSignals(true);
    serverCombo->blockSignals(true);
    imageSizeCombo->blockSignals(true);
    aspectRatioCombo->blockSignals(true);

    saveDraft();

    // 恢复 signals
    variantCombo->blockSignals(false);
    apiKeyCombo->blockSignals(false);
    serverCombo->blockSignals(false);
    imageSizeCombo->blockSignals(false);
    aspectRatioCombo->blockSignals(false);

    // 保存当前选择的模型
    DBManager::instance()->saveAppPreference("last_image_model_variant", currentModelValue());
}

void GeminiImagePage::restoreDraft()
{
    // 1. 先恢复用户上次选择的模型变体
    const QString lastModel = DBManager::instance()->loadAppPreference("last_image_model_variant");
    if (lastModel.isEmpty()) return;

    int idx = variantCombo->findData(lastModel);
    if (idx < 0) return;

    // blockSignals 阻止 setCurrentIndex 触发 onVariantChanged
    variantCombo->blockSignals(true);
    variantCombo->setCurrentIndex(idx);
    variantCombo->blockSignals(false);

    // 手动执行 UI 更新逻辑
    updateImageSizeVisibility();
    rebuildImageSizeOptions();
    rebuildAspectRatioOptions();

    // 2. 加载当前模型的草稿
    const QString currentModel = currentModelValue();
    ImageDraft draft = DBManager::instance()->loadImageDraft(currentModel);

    // 如果草稿完全为空，保持默认值
    if (draft.imageSize.isEmpty() && draft.aspectRatio.isEmpty() && draft.prompt.isEmpty()) {
        return;
    }

    // 恢复参数
    int serverIndex = serverCombo->findData(draft.serverUrl);
    if (serverIndex >= 0) serverCombo->setCurrentIndex(serverIndex);

    if (!draft.apiKeyId.isEmpty() && draft.apiKeyId != "0") {
        int keyIndex = apiKeyCombo->findData(draft.apiKeyId.toInt());
        if (keyIndex >= 0) apiKeyCombo->setCurrentIndex(keyIndex);
    }

    int aspectIndex = aspectRatioCombo->findData(draft.aspectRatio);
    if (aspectIndex >= 0) aspectRatioCombo->setCurrentIndex(aspectIndex);

    if (m_imageSizeVisible && !draft.imageSize.isEmpty()) {
        int sizeIndex = imageSizeCombo->findData(draft.imageSize);
        if (sizeIndex >= 0) imageSizeCombo->setCurrentIndex(sizeIndex);
    }

    if (!draft.prompt.isEmpty()) {
        promptInput->setPlainText(draft.prompt);
    }

    // 恢复参考图
    referenceImagePaths.clear();
    if (!draft.referenceImagePaths.isEmpty()) {
        QStringList paths = draft.referenceImagePaths.split("|");
        for (const QString &path : paths) {
            if (QFile::exists(path)) {
                referenceImagePaths.append(path);
            }
        }
    }
    updateThumbnailGrid();
}

void GeminiImagePage::restoreLastModelVariant()
{
    // 读取上次选择的模型变体
    const QString lastModel = DBManager::instance()->loadAppPreference("last_image_model_variant");
    m_previousVariantIndex = variantCombo->currentIndex();  // 记录切换前索引
    if (lastModel.isEmpty()) {
        m_initializing = false;
        // 无历史模型，恢复草稿
        restoreDraft();
        return;  // 没有保存的记录，保持默认选择
    }

    // 在 variantCombo 中查找对应的模型
    int index = variantCombo->findData(lastModel);
    if (index < 0) {
        m_initializing = false;
        // 找不到模型，恢复草稿
        restoreDraft();
        return;  // 找不到对应的模型，保持默认选择
    }

    // blockSignals 阻止 setCurrentIndex 触发 onVariantChanged，避免在 m_initializing=false 时错误保存默认参数
    variantCombo->blockSignals(true);
    variantCombo->setCurrentIndex(index);
    variantCombo->blockSignals(false);
    // 手动执行 onVariantChanged 中的 UI 更新逻辑（不触发保存）
    updateImageSizeVisibility();
    rebuildImageSizeOptions();
    rebuildAspectRatioOptions();

    const QString model = currentModelValue();
    if (model == "gemini-3.1-flash-image-preview") {
        referenceHintLabel->setVisible(true);
        referenceHintLabel->setText(
            "当前选择的变体模型「香蕉2」：\n"
            "• 最多 10 张与最终图片高度一致的对象图片\n"
            "• 最多 4 张角色图片，以保持角色一致性"
        );
        referenceCountLabel->setVisible(true);
    } else if (model == "gemini-3-pro-image-preview") {
        referenceHintLabel->setVisible(true);
        referenceHintLabel->setText(
            "当前选择的变体模型「香蕉Pro」：\n"
            "• 最多 6 张高保真对象图片，用于包含在最终图片中\n"
            "• 最多 5 张角色图片，以保持角色一致性"
        );
        referenceCountLabel->setVisible(true);
    } else if (model == "gemini-2.5-flash-image") {
        referenceHintLabel->setVisible(true);
        referenceHintLabel->setText(
            "当前选择的变体模型「香蕉1」支持上传 5 张参考图"
        );
        referenceCountLabel->setVisible(true);
    } else {
        referenceHintLabel->setVisible(false);
        referenceCountLabel->setVisible(false);
    }
    updateThumbnailGrid();

    // 初始化完成
    m_initializing = false;

    // 恢复草稿（从 image_draft 表恢复最后操作状态）
    restoreDraft();
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

    if (!referenceImagePaths.isEmpty()) {
        for (const QString &path : referenceImagePaths) {
            if (!QFile::exists(path)) {
                QMessageBox::warning(this, "提示", "参考图不存在或不可读");
                return;
            }
        }
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
    const QString generationMode = referenceImagePaths.isEmpty() ? "text_to_image" : "image_to_image";

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
    history.imagePath = referenceImagePaths.join("|"); // 多图路径用 | 分隔
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
        referenceImagePaths
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

    // 保存用户偏好设置
    savePreferences();

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
    connect(geminiPage, &GeminiImagePage::apiKeySelectionChanged,
            this, &ImageSingleTab::apiKeySelectionChanged);

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

void ImageSingleTab::saveCurrentModelPreferences()
{
    geminiPage->saveCurrentModelPreferences();
}

void ImageSingleTab::saveDraftOnClose()
{
    geminiPage->saveDraftOnClose();
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

void ImageHistoryTab::refreshApiKeys()
{
    // 图片历史页暂无 API 密钥选择功能，暂留空实现
    // 后续如需添加密钥过滤功能，可在此扩展
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
