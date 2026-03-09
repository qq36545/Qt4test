# Qt4test 项目开发记录

## 项目概述
鸡你太美 AI 图片/视频批量生成器 - 基于 Qt6 的桌面应用程序

---

## 2026-03-09 优化记录

### 1. DPI 感知的字体缩放系统

**问题描述**：
- UI 界面上的 Tab 标签和文本框、按钮等组件的文字太小
- 在高分辨率屏幕（4K/Retina）上显示效果差
- 所有字体大小都是硬编码的固定像素值，不会根据屏幕自适应

**解决方案**：
- 在 `MainWindow` 中实现 DPI 检测和字体缩放系统
- 添加 `calculateScaleFactor()` 方法：
  - 检测屏幕逻辑 DPI
  - 基准 DPI 为 96（Windows 标准）
  - 缩放因子限制在 0.8-2.0 之间
- 添加 `setupFonts()` 方法：
  - 设置应用全局基础字体 12pt
  - 字体会根据 DPI 自动缩放

**修改文件**：
- `mainwindow.h` - 添加方法声明
- `mainwindow.cpp` - 实现字体缩放逻辑

**技术要点**：
- 使用 `QScreen::logicalDotsPerInch()` 获取 DPI
- 使用 `QApplication::setFont()` 设置全局字体
- 使用 pt 单位而非 px，pt 会根据 DPI 自动缩放

---

### 2. UI 组件尺寸优化

**问题描述**：
- 下拉列表框（QComboBox）高度不合理，可能太长
- 按钮高度不统一，缺乏层次感
- 代码中存在大量硬编码的 `setFixedHeight()` 和 `setMinimumHeight()`

**解决方案**：
- 建立统一的组件尺寸规范：
  - **主按钮**（生成按钮）：48px，字体 15pt
  - **普通按钮**：36px，字体 13pt
  - **小按钮**（添加、刷新）：32px，字体 12pt
  - **下拉框**：最大高度 36px，字体 13pt
  - **文本框**：字体 13pt，padding 12px

- 移除代码中的硬编码高度：
  - `videogen.cpp` - 移除 4 处固定高度设置
  - 让组件根据字体大小和 QSS 样式自适应

**修改文件**：
- `styles/glassmorphism.qss` - 添加组件尺寸规范
- `styles/light.qss` - 同步明亮主题样式
- `widgets/videogen.cpp` - 移除硬编码高度

**技术要点**：
- 使用 `max-height` 和 `min-height` 设置合理范围
- 使用 QSS 选择器区分不同类型的按钮
- 使用 `QPushButton[text^="🚀"]` 匹配特定按钮

---

### 3. QTabWidget 样式优化

**问题描述**：
- Tab 标签字体太小，不清晰
- QSS 中没有 QTabWidget/QTabBar 样式定义
- Tab 使用系统默认样式，与整体设计不协调

**解决方案**：
- 添加完整的 QTabWidget/QTabBar 样式：
  - Tab 字体：16pt（从 14pt 增加）
  - 选中态：玫瑰红高亮 `rgba(225, 29, 72, 0.2)`
  - 悬停态：半透明白色
  - 圆角：8px
  - 内边距：10px 20px

- 实现动态宽度调整：
  - 所有 tab 总宽度占窗口宽度的 70%
  - 每个 tab 平分这个宽度
  - 窗口大小变化时自动调整

**修改文件**：
- `styles/glassmorphism.qss` - 添加 Tab 样式（暗黑主题）
- `styles/light.qss` - 添加 Tab 样式（明亮主题）
- `widgets/videogen.h/cpp` - 实现动态宽度调整
- `widgets/imagegen.h/cpp` - 实现动态宽度调整

**技术要点**：
- 重写 `resizeEvent()` 监听窗口大小变化
- 使用 `tabWidget->tabBar()->setStyleSheet()` 动态设置样式
- 计算公式：`tabWidth = (windowWidth * 0.7) / tabCount`

---

### 4. 主题切换功能

**问题描述**：
- 应用只有暗黑主题，缺少明亮主题选项
- 用户无法根据环境光线或个人偏好切换主题

**解决方案**：
- 实现完整的主题切换系统：
  - 添加主题切换按钮（侧边栏底部）
  - 按钮图标：🌙（暗黑模式）/ ☀️（明亮模式）
  - 点击按钮即时切换主题

- 创建明亮主题样式表：
  - 背景：白色/浅灰渐变
  - 文字：深灰/黑色
  - 强调色：玫瑰红（与暗黑主题保持一致）
  - 轻微阴影效果

- 实现主题持久化：
  - 使用 `QSettings` 保存主题偏好
  - 应用启动时自动加载上次的主题

