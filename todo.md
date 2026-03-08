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
