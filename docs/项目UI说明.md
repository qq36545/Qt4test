# ChickenAI 主窗体 UI 布局说明

## 窗口基本信息

- **窗口标题**: 鸡你太美ai 图片/视频批量生成器
- **最小尺寸**: 1200 × 800
- **默认尺寸**: 1400 × 900
- **主窗体类**: MainWindow (继承自 QMainWindow)

## ASCII 布局预览

```
┌─────────────────────────────────────────────────────────────────────┐
│  鸡你太美ai 图片/视频批量生成器                                      │
├───┬─────────────────────────────────────────────────────────────────┤
│   │                                                                 │
│ 🎬│                                                                 │
│   │                                                                 │
│ 🖼️│                                                                 │
│   │                                                                 │
│ ⚙️│                    内容区域                                      │
│   │              (QStackedWidget)                                   │
│ ℹ️│                                                                 │
│   │         根据侧边栏按钮切换显示不同页面：                          │
│ 📜│         - 视频生成页面 (VideoGenWidget)                         │
│   │         - 图片生成页面 (ImageGenWidget)                         │
│   │         - 配置页面 (ConfigWidget)                               │
│   │         - 关于页面 (AboutWidget)                                │
│   │         - 历史记录页面 (VideoSingleHistoryTab)                  │
│   │                                                                 │
│   │                                                                 │
│   │                                                                 │
│   │                                                                 │
│ 🌙│                                                                 │
│   │                                                                 │
└───┴─────────────────────────────────────────────────────────────────┘
 80px                      自适应宽度
```

## 主布局结构

### 1. 中心部件 (centralWidget)
- **控件类型**: QWidget
- **对象名称**: 无
- **变量名**: centralWidget
- **布局**: QHBoxLayout (mainLayout)
  - 边距: 0, 0, 0, 0
  - 间距: 0
- **说明**: 主窗口的中心部件，包含侧边栏和内容区域

### 2. 主布局子组件

#### 2.1 侧边栏 (Sidebar)
- **控件类型**: QWidget
- **对象名称**: "sidebar"
- **变量名**: sidebar
- **固定宽度**: 80px
- **布局**: QVBoxLayout (sidebarLayout)
  - 边距: 10, 20, 10, 20
  - 间距: 15
- **说明**: 左侧导航栏，包含功能切换按钮

#### 2.2 内容区域 (Content Area)
- **控件类型**: QStackedWidget
- **对象名称**: "contentArea"
- **变量名**: contentArea
- **说明**: 右侧主内容区域，通过堆栈切换显示不同功能页面

## 侧边栏按钮详细列表

### 功能按钮（从上到下）

| 序号 | 变量名 | 控件类型 | 对象名称 | 图标 | 提示文本 | 尺寸 | 功能说明 |
|------|--------|----------|----------|------|----------|------|----------|
| 1 | videoButton | QPushButton | "sidebarButton" | 🎬 | 视频生成 | 60×60 | 切换到视频生成页面 |
| 2 | imageButton | QPushButton | "sidebarButton" | 🖼️ | 图片生成 | 60×60 | 切换到图片生成页面 |
| 3 | configButton | QPushButton | "sidebarButton" | ⚙️ | 配置 | 60×60 | 切换到配置页面 |
| 4 | aboutButton | QPushButton | "sidebarButton" | ℹ️ | 关于 | 60×60 | 切换到关于页面 |
| 5 | historyButton | QPushButton | "sidebarButton" | 📜 | 历史记录 | 60×60 | 切换到历史记录页面 |
| - | (弹性空间) | addStretch() | - | - | - | - | 将主题按钮推到底部 |
| 6 | themeButton | QPushButton | "sidebarButton" | 🌙/☀️ | 切换主题 | 60×60 | 切换深色/浅色主题 |

### 按钮特性
- **可选中状态**: 前5个功能按钮支持 checkable (可选中)
- **互斥选择**: 同一时间只有一个功能按钮处于选中状态
- **主题按钮**: 根据当前主题显示不同图标（深色主题显示🌙，浅色主题显示☀️）

## 内容区域页面列表

| 序号 | 变量名 | 控件类型 | 页面说明 | 触发按钮 |
|------|--------|----------|----------|----------|
| 0 | videoGenWidget | VideoGenWidget | 视频生成功能页面 | videoButton |
| 1 | imageGenWidget | ImageGenWidget | 图片生成功能页面 | imageButton |
| 2 | configWidget | ConfigWidget | 配置管理页面（API密钥等） | configButton |
| 3 | aboutWidget | AboutWidget | 关于软件信息页面 | aboutButton |
| 4 | historyWidget | VideoSingleHistoryTab | 历史记录查看页面 | historyButton |

## 信号与槽连接关系

### 侧边栏按钮点击事件

