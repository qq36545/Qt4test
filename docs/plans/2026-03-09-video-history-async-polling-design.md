# 视频生成异步轮询与历史记录重构设计

**日期**: 2026-03-09
**作者**: Claude Opus 4.6
**状态**: 已批准

---

## 1. 概述

### 1.1 目标

重构视频生成流程，实现异步任务轮询、自动下载、历史记录分类展示，提升用户体验。

### 1.2 核心需求

1. **异步轮询**: 提交任务后自动轮询状态，20 分钟超时
2. **程序重启恢复**: 保存 task_id，重启后恢复未完成任务
3. **自动下载**: 完成后自动下载到本地，显示 ✅ 标记
4. **历史记录重构**: 4 个 tab（视频单个/批量，图片单个/批量）
5. **双视图模式**: 列表视图 + 缩略图视图，响应式布局
6. **用户操作**: 查看、浏览、重新查询、取消

---

## 2. 数据库设计

### 2.1 表结构修改

**`generation_history` 表新增字段**:

| 字段 | 类型 | 说明 |
|------|------|------|
| `task_id` | TEXT | API 返回的任务 ID |
| `video_path` | TEXT | 本地视频路径 |
| `download_status` | TEXT | 下载状态: pending/downloading/completed/failed |
| `video_url` | TEXT | API 返回的视频 URL |
| `progress` | INTEGER | 生成进度 0-100 |
| `created_at` | INTEGER | 任务创建时间戳 |
| `completed_at` | INTEGER | 任务完成时间戳 |
| `thumbnail_path` | TEXT | 缩略图路径 |

### 2.2 状态定义

**status 字段值**:
- `queued`: 排队中
- `processing`: 生成中
- `completed`: 已完成
- `failed`: 失败
- `timeout`: 超时
- `cancelled`: 已取消

**download_status 字段值**:
- `pending`: 待下载
- `downloading`: 下载中
- `completed`: 已下载
- `failed`: 下载失败

### 2.3 数据流

```
提交任务 → 插入记录(status=queued, task_id=xxx)
    ↓
轮询查询 → 更新 status 和 progress
    ↓
完成 → 自动下载 → 更新 video_path 和 download_status
    ↓
程序重启 → 扫描 status=processing → 恢复轮询
```

---

## 3. UI 架构重构

### 3.1 历史记录页面结构

```
VideoHistoryWidget (替代原 VideoHistoryTab)
├── QTabWidget
    ├── VideoSingleHistoryTab   - AI视频生成-单个记录
    ├── VideoBatchHistoryTab    - AI视频生成-批量记录
    ├── ImageSingleHistoryTab   - AI图片生成-单个记录 (预留)
    └── ImageBatchHistoryTab    - AI图片生成-批量记录 (预留)
```

### 3.2 VideoSingleHistoryTab 布局

**顶部工具栏**:
```
[刷新] [列表视图/缩略图视图切换] [搜索框]
```

**列表视图** (QTableWidget):
```
| 序号 | 缩略图 | 提示词 | 模型 | 状态 | 进度 | 创建时间 | 操作 |
```

**缩略图视图** (QScrollArea + QGridLayout):
- 响应式布局:
  - < 800px: 1 列
  - 800-1200px: 2 列
  - 1200-1600px: 3 列
  - \> 1600px: 4 列
- 卡片尺寸: 280px 宽，高度自适应

**状态图标**:
- ✅ 已完成并下载
- 🔄 生成中（显示进度条）
- ⏱️ 排队中
- ❌ 失败
- ⏰ 超时
- 🚫 已取消

### 3.3 生成页面修改

**VideoSingleTab 变更**:
```cpp
previewLabel->setText("生成结果在历史记录查看");
previewLabel->setStyleSheet("color: #3B82F6; cursor: pointer;");
```

**提交任务后流程**:
1. 调用 API → 获得 task_id
2. 保存到数据库 (status=queued)
3. 启动轮询
4. 显示提示: "任务已提交，请到历史记录查看进度"
5. 清空表单，允许继续提交

---

## 4. 轮询与下载机制

### 4.1 TaskPollManager 类设计

