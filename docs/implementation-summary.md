# 视频历史异步轮询功能实现总结

## 实现日期
2026-03-09

## 功能概述
实现了视频生成任务的异步轮询、自动下载、历史记录管理功能。

## 已完成功能

### 1. 数据库层
- ✅ 创建 `video_history` 表
- ✅ 字段：task_id, task_type, prompt, status, progress, video_url, video_path, thumbnail_path, download_status, created_at, completed_at
- ✅ CRUD 方法：
  - `insertVideoTask()` - 插入任务
  - `updateTaskStatus()` - 更新状态和进度
  - `updateVideoPath()` - 更新视频路径
  - `getTasksByType()` - 分页查询
  - `getTaskById()` - 根据 ID 查询
  - `getTaskCount()` - 统计数量
  - `getPendingTasks()` - 获取待轮询任务

### 2. 轮询管理器 (TaskPollManager)
- ✅ 单例模式
- ✅ 10 秒轮询间隔
- ✅ 20 分钟超时检测
- ✅ 自动下载视频
- ✅ 下载重试机制（最多 5 次）
- ✅ 程序重启恢复
- ✅ 信号：taskCompleted, taskFailed, taskTimeout, downloadProgress

### 3. UI 层
- ✅ VideoHistoryWidget - 4 tab 容器
  - AI视频生成-单个记录
  - AI生成视频-批量记录（占位）
  - AI生成图片-单个记录（占位）
  - AI生成图片-批量记录（占位）

- ✅ VideoSingleHistoryTab - 单个视频历史
  - 列表视图：显示任务 ID、提示词、状态、进度、时间、操作按钮
  - 缩略图视图：4 列网格布局，显示缩略图、状态图标、提示词摘要
  - 视图切换按钮
  - 刷新按钮

- ✅ 操作按钮：
  - 查看：打开视频文件（支持未下载提示）
  - 浏览：打开文件管理器并定位文件（macOS/Windows/Linux）
  - 重新查询：提示需要 API 信息

### 4. 集成
- ✅ VideoSingleTab 改为异步模式
- ✅ 任务创建后立即插入数据库
- ✅ 启动 TaskPollManager 轮询
- ✅ 提示用户到历史记录查看
- ✅ 清空输入表单

### 5. 下载机制
- ✅ 自动下载到 `outputs/videos/veo3_one_by_one/`
- ✅ 文件命名：`veo3_{timestamp}.mp4`
- ✅ 下载失败重试 5 次
- ✅ 显示错误信息和网络检查提示
- ✅ 下载进度回调

### 6. 程序重启恢复
- ✅ MainWindow 初始化时调用 `recoverPendingTasks()`
- ✅ 自动恢复未完成任务
- ✅ 过滤超时任务

## 待完善功能

### 1. 缩略图生成
- ⏳ 使用 Qt Multimedia 提取视频第一帧
- ⏳ 保存到 `outputs/videos/veo3_one_by_one/thumbnails/`

### 2. 分页加载
- ⏳ 列表视图：滚动到底部加载更多（50 条/页）
- ⏳ 缩略图视图：滚动到底部加载更多（20 条/页）

### 3. API 信息持久化
- ⏳ 存储 API Key 和 baseUrl 到任务记录
- ⏳ 支持重新查询功能

### 4. 其他优化
- ⏳ 批量视频历史记录
- ⏳ 图片生成历史记录
- ⏳ 任务删除功能
- ⏳ 导出历史记录

## 技术栈
- Qt 6
- C++17
- SQLite
- QNetworkAccessManager
- QTimer
- Qt Multimedia（待集成）

## 文件清单

### 新增文件
- `network/taskpollmanager.h` - 轮询管理器头文件
- `network/taskpollmanager.cpp` - 轮询管理器实现
- `docs/plans/2026-03-09-video-history-async-polling-design.md` - 设计文档
- `docs/plans/2026-03-09-video-history-async-polling.md` - 实现计划

### 修改文件
- `database/dbmanager.h` - 添加 VideoTask 结构体和方法
- `database/dbmanager.cpp` - 实现数据库操作
- `widgets/videogen.h` - 添加新类定义
- `widgets/videogen.cpp` - 实现 UI 和逻辑
- `mainwindow.cpp` - 添加恢复机制
- `CMakeLists.txt` - 添加新文件和依赖

## 测试建议

### 基本流程测试
1. 启动程序
2. 生成视频任务
3. 检查历史记录是否显示
4. 等待轮询完成
5. 检查视频是否自动下载
6. 测试操作按钮

### 边界测试
1. 网络断开场景
2. API 返回错误
3. 下载失败重试
4. 超时场景（20 分钟）
5. 程序重启恢复

### 视图测试
1. 列表/缩略图视图切换
2. 刷新功能
3. 响应式布局

## 已知问题
1. 缩略图生成功能未实现（显示占位符）
2. 分页加载未实现（一次加载所有记录）
3. 重新查询功能需要 API 信息持久化
4. 下载进度未在 UI 显示

## 后续计划
1. 实现缩略图生成
2. 实现分页加载
3. 完善其他 3 个历史记录 tab
4. 添加任务管理功能（删除、导出）
5. 优化 UI 交互体验
