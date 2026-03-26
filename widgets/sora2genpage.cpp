#include "sora2genpage.h"

#include "../database/dbmanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QComboBox>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QStackedWidget>
#include <QGroupBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
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
#include <QMouseEvent>
#include <QPixmap>
#include <QObject>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSettings>
#include <QTimer>
#include <QCryptographicHash>

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

    auto *keyLayout = new QHBoxLayout();
    auto *keyLabel = new QLabel("API 密钥:");
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

    auto *serverLayout = new QHBoxLayout();
    auto *serverLabel = new QLabel("请求服务器:");
    serverLabel->setStyleSheet("font-size: 14px;");
    serverCombo = new QComboBox();
    serverCombo->addItem("【主站】https://ai.kegeai.top", "https://ai.kegeai.top");
    serverCombo->addItem("【备用】https://api.kuai.host", "https://api.kuai.host");
    serverCombo->addItem("【备用】https://api.kegeai.top", "https://api.kegeai.top");
    serverCombo->setCurrentIndex(0);
    serverLayout->addWidget(serverLabel);
    serverLayout->addWidget(serverCombo, 1);
    contentLayout->addLayout(serverLayout);

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
    unifiedDurationCombo->setEditable(true);
    unifiedDurationLayout->addWidget(unifiedDurationLabel);
    unifiedDurationLayout->addWidget(unifiedDurationCombo);

    auto *unifiedWatermarkLayout = new QVBoxLayout();
    auto *unifiedWatermarkLabel = new QLabel("保留水印");
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
    openaiSecondsCombo->setEditable(true);
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
    auto *openaiWatermarkLabel = new QLabel("保留水印");
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
    clearPromptButton = new QPushButton("清空文本");
    clearPromptButton->setCursor(Qt::PointingHandCursor);
    connect(clearPromptButton, &QPushButton::clicked, this, [this]() {
        if (isSubmitting) {
            return;
        }
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

    referenceCountLabel = new QLabel("已选 0/10 张");
    referenceCountLabel->setStyleSheet("color: #94a3b8; font-size: 12px;");
    imagesTitleLayout->addWidget(referenceCountLabel);

    imagesTitleLayout->addStretch();

    clearImagesButton = new QPushButton("清除");
    clearImagesButton->setCursor(Qt::PointingHandCursor);
    connect(clearImagesButton, &QPushButton::clicked, this, [this]() {
        if (isSubmitting) {
            return;
        }
        uploadedImagePaths.clear();
        updateThumbnailGrid();
        queueSaveSettings();
    });
    imagesTitleLayout->addWidget(clearImagesButton);
    contentLayout->addLayout(imagesTitleLayout);

    thumbnailContainer = new QWidget();
    thumbnailContainer->setMinimumHeight(150);
    thumbnailContainer->setStyleSheet("background-color: #1e293b; border-radius: 8px; border: 1px solid #334155;");
    thumbnailLayout = new QGridLayout(thumbnailContainer);
    thumbnailLayout->setSpacing(8);
    thumbnailLayout->setContentsMargins(8, 8, 8, 8);
    for (int i = 0; i < 10; ++i) {
        thumbnailLayout->setRowStretch(i / 5, 1);
        thumbnailLayout->setColumnStretch(i % 5, 1);
    }
    thumbnailContainer->installEventFilter(this);
    contentLayout->addWidget(thumbnailContainer);

    submitButton = new QPushButton("开始生成");
    contentLayout->addWidget(submitButton);

    connect(submitButton, &QPushButton::clicked, this, &Sora2GenPage::onSubmitClicked);
    connect(unifiedFormatRadio, &QRadioButton::toggled, this, &Sora2GenPage::onApiFormatChanged);
    connect(openaiFormatRadio, &QRadioButton::toggled, this, &Sora2GenPage::onApiFormatChanged);
    connect(variantCombo, &QComboBox::currentTextChanged, this, [this](const QString &) {
        refreshDurationOptionsByVariant();
        queueSaveSettings();
    });
    connect(apiKeyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        QString selectedApiKeyValue;
        const int keyId = apiKeyCombo->currentData().toInt();
        if (keyId > 0) {
            const ApiKey key = DBManager::instance()->getApiKey(keyId);
            selectedApiKeyValue = key.apiKey;
        }
        emit apiKeySelectionChanged(selectedApiKeyValue);
        queueSaveSettings();
    });

    connect(serverCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        queueSaveSettings();
    });
    connect(promptInput, &QTextEdit::textChanged, this, [this]() {
        queueSaveSettings();
    });
    connect(unifiedSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        queueSaveSettings();
    });
    connect(unifiedOrientationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        queueSaveSettings();
    });
    connect(openaiSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        queueSaveSettings();
    });
    connect(openaiStyleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        queueSaveSettings();
    });
    connect(watermarkCheckBox, &QCheckBox::toggled, this, [this](bool) {
        queueSaveSettings();
    });
    connect(privateCheckBox, &QCheckBox::toggled, this, [this](bool) {
        queueSaveSettings();
    });

    connect(unifiedDurationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (syncingDuration) {
            return;
        }
        bool ok = false;
        const int value = currentDurationValue(unifiedDurationCombo, &ok);
        if (ok) {
            syncDurationValueToBoth(value, unifiedDurationCombo);
            queueSaveSettings();
        }
    });
    connect(openaiSecondsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (syncingDuration) {
            return;
        }
        bool ok = false;
        const int value = currentDurationValue(openaiSecondsCombo, &ok);
        if (ok) {
            syncDurationValueToBoth(value, openaiSecondsCombo);
            queueSaveSettings();
        }
    });

    if (unifiedDurationCombo->lineEdit()) {
        unifiedDurationCombo->lineEdit()->installEventFilter(this);
    }
    if (openaiSecondsCombo->lineEdit()) {
        openaiSecondsCombo->lineEdit()->installEventFilter(this);
    }

    contentLayout->addStretch();
    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);

    suppressAutoSave = true;
    loadApiKeys();
    refreshDurationOptionsByVariant();
    onApiFormatChanged();
    applyApiFormatRadioStyle();
    loadSettings();
    suppressAutoSave = false;
    lastSavedSettingsSnapshot = buildSettingsSnapshot();
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