| 按钮 | 信号 | 槽函数 | 功能 |
|------|------|--------|------|
| videoButton | clicked() | showVideoGen() | 显示视频生成页面，更新按钮选中状态 |
| imageButton | clicked() | showImageGen() | 显示图片生成页面，更新按钮选中状态 |
| configButton | clicked() | showConfig() | 显示配置页面，更新按钮选中状态 |
| aboutButton | clicked() | showAbout() | 显示关于页面，更新按钮选中状态 |
| historyButton | clicked() | showHistory() | 显示历史记录页面，更新按钮选中状态 |
| themeButton | clicked() | toggleTheme() | 切换主题（深色/浅色），更新样式表 |

### 跨页面通信

| 源组件 | 信号 | 目标组件 | 槽函数 | 说明 |
|--------|------|----------|--------|------|
| configWidget | apiKeysChanged() | videoGenWidget->getSingleTab() | refreshApiKeys() | 配置页面API密钥变化时刷新视频单个生成页面 |
| configWidget | apiKeysChanged() | videoGenWidget->getBatchTab() | refreshApiKeys() | 配置页面API密钥变化时刷新视频批量生成页面 |
| configWidget | apiKeysChanged() | videoGenWidget->getHistoryWidget()->getVideoSingleTab() | refreshApiKeys() | 配置页面API密钥变化时刷新历史记录页面 |
| configWidget | apiKeysChanged() | historyWidget | refreshApiKeys() | 配置页面API密钥变化时刷新独立历史记录页面 |

## 主题系统

### 主题枚举
```cpp
enum Theme { Dark, Light };
```

### 主题相关变量
- **currentTheme**: 当前主题（默认为 Dark）
- **样式文件**:
  - 深色主题: `:/styles/glassmorphism.qss`
  - 浅色主题: `:/styles/light.qss`

### 主题持久化
- **存储方式**: QSettings
- **组织名**: "ChickenAI"
- **应用名**: "Theme"
- **键名**: "currentTheme"

## 字体系统

### 字体配置
- **字体族**: Inter, -apple-system, BlinkMacSystemFont, Segoe UI, sans-serif
- **基础字号**: 12pt (根据DPI自动缩放)
- **缩放因子**: 基于屏幕逻辑DPI计算（基准96 DPI）
- **缩放范围**: 0.8 ~ 2.0

## 快速定位组件指南

### 修改侧边栏按钮
- **文件位置**: mainwindow.cpp:58-120 (setupSidebar函数)
- **样式定义**: 通过对象名称 "sidebarButton" 在QSS文件中统一设置

### 修改内容页面
- **文件位置**: mainwindow.cpp:122-152 (setupContentArea函数)
- **添加新页面步骤**:
  1. 在 mainwindow.h 中声明页面widget指针
  2. 在 setupContentArea() 中创建并添加到 contentArea
  3. 在侧边栏添加对应按钮
  4. 创建对应的 show函数

### 修改主题样式
- **深色主题**: resources/styles/glassmorphism.qss
- **浅色主题**: resources/styles/light.qss
- **应用方式**: applyStyles() 函数 (mainwindow.cpp:204-217)

### 调整窗口尺寸
- **文件位置**: mainwindow.cpp:37-39
- **最小尺寸**: setMinimumSize(1200, 800)
- **默认尺寸**: resize(1400, 900)

## 开发调试技巧

### 1. 快速切换页面测试
在 MainWindow 构造函数中修改默认显示页面：
```cpp
// mainwindow.cpp:24
showVideoGen();  // 改为 showConfig() 等其他函数
```

### 2. 调试按钮状态
所有功能按钮使用相同的对象名称 "sidebarButton"，可通过以下方式单独调试：
```cpp
videoButton->setObjectName("videoButton");  // 设置唯一名称
```

### 3. 查看当前显示页面
```cpp
int currentIndex = contentArea->currentIndex();
QWidget* currentWidget = contentArea->currentWidget();
```

### 4. 主题切换测试
```cpp
// 强制设置为浅色主题
currentTheme = Light;
applyStyles();
```

## 相关文件清单

| 文件路径 | 说明 |
|----------|------|
| mainwindow.h | 主窗口头文件，定义类结构和成员变量 |
| mainwindow.cpp | 主窗口实现文件，包含UI构建和事件处理 |
| widgets/videogen.h/cpp | 视频生成页面组件 |
| widgets/imagegen.h/cpp | 图片生成页面组件 |
| widgets/configwidget.h/cpp | 配置页面组件 |
| widgets/aboutwidget.h/cpp | 关于页面组件 |
| resources/styles/glassmorphism.qss | 深色主题样式表 |
| resources/styles/light.qss | 浅色主题样式表 |

---

**文档生成时间**: 2026-03-15
**项目版本**: ChickenAI
**Qt版本**: Qt6
