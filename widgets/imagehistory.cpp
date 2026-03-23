#include "imagehistory.h"
#include "../database/dbmanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QDialog>
#include <QLabel>
#include <QScrollArea>
#include <QMenu>
#include <QContextMenuEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QUrl>
#include <QLineEdit>

// ==================== ImageSingleHistoryTab ====================

ImageSingleHistoryTab::ImageSingleHistoryTab(QWidget *parent)
    : QWidget(parent)
    , isListView(true)
    , currentTimeFilter(All)
{
    setupUI();
    loadHistory();
}

void ImageSingleHistoryTab::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(10);

    // ========== 工具栏 ==========
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(10);

    // 全选复选框
    headerCheckBox = new QCheckBox();
    headerCheckBox->setText("全选");
    headerCheckBox->setToolTip("全选所有记录");
    connect(headerCheckBox, &QCheckBox::checkStateChanged, this, &ImageSingleHistoryTab::onSelectAllChanged);
    toolbarLayout->addWidget(headerCheckBox);

    // 视图切换按钮
    switchViewButton = new QPushButton("🖼️ 缩略图视图");
    switchViewButton->setFixedWidth(130);
    connect(switchViewButton, &QPushButton::clicked, this, &ImageSingleHistoryTab::switchView);
    toolbarLayout->addWidget(switchViewButton);

    // 状态筛选
    statusCombo = new QComboBox();
    statusCombo->addItem("全部状态", "all");
    statusCombo->addItem("生成中", "generating");
    statusCombo->addItem("成功", "success");
    statusCombo->addItem("失败", "failed");
    statusCombo->setFixedWidth(100);
    toolbarLayout->addWidget(new QLabel("状态:"));
    toolbarLayout->addWidget(statusCombo);

    // 时间筛选
    timeFilterCombo = new QComboBox();
    timeFilterCombo->addItem("全部", All);
    timeFilterCombo->addItem("今天", Today);
    timeFilterCombo->addItem("本周", ThisWeek);
    timeFilterCombo->addItem("本月", ThisMonth);
    timeFilterCombo->setFixedWidth(90);
    connect(timeFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImageSingleHistoryTab::onTimeFilterChanged);
    toolbarLayout->addWidget(new QLabel("时间:"));
    toolbarLayout->addWidget(timeFilterCombo);

    toolbarLayout->addStretch();

    // 删除按钮
    deleteButton = new QPushButton("🗑️ 删除");
    deleteButton->setFixedWidth(80);
    connect(deleteButton, &QPushButton::clicked, this, &ImageSingleHistoryTab::onDeleteSelected);
    toolbarLayout->addWidget(deleteButton);

    mainLayout->addLayout(toolbarLayout);

    // ========== 主视图区域 ==========
    viewStack = new QStackedWidget();

    // 列表视图
    listViewWidget = new QWidget();
    QVBoxLayout *listLayout = new QVBoxLayout(listViewWidget);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(0);
    setupListView();
    listLayout->addWidget(historyTable);
    viewStack->addWidget(listViewWidget);

    // 缩略图视图
    thumbnailViewWidget = new QWidget();
    QVBoxLayout *thumbLayout = new QVBoxLayout(thumbnailViewWidget);
    thumbLayout->setContentsMargins(0, 0, 0, 0);
    thumbLayout->setSpacing(0);
    setupThumbnailView();
    thumbLayout->addWidget(thumbnailScrollArea);
    viewStack->addWidget(thumbnailViewWidget);

    mainLayout->addWidget(viewStack);
}