bool Sora2GenPage::eventFilter(QObject *watched, QEvent *event)
{
    if (isSubmitting && event->type() == QEvent::MouseButtonPress) {
        return true;
    }

    if (event->type() == QEvent::FocusOut) {
        if (unifiedDurationCombo && unifiedDurationCombo->lineEdit() == watched) {
            validateDurationEditor(unifiedDurationCombo);
        } else if (openaiSecondsCombo && openaiSecondsCombo->lineEdit() == watched) {
            validateDurationEditor(openaiSecondsCombo);
        }
    }

    if (event->type() == QEvent::MouseButtonPress) {
        if (watched == thumbnailContainer) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            const QPoint pos = mouseEvent->pos();
            for (int i = 0; i < thumbnailLayout->count(); ++i) {
                QWidget *widget = thumbnailLayout->itemAt(i)->widget();
                if (widget && widget->geometry().contains(pos)) {
                    return QWidget::eventFilter(watched, event);
                }
            }
            onUploadImagesClicked();
            return true;
        }

        const QWidget *clickedWidget = qobject_cast<QWidget *>(watched);
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

    return QWidget::eventFilter(watched, event);
}

QList<int> Sora2GenPage::durationsForVariant(const QString &variant) const
{
    if (variant == "sora-2-all" || variant == "sora-2-vip-all") {
        return {10, 15};
    }
    if (variant == "sora-2" || variant == "sora-2-pro") {
        return {4, 8, 12};
    }
    if (variant == "sora-2-pro-all") {
        return {10, 15, 25};
    }
    return {10, 15};
}

int Sora2GenPage::currentDurationValue(QComboBox *combo, bool *ok) const
{
    bool parsed = false;
    int value = 0;

    if (combo && combo->lineEdit()) {
        const QString text = combo->lineEdit()->text().trimmed();
        if (!text.isEmpty()) {
            value = text.toInt(&parsed);
            if (!parsed && text.endsWith("秒")) {
                const QString numericText = text.left(text.size() - 1).trimmed();
                value = numericText.toInt(&parsed);
                if (!parsed) {
                    const int idx = combo->findText(text);
                    if (idx >= 0) {
                        value = combo->itemData(idx).toString().toInt(&parsed);
                    }
                }
            }
        }
    }

    if (!parsed && combo) {
        value = combo->currentData().toString().toInt(&parsed);
    }

    if (ok) {
        *ok = parsed;
    }
    return value;
}

