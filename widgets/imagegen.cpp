#include "imagegen.h"
#include <QMessageBox>

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

    // 标题
    QLabel *titleLabel = new QLabel("🖼️ AI 图片生成");
    titleLabel->setObjectName("titleLabel");
    titleLabel->setStyleSheet("font-size: 28px; font-weight: 600; color: #F8FAFC;");
    mainLayout->addWidget(titleLabel);

    // 提示词输入
    promptInput = new QTextEdit();
    promptInput->setObjectName("promptInput");
    promptInput->setPlaceholderText("输入图片生成提示词...\n例如：赛博朋克风格的城市夜景，霓虹灯闪烁，未来感十足");
    promptInput->setMinimumHeight(120);
    mainLayout->addWidget(promptInput);

    // 参数设置行
    QHBoxLayout *paramsLayout = new QHBoxLayout();
    paramsLayout->setSpacing(15);

    // 分辨率
    QVBoxLayout *resLayout = new QVBoxLayout();
    QLabel *resLabel = new QLabel("分辨率");
    resLabel->setStyleSheet("color: #94A3B8; font-size: 14px;");
    resolutionCombo = new QComboBox();
    resolutionCombo->setObjectName("paramCombo");
    resolutionCombo->addItems({"1024x1024", "1920x1080", "1080x1920", "2048x2048"});
    resLayout->addWidget(resLabel);
    resLayout->addWidget(resolutionCombo);

    // 宽高比
    QVBoxLayout *aspectLayout = new QVBoxLayout();
    QLabel *aspectLabel = new QLabel("宽高比");
    aspectLabel->setStyleSheet("color: #94A3B8; font-size: 14px;");
    aspectRatioCombo = new QComboBox();
    aspectRatioCombo->setObjectName("paramCombo");
    aspectRatioCombo->addItems({"1:1", "16:9", "9:16", "4:3", "3:4"});
    aspectLayout->addWidget(aspectLabel);
    aspectLayout->addWidget(aspectRatioCombo);

    // 风格
    QVBoxLayout *styleLayout = new QVBoxLayout();
    QLabel *styleLabel = new QLabel("风格");
    styleLabel->setStyleSheet("color: #94A3B8; font-size: 14px;");
    styleCombo = new QComboBox();
    styleCombo->setObjectName("paramCombo");
    styleCombo->addItems({"Photorealistic", "Anime", "Oil Painting", "Watercolor", "Digital Art"});
    styleLayout->addWidget(styleLabel);
    styleLayout->addWidget(styleCombo);

    paramsLayout->addLayout(resLayout);
    paramsLayout->addLayout(aspectLayout);
    paramsLayout->addLayout(styleLayout);
    paramsLayout->addStretch();

    mainLayout->addLayout(paramsLayout);

    // 预览区域
    previewLabel = new QLabel();
    previewLabel->setObjectName("previewArea");
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setText("生成结果将显示在这里");
    previewLabel->setMinimumHeight(400);
    previewLabel->setStyleSheet(
        "background: rgba(30, 27, 75, 0.5);"
        "border: 1px solid rgba(248, 250, 252, 0.1);"
        "border-radius: 12px;"
        "color: #64748B;"
        "font-size: 16px;"
    );
    mainLayout->addWidget(previewLabel);

    // 生成按钮
    generateButton = new QPushButton("🚀 生成图片");
    generateButton->setObjectName("generateButton");
    generateButton->setMinimumHeight(50);
    generateButton->setCursor(Qt::PointingHandCursor);
    connect(generateButton, &QPushButton::clicked, this, &ImageGenWidget::generateImage);
    mainLayout->addWidget(generateButton, 0, Qt::AlignCenter);
}

void ImageGenWidget::generateImage()
{
    QString prompt = promptInput->toPlainText().trimmed();
    if (prompt.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入图片生成提示词");
        return;
    }

    QString resolution = resolutionCombo->currentText();
    QString aspectRatio = aspectRatioCombo->currentText();
    QString style = styleCombo->currentText();

    QMessageBox::information(this, "生成中",
        QString("正在生成图片...\n\n提示词: %1\n分辨率: %2\n宽高比: %3\n风格: %4\n\n(这是演示版本，暂未接入真实 AI 模型)")
        .arg(prompt).arg(resolution).arg(aspectRatio).arg(style));
}