**修改文件**：
- `mainwindow.h` - 添加主题相关声明
- `mainwindow.cpp` - 实现主题切换逻辑
- `styles/light.qss` - 创建明亮主题样式表（新文件）
- `resources.qrc` - 添加新样式表到资源

**技术要点**：
- 使用枚举 `enum Theme { Dark, Light }` 管理主题状态
- `toggleTheme()` - 切换主题并更新图标
- `loadTheme()` - 从 QSettings 加载主题
- `saveTheme()` - 保存主题到 QSettings
- `applyStyles()` - 根据主题加载对应的 QSS 文件

**QSettings 配置**：
```cpp
QSettings settings("ChickenAI", "Theme");
settings.setValue("currentTheme", static_cast<int>(currentTheme));
```

---

## 关键经验总结

### 1. Qt DPI 缩放最佳实践
- **使用 pt 单位而非 px**：pt 会根据 DPI 自动缩放
- **检测逻辑 DPI**：使用 `QScreen::logicalDotsPerInch()`
- **限制缩放范围**：避免极端缩放导致 UI 异常
- **全局字体设置**：使用 `QApplication::setFont()` 统一管理

### 2. QSS 样式管理
- **分离主题文件**：每个主题独立的 QSS 文件
- **使用语义化选择器**：`QPushButton[text^="🚀"]` 匹配特定按钮
- **避免硬编码尺寸**：使用 max/min-height 设置范围
- **动态样式注入**：使用 `setStyleSheet()` 运行时修改样式

### 3. 响应式 UI 设计
- **重写 resizeEvent()**：监听窗口大小变化
- **动态计算布局**：根据窗口尺寸调整组件
- **避免固定宽度**：使用百分比或动态计算
- **初始化调用**：在 `setupUI()` 后立即调用 `updateTabWidths()`

### 4. 主题系统设计
- **枚举管理状态**：清晰的主题状态管理
- **持久化偏好**：使用 QSettings 保存用户选择
- **即时切换**：无需重启应用即可切换主题
- **图标同步更新**：主题切换时更新按钮图标

### 5. 代码组织原则
- **单一职责**：每个方法只做一件事
- **避免硬编码**：使用常量或配置文件
- **统一规范**：建立组件尺寸规范并严格遵守
- **可维护性**：清晰的命名和注释

---

## 待优化项目

### 高优先级
- [ ] 实现图片/视频生成的实际功能（当前只是 UI 框架）
- [ ] 添加 API 密钥管理功能
- [ ] 实现历史记录的数据库存储和查询
- [ ] 添加生成进度显示和取消功能

### 中优先级
- [ ] 优化明亮主题的对比度（确保所有文字清晰可读）
- [ ] 添加更多主题选项（如高对比度主题）
- [ ] 实现批量生成的队列管理
- [ ] 添加生成结果的预览和导出功能

### 低优先级
- [ ] 添加键盘快捷键支持
- [ ] 实现拖放上传图片功能
- [ ] 添加生成参数的预设模板
- [ ] 优化应用启动速度

---

## 技术栈

- **框架**：Qt 6.x
- **语言**：C++ 17
- **构建工具**：CMake
- **数据库**：SQLite（通过 Qt SQL 模块）
- **样式**：QSS（Qt Style Sheets）

---

## 项目结构

```
Qt4test/
├── mainwindow.h/cpp          # 主窗口（侧边栏、主题切换）
├── widgets/
│   ├── videogen.h/cpp        # 视频生成页面
│   ├── imagegen.h/cpp        # 图片生成页面
│   ├── configwidget.h/cpp    # 配置页面
│   └── aboutwidget.h/cpp     # 关于页面
├── database/
│   └── dbmanager.h/cpp       # 数据库管理
├── styles/
│   ├── glassmorphism.qss     # 暗黑主题样式
│   └── light.qss             # 明亮主题样式
└── resources/
    └── logo.png              # 应用图标
```

---

## Git 提交记录

### 2026-03-09
- `f3400b4` - Implement DPI-aware font scaling and optimize UI component sizes
  - 实现 DPI 感知的字体缩放系统
  - 优化 UI 组件尺寸规范
  - 添加 QTabWidget 样式
  - 移除硬编码的组件高度

---

## 参考资料