void Sora2GenPage::setDurationComboItems(QComboBox *combo, const QList<int> &values, int preferValue)
{
    if (!combo || values.isEmpty()) {
        return;
    }
    const QSignalBlocker blocker(combo);
    combo->clear();
    for (int v : values) {
        combo->addItem(QString::number(v) + " 秒", QString::number(v));
    }

    int target = preferValue;
    if (target <= 0) {
        target = values.first();
    }

    int idx = combo->findData(QString::number(target));
    if (idx < 0) {
        combo->addItem(QString::number(target) + " 秒", QString::number(target));
        idx = combo->findData(QString::number(target));
    }
    combo->setCurrentIndex(idx >= 0 ? idx : 0);
    if (combo->lineEdit()) {
        combo->lineEdit()->setText(QString::number(target) + " 秒");
    }
}

void Sora2GenPage::syncDurationValueToBoth(int value, QComboBox *source)
{
    if (value <= 0) {
        return;
    }

    syncingDuration = true;
    const QList<int> values = durationsForVariant(currentVariant());
    if (source != unifiedDurationCombo) {
        setDurationComboItems(unifiedDurationCombo, values, value);
    }
    if (source != openaiSecondsCombo) {
        setDurationComboItems(openaiSecondsCombo, values, value);
    }
    syncingDuration = false;

    lastValidDuration = value;
}

void Sora2GenPage::refreshDurationOptionsByVariant()
{
    const QList<int> values = durationsForVariant(currentVariant());
    bool hasCurrent = false;
    const int currentValue = currentDurationValue(unifiedDurationCombo, &hasCurrent);
    int preferValue = hasCurrent ? currentValue : lastValidDuration;
    if (preferValue <= 0) {
        preferValue = values.isEmpty() ? 10 : values.first();
    }

    syncingDuration = true;
    setDurationComboItems(unifiedDurationCombo, values, preferValue);
    setDurationComboItems(openaiSecondsCombo, values, preferValue);
    syncingDuration = false;
    lastValidDuration = preferValue;
}

bool Sora2GenPage::validateDurationEditor(QComboBox *combo)
{
    bool ok = false;
    const int value = currentDurationValue(combo, &ok);
    if (!ok || value <= 0) {
        QMessageBox::warning(this, "提示", "非整数，重新输入");
        syncDurationValueToBoth(lastValidDuration);
        return false;
    }

    syncDurationValueToBoth(value, combo);
    queueSaveSettings();
    return true;
}

int Sora2GenPage::maxReferenceImages() const
{
    return 10;
}

void Sora2GenPage::updateThumbnailGrid()
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