void ImageSingleHistoryTab::setupListView()
{
    historyTable = new QTableWidget();

    // 13列：选择、序号、任务ID、模型变体、提示词、参考图数、API密钥、服务器、尺寸、宽高比、状态、创建时间、操作
    historyTable->setColumnCount(13);
    historyTable->setHorizontalHeaderLabels({
        "选择", "序号", "任务ID", "模型变体", "提示词",
        "参考图数", "API密钥", "服务器", "尺寸", "宽高比",
        "状态", "创建时间", "操作"
    });

    // 设置列宽（按可见组件与文字宽度分配）
    historyTable->setColumnWidth(0, 48);   // 选择
    historyTable->setColumnWidth(1, 56);   // 序号
    historyTable->setColumnWidth(2, 120);  // 任务ID
    historyTable->setColumnWidth(3, 192);  // 模型变体（在当前基础上加倍）
    historyTable->setColumnWidth(4, 407);  // 提示词（继续从操作列转入30%宽度）
    historyTable->setColumnWidth(5, 70);   // 参考图数
    historyTable->setColumnWidth(6, 100);  // API密钥
    historyTable->setColumnWidth(7, 70);   // 服务器
    historyTable->setColumnWidth(8, 60);   // 尺寸
    historyTable->setColumnWidth(9, 70);   // 宽高比
    historyTable->setColumnWidth(10, 72);  // 状态
    historyTable->setColumnWidth(11, 140); // 创建时间
    historyTable->setColumnWidth(12, 73);  // 操作（再转出30%给提示词）

    historyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);      // 全部列支持拖拽调整列宽
    historyTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);    // 提示词列允许自由拖拽
    connect(historyTable->horizontalHeader(), &QHeaderView::sectionResized, this,
            [this](int logicalIndex, int, int) {
                if (logicalIndex != 12) {
                    absorbRightWhitespaceToActionColumn();
                }
            });

    // 隐藏列：任务ID、参考图数、API密钥、服务器、尺寸、宽高比、创建时间
    historyTable->setColumnHidden(2, true);
    historyTable->setColumnHidden(5, true);
    historyTable->setColumnHidden(6, true);
    historyTable->setColumnHidden(7, true);
    historyTable->setColumnHidden(8, true);
    historyTable->setColumnHidden(9, true);
    historyTable->setColumnHidden(11, true);

    historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    historyTable->setWordWrap(true);
    historyTable->setTextElideMode(Qt::ElideNone);
    historyTable->verticalHeader()->setVisible(false);
    historyTable->setAlternatingRowColors(false);
    historyTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    historyTable->verticalHeader()->setMinimumSectionSize(40);
    historyTable->setContextMenuPolicy(Qt::CustomContextMenu);

    // 表头全选复选框
    headerCheckBox = new QCheckBox();
    headerCheckBox->setText("");
    headerCheckBox->setToolTip("全选");
    headerCheckBox->setFixedHeight(25);
    headerCheckBox->setStyleSheet("QCheckBox { margin: 0 auto; }");
    connect(headerCheckBox, &QCheckBox::checkStateChanged, this, &ImageSingleHistoryTab::onSelectAllChanged);
}

void ImageSingleHistoryTab::setupThumbnailView()
{
    thumbnailScrollArea = new QScrollArea();
    thumbnailScrollArea->setWidgetResizable(true);
    thumbnailScrollArea->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    thumbnailContainer = new QWidget();
    thumbnailLayout = new QGridLayout(thumbnailContainer);
    thumbnailLayout->setSpacing(15);
    thumbnailLayout->setContentsMargins(5, 5, 5, 5);

    thumbnailScrollArea->setWidget(thumbnailContainer);
}

void ImageSingleHistoryTab::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    absorbRightWhitespaceToActionColumn();
}

void ImageSingleHistoryTab::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refreshHistory();
    absorbRightWhitespaceToActionColumn();
}

void ImageSingleHistoryTab::loadHistory()
{
    if (isListView) {
        loadListViewData();
    } else {
        loadThumbnailCards();
    }
}

