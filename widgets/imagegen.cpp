#include "imagegen.h"
#include "../database/dbmanager.h"
#include <QMessageBox>
#include <QResizeEvent>
#include <QTabBar>

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

    QLabel *titleLabel = new QLabel("🖼️ AI 图片生成");
    titleLabel->setStyleSheet("font-size: 28px; font-weight: bold; color: #F8FAFC;");
    mainLayout->addWidget(titleLabel);

    tabWidget = new QTabWidget();
    singleTab = new ImageSingleTab();
    batchTab = new ImageBatchTab();
    historyTab = new ImageHistoryTab();

    tabWidget->addTab(singleTab, "AI图片生成-单个");
    tabWidget->addTab(batchTab, "AI图片生成-批量");
    tabWidget->addTab(historyTab, "生成历史记录");

    mainLayout->addWidget(tabWidget);

    // 初始化 tab 宽度
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

// ImageSingleTab 实现（简化版）
ImageSingleTab::ImageSingleTab(QWidget *parent) : QWidget(parent)
{
    setupUI();
    loadApiKeys();
}

void ImageSingleTab::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QLabel *label = new QLabel("图片生成功能（谷歌香蕉2模型）");
    label->setStyleSheet("font-size: 18px; color: #F8FAFC;");
    mainLayout->addWidget(label);

    promptInput = new QTextEdit();
    promptInput->setPlaceholderText("输入图片生成提示词...");
    mainLayout->addWidget(promptInput);

    apiKeyCombo = new QComboBox();
    addKeyButton = new QPushButton("➕ 添加密钥");
    mainLayout->addWidget(apiKeyCombo);
    mainLayout->addWidget(addKeyButton);

    generateButton = new QPushButton("🚀 生成图片");
    connect(generateButton, &QPushButton::clicked, this, &ImageSingleTab::generateImage);
    mainLayout->addWidget(generateButton);
}

void ImageSingleTab::loadApiKeys()
{
    apiKeyCombo->clear();
    QList<ApiKey> keys = DBManager::instance()->getAllApiKeys();
    for (const ApiKey& key : keys) {
        apiKeyCombo->addItem(key.name, key.id);
    }
}

void ImageSingleTab::generateImage()
{
    QMessageBox::information(this, "提示", "图片生成功能待实现");
}

// ImageBatchTab 实现（简化版）
ImageBatchTab::ImageBatchTab(QWidget *parent) : QWidget(parent)
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

// ImageHistoryTab 实现（简化版）
ImageHistoryTab::ImageHistoryTab(QWidget *parent) : QWidget(parent)
{
    setupUI();
}

void ImageHistoryTab::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QLabel *label = new QLabel("图片历史记录待实现");
    mainLayout->addWidget(label);
}

void ImageHistoryTab::loadHistory() {}
void ImageHistoryTab::refreshHistory() {}
void ImageHistoryTab::onRowDoubleClicked(int, int) {}