- [Qt Documentation - High DPI Displays](https://doc.qt.io/qt-6/highdpi.html)
- [Qt Style Sheets Reference](https://doc.qt.io/qt-6/stylesheet-reference.html)
- [QSettings Documentation](https://doc.qt.io/qt-6/qsettings.html)
- [Qt Widget Classes](https://doc.qt.io/qt-6/widget-classes.html)

---

**最后更新**：2026-03-09
**维护者**：Claude Sonnet 4.6

---

## 2026-03-09 下午 - Veo3 模型全面支持与 API 集成

### 5. Veo3 模型变体扩展

**问题描述**：
- 原有只支持 2 个 Veo3 模型变体（veo_3_1 和 veo_3_1-fast）
- 无法满足不同场景的视频生成需求
- 缺少 4K、多图垫图等高级功能

**解决方案**：
- 扩展到 18 个 Veo3 模型变体
- 实现基于模型特性的动态 UI 适配
- 添加服务器选择功能（主站/备用）

**模型分类**：
1. **components 模型**（6个）：
   - veo_3_1-fast-components-4K
   - veo3.1-fast-components
   - veo3.1-components
   - veo3.1-components-4k
   - 特性：支持 1-3 张首帧图片多图垫图

2. **frames 模型**（3个）：
   - veo3-pro-frames
   - veo3-fast-frames
   - veo3-frames
   - 特性：仅支持单张首帧图片垫图

3. **标准模型**（9个）：
   - veo_3_1-fast, veo_3_1, veo3.1-fast, veo3.1, veo3, veo3-fast, veo3-pro
   - veo_3_1-fast-4K, veo3.1-4k, veo3.1-pro-4k
   - 特性：支持首尾帧垫图

**技术实现**：
```cpp
void VideoSingleTab::onModelVariantChanged(int index) {
    QString modelName = modelVariantCombo->currentData().toString();
    updateImageUploadUI(modelName);
    updateResolutionOptions(modelName.contains("4K") || modelName.contains("4k"));
}

void VideoSingleTab::updateImageUploadUI(const QString &modelName) {
    bool isComponents = modelName.contains("components");
    bool isFrames = modelName.contains("frames");

    if (isComponents) {
        imageLabel->setText("首帧图片（1-3张）:");
        endFrameWidget->setVisible(false);
    } else if (isFrames) {
        imageLabel->setText("首帧图片（单张）:");
        endFrameWidget->setVisible(false);
    } else {
        imageLabel->setText("首帧图片:");
        endFrameWidget->setVisible(true);
    }
}
```

**修改文件**：
- `widgets/videogen.h` - 添加新的槽函数和辅助方法
- `widgets/videogen.cpp` - 实现动态 UI 逻辑
- `docs/veo3-api.md` - 添加 Veo3 API 文档

**Git Commit**: `8325dd1` - Add comprehensive Veo3 model support with 18 variants and dynamic UI

---

### 6. 服务器选择与 API 集成

**问题描述**：
- 缺少实际的 API 调用功能
- 无法切换不同的 API 服务器
- 视频生成只是演示版本

**解决方案**：

#### 6.1 服务器选择
- 添加"请求服务器"下拉框
- 主站：`https://ai.kegeai.top`（默认）
- 备用：`https://api.kuai.host`

#### 6.2 Veo3 API 封装
创建 `Veo3API` 类封装所有 API 调用：

```cpp
class Veo3API : public QObject {
    Q_OBJECT
public:
    // 创建视频生成任务
    void createVideo(const QString &apiKey, const QString &baseUrl,
                     const QString &model, const QString &prompt,
                     const QStringList &imagePaths, const QString &size,
                     const QString &seconds, bool watermark);

    // 查询任务状态
    void queryTask(const QString &apiKey, const QString &baseUrl,
                   const QString &taskId);

    // 下载视频
    void downloadVideo(const QString &apiKey, const QString &baseUrl,
                       const QString &taskId, const QString &savePath);

signals:
    void videoCreated(const QString &taskId, const QString &status);
    void taskStatusUpdated(const QString &taskId, const QString &status,
                           const QString &videoUrl, int progress);
    void errorOccurred(const QString &error);
};
```

#### 6.3 multipart/form-data 上传
```cpp
QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

// 添加文本参数
QHttpPart modelPart;
modelPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                    QVariant("form-data; name=\"model\""));
modelPart.setBody(model.toUtf8());
multiPart->append(modelPart);

// 添加图片文件
for (const QString &imagePath : imagePaths) {
    QFile *file = new QFile(imagePath);
    file->open(QIODevice::ReadOnly);

    QHttpPart imagePart;
    imagePart.setHeader(QNetworkRequest::ContentTypeHeader, "image/jpeg");
    imagePart.setHeader(QNetworkRequest::ContentDispositionHeader,
        QVariant(QString("form-data; name=\"input_reference\"; filename=\"%1\"")
                 .arg(QFileInfo(imagePath).fileName())));
    imagePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(imagePart);
}

QNetworkReply *reply = networkManager->post(request, multiPart);
multiPart->setParent(reply);  // 自动内存管理
```

**新增文件**：
- `network/veo3api.h` - API 类定义
- `network/veo3api.cpp` - API 实现

**修改文件**：
- `CMakeLists.txt` - 添加 Qt6::Network 模块和 network 目录
- `widgets/videogen.h` - 添加 Veo3API 实例和回调方法
- `widgets/videogen.cpp` - 集成 API 调用

**Git Commits**:
- `f2501e4` - Implement multi-image upload and Veo3 API integration
- `cc8ef37` - Update main server URL to https://ai.kegeai.top

---

### 7. 多图上传功能

**问题描述**：
- components 模型需要支持 1-3 张图片
- 原有实现只支持单张图片
- 缺少图片数量验证

**解决方案**：
- 将 `uploadedImagePath` (QString) 改为 `uploadedImagePaths` (QStringList)
- 添加图片数量验证逻辑
- 实现多图信息显示

**技术实现**：
```cpp
void VideoSingleTab::uploadImage() {
    QString modelName = modelVariantCombo->currentData().toString();
    bool isComponents = modelName.contains("components");

    if (isComponents && uploadedImagePaths.size() >= 3) {
        QMessageBox::warning(this, "提示", "components 模型最多支持 3 张图片");
        return;
    }

    bool isFrames = modelName.contains("frames");
    if (isFrames && uploadedImagePaths.size() >= 1) {
        QMessageBox::warning(this, "提示", "frames 模型只支持单张图片");
        return;
    }

    QString fileName = QFileDialog::getOpenFileName(this, "选择首帧图片", "",
                                                    "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif)");
    if (!fileName.isEmpty()) {
        uploadedImagePaths.append(fileName);
        updateImagePreview();
    }
}
```

**Git Commit**: `f2501e4` - Implement multi-image upload and Veo3 API integration

---

### 8. UI 滚动支持与缩略图预览

**问题描述**：
- 内容过多时无法完整显示，缺少滚动条
- 图片上传后只显示文件名，无法预览
- 无法点击图片区域重新上传
- 提示词文本框高度不够

**解决方案**：

#### 8.1 添加滚动区域
```cpp
void VideoSingleTab::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // 创建滚动区域
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    // 创建内容容器
    QWidget *contentWidget = new QWidget();
    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(20, 20, 20, 20);

    // ... 添加所有组件到 contentLayout ...

    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);
}
```

#### 8.2 显示图片缩略图
```cpp
void VideoSingleTab::updateImagePreview() {
    if (uploadedImagePaths.isEmpty()) {
        imagePreviewLabel->setText("未选择图片\n点击此处上传");
        imagePreviewLabel->setPixmap(QPixmap());
    } else {
        // 显示第一张图片的缩略图
        QPixmap pixmap(uploadedImagePaths[0]);
        if (!pixmap.isNull()) {
            QPixmap scaledPixmap = pixmap.scaled(200, 120,
                                                  Qt::KeepAspectRatio,
                                                  Qt::SmoothTransformation);
            imagePreviewLabel->setPixmap(scaledPixmap);
            imagePreviewLabel->setText("");
        }
    }
}
```

#### 8.3 点击重新上传
```cpp
// 安装事件过滤器
imagePreviewLabel->installEventFilter(this);
imagePreviewLabel->setCursor(Qt::PointingHandCursor);

// 实现事件过滤器
bool VideoSingleTab::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress) {
        if (obj == imagePreviewLabel) {
            uploadImage();
            return true;
        } else if (obj == endFramePreviewLabel) {
            uploadEndFrameImage();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}
```

#### 8.4 UI 尺寸调整
- 提示词文本框：
  - 单个生成：100px → 200px
  - 批量生成：150px → 300px
- 图片预览区域：60px → 120px
- 添加"点击此处上传"提示文本

**修改文件**：
- `widgets/videogen.h` - 添加 `eventFilter()` 方法声明
- `widgets/videogen.cpp` - 实现滚动区域、缩略图、事件过滤器
- 包含头文件：`<QPixmap>`, `<QEvent>`, `<QMouseEvent>`, `<QScrollArea>`

**Git Commit**: `e82417f` - Add scrollable UI and image thumbnail preview

---

### 9. 视频时长规格修正

**问题描述**：
- 原有时长选项为 8秒、10秒、15秒
- 根据 veo3-api.md 文档，Veo3 API 只支持 8 秒

**解决方案**：
- 修改时长下拉框为固定 8 秒
- 禁用下拉框选择
- 显示"8秒（固定）"提示

```cpp
durationCombo->addItem("8秒（固定）", "8");
durationCombo->setEnabled(false);  // VEO3 固定 8 秒
```

**Git Commit**: `8325dd1` - Add comprehensive Veo3 model support with 18 variants and dynamic UI

---

## 技术要点总结

### Qt Network 编程
1. **QNetworkAccessManager**：
   - 单例模式管理网络请求
   - 使用信号/槽处理异步响应
   - 自动内存管理（parent-child 机制）

2. **multipart/form-data 上传**：
   - 使用 `QHttpMultiPart` 构建表单
   - 文件上传需要设置 `ContentDispositionHeader`
   - 将 QFile 设置为 QHttpPart 的 parent 确保生命周期

3. **内存管理**：
   ```cpp
   QFile *file = new QFile(path);
   file->setParent(multiPart);  // multiPart 删除时自动删除 file
   multiPart->setParent(reply);  // reply 删除时自动删除 multiPart
   ```

### 事件处理
1. **事件过滤器**：
   - 用于拦截和处理特定对象的事件
   - 安装：`widget->installEventFilter(this)`
   - 实现：重写 `eventFilter(QObject*, QEvent*)`
   - 返回 true 表示事件已处理，不再传递

2. **鼠标事件**：
   - `QEvent::MouseButtonPress` - 鼠标按下
   - 设置光标：`setCursor(Qt::PointingHandCursor)`

### 图片处理
1. **缩略图生成**：
   ```cpp
   QPixmap pixmap(imagePath);
   QPixmap scaled = pixmap.scaled(width, height,
                                   Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
   ```

2. **QLabel 显示图片**：
   - `setPixmap()` - 显示图片
   - `setScaledContents()` - 是否缩放内容
   - `setAlignment()` - 对齐方式

### 滚动区域
1. **QScrollArea 使用**：
   ```cpp
   QScrollArea *scrollArea = new QScrollArea();
   scrollArea->setWidgetResizable(true);  // 自动调整大小
   scrollArea->setFrameShape(QFrame::NoFrame);  // 无边框
   scrollArea->setWidget(contentWidget);
   ```

2. **布局嵌套**：
   - 外层：QVBoxLayout（包含 QScrollArea）
   - 中层：QScrollArea（包含 contentWidget）
   - 内层：contentWidget + QVBoxLayout（包含所有组件）

---

## API 接口文档

### Veo3 API 规格

#### 1. 创建视频任务
```
POST /v1/videos
Content-Type: multipart/form-data
Authorization: Bearer {API_KEY}

参数：
- model: 模型名称 (veo_3_1, veo_3_1-fast, etc.)
- prompt: 提示词
- seconds: 时长 (固定为 "8")
- input_reference: 图片文件 (可多张)
- size: 分辨率
  * 标准：1280x720 (横屏), 720x1280 (竖屏)
  * 4K：3840x2160 (横屏), 2160x3840 (竖屏)
- watermark: 是否添加水印 (true/false)

响应：
{
    "id": "video_xxx",
    "object": "video",
    "model": "veo_3_1",
    "status": "queued",
    "progress": 0,
    "created_at": 1762336916,
    "seconds": "8",
    "size": "16x9"
}
```

#### 2. 查询任务状态
```
GET /v1/videos/{id}
Authorization: Bearer {API_KEY}

响应：
{
    "id": "video_xxx",
    "status": "completed",  // queued, processing, completed, failed
    "video_url": "https://...",
    "progress": 100
}
```

#### 3. 下载视频
```
GET /v1/videos/{id}/content
Authorization: Bearer {API_KEY}

响应：视频文件二进制数据
```

---

## 待优化项目（更新）

### 高优先级
- [x] 实现 Veo3 API 集成
- [x] 添加多图上传支持
- [x] 实现图片缩略图预览
- [x] 添加滚动条支持
- [ ] 实现任务进度轮询
- [ ] 实现视频下载功能
- [ ] 添加错误重试机制

### 中优先级
- [ ] 实现 CSV 批量导入
- [ ] 添加图片拖拽上传
- [ ] 实现多图管理（删除、排序）
- [ ] 添加图片预览放大功能
- [ ] 优化网络请求超时处理
- [ ] 添加上传进度显示

### 低优先级
- [ ] 实现图片缩略图缓存
- [ ] 添加视频预览功能
- [ ] 实现生成参数预设
- [ ] 添加批量下载功能

---

## Git 提交记录（更新）

### 2026-03-09 下午
- `e82417f` - Add scrollable UI and image thumbnail preview
  - 添加 QScrollArea 滚动支持
  - 实现图片缩略图显示（200x120）
  - 支持点击图片区域重新上传
  - 提示词文本框高度加倍

- `cc8ef37` - Update main server URL to https://ai.kegeai.top
  - 修改主站 URL

- `f2501e4` - Implement multi-image upload and Veo3 API integration
  - 实现多图上传（1-3张）
  - 创建 Veo3API 类
  - 集成真实 API 调用
  - 添加 Qt Network 模块

- `8325dd1` - Add comprehensive Veo3 model support with 18 variants and dynamic UI
  - 扩展到 18 个 Veo3 模型变体
  - 实现动态 UI 适配
  - 添加服务器选择功能
  - 添加 4K 分辨率支持
  - 修正时长为固定 8 秒

---

## 项目结构（更新）

```
Qt4test/
├── mainwindow.h/cpp          # 主窗口（侧边栏、主题切换）
├── widgets/
│   ├── videogen.h/cpp        # 视频生成页面（已完善）
│   ├── imagegen.h/cpp        # 图片生成页面
│   ├── configwidget.h/cpp    # 配置页面
│   └── aboutwidget.h/cpp     # 关于页面
├── database/
│   └── dbmanager.h/cpp       # 数据库管理
├── network/                   # 【新增】网络请求模块
│   └── veo3api.h/cpp         # Veo3 API 封装
├── styles/
│   ├── glassmorphism.qss     # 暗黑主题样式
│   └── light.qss             # 明亮主题样式
├── docs/                      # 【新增】文档目录
│   └── veo3-api.md           # Veo3 API 文档
└── resources/
    ├── logo*.png             # 应用图标（多尺寸）
    ├── app.icns              # macOS 图标
    └── app.ico               # Windows 图标
```

---

## 2026-03-09 晚间 - 主题适配与 UI 优化

### 10. 提示词输入框高度调整

**问题描述**：
- 提示词输入框高度过高（单个 200px，批量 300px）
- 占用过多屏幕空间
- 用户希望更紧凑的布局

**解决方案**：
- 统一调整为 120px
- QTextEdit 自带滚动条支持
- 文字超出时自动显示垂直滚动条

**修改文件**：
- `widgets/videogen.cpp` - 两处 `setMinimumHeight()` 修改

**技术要点**：
- QTextEdit 默认支持多行输入和滚动
- 设置 `setMinimumHeight()` 限制初始高度
- 内容超出时自动显示滚动条

---

### 11. 明亮主题字体颜色修复

**问题描述**：
- 明亮主题下，白色字体在白色背景上看不清
- videogen.cpp 中硬编码了大量白色字体 `color: #F8FAFC`
- 硬编码样式覆盖了 QSS 文件的主题设置

**解决方案**：

#### 11.1 移除硬编码颜色
移除所有 QLabel、QCheckBox 的硬编码 `color` 属性：
```cpp
// 修改前
modelLabel->setStyleSheet("color: #F8FAFC; font-size: 14px;");

// 修改后
modelLabel->setStyleSheet("font-size: 14px;");
```

#### 11.2 使用 objectName + QSS 管理
为特殊组件设置 objectName，在 QSS 中统一管理：
```cpp
imagePreviewLabel->setObjectName("imagePreviewLabel");
imageDropArea->setObjectName("imageDropArea");
previewLabel->setObjectName("videoPreviewLabel");
```

#### 11.3 QSS 主题适配
**暗色主题**（glassmorphism.qss）：
```css
#imagePreviewLabel {
    background: rgba(30, 27, 75, 0.9);
    border: 1px solid rgba(248, 250, 252, 0.1);
    color: #94A3B8;  /* 浅灰色 */
}

#imagePreviewLabel[hasImage="true"] {
    background: rgba(34, 197, 94, 0.15);
    border: 1px solid rgba(34, 197, 94, 0.4);
    color: #22C55E;  /* 绿色 */
}
```

**明亮主题**（light.qss）：
```css
#imagePreviewLabel {
    background: #F8FAFC;  /* 浅灰白 */
    border: 1px solid rgba(15, 23, 42, 0.2);
    color: #64748B;  /* 深灰色 */
}

#imagePreviewLabel[hasImage="true"] {
    background: rgba(34, 197, 94, 0.1);
    border: 1px solid rgba(34, 197, 94, 0.5);
    color: #15803D;  /* 深绿色 */
}
```

**修改文件**：
- `widgets/videogen.cpp` - 移除所有硬编码颜色，添加 objectName
- `styles/glassmorphism.qss` - 添加预览组件样式
- `styles/light.qss` - 添加预览组件样式

**技术要点**：
1. **分离关注点**：颜色由 QSS 管理，C++ 只管逻辑
2. **属性选择器**：使用 `[hasImage="true"]` 实现状态样式
3. **动态刷新**：使用 `unpolish()` + `polish()` 强制刷新样式

---

### 12. 暗色主题组件背景加深

**问题描述**：
- QComboBox、QTextEdit 等组件背景透明度过高（0.6）
- 底层浅色背景透出，导致白色字体看不清
- 用户反馈"视频模型"等标签文字无法看到

**解决方案**：
增加所有输入组件的背景不透明度：

```css
/* 修改前 */
QComboBox {
    background: rgba(30, 27, 75, 0.6);  /* 60% 不透明 */
}

QTextEdit {
    background: rgba(30, 27, 75, 0.6);
}

QPushButton {
    background: rgba(248, 250, 252, 0.05);  /* 5% 不透明 */
}

/* 修改后 */
QComboBox {
    background: rgba(30, 27, 75, 0.95);  /* 95% 不透明 */
}

QTextEdit {
    background: rgba(30, 27, 75, 0.95);
}

QPushButton {
    background: rgba(30, 27, 75, 0.9);  /* 90% 不透明 */
}
```

**修改文件**：
- `styles/glassmorphism.qss` - 调整所有组件背景透明度

**效果**：
- 深色背景几乎不透明，白色字体清晰可见
- 保持轻微透明度，维持玻璃态美学

---

### 13. Tab 背景色统一修复

**问题描述**：
- "AI视频生成-单个"和"AI视频生成-批量"tab 背景是白色/浅灰色
- "生成历史记录"tab 背景是深色
- 三个 tab 背景不一致，前两个 tab 的白色文字看不清

**根本原因**：
- VideoSingleTab 和 VideoBatchTab 使用了 QScrollArea
- QScrollArea 及其 viewport 有默认的白色背景
- 覆盖了 QTabWidget::pane 的深色背景

**解决方案**：

#### 13.1 在 QSS 中设置 QScrollArea 透明
**暗色主题**（glassmorphism.qss）：
```css
QScrollArea {
    background: transparent;
    border: none;
}

QScrollArea > QWidget > QWidget {
    background: transparent;  /* viewport 透明 */
}
```

**明亮主题**（light.qss）：
```css
QScrollArea {
    background: transparent;
    border: none;
}

QScrollArea > QWidget > QWidget {
    background: transparent;
}
```

#### 13.2 移除 C++ 中的硬编码样式
```cpp
// 修改前
scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");
contentWidget->setStyleSheet("background: transparent;");

// 修改后
// 不设置任何样式，让 QSS 统一管理
```

**修改文件**：
- `styles/glassmorphism.qss` - 添加 QScrollArea 透明样式
- `styles/light.qss` - 添加 QScrollArea 透明样式
- `widgets/videogen.cpp` - 移除硬编码样式

**技术要点**：
1. **QScrollArea 结构**：
   - QScrollArea（外层容器）
   - viewport（中间层，实际显示内容的区域）
   - contentWidget（内容组件）

2. **CSS 选择器**：
   - `QScrollArea` - 选择 QScrollArea 本身
   - `QScrollArea > QWidget > QWidget` - 选择 viewport

3. **样式优先级**：
   - QSS 文件 < C++ setStyleSheet()
   - 统一在 QSS 中管理，避免优先级混乱

**效果**：
- 三个 tab 背景色完全一致
- 都继承 QTabWidget::pane 的深色背景 `rgba(30, 27, 75, 0.3)`
- 所有文字清晰可见

---

### 14. 滚动条样式优化

**问题描述**：
- 明亮主题下滚动条使用半透明背景，不够明显
- 滚动条滑块颜色对比度不足

**解决方案**：
改用实色背景，提高对比度：

**明亮主题**（light.qss）：
```css
/* 修改前 */
QScrollBar:vertical {
    background: rgba(241, 245, 249, 0.5);  /* 半透明 */
}

QScrollBar::handle:vertical {
    background: rgba(148, 163, 184, 0.5);  /* 半透明 */
}

/* 修改后 */
QScrollBar:vertical {
    background: #E2E8F0;  /* 实色浅灰 */
}

QScrollBar::handle:vertical {
    background: #94A3B8;  /* 实色中灰 */
}

QScrollBar::handle:vertical:hover {
    background: #64748B;  /* 实色深灰 */
}
```

**修改文件**：
- `styles/light.qss` - 滚动条样式优化

**效果**：
- 滚动条在明亮主题下清晰可见
- hover 时颜色加深，交互反馈明确

---

## 关键经验总结（更新）

### 6. 主题适配最佳实践

#### 6.1 颜色管理原则
1. **分离关注点**：
   - C++ 代码：只管逻辑和布局
   - QSS 文件：统一管理所有颜色和样式
   - 避免在 C++ 中硬编码颜色

2. **主题文件结构**：
   ```
   styles/
   ├── glassmorphism.qss  # 暗色主题
   └── light.qss          # 明亮主题
   ```

3. **颜色对比度**：
   - 暗色主题：深色背景 + 浅色文字
   - 明亮主题：浅色背景 + 深色文字
   - 确保 WCAG AA 级别对比度（至少 4.5:1）

#### 6.2 组件样式管理
1. **使用 objectName**：
   ```cpp
   widget->setObjectName("uniqueName");
   ```
   ```css
   #uniqueName {
       background: #color;
   }
   ```

2. **属性选择器**：
   ```cpp
   widget->setProperty("state", "active");
   widget->style()->unpolish(widget);
   widget->style()->polish(widget);
   ```
   ```css
   #widget[state="active"] {
       background: #activeColor;
   }
   ```

3. **避免硬编码**：
   ```cpp
   // ❌ 错误
   label->setStyleSheet("color: #FFFFFF; font-size: 14px;");

   // ✅ 正确
   label->setStyleSheet("font-size: 14px;");  // 颜色由 QSS 管理
   ```

#### 6.3 透明度处理
1. **QScrollArea 透明**：
   - 必须同时设置 QScrollArea 和 viewport 透明
   - 使用 CSS 选择器：`QScrollArea > QWidget > QWidget`

2. **背景透明度选择**：
   - 完全透明：`transparent` 或 `rgba(0,0,0,0)`
   - 半透明：`rgba(r,g,b,0.3-0.5)` - 用于玻璃态效果
   - 几乎不透明：`rgba(r,g,b,0.9-0.95)` - 确保文字可读

3. **透明度调试**：
   - 如果文字看不清，逐步增加背景不透明度
   - 从 0.6 → 0.8 → 0.9 → 0.95 → 1.0

#### 6.4 样式优先级
```
内联样式 (setStyleSheet) > QSS 文件 > 默认样式
```

**最佳实践**：
- 全局样式：QSS 文件
- 动态样式：setStyleSheet() + objectName
- 避免混用，优先使用 QSS 文件

---

### 7. QScrollArea 使用要点

#### 7.1 基本设置
```cpp
QScrollArea *scrollArea = new QScrollArea();
scrollArea->setWidgetResizable(true);  // 关键：自动调整大小
scrollArea->setFrameShape(QFrame::NoFrame);  // 去掉边框

QWidget *contentWidget = new QWidget();
scrollArea->setWidget(contentWidget);
```

#### 7.2 背景透明
**方法1：QSS 文件**（推荐）
```css
QScrollArea {
    background: transparent;
    border: none;
}

QScrollArea > QWidget > QWidget {
    background: transparent;
}
```

**方法2：C++ 代码**
```cpp
scrollArea->setStyleSheet("QScrollArea { background: transparent; }");
scrollArea->viewport()->setStyleSheet("background: transparent;");
```

#### 7.3 常见问题
1. **内容不滚动**：
   - 检查 `setWidgetResizable(true)` 是否设置
   - 检查 contentWidget 的布局是否正确

2. **背景不透明**：
   - 必须同时设置 QScrollArea 和 viewport 透明
   - 检查是否有其他样式覆盖

3. **滚动条不显示**：
   - 检查内容高度是否超过可视区域
   - 检查滚动条样式是否被隐藏

---

## 待优化项目（更新）

### 高优先级
- [x] 主题适配修复
- [x] Tab 背景色统一
- [x] 提示词输入框高度调整
- [ ] 实现任务进度轮询
- [ ] 实现视频下载功能
- [ ] 添加错误重试机制

### 中优先级
- [ ] 优化明亮主题的整体视觉效果
- [ ] 添加主题切换动画
- [ ] 实现 CSV 批量导入
- [ ] 添加图片拖拽上传
- [ ] 实现多图管理（删除、排序）

### 低优先级
- [ ] 添加更多主题选项（高对比度、护眼模式）
- [ ] 实现主题自定义功能
- [ ] 添加暗色/明亮主题自动切换（跟随系统）
- [ ] 优化滚动条动画效果

---

## Git 提交记录（更新）

### 2026-03-09 晚间
- 提示词输入框高度调整（200/300px → 120px）
- 明亮主题字体颜色修复（移除硬编码白色字体）
- 暗色主题组件背景加深（0.6 → 0.95 透明度）
- Tab 背景色统一（QScrollArea 透明处理）
- 滚动条样式优化（半透明 → 实色）
- 预览组件样式管理（objectName + QSS）

**修改文件**：
- `widgets/videogen.cpp` - 移除硬编码样式，添加 objectName
- `styles/glassmorphism.qss` - 调整透明度，添加 QScrollArea 样式
- `styles/light.qss` - 添加明亮主题完整样式

---

**最后更新**：2026-03-09 晚间
**维护者**：Claude Opus 4.6
