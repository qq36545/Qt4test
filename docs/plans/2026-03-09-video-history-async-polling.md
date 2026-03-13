# 视频历史异步轮询实现计划

## 概览
基于 [设计文档](./2026-03-09-video-history-async-polling-design.md) 实现异步轮询、4 tab 历史记录、自动下载、缩略图预览功能。

## 阶段 1: 数据库层 (database/)

### Task 1.1: 扩展 video_history 表
- [ ] 修改 `database/dbmanager.h` 添加新字段常量
- [ ] 修改 `database/dbmanager.cpp` 的 `initDatabase()` 添加 ALTER TABLE 语句
- [ ] 新增字段: task_id, video_path, download_status, video_url, progress, created_at, completed_at, thumbnail_path, task_type
- [ ] 测试: 启动程序验证表结构升级成功

### Task 1.2: 新增数据库操作方法
- [ ] `insertVideoTask()`: 插入新任务记录
- [ ] `updateTaskStatus()`: 更新任务状态和进度
- [ ] `updateVideoPath()`: 更新视频路径和缩略图
- [ ] `getTasksByType(type, offset, limit)`: 按类型分页查询任务列表
- [ ] `getTaskCount(type)`: 获取指定类型任务总数
- [ ] `getPendingTasks()`: 获取待轮询任务
- [ ] 测试: 单元测试验证 CRUD 操作

## 阶段 2: 轮询管理器 (network/)

### Task 2.1: 创建 TaskPollManager 单例
- [ ] 创建 `network/taskpollmanager.h`
- [ ] 创建 `network/taskpollmanager.cpp`
- [ ] 实现单例模式 `getInstance()`
- [ ] 添加 QTimer 成员 (10s 间隔)
- [ ] 添加 `QMap<QString, TaskInfo>` 跟踪任务
- [ ] 测试: 验证单例创建和销毁

### Task 2.2: 实现轮询核心逻辑
- [ ] `startPolling(taskId)`: 启动单个任务轮询
- [ ] `stopPolling(taskId)`: 停止单个任务轮询
- [ ] `onPollTimer()`: 定时器回调，批量查询所有待轮询任务
- [ ] 超时检测: 20 分钟无结果标记为 timeout
- [ ] 信号: `taskCompleted()`, `taskFailed()`, `taskTimeout()`
- [ ] 测试: 模拟 API 响应验证轮询逻辑

### Task 2.3: 实现自动下载
- [ ] `downloadVideo(taskId, videoUrl)`: 下载视频到 outputs/videos/veo3_one_by_one/
- [ ] 文件命名: veo3_{timestamp}.mp4
- [ ] 下载进度回调: `downloadProgress(taskId, progress)`
- [ ] 下载失败重试: 最多 5 次，失败后提示网络连接或具体错误原因
- [ ] 下载完成后更新数据库 video_path
- [ ] 生成缩略图: 使用 Qt Multimedia (QMediaPlayer + QVideoSink) 提取第一帧
- [ ] 缩略图保存: outputs/videos/veo3_one_by_one/thumbnails/veo3_{timestamp}_thumb.jpg
- [ ] 测试: 模拟下载验证文件保存和数据库更新

### Task 2.4: 程序重启恢复
- [ ] `recoverPendingTasks()`: 从数据库加载未完成任务
- [ ] 在 MainWindow 初始化时调用
- [ ] 过滤掉已超时任务
- [ ] 重新启动轮询
- [ ] 测试: 重启程序验证任务恢复

## 阶段 3: UI 层 - 历史记录重构 (widgets/)

### Task 3.1: 创建 VideoHistoryWidget (4 tab 容器)
- [ ] 重命名 `VideoHistoryTab` 为 `VideoHistoryWidget`
- [ ] 添加 QTabWidget 成员
- [ ] 创建 4 个子 tab: VideoSingleHistoryTab, VideoBatchHistoryTab, ImageSingleHistoryTab, ImageBatchHistoryTab
- [ ] 测试: 验证 4 个 tab 正常显示

### Task 3.2: 实现 VideoSingleHistoryTab (列表视图)
- [ ] 创建 QTableWidget: 列 [任务ID, 提示词, 状态, 进度, 创建时间, 完成时间, 操作]
- [ ] 加载数据: 调用 `getTasksByType("video_single", offset, limit=50)`
- [ ] 状态显示: pending(⏳), processing(🔄), completed(✅), failed(❌), timeout(⏱️)
- [ ] 操作按钮: [查看], [浏览], [重新查询]
- [ ] 分页加载: 每页 50 条，滚动到底部自动加载下一页
- [ ] 测试: 验证数据加载和按钮响应

### Task 3.3: 实现缩略图网格视图
- [ ] 添加视图切换按钮: [列表视图] / [缩略图视图]
- [ ] 创建 QScrollArea + QGridLayout
- [ ] 每个缩略图卡片: 图片 + 状态图标 + 提示词摘要
- [ ] 响应式布局: 根据窗口宽度自动调整列数
- [ ] 分页加载: 每页 20 条，滚动到底部自动加载下一页
- [ ] 点击卡片: 显示详情对话框
- [ ] 测试: 验证视图切换和响应式布局

### Task 3.4: 实现操作按钮功能
- [ ] [查看]: 播放视频 (QMediaPlayer)，缺失则下载并显示进度条
- [ ] [浏览]: 打开文件管理器并定位文件 (QDesktopServices::openUrl)
- [ ] [重新查询]: 手动触发轮询 (调用 TaskPollManager::startPolling)
- [ ] 测试: 验证每个按钮功能

## 阶段 4: 集成 VideoSingleTab

### Task 4.1: 修改生成逻辑
- [ ] 移除即时结果显示
- [ ] 创建任务后插入数据库: `insertVideoTask()`
- [ ] 启动轮询: `TaskPollManager::getInstance()->startPolling(taskId)`
- [ ] 显示提示: "生成结果在历史记录查看"
- [ ] 测试: 验证生成流程和数据库插入

### Task 4.2: 连接信号槽
- [ ] 连接 `TaskPollManager::taskCompleted` 到历史记录刷新
- [ ] 连接 `TaskPollManager::downloadProgress` 到进度更新
- [ ] 测试: 验证实时更新

## 阶段 5: CMakeLists 和构建

### Task 5.1: 更新 CMakeLists.txt
- [ ] 添加 Qt6::Multimedia 依赖
- [ ] 添加新文件: taskpollmanager.h/cpp
- [ ] 测试: 验证编译通过

## 阶段 6: 端到端测试

### Task 6.1: 功能测试
- [ ] 测试完整流程: 生成 -> 轮询 -> 下载 -> 查看
- [ ] 测试超时场景
- [ ] 测试失败重试 (5 次)
- [ ] 测试程序重启恢复
- [ ] 测试视图切换 (列表 <-> 缩略图)
- [ ] 测试响应式布局
- [ ] 测试分页加载 (列表 50 条/页, 缩略图 20 条/页)

### Task 6.2: 边界测试
- [ ] 网络断开场景
- [ ] API 返回错误
- [ ] 磁盘空间不足
- [ ] 并发多任务
- [ ] 缩略图生成失败

## 技术决策
1. **缩略图生成**: Qt Multimedia (QMediaPlayer + QVideoSink)
2. **下载重试**: 5 次，失败后显示网络连接或具体错误
3. **分页策略**:
   - 列表视图: 50 条/页
   - 缩略图视图: 20 条/页
   - 滚动到底部自动加载下一页