void ImageSingleHistoryTab::loadListViewData()
{
    if (!historyTable || !statusCombo) return;

    historyTable->setRowCount(0);
    selectedIds.clear();

    QList<GenerationHistory> histories = DBManager::instance()->getGenerationHistoryByModelAndType("image", "single");

    int row = 0;
    for (const GenerationHistory &history : histories) {
        // 时间筛选
        if (currentTimeFilter != All) {
            QDateTime recordTime = history.createdAt;
            QDateTime now = QDateTime::currentDateTime();

            bool keep = false;
            switch (currentTimeFilter) {
                case Today:
                    keep = (recordTime.date() == now.date());
                    break;
                case ThisWeek:
                    keep = (recordTime.date().daysTo(now.date()) <= 7);
                    break;
                case ThisMonth:
                    keep = (recordTime.date().year() == now.date().year() &&
                            recordTime.date().month() == now.date().month());
                    break;
                default:
                    keep = true;
            }
            if (!keep) continue;
        }

        // 状态筛选
        QString statusFilter = statusCombo->currentData().toString();
        if (statusFilter != "all" && history.status != statusFilter) {
            continue;
        }

        historyTable->insertRow(row);

        // 解析 parameters JSON
        QString modelVariant, apiKeyName, serverUrl, imageSize, aspectRatio, referenceCount;
        QJsonParseError parseError;
        QJsonDocument paramsDoc = QJsonDocument::fromJson(history.parameters.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError && paramsDoc.isObject()) {
            QJsonObject obj = paramsDoc.object();
            modelVariant = obj.value("model").toString();
            apiKeyName = obj.value("apiKeyName").toString();
            serverUrl = obj.value("serverUrl").toString();
            imageSize = obj.value("imageSize").toString();
            aspectRatio = obj.value("aspectRatio").toString();
            referenceCount = obj.value("referenceCount").toString();
        }

        // 模型变体显示名称转换
        QString variantDisplay = modelVariant;
        if (modelVariant.contains("flash-image-preview")) {
            variantDisplay = "香蕉2";
        } else if (modelVariant.contains("pro-image-preview")) {
            variantDisplay = "香蕉Pro";
        } else if (modelVariant.contains("flash")) {
            variantDisplay = "香蕉1";
        }

        // 服务器显示名称
        QString serverDisplay = serverUrl.contains("back") ? "备用" : "主站";

        // 勾选框
        QWidget *checkBoxWidget = new QWidget();
        QHBoxLayout *checkBoxLayout = new QHBoxLayout(checkBoxWidget);
        checkBoxLayout->setContentsMargins(0, 0, 0, 0);
        checkBoxLayout->setAlignment(Qt::AlignCenter);
        QCheckBox *checkBox = new QCheckBox();
        checkBox->setProperty("historyId", history.id);
        connect(checkBox, &QCheckBox::checkStateChanged, this, &ImageSingleHistoryTab::onCheckBoxStateChanged);
        checkBoxLayout->addWidget(checkBox);
        historyTable->setCellWidget(row, 0, checkBoxWidget);

        // 序号
        historyTable->setItem(row, 1, new QTableWidgetItem(QString::number(row + 1)));

        // 任务ID
        QTableWidgetItem *taskIdItem = new QTableWidgetItem(QString::number(history.id));
        taskIdItem->setToolTip(QString::number(history.id));
        historyTable->setItem(row, 2, taskIdItem);

        // 模型变体
        historyTable->setItem(row, 3, new QTableWidgetItem(variantDisplay));

        // 提示词（换行显示）
        QTableWidgetItem *promptItem = new QTableWidgetItem(history.prompt);
        promptItem->setToolTip(history.prompt);
        historyTable->setItem(row, 4, promptItem);

        // 参考图数
        historyTable->setItem(row, 5, new QTableWidgetItem(referenceCount.isEmpty() ? "0" : referenceCount));

        // API密钥
        historyTable->setItem(row, 6, new QTableWidgetItem(apiKeyName));

        // 服务器
        historyTable->setItem(row, 7, new QTableWidgetItem(serverDisplay));

        // 尺寸
        historyTable->setItem(row, 8, new QTableWidgetItem(imageSize));

        // 宽高比
        historyTable->setItem(row, 9, new QTableWidgetItem(aspectRatio));

        // 状态
        QString statusText = history.status;
        if (history.status == "success") statusText = "✅ 成功";
        else if (history.status == "failed") statusText = "❌ 失败";
        else if (history.status == "generating") statusText = "🔄 生成中";
        QTableWidgetItem *statusItem = new QTableWidgetItem(statusText);
        historyTable->setItem(row, 10, statusItem);

        // 创建时间
        historyTable->setItem(row, 11, new QTableWidgetItem(
            history.createdAt.isValid() ? history.createdAt.toString("yyyy-MM-dd HH:mm") : history.date));

        // 操作按钮
        QWidget *btnWidget = new QWidget();
        QHBoxLayout *btnLayout = new QHBoxLayout(btnWidget);
        btnLayout->setContentsMargins(2, 4, 2, 4);
        btnLayout->setSpacing(4);

        QPushButton *viewBtn = new QPushButton("查看");
        QPushButton *browseBtn = new QPushButton("浏览");
        QPushButton *regenerateBtn = new QPushButton("重新生成");

        viewBtn->setMinimumHeight(30);
        browseBtn->setMinimumHeight(30);
        regenerateBtn->setMinimumHeight(30);

        viewBtn->setMaximumWidth(56);
        browseBtn->setMaximumWidth(56);
        regenerateBtn->setMaximumWidth(92);

        viewBtn->setEnabled(history.status == "success" && !history.resultPath.isEmpty());
        browseBtn->setEnabled(history.status == "success" && !history.resultPath.isEmpty());

        QString taskId = QString::number(history.id);
        connect(viewBtn, &QPushButton::clicked, [this, taskId]() { onViewImage(taskId); });
        connect(browseBtn, &QPushButton::clicked, [this, taskId]() { onBrowseFile(taskId); });
        connect(regenerateBtn, &QPushButton::clicked, [this, taskId]() { onRegenerate(taskId); });

        btnLayout->addWidget(viewBtn);
        btnLayout->addWidget(browseBtn);
        btnLayout->addWidget(regenerateBtn);
        historyTable->setCellWidget(row, 12, btnWidget);

        // 存储完整数据到行（使用 taskIdItem，即第2列）
        taskIdItem->setData(Qt::UserRole, history.id);
        taskIdItem->setData(Qt::UserRole + 1, QVariant::fromValue(history));

        row++;
    }

    historyTable->resizeRowsToContents();
    absorbRightWhitespaceToActionColumn();
    updateHeaderCheckBoxPosition();
}