void Sora2GenPage::removeReferenceImage(int index)
{
    if (isSubmitting) {
        return;
    }

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

void Sora2GenPage::replaceReferenceImage(int index)
{
    if (isSubmitting) {
        return;
    }

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

    QFileInfo info(filePath);
    QImageReader reader(filePath);
    if (!info.exists() || !info.isFile() || !reader.canRead()) {
        QMessageBox::warning(this, "提示", "未选择有效图片文件");
        return;
    }

    uploadedImagePaths[index] = filePath;
    updateThumbnailGrid();
    queueSaveSettings();
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
    payload["duration"] = QString::number(currentDurationValue(unifiedDurationCombo));
    payload["watermark"] = watermarkCheckBox->isChecked();
    payload["private"] = privateCheckBox->isChecked();
    payload["is_image_to_video"] = isImageToVideo();

    QString selectedApiKeyValue;
    const int keyId = apiKeyCombo ? apiKeyCombo->currentData().toInt() : 0;
    if (keyId > 0) {
        const ApiKey key = DBManager::instance()->getApiKey(keyId);
        selectedApiKeyValue = key.apiKey;
    }
    payload["api_key_value"] = selectedApiKeyValue;
    payload["server_url"] = serverCombo ? serverCombo->currentData().toString().trimmed() : QString();

    return payload;
}

QVariantMap Sora2GenPage::buildOpenAIPayload() const
{
    QVariantMap payload;
    payload["api_format"] = "openai";
    payload["model"] = currentVariant();
    payload["prompt"] = promptInput->toPlainText().trimmed();
    payload["seconds"] = QString::number(currentDurationValue(openaiSecondsCombo));
    payload["size"] = openaiSizeCombo->currentData().toString();
    payload["style"] = openaiStyleCombo->currentData().toString();
    payload["images"] = currentImagePaths();
    payload["watermark"] = watermarkCheckBox->isChecked();
    payload["private"] = privateCheckBox->isChecked();
    payload["is_image_to_video"] = isImageToVideo();

    QString selectedApiKeyValue;
    const int keyId = apiKeyCombo ? apiKeyCombo->currentData().toInt() : 0;
    if (keyId > 0) {
        const ApiKey key = DBManager::instance()->getApiKey(keyId);
        selectedApiKeyValue = key.apiKey;
    }
    payload["api_key_value"] = selectedApiKeyValue;
    payload["server_url"] = serverCombo ? serverCombo->currentData().toString().trimmed() : QString();

    return payload;
}

void Sora2GenPage::loadApiKeys()
{
    QString selectedApiKeyValue;
    const int currentKeyId = apiKeyCombo->currentData().toInt();
    if (currentKeyId > 0) {
        const ApiKey currentKey = DBManager::instance()->getApiKey(currentKeyId);
        selectedApiKeyValue = currentKey.apiKey;
    }

    apiKeyCombo->clear();
    const QList<ApiKey> keys = DBManager::instance()->getAllApiKeys();
    if (keys.isEmpty()) {
        apiKeyCombo->addItem("无可用密钥");
        apiKeyCombo->setEnabled(false);
        addKeyButton->setVisible(true);
        return;
    }

    apiKeyCombo->setEnabled(true);
    addKeyButton->setVisible(false);
    int matchedIndex = -1;
    for (int i = 0; i < keys.size(); ++i) {
        const ApiKey &key = keys.at(i);
        apiKeyCombo->addItem(key.name, key.id);
        if (!selectedApiKeyValue.isEmpty() && key.apiKey == selectedApiKeyValue) {
            matchedIndex = i;
        }
    }

    if (matchedIndex >= 0) {
        apiKeyCombo->setCurrentIndex(matchedIndex);
    }
}

void Sora2GenPage::refreshApiKeys()
{
    loadApiKeys();
}

void Sora2GenPage::restoreDraftSettings()
{
    suppressAutoSave = true;
    loadSettings();
    suppressAutoSave = false;
    lastSavedSettingsSnapshot = buildSettingsSnapshot();
}

void Sora2GenPage::setSubmitEnabled(bool enabled)
{
    setSubmitting(!enabled);
}

void Sora2GenPage::setSubmitting(bool submitting)
{
    isSubmitting = submitting;

    if (submitButton) submitButton->setEnabled(!submitting);
    if (clearPromptButton) clearPromptButton->setEnabled(!submitting);
    if (clearImagesButton) clearImagesButton->setEnabled(!submitting);
    if (unifiedFormatRadio) unifiedFormatRadio->setEnabled(!submitting);
    if (openaiFormatRadio) openaiFormatRadio->setEnabled(!submitting);
    if (variantCombo) variantCombo->setEnabled(!submitting);
    if (apiKeyCombo) apiKeyCombo->setEnabled(!submitting);
    if (serverCombo) serverCombo->setEnabled(!submitting);
    if (unifiedSizeCombo) unifiedSizeCombo->setEnabled(!submitting);
    if (unifiedOrientationCombo) unifiedOrientationCombo->setEnabled(!submitting);
    if (unifiedDurationCombo) unifiedDurationCombo->setEnabled(!submitting);
    if (openaiSecondsCombo) openaiSecondsCombo->setEnabled(!submitting);
    if (openaiSizeCombo) openaiSizeCombo->setEnabled(!submitting);
    if (openaiStyleCombo) openaiStyleCombo->setEnabled(!submitting);
    if (watermarkCheckBox) watermarkCheckBox->setEnabled(!submitting);
    if (privateCheckBox) privateCheckBox->setEnabled(!submitting);
    if (promptInput) promptInput->setReadOnly(submitting);
}

void Sora2GenPage::onSubmitClicked()
{
    if (isSubmitting) {
        return;
    }

    const QString prompt = promptInput->toPlainText().trimmed();
    if (prompt.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入提示词");
        return;
    }

    if (!apiKeyCombo || !apiKeyCombo->isEnabled() || apiKeyCombo->count() <= 0) {
        QMessageBox::warning(this, "提示", "请先添加 API 密钥");
        return;
    }

    const int keyId = apiKeyCombo->currentData().toInt();
    if (keyId <= 0) {
        QMessageBox::warning(this, "提示", "请选择有效的 API 密钥");
        return;
    }

    const ApiKey selectedKey = DBManager::instance()->getApiKey(keyId);
    if (selectedKey.apiKey.trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "请选择有效的 API 密钥");
        return;
    }

    if (currentApiFormat() == "unified") {
        if (!validateDurationEditor(unifiedDurationCombo)) {
            return;
        }
        emit createTaskRequested(buildUnifiedPayload());
    } else {
        if (!validateDurationEditor(openaiSecondsCombo)) {
            return;
        }
        emit createTaskRequested(buildOpenAIPayload());
    }
}

