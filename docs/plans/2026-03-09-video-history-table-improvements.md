# 视频历史记录表格改进实现计划

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 改进 VideoSingleHistoryTab 的表格显示，增加序号列、调整列宽、添加重新生成按钮

**Architecture:** 修改 VideoSingleHistoryTab 类的 setupListView() 和 loadHistory() 方法，调整表格列数、列宽和按钮布局

**Tech Stack:** Qt 5/6, C++, QTableWidget

---

## 修改点概览

1. 表格列数从 7 列增加到 8 列（新增序号列）
2. 调整列宽分配：提示词列固定宽度，操作列扩展
3. 操作列增加"重新生成"按钮
4. 修改按钮文案："重新查询" → "刷新"
5. 移除表格左右边距，100% 撑满

---

### Task 1: 修改表格列定义

**Files:**
- Modify: `widgets/videogen.cpp:1129-1148`

**Step 1: 修改列数和表头**

在 `VideoSingleHistoryTab::setupListView()` 中：

```cpp
historyTable = new QTableWidget();
historyTable->setColumnCount(8);  // 从 7 改为 8
historyTable->setHorizontalHeaderLabels({
    "序号", "任务ID", "提示词", "状态", "进度", "创建时间", "完成时间", "操作"
});
```

**Step 2: 调整列宽设置**

```cpp
// 序号列
historyTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
historyTable->setColumnWidth(0, 50);

// 任务ID列
historyTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
historyTable->setColumnWidth(1, 100);

// 提示词列 - 改为固定宽度
historyTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
historyTable->setColumnWidth(2, 200);

// 状态列
historyTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
historyTable->setColumnWidth(3, 80);

// 进度列
historyTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
historyTable->setColumnWidth(4, 80);

// 创建时间列
historyTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
historyTable->setColumnWidth(5, 150);

// 完成时间列
historyTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
historyTable->setColumnWidth(6, 150);

// 操作列 - 扩展宽度以容纳新按钮
historyTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Stretch);
```

**Step 3: 提交修改**

```bash
git add widgets/videogen.cpp
git commit -m "feat: 调整视频历史表格列定义和宽度"
```

---

### Task 2: 修改数据加载逻辑

**Files:**
- Modify: `widgets/videogen.cpp:1177-1249`

**Step 1: 在 loadHistory() 中添加序号列数据**

在 `VideoSingleHistoryTab::loadHistory()` 的循环中：

```cpp
int row = 0;
for (const VideoTask& task : tasks) {
    historyTable->insertRow(row);

    // 序号列（新增）
    historyTable->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));

    // 任务ID（列索引从 0 改为 1）
    historyTable->setItem(row, 1, new QTableWidgetItem(task.taskId.left(8) + "..."));

    // 提示词（列索引从 1 改为 2，字符数从 50 改为 30）
    QString promptPreview = task.prompt.left(30);
    if (task.prompt.length() > 30) promptPreview += "...";
    historyTable->setItem(row, 2, new QTableWidgetItem(promptPreview));

    // 状态（列索引从 2 改为 3）
    QString statusText;
    if (task.status == "pending") statusText = "⏳ 等待中";
    else if (task.status == "processing") statusText = "🔄 处理中";
    else if (task.status == "completed") statusText = "✅ 已完成";
    else if (task.status == "failed") statusText = "❌ 失败";
    else if (task.status == "timeout") statusText = "⏱️ 超时";
    historyTable->setItem(row, 3, new QTableWidgetItem(statusText));

    // 进度（列索引从 3 改为 4）
    historyTable->setItem(row, 4, new QTableWidgetItem(QString::number(task.progress) + "%"));

    // 创建时间（列索引从 4 改为 5）
    historyTable->setItem(row, 5, new QTableWidgetItem(task.createdAt.toString("yyyy-MM-dd HH:mm:ss")));

    // 完成时间（列索引从 5 改为 6）
    QString completedTime = task.completedAt.isValid() ?
        task.completedAt.toString("yyyy-MM-dd HH:mm:ss") : "-";
    historyTable->setItem(row, 6, new QTableWidgetItem(completedTime));

    // 操作按钮（列索引从 6 改为 7）
    // ... 按钮代码在下一个 task 中修改

    row++;
}
```

**Step 2: 提交修改**

```bash
git add widgets/videogen.cpp
git commit -m "feat: 添加序号列并调整提示词显示长度"
```

---

### Task 3: 修改操作列按钮

**Files:**
- Modify: `widgets/videogen.cpp:1218-1246`
- Modify: `widgets/videogen.h:128`

**Step 1: 添加重新生成按钮的槽函数声明**

在 `widgets/videogen.h` 的 `VideoSingleHistoryTab` 类中：

```cpp
private slots:
    void refreshHistory();
    void switchView();
    void onViewVideo(const QString& taskId);
    void onBrowseFile(const QString& taskId);
    void onRetryQuery(const QString& taskId);
    void onRegenerate(const QString& taskId);  // 新增
```

**Step 2: 修改操作列按钮布局**

在 `VideoSingleHistoryTab::loadHistory()` 中：