```cpp
class TaskPollManager : public QObject {
    Q_OBJECT
public:
    void startPolling(int historyId, const QString &taskId,
                      const QString &apiKey, const QString &baseUrl);
    void stopPolling(int historyId);
    void recoverPendingTasks();

signals:
    void taskUpdated(int historyId, const QString &status, int progress);
    void taskCompleted(int historyId, const QString &videoUrl);
    void taskFailed(int historyId, const QString &error);
    void taskTimeout(int historyId);
    void downloadProgress(int historyId, int progress);
    void downloadCompleted(int historyId, const QString &path);

private:
    struct PollTask {
        int historyId;
        QString taskId;
        QTimer *timer;
        QDateTime startTime;
        int retryCount;
    };

    QMap<int, PollTask> activeTasks;
    Veo3API *api;
    QNetworkAccessManager *networkManager;
};
```

### 4.2 轮询流程

```
提交任务 → 启动 QTimer (10秒间隔)
    ↓
每 10 秒调用 GET /v1/videos/{id}
    ↓
收到响应 → 更新数据库 status 和 progress
    ↓
status=completed → 触发自动下载 → 停止轮询
    ↓
超过 20 分钟 → 标记 timeout → 停止轮询
```

### 4.3 自动下载机制

**下载流程**:
1. 构建本地路径: `outputs/videos/veo3_one_by_one/veo3_{timestamp}.mp4`
2. 确保目录存在
3. 使用 QNetworkAccessManager 下载
4. 显示下载进度
5. 保存文件
6. 更新数据库 video_path 和 download_status
7. 生成缩略图

**缩略图生成**:
```cpp
// 使用 Qt Multimedia 提取第一帧
QMediaPlayer *player = new QMediaPlayer();
QVideoSink *sink = new QVideoSink();
player->setVideoSink(sink);

connect(sink, &QVideoSink::videoFrameChanged, [=](const QVideoFrame &frame) {
    QImage image = frame.toImage();
    QPixmap thumbnail = QPixmap::fromImage(image)
        .scaled(280, 158, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    thumbnail.save(thumbPath, "JPEG", 85);
});
```

### 4.4 程序重启恢复

**启动时扫描未完成任务**:
```cpp
void TaskPollManager::recoverPendingTasks() {
    QList<GenerationHistory> pending = DBManager::instance()
        ->getPendingVideoTasks();  // status=queued/processing

    for (const auto &history : pending) {
        // 检查是否超时 (创建时间 > 20分钟)
        QDateTime createTime = QDateTime::fromSecsSinceEpoch(history.created_at);
        if (createTime.secsTo(QDateTime::currentDateTime()) > 20 * 60) {
            DBManager::instance()->updateTaskStatus(history.id, "timeout");
            continue;
        }

        // 恢复轮询
        startPolling(history.id, history.task_id,
                     history.apiKey, history.baseUrl);
    }
}
```

---

## 5. 用户交互与操作

### 5.1 操作按钮功能

**[查看] 按钮**:
- status=completed 且 video_path 存在 → 直接播放
- status=completed 但 video_path 不存在 → 先下载 (显示进度条) → 播放
- 其他状态 → 禁用

**[浏览] 按钮**:
- status=completed 且 video_path 存在 → 打开文件管理器并定位文件
- macOS: `open -R {path}`
- Windows: `explorer /select,{path}`
- Linux: 打开目录

**[重新查询] 按钮**:
- status=timeout/failed → 启用
- 点击后重新启动轮询 (重置 20 分钟计时器)
- 更新状态为 processing

**[取消] 按钮**:
- status=queued/processing → 启用
- 点击后停止轮询，标记状态为 cancelled

### 5.2 状态文案

| 状态 | 文案 |
|------|------|
| queued | ⏱️ 排队中... |
| processing | 🔄 生成中 (45%) |
| completed + downloaded | ✅ 已完成 |
| completed + not downloaded | ⬇️ 待下载 |
| failed | ❌ 生成失败: {原因} |
| timeout | ⏰ 超时，请重新查询 |
| cancelled | 🚫 已取消 |

### 5.3 进度提示

- 0-30%: "正在处理提示词..."
- 30-70%: "正在生成视频..."
- 70-100%: "正在渲染..."
- 下载中: "下载中 (2.5MB / 10MB)"

---

## 6. 错误处理与边界情况

### 6.1 网络错误

- API 调用失败 → 重试 3 次，间隔 5 秒
- 3 次失败后 → 暂停轮询，显示"网络错误，请点击重新查询"
- 不标记为 failed，保留重试机会

### 6.2 API 错误响应