void Sora2GenPage::onApiFormatChanged()
{
    paramsStack->setCurrentIndex(unifiedFormatRadio->isChecked() ? 0 : 1);
    queueSaveSettings();
}

void Sora2GenPage::onUploadImagesClicked()
{
    if (isSubmitting) {
        return;
    }

    const int maxImages = maxReferenceImages();
    if (uploadedImagePaths.size() >= maxImages) {
        QMessageBox::warning(this, "提示", QString("最多上传 %1 张参考图").arg(maxImages));
        return;
    }

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

    const int remain = maxImages - uploadedImagePaths.size();
    if (validFiles.size() > remain) {
        QMessageBox::warning(this, "提示", QString("最多上传 %1 张参考图，还可添加 %2 张").arg(maxImages).arg(remain));
        return;
    }

    bool changed = false;
    for (const QString &path : validFiles) {
        if (!uploadedImagePaths.contains(path)) {
            uploadedImagePaths.append(path);
            changed = true;
        }
    }
    if (changed) {
        updateThumbnailGrid();
        queueSaveSettings();
    }
}

void Sora2GenPage::loadFromTask(const VideoTask &task)
{
    suppressAutoSave = true;

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
                    const QString path = v.toString();
                    if (!path.isEmpty() && QFile::exists(path)) {
                        uploadedImagePaths.append(path);
                    }
                }
            }
        }
    }

    const int maxImages = maxReferenceImages();
    if (uploadedImagePaths.size() > maxImages) {
        uploadedImagePaths = uploadedImagePaths.mid(0, maxImages);
    }

    updateThumbnailGrid();
    suppressAutoSave = false;
}

void Sora2GenPage::queueSaveSettings()
{
    if (suppressAutoSave) {
        return;
    }
    if (pendingSaveSettings) {
        return;
    }

    pendingSaveSettings = true;
    QTimer::singleShot(0, this, [this]() {
        pendingSaveSettings = false;
        saveSettings();
    });
}

QString Sora2GenPage::buildSettingsSnapshot() const
{
    QString selectedApiKeyValue;
    const int keyId = apiKeyCombo->currentData().toInt();
    if (keyId > 0) {
        const ApiKey key = DBManager::instance()->getApiKey(keyId);
        selectedApiKeyValue = key.apiKey;
    }

    const bool unifiedIsCurrent = currentApiFormat() == "unified";
    const int durationValue = unifiedIsCurrent
        ? currentDurationValue(unifiedDurationCombo)
        : currentDurationValue(openaiSecondsCombo);
    const QString durationText = unifiedIsCurrent && unifiedDurationCombo->lineEdit()
        ? unifiedDurationCombo->lineEdit()->text().trimmed()
        : (openaiSecondsCombo->lineEdit() ? openaiSecondsCombo->lineEdit()->text().trimmed() : QString());

    const QStringList snapshotParts = {
        currentApiFormat(),
        currentVariant(),
        apiKeyCombo->currentData().toString(),
        selectedApiKeyValue,
        serverCombo->currentData().toString(),
        promptInput->toPlainText(),
        unifiedSizeCombo->currentData().toString(),
        unifiedOrientationCombo->currentData().toString(),
        QString::number(durationValue),
        durationText,
        openaiSizeCombo->currentData().toString(),
        openaiStyleCombo->currentData().toString(),
        QString::number(watermarkCheckBox->isChecked()),
        QString::number(privateCheckBox->isChecked()),
        uploadedImagePaths.join("\n")
    };

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(snapshotParts.join("\x1F").toUtf8());
    return QString(hash.result().toHex());
}

