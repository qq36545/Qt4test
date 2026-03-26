#include "configwidget.h"
#include "../database/dbmanager.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QFormLayout>
#include <QResizeEvent>
#include <QScreen>
#include <QEvent>
#include <QPalette>

namespace {
QVBoxLayout* createCenteredContentLayout(QWidget *parent, QVBoxLayout *parentLayout, int spacing)
{
    QHBoxLayout *containerLayout = new QHBoxLayout();
    containerLayout->setContentsMargins(0, 0, 0, 0);

    QWidget *contentWidget = new QWidget(parent);
    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(spacing);

    containerLayout->addStretch(1);
    containerLayout->addWidget(contentWidget, 8);
    containerLayout->addStretch(1);

    parentLayout->addLayout(containerLayout);
    return contentLayout;
}

bool isDarkThemeWidget(const QWidget *widget)
{
    const QWidget *host = widget && widget->window() ? widget->window() : widget;
    if (!host) return false;
    const QColor bgColor = host->palette().color(QPalette::Window);
    return bgColor.lightness() < 128;
}

void applyKeyDialogTheme(QDialog *dialog, bool isDarkTheme)
{
    if (isDarkTheme) {
        dialog->setStyleSheet(
            "QDialog { background: #111827; color: #F9FAFB; }"
            "QLabel { color: #F9FAFB; font-size: 14px; }"
            "QLineEdit { background: #1F2937; color: #F9FAFB; border: 1px solid #374151; border-radius: 8px; padding: 10px; font-size: 14px; }"
            "QLineEdit:focus { border: 1px solid #60A5FA; }"
            "QPushButton { background: #374151; color: #F9FAFB; border: 1px solid #4B5563; border-radius: 8px; padding: 8px 16px; min-height: 40px; }"
            "QPushButton:hover { background: #4B5563; }"
            "QPushButton:pressed { background: #1F2937; }");
    } else {
        dialog->setStyleSheet(
            "QDialog { background: #FFFFFF; color: #111111; }"
            "QLabel { color: #111111; font-size: 14px; }"
            "QLineEdit { background: #FFFFFF; color: #111111; border: 1px solid #D1D5DB; border-radius: 8px; padding: 10px; font-size: 14px; }"
            "QLineEdit:focus { border: 1px solid #2563EB; }"
            "QPushButton { min-height: 40px; }");
    }
}
}

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

    QLabel *titleLabel = new QLabel("⚙️ 密钥配置");
    titleLabel->setStyleSheet("font-size: 28px; font-weight: bold; color: #F8FAFC;");
    mainLayout->addWidget(titleLabel);

    tabWidget = new QTabWidget();
    mainLayout->addWidget(tabWidget);

    QWidget *apiTab = new QWidget();
    setupApiKeyTab(apiTab);
    tabWidget->addTab(apiTab, "AI服务商密钥配置");
}

void ConfigWidget::setupApiKeyTab(QWidget *tab)
{
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(15);

    QVBoxLayout *contentLayout = createCenteredContentLayout(tab, layout, 15);

    QWidget *buttonRow = new QWidget();
    QHBoxLayout *buttonLayout = new QHBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    addButton = new QPushButton("➕ 添加密钥");
    addButton->setObjectName("primaryButton");
    addButton->setFixedHeight(40);
    addButton->setMinimumWidth(120);
    connect(addButton, &QPushButton::clicked, this, &ConfigWidget::addApiKey);

    QLabel *registerLabel = new QLabel(
        "<span style='font-size:13px;'>没有密钥？点击"
        "<a href=\"https://ai.kegeai.top/register?aff=78Gs\" style='color:#F97316; font-weight:700; font-size:15px; text-decoration:none;'>这里</a>"
        "注册👉</span>");
    registerLabel->setOpenExternalLinks(true);
    registerLabel->setTextFormat(Qt::RichText);

    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(registerLabel);
    buttonLayout->addStretch();
    contentLayout->addWidget(buttonRow);

    apiKeyTable = new QTableWidget();
    apiKeyTable->setObjectName("apiKeyTable");
    apiKeyTable->setColumnCount(5);
    apiKeyTable->setHorizontalHeaderLabels({"序号", "名称", "密钥", "操作", "查看"});
    setupTableStyle(apiKeyTable);
    contentLayout->addWidget(apiKeyTable);
}

double ConfigWidget::calculateScaleFactor()
{
    QScreen *screen = QApplication::primaryScreen();
    if (!screen) return 1.0;
    qreal dpi = screen->logicalDotsPerInch();
    double sf = dpi / 96.0;
    if (sf < 0.8) sf = 0.8;
    if (sf > 2.0) sf = 2.0;
    return sf;
}

