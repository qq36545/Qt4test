# History List Download Button Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在“AI视频生成-单个”历史记录列表中新增“下载”按钮，点击后复制视频URL到系统剪切板，弹出悬浮提示，并触发文件下载。

**Architecture:**
1. 在 `VideoSingleHistoryTab` 类定义中声明新的槽函数 `onDownloadVideo(const QString& taskId)`。
2. 在 `VideoSingleHistoryTab::loadHistory` 和 `VideoSingleHistoryTab::loadThumbnailCards` 方法的列表项 UI 生成中，新建 `QPushButton`（下载按钮），连接槽函数。
3. 调整包含所有操作按钮的容器（`QHBoxLayout`）及其它操作按钮的样式/宽度设定，保证不会挤压变形。
4. `onDownloadVideo` 槽函数负责通过 `TaskPollManager` 重发下载请求、调用 `QApplication::clipboard()` 写入系统剪贴板，使用 `QToolTip::showText` 冒泡反馈。

**Tech Stack:** Qt 6 (Widgets, Network, Core)

---

### Task 1: 定义 `onDownloadVideo` 槽函数

**Files:**
- Modify: `widgets/videogen.h`

- [ ] **Step 1: 在头文件中添加槽函数声明**

修改 `widgets/videogen.h` 的 `VideoSingleHistoryTab` 声明，在 private slots 中增加 `onDownloadVideo`：

```cpp
// 在 private slots: 区块增加
    void onDownloadVideo(const QString& taskId);
```

- [ ] **Step 2: 确认编译不报错**

Run: `cd build && cmake .. && make -j4`
Expected: 编译可能会因为缺少函数定义而报链接错，但语法应通过。

### Task 2: 实现 `onDownloadVideo` 的具体逻辑

**Files:**
- Modify: `widgets/videogen.cpp`

- [ ] **Step 1: 添加 `onDownloadVideo` 的函数实现**

在 `widgets/videogen.cpp` 中（例如放在 `onRetryDownload` 下方），加入如下实现代码，引入了处理所需的头文件（如果是已有的 `QApplication`, `QClipboard`, `QToolTip`, `QCursor` 请检查是否需在文件开头增加 `#include`，当前 `videogen.cpp` 已有多数头文件）：

```cpp
void VideoSingleHistoryTab::onDownloadVideo(const QString& taskId)
{
    // 获取当前选择的密钥（重发下载需要用到）
    if (!apiKeyCombo || apiKeyCombo->currentIndex() < 0) {
        QMessageBox::warning(this, "错误", "请先选择一个API密钥");
        return;
    }

    int apiKeyId = apiKeyCombo->currentData().toInt();
    if (apiKeyId <= 0) {
        QMessageBox::warning(this, "错误", "无效的API密钥");
        return;
    }

    ApiKey selectedKey = DBManager::instance()->getApiKey(apiKeyId);
    if (selectedKey.apiKey.isEmpty()) {
        QMessageBox::warning(this, "错误", "未找到选择的API密钥");
        return;
    }

    QString baseUrl = serverCombo->currentData().toString();
    if (baseUrl.isEmpty()) {
        QMessageBox::warning(this, "错误", "请选择服务器");
        return;
    }

    // 根据taskId查询获取VideoTask
    VideoTask task = DBManager::instance()->getTaskById(taskId);
    if (task.videoUrl.isEmpty()) {
        QMessageBox::warning(this, "提示", "该记录没有可用的视频链接");
        return;
    }

    // 写入系统剪切板
    QApplication::clipboard()->setText(task.videoUrl);

    // 冒泡提示用户
    QToolTip::showText(QCursor::pos(), "视频url链接已经在剪切板，你也可以粘贴到浏览器手动下载");

    // 复用 TaskPollManager 将任务推入下载流程覆盖本地视频文件
    TaskPollManager::getInstance()->triggerDownload(task.taskId, task.videoUrl, selectedKey.apiKey, baseUrl, task.taskType);
}
```

- [ ] **Step 2: 验证编译并确保相关头文件存在**

Run: `cd build && cmake .. && make -j4`
Expected: 成功编译。

### Task 3: 列表视图 (List View) 添加“下载”按钮

**Files:**
- Modify: `widgets/videogen.cpp:loadHistory` 约1600行左右