void Sora2GenPage::saveSettings()
{
    const QString snapshot = buildSettingsSnapshot();
    if (!lastSavedSettingsSnapshot.isEmpty() && snapshot == lastSavedSettingsSnapshot) {
        return;
    }

    QString selectedApiKeyValue;
    const int keyId = apiKeyCombo->currentData().toInt();
    if (keyId > 0) {
        const ApiKey key = DBManager::instance()->getApiKey(keyId);
        selectedApiKeyValue = key.apiKey;
    }

    const bool unifiedIsCurrent = currentApiFormat() == "unified";
    const int durationValue = unifiedIsCurrent
        ? currentDurationValue(unifiedDurationCombo)
        : currentDurationValue(openaiSecondsCombo);
    const QString durationText = unifiedIsCurrent && unifiedDurationCombo->lineEdit()
        ? unifiedDurationCombo->lineEdit()->text().trimmed()
        : (openaiSecondsCombo->lineEdit() ? openaiSecondsCombo->lineEdit()->text().trimmed() : QString());

    QSettings settings("ChickenAI", "VideoGen");
    settings.beginGroup("VideoSingleTab/Sora2Draft");
    settings.setValue("api_format", currentApiFormat());
    settings.setValue("variant", currentVariant());
    settings.setValue("apiKey", apiKeyCombo->currentIndex());
    settings.setValue("selectedApiKeyValue", selectedApiKeyValue);
    settings.setValue("server", serverCombo->currentIndex());
    settings.setValue("serverUrl", serverCombo->currentData().toString());
    settings.setValue("prompt", promptInput->toPlainText());

    settings.setValue("unified_size", unifiedSizeCombo->currentData().toString());
    settings.setValue("unified_orientation", unifiedOrientationCombo->currentData().toString());

    settings.setValue("duration", durationValue);
    settings.setValue("duration_text", durationText);

    settings.setValue("openai_size", openaiSizeCombo->currentData().toString());
    settings.setValue("openai_style", openaiStyleCombo->currentData().toString());

    settings.setValue("watermark", watermarkCheckBox->isChecked());
    settings.setValue("private", privateCheckBox->isChecked());
    settings.setValue("imagePaths", uploadedImagePaths);
    settings.endGroup();
    settings.sync();

    lastSavedSettingsSnapshot = snapshot;
}