- status=failed → 停止轮询，显示失败原因
- 401 Unauthorized → 提示"API 密钥无效"
- 429 Rate Limit → 延长轮询间隔到 30 秒
- 500 Server Error → 重试 3 次

### 6.3 文件系统错误

- 下载目录无写权限 → 提示用户，尝试使用临时目录
- 磁盘空间不足 → 提示用户清理空间
- 文件已存在 → 自动重命名 (添加 _1, _2 后缀)

### 6.4 边界情况

**程序异常退出**:
- 轮询中的任务 → 数据库已保存 task_id
- 重启后 → `recoverPendingTasks()` 自动恢复
- 超过 20 分钟 → 标记 timeout

**并发轮询限制**:
- 最多同时轮询 10 个任务
- 超过限制 → 新任务进入等待队列
- 有任务完成 → 自动启动等待队列中的任务

**视频文件损坏**:
- 下载完成后验证文件大小 > 0
- 尝试打开视频 → 失败则标记 download_status=failed
- 提供"重新下载"按钮

---

## 7. 技术实现要点

### 7.1 核心类

| 类名 | 职责 |
|------|------|
| `TaskPollManager` | 管理所有轮询任务，单例模式 |
| `VideoHistoryWidget` | 历史记录主容器，包含 4 个 tab |
| `VideoSingleHistoryTab` | 单个视频历史记录，支持列表/缩略图视图 |
| `VideoCardWidget` | 缩略图视图的单个卡片组件 |
| `DBManager` | 数据库操作，新增轮询相关方法 |

### 7.2 信号/槽连接

```cpp
// TaskPollManager → VideoSingleHistoryTab
connect(pollManager, &TaskPollManager::taskUpdated,
        historyTab, &VideoSingleHistoryTab::onTaskUpdated);

// VideoSingleTab → VideoHistoryWidget
connect(singleTab, &VideoSingleTab::switchToHistoryTab,
        mainWidget, &VideoGenWidget::switchToHistoryTab);
```

### 7.3 依赖模块

- Qt6::Network - 网络请求
- Qt6::Multimedia - 视频缩略图提取
- Qt6::Sql - 数据库操作
- QTimer - 轮询定时器

---

## 8. 实施计划

### 阶段 1: 数据库与核心逻辑
1. 修改数据库表结构
2. 实现 TaskPollManager 类
3. 实现自动下载机制
4. 实现程序重启恢复

### 阶段 2: UI 重构
1. 创建 VideoHistoryWidget 和 4 个 tab
2. 实现 VideoSingleHistoryTab 列表视图
3. 实现缩略图视图和响应式布局
4. 实现操作按钮功能

### 阶段 3: 集成与测试
1. 修改 VideoSingleTab 生成流程
2. 集成轮询管理器
3. 测试各种边界情况
4. 优化用户体验

---

## 9. 测试用例

### 9.1 正常流程
- [ ] 提交任务 → 自动轮询 → 完成 → 自动下载 → 显示 ✅
- [ ] 程序重启 → 恢复未完成任务 → 继续轮询
- [ ] 点击"查看" → 播放视频
- [ ] 点击"浏览" → 打开文件管理器并定位文件

### 9.2 异常情况
- [ ] 网络断开 → 重试 3 次 → 显示错误提示
- [ ] 20 分钟超时 → 标记 timeout → 显示"重新查询"按钮
- [ ] API 返回 failed → 显示失败原因
- [ ] 下载失败 → 标记 download_status=failed → 提供"重新下载"

### 9.3 边界情况
- [ ] 并发 10+ 任务 → 等待队列正常工作
- [ ] 磁盘空间不足 → 提示用户
- [ ] 文件已存在 → 自动重命名
- [ ] 视频文件损坏 → 标记失败，提供重新下载

---

## 10. 未来优化

1. **智能轮询间隔**: 前 5 分钟 10 秒，5-15 分钟 30 秒，15-20 分钟 60 秒
2. **批量下载**: 支持选中多个任务批量下载
3. **视频预览**: 鼠标悬停显示视频预览（GIF 或短视频）
4. **导出功能**: 导出历史记录为 CSV/JSON
5. **统计面板**: 显示总生成数、成功率、平均耗时等

---

**设计批准**: ✅
**批准日期**: 2026-03-09
**下一步**: 调用 writing-plans 技能创建实施计划