void ImageSingleHistoryTab::absorbRightWhitespaceToActionColumn()
{
    if (!historyTable) return;

    const int actionCol = 12;
    if (historyTable->isColumnHidden(actionCol)) return;

    const int viewportWidth = historyTable->viewport()->width();
    if (viewportWidth <= 0) return;

    int visibleWidthSum = 0;
    for (int col = 0; col < historyTable->columnCount(); ++col) {
        if (!historyTable->isColumnHidden(col)) {
            visibleWidthSum += historyTable->columnWidth(col);
        }
    }

    const int rightWhitespace = viewportWidth - visibleWidthSum;
    if (rightWhitespace > 0) {
        historyTable->setColumnWidth(actionCol, historyTable->columnWidth(actionCol) + rightWhitespace);
    }
}

void ImageSingleHistoryTab::loadThumbnailCards()
{
    if (!thumbnailLayout) return;

    // 清空现有卡片
    QLayoutItem *child;
    while ((child = thumbnailLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            delete child->widget();
        }
        delete child;
    }

    QList<GenerationHistory> histories = DBManager::instance()->getGenerationHistoryByModelAndType("image", "single");

    int col = 0;
    int row = 0;
    const int maxCols = 4;

    for (const GenerationHistory &history : histories) {
        // 时间筛选
        if (currentTimeFilter != All) {
            QDateTime recordTime = history.createdAt;
            QDateTime now = QDateTime::currentDateTime();

            bool keep = false;
            switch (currentTimeFilter) {
                case Today:
                    keep = (recordTime.date() == now.date());
                    break;
                case ThisWeek:
                    keep = (recordTime.date().daysTo(now.date()) <= 7);
                    break;
                case ThisMonth:
                    keep = (recordTime.date().year() == now.date().year() &&
                            recordTime.date().month() == now.date().month());
                    break;
                default:
                    keep = true;
            }
            if (!keep) continue;
        }

        // 状态筛选
        QString statusFilter = statusCombo->currentData().toString();
        if (statusFilter != "all" && history.status != statusFilter) {
            continue;
        }

        // 创建卡片
        QWidget *card = new QWidget();
        card->setFixedSize(200, 260);
        card->setStyleSheet(R"(
            QWidget {
                background: rgba(30, 30, 40, 0.9);
                border: 1px solid rgba(255, 255, 255, 0.1);
                border-radius: 8px;
            }
        )");

        QVBoxLayout *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(8, 8, 8, 8);
        cardLayout->setSpacing(5);

        // 缩略图或占位符
        QLabel *thumbLabel = new QLabel();
        thumbLabel->setFixedSize(184, 140);
        thumbLabel->setAlignment(Qt::AlignCenter);
        thumbLabel->setStyleSheet("background: rgba(0,0,0,0.3); border-radius: 4px;");

        if (!history.resultPath.isEmpty() && QFileInfo(history.resultPath).exists()) {
            QPixmap pixmap(history.resultPath);
            if (!pixmap.isNull()) {
                thumbLabel->setPixmap(pixmap.scaled(184, 140, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        } else {
            thumbLabel->setText("🖼️");
            thumbLabel->setStyleSheet("background: rgba(0,0,0,0.3); border-radius: 4px; font-size: 48px;");
        }
        cardLayout->addWidget(thumbLabel);

        // 状态
        QString statusText = history.status;
        if (history.status == "success") statusText = "✅ 成功";
        else if (history.status == "failed") statusText = "❌ 失败";
        else if (history.status == "generating") statusText = "🔄 生成中";
        QLabel *statusLabel = new QLabel(statusText);
        statusLabel->setStyleSheet("color: #aaa; font-size: 12px;");
        cardLayout->addWidget(statusLabel);

        // 提示词摘要
        QString promptSummary = history.prompt;
        if (promptSummary.length() > 30) {
            promptSummary = promptSummary.left(30) + "...";
        }
        QLabel *promptLabel = new QLabel(promptSummary);
        promptLabel->setStyleSheet("color: #fff; font-size: 12px;");
        promptLabel->setWordWrap(true);
        promptLabel->setMaximumHeight(40);
        promptLabel->setToolTip(history.prompt);
        cardLayout->addWidget(promptLabel);

        cardLayout->addStretch();

        // 操作按钮
        QHBoxLayout *btnLayout = new QHBoxLayout();
        btnLayout->setSpacing(5);

        QPushButton *viewBtn = new QPushButton("查看");
        QPushButton *browseBtn = new QPushButton("浏览");

        viewBtn->setMaximumWidth(70);
        browseBtn->setMaximumWidth(70);

        viewBtn->setEnabled(history.status == "success" && !history.resultPath.isEmpty());
        browseBtn->setEnabled(history.status == "success" && !history.resultPath.isEmpty());

        QString taskId = QString::number(history.id);
        connect(viewBtn, &QPushButton::clicked, [this, taskId]() { onViewImage(taskId); });
        connect(browseBtn, &QPushButton::clicked, [this, taskId]() { onBrowseFile(taskId); });

        btnLayout->addWidget(viewBtn);
        btnLayout->addWidget(browseBtn);
        cardLayout->addLayout(btnLayout);

        thumbnailLayout->addWidget(card, row, col);

        col++;
        if (col >= maxCols) {
            col = 0;
            row++;
        }
    }
}

void ImageSingleHistoryTab::switchView()
{
    isListView = !isListView;
    viewStack->setCurrentWidget(isListView ? listViewWidget : thumbnailViewWidget);
    switchViewButton->setText(isListView ? "🖼️ 缩略图视图" : "📋 列表视图");
    loadHistory();
}

void ImageSingleHistoryTab::onViewImage(const QString& taskId)
{
    bool ok;
    int id = taskId.toInt(&ok);
    if (!ok) return;

    GenerationHistory history = DBManager::instance()->getGenerationHistory(id);
    if (history.resultPath.isEmpty() || !QFileInfo(history.resultPath).exists()) {
        QMessageBox::warning(this, "提示", "图片文件不存在");
        return;
    }

    // 创建预览对话框
    QDialog dialog(this);
    dialog.setWindowTitle("图片预览");
    dialog.setMinimumSize(800, 600);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *imageLabel = new QLabel();
    QPixmap pixmap(history.resultPath);
    if (!pixmap.isNull()) {
        imageLabel->setPixmap(pixmap.scaled(780, 520, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        imageLabel->setText("无法加载图片");
    }
    imageLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(imageLabel);

    QPushButton *closeBtn = new QPushButton("关闭");
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(closeBtn, 0, Qt::AlignCenter);

    dialog.exec();
}

void ImageSingleHistoryTab::onBrowseFile(const QString& taskId)
{
    bool ok;
    int id = taskId.toInt(&ok);
    if (!ok) return;

    GenerationHistory history = DBManager::instance()->getGenerationHistory(id);
    if (history.resultPath.isEmpty()) {
        QMessageBox::warning(this, "提示", "该记录暂无结果路径");
        return;
    }

    QFileInfo fileInfo(history.resultPath);
    if (!fileInfo.exists()) {
        QMessageBox::warning(this, "提示", "文件不存在: " + history.resultPath);
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absolutePath()));
}

void ImageSingleHistoryTab::onRegenerate(const QString& taskId)
{
    bool ok;
    int id = taskId.toInt(&ok);
    if (!ok) return;

    GenerationHistory history = DBManager::instance()->getGenerationHistory(id);

    // 解析 parameters JSON
    QString modelVariant, serverUrl, imageSize, aspectRatio, referencePathsStr;
    int apiKeyId = 0;
    QJsonParseError parseError;
    QJsonDocument paramsDoc = QJsonDocument::fromJson(history.parameters.toUtf8(), &parseError);
    if (parseError.error == QJsonParseError::NoError && paramsDoc.isObject()) {
        QJsonObject obj = paramsDoc.object();
        modelVariant = obj.value("model").toString();
        apiKeyId = obj.value("apiKeyId").toString().toInt();
        serverUrl = obj.value("serverUrl").toString();
        imageSize = obj.value("imageSize").toString();
        aspectRatio = obj.value("aspectRatio").toString();
        referencePathsStr = obj.value("referencePaths").toString();
    }

    // 解析参考图路径列表
    QStringList referencePaths;
    if (!referencePathsStr.isEmpty()) {
        referencePaths = referencePathsStr.split("|");
    }

    // 发射重新生成信号
    emit regenerateRequested(
        history.prompt,
        modelVariant,
        referencePaths,
        apiKeyId,
        serverUrl,
        imageSize,
        aspectRatio
    );
}

void ImageSingleHistoryTab::onDeleteSelected()
{
    if (selectedIds.isEmpty()) {
        QMessageBox::information(this, "提示", "请先选择要删除的记录");
        return;
    }

    int ret = QMessageBox::question(this, "确认删除",
        QString("确定要删除选中的 %1 条记录吗？").arg(selectedIds.size()),
        QMessageBox::Yes | QMessageBox::No);

    if (ret != QMessageBox::Yes) return;

    int deletedCount = 0;
    for (int id : selectedIds) {
        if (DBManager::instance()->deleteGenerationHistory(id)) {
            deletedCount++;
        }
    }

    QMessageBox::information(this, "完成", QString("已删除 %1 条记录").arg(deletedCount));
    selectedIds.clear();
    loadHistory();
}

void ImageSingleHistoryTab::onSelectAllChanged(int state)
{
    bool checked = (state == Qt::Checked);
    // 阻止行复选框的信号，避免触发 onCheckBoxStateChanged
    headerCheckBox->blockSignals(true);
    for (int row = 0; row < historyTable->rowCount(); ++row) {
        QWidget *cellWidget = historyTable->cellWidget(row, 0);
        if (cellWidget) {
            QCheckBox *checkBox = cellWidget->findChild<QCheckBox*>();
            if (checkBox && checkBox->isEnabled()) {
                checkBox->setChecked(checked);
            }
        }
    }
    // 更新选中状态
    selectedIds.clear();
    if (checked) {
        for (int row = 0; row < historyTable->rowCount(); ++row) {
            QWidget *cellWidget = historyTable->cellWidget(row, 0);
            if (cellWidget) {
                QCheckBox *checkBox = cellWidget->findChild<QCheckBox*>();
                if (checkBox && checkBox->isEnabled()) {
                    int id = checkBox->property("historyId").toInt();
                    if (id > 0) {
                        selectedIds.insert(id);
                    }
                }
            }
        }
    }
    headerCheckBox->blockSignals(false);
}

void ImageSingleHistoryTab::onCheckBoxStateChanged()
{
    selectedIds.clear();
    int checkedCount = 0;
    for (int row = 0; row < historyTable->rowCount(); ++row) {
        QWidget *cellWidget = historyTable->cellWidget(row, 0);
        if (cellWidget) {
            QCheckBox *checkBox = cellWidget->findChild<QCheckBox*>();
            if (checkBox && checkBox->isChecked()) {
                int id = checkBox->property("historyId").toInt();
                if (id > 0) {
                    selectedIds.insert(id);
                    checkedCount++;
                }
            }
        }
    }
    // 同步全选按钮状态（阻止信号避免触发 onSelectAllChanged）
    if (headerCheckBox) {
        headerCheckBox->blockSignals(true);
        if (checkedCount == 0) {
            headerCheckBox->setCheckState(Qt::Unchecked);
        } else if (checkedCount == historyTable->rowCount()) {
            headerCheckBox->setCheckState(Qt::Checked);
        } else {
            headerCheckBox->setCheckState(Qt::PartiallyChecked);
        }
        headerCheckBox->blockSignals(false);
    }
}

void ImageSingleHistoryTab::onTimeFilterChanged(int index)
{
    currentTimeFilter = static_cast<TimeFilter>(timeFilterCombo->itemData(index).toInt());
    loadHistory();
}

void ImageSingleHistoryTab::updateHeaderCheckBoxPosition()
{
    if (!headerCheckBox) return;
    // 勾选框位置由 Qt 自动管理
}

void ImageSingleHistoryTab::refreshHistory()
{
    loadHistory();
}
