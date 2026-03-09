#include "configwidget.h"
#include "../database/dbmanager.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QResizeEvent>
#include <QScreen>

ConfigWidget::ConfigWidget(QWidget *parent)
    : QWidget(parent), scaleFactor(1.0)
{
    scaleFactor = calculateScaleFactor();
    setupUI();
    loadApiKeys();
}

void ConfigWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(40, 40, 40, 40);
    mainLayout->setSpacing(20);

    // 标题
    QLabel *titleLabel = new QLabel("⚙️ API 密钥配置");
    titleLabel->setStyleSheet("font-size: 28px; font-weight: bold; color: #F8FAFC;");
    mainLayout->addWidget(titleLabel);

    // 添加按钮
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    addButton = new QPushButton("➕ 添加密钥");
    addButton->setObjectName("primaryButton");
    addButton->setFixedHeight(40);
    addButton->setMinimumWidth(120);
    connect(addButton, &QPushButton::clicked, this, &ConfigWidget::addApiKey);
    buttonLayout->addWidget(addButton);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    // 密钥表格
    apiKeyTable = new QTableWidget();
    apiKeyTable->setObjectName("apiKeyTable");
    apiKeyTable->setColumnCount(5);
    apiKeyTable->setHorizontalHeaderLabels({"序号", "名称", "密钥", "操作", ""});

    // 设置列的调整模式
    apiKeyTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    apiKeyTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    apiKeyTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    apiKeyTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    apiKeyTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);

    // 初始列宽（会在 updateColumnWidths 中动态调整）
    apiKeyTable->setColumnWidth(0, 80);

    apiKeyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    apiKeyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    apiKeyTable->verticalHeader()->setVisible(false);
    apiKeyTable->setMinimumHeight(400);

    // 设置表格字体大小为 12pt
    QFont tableFont = apiKeyTable->font();
    tableFont.setPointSize(12);
    apiKeyTable->setFont(tableFont);
    apiKeyTable->horizontalHeader()->setFont(tableFont);

    mainLayout->addWidget(apiKeyTable);

    // 初始化行高和列宽
    updateRowHeight();
    updateColumnWidths();
}

double ConfigWidget::calculateScaleFactor()
{
    QScreen *screen = QApplication::primaryScreen();
    if (!screen) return 1.0;

    // 获取逻辑 DPI
    qreal dpi = screen->logicalDotsPerInch();

    // 基准 DPI 为 96
    double scaleFactor = dpi / 96.0;

    // 限制缩放范围在 0.8 到 2.0 之间
    if (scaleFactor < 0.8) scaleFactor = 0.8;
    if (scaleFactor > 2.0) scaleFactor = 2.0;

    return scaleFactor;
}

void ConfigWidget::updateRowHeight()
{
    if (!apiKeyTable) return;

    // 基础按钮高度 36px，根据缩放因子调整
    int baseButtonHeight = 36;
    int buttonHeight = static_cast<int>(baseButtonHeight * scaleFactor);

    // 行高 = 按钮高度 + 上下 margin (各5px) + 额外空间 (14px)
    // 确保行高比按钮高度高一些，留出足够的垂直空间
    int rowHeight = buttonHeight + 10 + 14;  // 按钮 + margin + 额外空间

    apiKeyTable->verticalHeader()->setDefaultSectionSize(rowHeight);

    // 更新已有行的高度
    for (int i = 0; i < apiKeyTable->rowCount(); ++i) {
        apiKeyTable->setRowHeight(i, rowHeight);
    }
}

void ConfigWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateColumnWidths();
    updateRowHeight();
}

void ConfigWidget::updateColumnWidths()
{
    if (!apiKeyTable) return;

    // 获取表格宽度
    int tableWidth = apiKeyTable->width();

    // 固定列宽
    int idColumnWidth = 80;      // 序号列
    int viewColumnWidth = 80;    // 查看列

    // 计算可用宽度
    int availableWidth = tableWidth - idColumnWidth - viewColumnWidth;

    // 名称列：10%
    int nameColumnWidth = availableWidth * 0.10;

    // 剩余宽度由密钥列和操作列平分
    int remainingWidth = availableWidth - nameColumnWidth;
    int keyColumnWidth = remainingWidth * 0.5;
    int actionColumnWidth = remainingWidth * 0.5;

    // 设置列宽
    apiKeyTable->setColumnWidth(0, idColumnWidth);
    apiKeyTable->setColumnWidth(1, nameColumnWidth);
    apiKeyTable->setColumnWidth(2, keyColumnWidth);
    apiKeyTable->setColumnWidth(3, actionColumnWidth);
    apiKeyTable->setColumnWidth(4, viewColumnWidth);
}