- [ ] **Step 1: 增加并配置新按钮**

在 `VideoSingleHistoryTab::loadHistory(int offset, int limit)` 函数内部（处理 `isListView == true` 的循环部分），为操作列创建一个新的 `QPushButton`，并调整部分宽度保证对齐。

```cpp
// 找到原有的按钮创建部分，并修改为：
            QPushButton *viewBtn = new QPushButton("查看");
            QPushButton *browseBtn = new QPushButton("浏览");
            QPushButton *downloadBtn = new QPushButton("下载");
            QPushButton *retryDownloadBtn = new QPushButton("重新下载");
            QPushButton *refreshBtn = new QPushButton("刷新");
            QPushButton *regenerateBtn = new QPushButton("重新生成");

            viewBtn->setMaximumWidth(60);
            browseBtn->setMaximumWidth(60);
            downloadBtn->setMinimumWidth(60);
            downloadBtn->setMaximumWidth(60);
            retryDownloadBtn->setMinimumWidth(80);
            retryDownloadBtn->setMaximumWidth(80);
            refreshBtn->setMaximumWidth(60);
            regenerateBtn->setMinimumWidth(110);
            regenerateBtn->setMaximumWidth(110);

            // ...（保留原有的按钮 setEnabled 等设置逻辑）...

            // 为新增的 downloadBtn 设置状态：只有当能拿到 task.videoUrl 时才可用，如果不可用给它加个Tooltip
            bool canDownload = !task.videoUrl.trimmed().isEmpty();
            downloadBtn->setEnabled(canDownload);
            if (!canDownload) {
                downloadBtn->setToolTip("该任务尚未提供视频链接");
            }

            // ...（保留原有的 connect 逻辑）...

            connect(downloadBtn, &QPushButton::clicked, [this, task]() {
                onDownloadVideo(task.taskId);
            });

            btnLayout->addWidget(viewBtn);
            btnLayout->addWidget(browseBtn);
            btnLayout->addWidget(downloadBtn);
            if (showRetryDownload) {
                btnLayout->addWidget(retryDownloadBtn);
            }
            btnLayout->addWidget(refreshBtn);
            btnLayout->addWidget(regenerateBtn);
```

- [ ] **Step 2: 编译测试**

Run: `cd build && make -j4`
Expected: 成功编译无错误。

### Task 4: 缩略图视图 (Thumbnail View) 添加“下载”按钮

**Files:**
- Modify: `widgets/videogen.cpp:loadHistory` 处理 `isListView == false` 的部分

- [ ] **Step 1: 缩略图卡片新增按钮**

在 `loadHistory` 处理卡片的循环中，添加按钮：

```cpp
            // 在原有的卡片底部 btnLayout 新增下载按钮
            QPushButton* viewBtn = new QPushButton("查看");
            QPushButton* browseBtn = new QPushButton("浏览");
            QPushButton* downloadBtn = new QPushButton("下载");

            viewBtn->setMaximumWidth(70);
            browseBtn->setMaximumWidth(70);
            downloadBtn->setMaximumWidth(70);

            viewBtn->setEnabled(displayState.canPlay);
            browseBtn->setEnabled(displayState.canBrowse);
            bool canDownloadCard = !task.videoUrl.trimmed().isEmpty();
            downloadBtn->setEnabled(canDownloadCard);

            connect(viewBtn, &QPushButton::clicked, [this, task]() {
                onViewVideo(task.taskId);
            });
            connect(browseBtn, &QPushButton::clicked, [this, task]() {
                onBrowseFile(task.taskId);
            });
            connect(downloadBtn, &QPushButton::clicked, [this, task]() {
                onDownloadVideo(task.taskId);
            });

            QHBoxLayout* btnLayout = new QHBoxLayout();
            btnLayout->setSpacing(5);
            btnLayout->addWidget(viewBtn);
            btnLayout->addWidget(browseBtn);
            btnLayout->addWidget(downloadBtn);

            cardLayout->addLayout(btnLayout);
```

- [ ] **Step 2: Commit All Changes**

Run: `git add widgets/videogen.h widgets/videogen.cpp docs/superpowers/plans/2026-04-01-history-list-download-button.md && git commit -m "feat: add download button to history list with clipboard URL copy and local retry"`

---