```cpp
// 操作按钮
QWidget *btnWidget = new QWidget();
QHBoxLayout *btnLayout = new QHBoxLayout(btnWidget);
btnLayout->setContentsMargins(5, 2, 5, 2);
btnLayout->setSpacing(5);

QPushButton *viewBtn = new QPushButton("查看");
QPushButton *browseBtn = new QPushButton("浏览");
QPushButton *refreshBtn = new QPushButton("刷新");  // 改名
QPushButton *regenerateBtn = new QPushButton("重新生成");  // 新增

viewBtn->setMaximumWidth(60);
browseBtn->setMaximumWidth(60);
refreshBtn->setMaximumWidth(60);  // 改名
regenerateBtn->setMaximumWidth(80);  // 新增

connect(viewBtn, &QPushButton::clicked, [this, task]() {
    onViewVideo(task.taskId);
});
connect(browseBtn, &QPushButton::clicked, [this, task]() {
    onBrowseFile(task.taskId);
});
connect(refreshBtn, &QPushButton::clicked, [this, task]() {
    onRetryQuery(task.taskId);
});
connect(regenerateBtn, &QPushButton::clicked, [this, task]() {
    onRegenerate(task.taskId);
});

btnLayout->addWidget(viewBtn);
btnLayout->addWidget(browseBtn);
btnLayout->addWidget(refreshBtn);
btnLayout->addWidget(regenerateBtn);  // 新增

historyTable->setCellWidget(row, 7, btnWidget);  // 列索引从 6 改为 7
```

**Step 3: 实现 onRegenerate 槽函数**

在 `widgets/videogen.cpp` 中添加：

```cpp
void VideoSingleHistoryTab::onRegenerate(const QString& taskId)
{
    // 获取任务信息
    QList<VideoTask> tasks = DBManager::instance()->getTasksByType("video_single");
    VideoTask targetTask;
    bool found = false;

    for (const VideoTask& task : tasks) {
        if (task.taskId == taskId) {
            targetTask = task;
            found = true;
            break;
        }
    }

    if (!found) {
        QMessageBox::warning(this, "错误", "未找到任务信息");
        return;
    }

    // 确认对话框
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "确认重新生成",
        "确定要使用相同的提示词重新生成视频吗？",
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        // TODO: 调用 API 重新生成视频
        // 这里需要根据实际的 API 调用逻辑来实现
        QMessageBox::information(this, "提示", "重新生成功能待实现");
    }
}
```

**Step 4: 提交修改**

```bash
git add widgets/videogen.h widgets/videogen.cpp
git commit -m "feat: 添加重新生成按钮并修改按钮文案"
```

---

### Task 4: 移除表格边距

**Files:**
- Modify: `widgets/videogen.cpp:1083-1084`

**Step 1: 修改 mainLayout 边距**

在 `VideoSingleHistoryTab::setupUI()` 中：

```cpp
QVBoxLayout *mainLayout = new QVBoxLayout(this);
mainLayout->setContentsMargins(0, 20, 0, 20);  // 左右边距改为 0
mainLayout->setSpacing(15);
```

**Step 2: 提交修改**

```bash
git add widgets/videogen.cpp
git commit -m "feat: 移除表格左右边距实现100%宽度"
```

---

### Task 5: 编译测试

**Step 1: 编译项目**

```bash
cd /Users/wuyingyi/Downloads/code/test2/demos/Qt4test
qmake
make
```

Expected: 编译成功，无错误

**Step 2: 运行程序测试**

```bash
./Qt4test
```

测试点：
1. 打开"生成历史记录"tab
2. 检查序号列是否正确显示（1, 2, 3...）
3. 检查提示词列是否只显示前30个字
4. 检查操作列是否有4个按钮：查看、浏览、刷新、重新生成
5. 检查表格是否100%宽度撑满
6. 点击"重新生成"按钮测试功能

**Step 3: 如果测试通过，创建最终提交**

```bash
git add -A
git commit -m "feat: 完成视频历史表格改进

- 添加序号列用于快速定位
- 调整提示词列宽度并限制显示30字
- 操作列增加重新生成按钮
- 修改按钮文案：重新查询→刷新
- 移除左右边距实现100%宽度"
```

---

## 潜在问题

1. **序号列的排序问题**：如果用户对表格进行排序，序号可能会不连续。建议禁用序号列的排序功能。

2. **重新生成功能的实现**：当前只是占位实现，需要根据实际的 API 调用逻辑来完成。

3. **列宽在不同屏幕尺寸下的表现**：固定宽度可能在小屏幕上显示不佳，建议测试不同分辨率。

4. **按钮宽度**：4个按钮可能会导致操作列过宽，可以考虑使用图标按钮或下拉菜单。

---

## 测试用例

1. **序号列测试**
   - 加载历史记录，检查序号是否从1开始递增
   - 刷新列表，检查序号是否重新计算

2. **提示词显示测试**
   - 短提示词（<30字）：完整显示
   - 长提示词（>30字）：显示前30字+省略号

3. **按钮功能测试**
   - 查看按钮：打开视频查看
   - 浏览按钮：打开文件浏览器
   - 刷新按钮：重新查询任务状态
   - 重新生成按钮：弹出确认对话框

4. **布局测试**
   - 检查表格是否100%宽度
   - 检查各列宽度是否符合预期
   - 调整窗口大小，检查表格自适应

---

## 执行选项

**Plan complete and saved to `docs/plans/2026-03-09-video-history-table-improvements.md`. Two execution options:**

**1. Subagent-Driven (this session)** - I dispatch fresh subagent per task, review between tasks, fast iteration

**2. Parallel Session (separate)** - Open new session with executing-plans, batch execution with checkpoints

**Which approach?**