void ConfigWidget::loadApiKeys()
{
    apiKeyTable->setRowCount(0);
    QList<ApiKey> keys = DBManager::instance()->getAllApiKeys();

    // 设置按钮字体为 12pt
    QFont buttonFont;
    buttonFont.setPointSize(12);

    // 基础按钮高度 36px，根据缩放因子调整
    int baseButtonHeight = 36;
    int buttonHeight = static_cast<int>(baseButtonHeight * scaleFactor);

    for (int i = 0; i < keys.size(); ++i) {
        const ApiKey& key = keys[i];
        apiKeyTable->insertRow(i);

        // 序号
        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(key.id));
        idItem->setTextAlignment(Qt::AlignCenter);
        apiKeyTable->setItem(i, 0, idItem);

        // 名称
        QTableWidgetItem *nameItem = new QTableWidgetItem(key.name);
        apiKeyTable->setItem(i, 1, nameItem);

        // 密钥（遮罩）
        QTableWidgetItem *keyItem = new QTableWidgetItem(maskApiKey(key.apiKey));
        apiKeyTable->setItem(i, 2, keyItem);

        // 操作按钮
        QWidget *actionWidget = new QWidget();
        QHBoxLayout *actionLayout = new QHBoxLayout(actionWidget);
        actionLayout->setContentsMargins(5, 5, 5, 5);
        actionLayout->setSpacing(5);

        QPushButton *editBtn = new QPushButton("编辑");
        editBtn->setFont(buttonFont);
        editBtn->setMinimumHeight(buttonHeight);
        editBtn->setProperty("keyId", key.id);
        connect(editBtn, &QPushButton::clicked, this, &ConfigWidget::editApiKey);

        QPushButton *deleteBtn = new QPushButton("删除");
        deleteBtn->setFont(buttonFont);
        deleteBtn->setMinimumHeight(buttonHeight);
        deleteBtn->setProperty("keyId", key.id);
        connect(deleteBtn, &QPushButton::clicked, this, &ConfigWidget::deleteApiKey);

        actionLayout->addWidget(editBtn);
        actionLayout->addWidget(deleteBtn);
        apiKeyTable->setCellWidget(i, 3, actionWidget);

        // 查看/复制按钮
        QWidget *viewWidget = new QWidget();
        QHBoxLayout *viewLayout = new QHBoxLayout(viewWidget);
        viewLayout->setContentsMargins(5, 5, 5, 5);
        viewLayout->setSpacing(5);

        QPushButton *viewBtn = new QPushButton("👁");
        viewBtn->setFont(buttonFont);
        viewBtn->setMinimumHeight(buttonHeight);
        viewBtn->setProperty("keyId", key.id);
        viewBtn->setToolTip("查看完整密钥");
        connect(viewBtn, &QPushButton::clicked, this, &ConfigWidget::viewApiKey);

        viewLayout->addWidget(viewBtn);
        apiKeyTable->setCellWidget(i, 4, viewWidget);
    }
}

QString ConfigWidget::maskApiKey(const QString& apiKey)
{
    if (apiKey.length() <= 7) {
        return apiKey;
    }
    QString prefix = apiKey.left(3);
    QString suffix = apiKey.right(4);
    return prefix + "***********" + suffix;
}

void ConfigWidget::addApiKey()
{
    ApiKeyDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QString name = dialog.getName();
        QString apiKey = dialog.getApiKey();

        if (name.isEmpty() || apiKey.isEmpty()) {
            QMessageBox::warning(this, "错误", "名称和密钥不能为空");
            return;
        }

        if (DBManager::instance()->addApiKey(name, apiKey)) {
            QMessageBox::information(this, "成功", "密钥添加成功");
            loadApiKeys();
            emit apiKeysChanged();
        } else {
            QMessageBox::critical(this, "错误", "密钥添加失败");
        }
    }
}

