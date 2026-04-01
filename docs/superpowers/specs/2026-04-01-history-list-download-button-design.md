# AI视频生成-单个记录: 增加下载按钮及链接复制功能设计文档

## 1. 需求与目标
在“AI视频生成-单个记录”的生成历史记录列表中，为每一条记录的“操作”列增加一个“下载”按钮。用户点击该按钮后：
1. 会重新触发该记录所对应视频的下载流程；
2. 将该视频的 URL 链接放入系统剪切板中；
3. 在鼠标点击位置冒泡提示：“视频url链接已经在剪切板，你也可以粘贴到浏览器手动下载”。

## 2. 方案设计

### 2.1 UI 与交互设计 (VideoSingleHistoryTab)
- **新增按钮**：在现有的历史记录操作列中（例如：查看、浏览、重新下载、刷新、重新生成）新增“下载”按钮（`downloadBtn`）。
- **可用性控制**：仅当 `Task` 中存在有效的 `videoUrl` 且任务状态允许（例如已成功或至少返回了视频链接）时，“下载”按钮才可点击。
- **按钮布局调整**：因为操作列增加了一个按钮，需要确保整个水平布局（`QHBoxLayout`）不会因为拥挤而折叠。如果有必要，会稍微微调各按钮的最大/最小宽度。

### 2.2 核心逻辑实现
增加一个专用的槽函数处理下载按钮点击事件：
```cpp
void VideoSingleHistoryTab::onDownloadVideo(const QString& taskId)
{
    // 1. 根据taskId查询数据库，获取VideoTask
    VideoTask task = DBManager::instance()->getTaskById(taskId);
    if (task.videoUrl.isEmpty()) {
        QMessageBox::warning(this, "提示", "该记录没有可用的视频链接");
        return;
    }

    // 2. 写入系统剪切板
    QApplication::clipboard()->setText(task.videoUrl);

    // 3. 冒泡提示用户
    QToolTip::showText(QCursor::pos(), "视频url链接已经在剪切板，你也可以粘贴到浏览器手动下载");

    // 4. 重复已有的重新下载流程（如果有必要，可直接复用现有 onRetryDownload(taskId) 等逻辑）
    // 通过 TaskPollManager 重新触发下载覆盖本地视频
    // 注意：需要获取 apiKey 和 baseUrl 以正确发起带有 Authorization 头的请求
}
```

### 2.3 异常处理与边界
- 若任务生成失败、超时，或者由于旧任务没有保存 `videoUrl`，按钮将被禁用，或点击时弹窗提示“该记录没有可用的视频链接”。
- 对于正在下载或生成中的任务，可以配置为按钮可用（如果有 `videoUrl`），但是会避免与当前的 `TaskPollManager` 下载队列冲突。

## 3. 测试验证
1. 选取一个历史记录点击“下载”按钮。
2. 验证剪切板中是否成功写入了该视频 URL。
3. 验证是否看到冒泡提示。
4. 验证本地文件路径下，旧的视频是否被新的下载请求覆盖。