void ConfigWidget::updateRowHeight()
{
    if (!apiKeyTable) return;

    int rowHeight = static_cast<int>(36 * scaleFactor) + 10 + 14;
    apiKeyTable->verticalHeader()->setDefaultSectionSize(rowHeight);
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

void ConfigWidget::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
        updateThemeStyles();
    }
}

void ConfigWidget::updateThemeStyles()
{
}

void ConfigWidget::updateColumnWidths()
{
    if (!apiKeyTable) return;

    const int actionColWidth = 160;
    const int lastColWidth = 100;

    apiKeyTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    apiKeyTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    apiKeyTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    apiKeyTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    apiKeyTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    apiKeyTable->setColumnWidth(3, actionColWidth);
    apiKeyTable->setColumnWidth(4, lastColWidth);
}

void ConfigWidget::setupTableStyle(QTableWidget *table)
{
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(false);
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QFont tableFont = table->font();
    tableFont.setPointSize(12);
    table->setFont(tableFont);
    table->horizontalHeader()->setFont(tableFont);
}

void ConfigWidget::loadApiKeys()
{
    apiKeyTable->setRowCount(0);
    QList<ApiKey> keys = DBManager::instance()->getAllApiKeys();

    QFont buttonFont;
    buttonFont.setPointSize(12);
    int buttonHeight = static_cast<int>(36 * scaleFactor);

    for (int i = 0; i < keys.size(); ++i) {
        const ApiKey& key = keys[i];
        apiKeyTable->insertRow(i);

        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(key.id));
        idItem->setTextAlignment(Qt::AlignCenter);
        apiKeyTable->setItem(i, 0, idItem);

        apiKeyTable->setItem(i, 1, new QTableWidgetItem(key.name));
        apiKeyTable->setItem(i, 2, new QTableWidgetItem(maskApiKey(key.apiKey)));

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

        QWidget *viewWidget = new QWidget();
        QHBoxLayout *viewLayout = new QHBoxLayout(viewWidget);
        viewLayout->setContentsMargins(5, 5, 5, 5);

        QPushButton *viewBtn = new QPushButton("👁");
        viewBtn->setFont(buttonFont);
        viewBtn->setMinimumHeight(buttonHeight);
        viewBtn->setProperty("keyId", key.id);
        viewBtn->setToolTip("查看完整密钥");
        connect(viewBtn, &QPushButton::clicked, this, &ConfigWidget::viewApiKey);

        viewLayout->addWidget(viewBtn);
        apiKeyTable->setCellWidget(i, 4, viewWidget);
    }
    updateColumnWidths();
    updateRowHeight();
}

QString ConfigWidget::maskApiKey(const QString& apiKey)
{
    if (apiKey.length() <= 7) return apiKey;
    return apiKey.left(3) + "***********" + apiKey.right(4);
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
    if (QMessageBox::question(this, "确认删除", "确定要删除这个密钥吗？",
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
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

// ApiKeyDialog
ApiKeyDialog::ApiKeyDialog(QWidget *parent, int keyId)
    : QDialog(parent), keyId(keyId)
{
    setupUI();
    if (keyId != -1) loadApiKey(keyId);
}

void ApiKeyDialog::setupUI()
{
    setWindowTitle(keyId == -1 ? "添加密钥" : "编辑密钥");
    setMinimumWidth(500);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QFormLayout *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    formLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    nameEdit = new QLineEdit();
    nameEdit->setPlaceholderText("例如: sora2密钥");

    apiKeyEdit = new QLineEdit();
    apiKeyEdit->setPlaceholderText("请输入 API Key");

    formLayout->addRow("名称:", nameEdit);
    formLayout->addRow("密钥:", apiKeyEdit);
    mainLayout->addLayout(formLayout);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    saveButton = new QPushButton("保存");
    saveButton->setFixedHeight(40);
    saveButton->setMinimumWidth(100);
    connect(saveButton, &QPushButton::clicked, this, &QDialog::accept);

    cancelButton = new QPushButton("取消");
    cancelButton->setFixedHeight(40);
    cancelButton->setMinimumWidth(100);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    buttonLayout->setAlignment(Qt::AlignCenter);
    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);

    applyKeyDialogTheme(this, isDarkThemeWidget(this));
}

void ApiKeyDialog::loadApiKey(int id)
{
    ApiKey key = DBManager::instance()->getApiKey(id);
    nameEdit->setText(key.name);
    apiKeyEdit->setText(key.apiKey);
}