void ConfigWidget::editApiKey()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    int keyId = btn->property("keyId").toInt();
    ApiKeyDialog dialog(this, keyId);

    if (dialog.exec() == QDialog::Accepted) {
        QString name = dialog.getName();
        QString apiKey = dialog.getApiKey();

        if (name.isEmpty() || apiKey.isEmpty()) {
            QMessageBox::warning(this, "错误", "名称和密钥不能为空");
            return;
        }

        if (DBManager::instance()->updateApiKey(keyId, name, apiKey)) {
            QMessageBox::information(this, "成功", "密钥更新成功");
            loadApiKeys();
            emit apiKeysChanged();
        } else {
            QMessageBox::critical(this, "错误", "密钥更新失败");
        }
    }
}

void ConfigWidget::deleteApiKey()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    int keyId = btn->property("keyId").toInt();

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认删除", "确定要删除这个密钥吗？",
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        if (DBManager::instance()->deleteApiKey(keyId)) {
            QMessageBox::information(this, "成功", "密钥删除成功");
            loadApiKeys();
            emit apiKeysChanged();
        } else {
            QMessageBox::critical(this, "错误", "密钥删除失败");
        }
    }
}

void ConfigWidget::viewApiKey()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    int keyId = btn->property("keyId").toInt();
    ApiKey key = DBManager::instance()->getApiKey(keyId);

    QDialog dialog(this);
    dialog.setWindowTitle("查看密钥");
    dialog.setMinimumWidth(500);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *nameLabel = new QLabel("名称: " + key.name);
    nameLabel->setStyleSheet("font-size: 16px; color: #F8FAFC;");
    layout->addWidget(nameLabel);

    QLineEdit *keyEdit = new QLineEdit(key.apiKey);
    keyEdit->setReadOnly(true);
    keyEdit->setStyleSheet("font-size: 14px; padding: 10px;");
    layout->addWidget(keyEdit);

    QPushButton *copyBtn = new QPushButton("📋 复制到剪贴板");
    copyBtn->setFixedHeight(40);
    connect(copyBtn, &QPushButton::clicked, [key]() {
        QApplication::clipboard()->setText(key.apiKey);
        QMessageBox::information(nullptr, "成功", "密钥已复制到剪贴板");
    });
    layout->addWidget(copyBtn);

    QPushButton *closeBtn = new QPushButton("关闭");
    closeBtn->setFixedHeight(40);
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(closeBtn);

    dialog.exec();
}

void ConfigWidget::refreshTable()
{
    loadApiKeys();
}

// ApiKeyDialog 实现
ApiKeyDialog::ApiKeyDialog(QWidget *parent, int keyId)
    : QDialog(parent), keyId(keyId)
{
    setupUI();
    if (keyId != -1) {
        loadApiKey(keyId);
    }
}

void ApiKeyDialog::setupUI()
{
    setWindowTitle(keyId == -1 ? "添加密钥" : "编辑密钥");
    setMinimumWidth(500);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QFormLayout *formLayout = new QFormLayout();

    nameEdit = new QLineEdit();
    nameEdit->setPlaceholderText("例如: sora2密钥");
    nameEdit->setStyleSheet("padding: 10px; font-size: 14px;");

    apiKeyEdit = new QLineEdit();
    apiKeyEdit->setPlaceholderText("例如: sk-***********jshfg");
    apiKeyEdit->setStyleSheet("padding: 10px; font-size: 14px;");

    formLayout->addRow("名称:", nameEdit);
    formLayout->addRow("密钥:", apiKeyEdit);

    mainLayout->addLayout(formLayout);

    // 按钮
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    saveButton = new QPushButton("保存");
    saveButton->setFixedHeight(40);
    saveButton->setMinimumWidth(100);
    connect(saveButton, &QPushButton::clicked, this, &QDialog::accept);

    cancelButton = new QPushButton("取消");
    cancelButton->setFixedHeight(40);
    cancelButton->setMinimumWidth(100);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    buttonLayout->addStretch();
    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(cancelButton);

    mainLayout->addLayout(buttonLayout);
}

void ApiKeyDialog::loadApiKey(int id)
{
    ApiKey key = DBManager::instance()->getApiKey(id);
    nameEdit->setText(key.name);
    apiKeyEdit->setText(key.apiKey);
}