void Sora2GenPage::loadSettings()
{
    QSettings settings("ChickenAI", "VideoGen");
    settings.beginGroup("VideoSingleTab/Sora2Draft");

    QSignalBlocker blockVariant(variantCombo);
    QSignalBlocker blockApiKey(apiKeyCombo);
    QSignalBlocker blockServer(serverCombo);
    QSignalBlocker blockPrompt(promptInput);
    QSignalBlocker blockUnifiedSize(unifiedSizeCombo);
    QSignalBlocker blockUnifiedOrientation(unifiedOrientationCombo);
    QSignalBlocker blockUnifiedDuration(unifiedDurationCombo);
    QSignalBlocker blockOpenaiSeconds(openaiSecondsCombo);
    QSignalBlocker blockOpenaiSize(openaiSizeCombo);
    QSignalBlocker blockOpenaiStyle(openaiStyleCombo);
    QSignalBlocker blockWatermark(watermarkCheckBox);
    QSignalBlocker blockPrivate(privateCheckBox);
    QSignalBlocker blockUnifiedRadio(unifiedFormatRadio);
    QSignalBlocker blockOpenaiRadio(openaiFormatRadio);

    const QString prompt = settings.value("prompt", "").toString();
    promptInput->setPlainText(prompt);

    const QString savedFormat = settings.value("api_format", "unified").toString();
    unifiedFormatRadio->setChecked(savedFormat != "openai");
    openaiFormatRadio->setChecked(savedFormat == "openai");
    paramsStack->setCurrentIndex(savedFormat == "openai" ? 1 : 0);

    const QString savedVariant = settings.value("variant", "").toString();
    if (!savedVariant.isEmpty()) {
        const int index = variantCombo->findText(savedVariant);
        if (index >= 0) {
            variantCombo->setCurrentIndex(index);
        }
    }

    const int savedApiKeyIndex = settings.value("apiKey", 0).toInt();
    const QString savedApiKeyValue = settings.value("selectedApiKeyValue", "").toString();
    int matchedApiKeyIndex = -1;
    if (!savedApiKeyValue.isEmpty()) {
        for (int i = 0; i < apiKeyCombo->count(); ++i) {
            const int savedId = apiKeyCombo->itemData(i).toInt();
            if (savedId <= 0) {
                continue;
            }
            const ApiKey key = DBManager::instance()->getApiKey(savedId);
            if (key.apiKey == savedApiKeyValue) {
                matchedApiKeyIndex = i;
                break;
            }
        }
    }
    if (matchedApiKeyIndex >= 0) {
        apiKeyCombo->setCurrentIndex(matchedApiKeyIndex);
    } else if (savedApiKeyIndex >= 0 && savedApiKeyIndex < apiKeyCombo->count()) {
        apiKeyCombo->setCurrentIndex(savedApiKeyIndex);
    }

    const QString savedServerUrl = settings.value("serverUrl", "").toString().trimmed();
    int matchedServerIndex = -1;
    if (!savedServerUrl.isEmpty()) {
        matchedServerIndex = serverCombo->findData(savedServerUrl);
    }
    if (matchedServerIndex >= 0) {
        serverCombo->setCurrentIndex(matchedServerIndex);
    } else {
        const int savedServerIndex = settings.value("server", 0).toInt();
        if (savedServerIndex >= 0 && savedServerIndex < serverCombo->count()) {
            serverCombo->setCurrentIndex(savedServerIndex);
        }
    }

    const QString savedUnifiedSize = settings.value("unified_size", "").toString();
    if (!savedUnifiedSize.isEmpty()) {
        const int index = unifiedSizeCombo->findData(savedUnifiedSize);
        if (index >= 0) {
            unifiedSizeCombo->setCurrentIndex(index);
        }
    }

    const QString savedUnifiedOrientation = settings.value("unified_orientation", "").toString();
    if (!savedUnifiedOrientation.isEmpty()) {
        const int index = unifiedOrientationCombo->findData(savedUnifiedOrientation);
        if (index >= 0) {
            unifiedOrientationCombo->setCurrentIndex(index);
        }
    }

    const QString savedOpenaiSize = settings.value("openai_size", "").toString();
    if (!savedOpenaiSize.isEmpty()) {
        const int index = openaiSizeCombo->findData(savedOpenaiSize);
        if (index >= 0) {
            openaiSizeCombo->setCurrentIndex(index);
        }
    }

    const QString savedOpenaiStyle = settings.value("openai_style", "").toString();
    if (!savedOpenaiStyle.isEmpty() || openaiStyleCombo->findData("") >= 0) {
        const int index = openaiStyleCombo->findData(savedOpenaiStyle);
        if (index >= 0) {
            openaiStyleCombo->setCurrentIndex(index);
        }
    }

    watermarkCheckBox->setChecked(settings.value("watermark", false).toBool());
    privateCheckBox->setChecked(settings.value("private", false).toBool());

    const QStringList values = settings.value("imagePaths").toStringList();
    uploadedImagePaths.clear();
    for (const QString &path : values) {
        if (!path.isEmpty() && QFile::exists(path) && !uploadedImagePaths.contains(path)) {
            uploadedImagePaths.append(path);
        }
    }
    if (uploadedImagePaths.size() > maxReferenceImages()) {
        uploadedImagePaths = uploadedImagePaths.mid(0, maxReferenceImages());
    }

    const int savedDurationValue = settings.value("duration", 0).toInt();
    const QString savedDurationText = settings.value("duration_text", "").toString().trimmed();
    int preferDuration = savedDurationValue > 0 ? savedDurationValue : lastValidDuration;
    if (preferDuration <= 0) {
        preferDuration = 10;
    }

    syncingDuration = true;
    const QList<int> durationOptions = durationsForVariant(currentVariant());
    setDurationComboItems(unifiedDurationCombo, durationOptions, preferDuration);
    setDurationComboItems(openaiSecondsCombo, durationOptions, preferDuration);
    syncingDuration = false;

    if (!savedDurationText.isEmpty()) {
        if (unifiedDurationCombo->lineEdit()) {
            unifiedDurationCombo->lineEdit()->setText(savedDurationText);
        }
        if (openaiSecondsCombo->lineEdit()) {
            openaiSecondsCombo->lineEdit()->setText(savedDurationText);
        }
    }
    bool durationOk = false;
    const int normalizedDuration = currentDurationValue(
        savedFormat == "openai" ? openaiSecondsCombo : unifiedDurationCombo,
        &durationOk);
    if (durationOk && normalizedDuration > 0) {
        syncDurationValueToBoth(normalizedDuration);
    } else {
        syncDurationValueToBoth(preferDuration);
    }

    updateThumbnailGrid();
    settings.endGroup();
}
