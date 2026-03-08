# Qt4test - AI 视频/图片生成演示应用

## 项目简介

Qt4test 是一个基于 Qt 6 的 macOS 桌面应用，展示了现代化的玻璃态暗黑风格 UI 设计。应用提供 AI 视频生成和 AI 图片生成的演示界面（UI only，暂未接入真实 AI 模型）。

## 设计风格

**方案：玻璃态暗黑（Glassmorphism Dark）**

- **视觉特点**：玻璃毛玻璃效果、科技感、专业
- **配色方案**：
  - 主背景：`#000000`（纯黑）
  - 卡片背景：`rgba(15, 15, 35, 0.8)`（深紫蓝 + 80% 透明度）
  - 文字：`#F8FAFC`（浅灰白）
  - CTA 按钮：`#E11D48`（红色）
  - 边框：`rgba(248, 250, 252, 0.1)`（微弱白色）
- **字体**：Inter（专业系统感）
- **性能**：⚡ 优秀
- **可访问性**：✓ WCAG AA+

## 功能特性

### 1. 视频生成
- 提示词输入
- 分辨率选择（1920x1080, 1280x720, 3840x2160, 1080x1920）
- 时长设置（5秒, 10秒, 15秒, 30秒）
- 风格选择（Cinematic, Anime, Realistic, Abstract, Cartoon）
- 预览区域

### 2. 图片生成
- 提示词输入
- 分辨率选择（1024x1024, 1920x1080, 1080x1920, 2048x2048）
- 宽高比设置（1:1, 16:9, 9:16, 4:3, 3:4）
- 风格选择（Photorealistic, Anime, Oil Painting, Watercolor, Digital Art）
- 预览区域

### 3. 历史记录
- 占位页面（待实现）

## 项目结构

```
Qt4test/
├── CMakeLists.txt          # CMake 构建配置
├── main.cpp                # 应用入口
├── mainwindow.h/cpp        # 主窗口
├── styles/
│   └── glassmorphism.qss   # 玻璃态暗黑样式表
├── widgets/
│   ├── videogen.h/cpp      # 视频生成组件
│   └── imagegen.h/cpp      # 图片生成组件
├── resources.qrc           # Qt 资源文件
└── build/                  # 构建目录
    └── Qt4test.app         # macOS 应用包
```

## 构建说明

### 环境要求

- macOS 10.15+
- Qt 6.x（通过 Homebrew 安装）
- CMake 3.16+
- C++17 编译器

### 安装依赖

```bash
# 安装 Qt 6
brew install qt@6

# 设置环境变量
export PATH="/usr/local/opt/qt@6/bin:$PATH"
```

### 构建项目

```bash
# 创建构建目录
mkdir -p build
cd build

# 配置 CMake
cmake ..

# 编译
make -j4

# 运行
open Qt4test.app
```

## 使用说明

1. **启动应用**：双击 `Qt4test.app` 或通过命令行 `open Qt4test.app`
2. **切换功能**：点击左侧边栏的图标切换视频生成/图片生成/历史记录
3. **生成内容**：
   - 输入提示词
   - 选择参数（分辨率、时长/宽高比、风格）
   - 点击"生成"按钮
4. **查看结果**：生成结果将显示在预览区域（当前为演示版本，显示提示信息）

## 技术栈

| 组件 | 技术 |
|------|------|
| UI 框架 | Qt 6.10.2 |
| 构建系统 | CMake 3.16+ |
| 样式系统 | QSS（Qt Style Sheets） |
| 编程语言 | C++17 |
| 平台 | macOS |

## UI 设计亮点

### 1. 玻璃态效果
- 半透明背景 + 模糊效果
- 微妙的边框发光
- 渐变色叠加

### 2. 交互反馈
- 按钮 hover 状态
- 输入框 focus 状态
- 平滑过渡动画（200-300ms）

### 3. 专业配色
- 暗色背景护眼
- 高对比度文字（WCAG AA+）
- 红色 CTA 按钮醒目

### 4. 布局设计
- 侧边栏导航（80px 宽）
- 主区域内容（自适应）
- 大间距（40px padding）

## 后续扩展

### 短期计划
- [ ] 接入真实 AI 模型（Stable Diffusion, Runway, etc.）
- [ ] 实现历史记录功能
- [ ] 添加进度条和加载动画
- [ ] 支持导出生成结果

### 长期计划
- [ ] 批量生成功能
- [ ] 模型管理（切换不同 AI 模型）
- [ ] 云同步（保存到云端）
- [ ] 插件系统（支持第三方扩展）

## 许可证

MIT License

## 作者

Qt4test Team

---

**注意**：这是一个演示项目，暂未接入真实 AI 模型。点击"生成"按钮会显示参数信息的提示框。
