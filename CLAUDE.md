# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 文档边界
- 只记录仓库内可验证事实与可执行命令。
- 命令按 **本地开发 / CI** 分开，避免误用。
- 若 README 与代码冲突，以 `CMakeLists.txt` 与 `.github/workflows/*` 的当前事实为准。

## 事实来源（已核验）

### 命令来源
- `README.md`
- `.github/workflows/build-macos.yml`
- `.github/workflows/build-windows.yml`
- `CMakeLists.txt`

### 架构来源
- `main.cpp`
- `mainwindow.h`
- `mainwindow.cpp`
- `network/videoapi.h`
- `network/taskpollmanager.h`
- `database/dbmanager.h`
- `network/updatemanager.h`
- `network/imageapi.h`
- `network/imageuploader.h`
- `widgets/videogen.h`
- `widgets/imagegen.h`
- `widgets/imagehistory.h`
- `widgets/configwidget.h`
- `resources.qrc`

## 命令真值表
> 仅收录仓库中可证实命令；未配置项保持“未配置”。

### 本地开发（macOS，来源：`README.md`）
```bash
mkdir -p build
cd build
cmake ..
make -j4
open ChickenAI.app
```

### CI（macOS ARM64，来源：`.github/workflows/build-macos.yml`）
```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
make -j$(sysctl -n hw.ncpu)
macdeployqt ChickenAI.app -dmg
```

### CI（macOS x86_64 交叉构建，来源：`.github/workflows/build-macos.yml`）
```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
make -j$(sysctl -n hw.ncpu)
macdeployqt ChickenAI.app -dmg
```

### CI（Windows，来源：`.github/workflows/build-windows.yml`）
> 注：此段为 CI 命令链（Windows runner），不要当作本地默认开发命令使用。
```powershell
New-Item -ItemType Directory -Path build -Force
Set-Location build
cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
nmake
Set-Location ..
New-Item -ItemType Directory -Path deploy -Force
Copy-Item build\ChickenAI.exe deploy\
Copy-Item LICENSE.txt deploy\
windeployqt --release --no-translations deploy\ChickenAI.exe
choco install nsis -y
& "C:\Program Files (x86)\NSIS\makensis.exe" "/DAPP_VERSION=$env:APP_VERSION" installer.nsi
```

### 构建产物事实（来源：`CMakeLists.txt`）
- 项目名：`project(ChickenAI ...)`
- Windows 输出名：`OUTPUT_NAME "ChickenAI"`
- macOS 打包后置步骤仅在 `CMAKE_BUILD_TYPE=Release` 且系统可找到 `macdeployqt` 时执行。

### Lint / Test / 单元测试
- Lint：**未配置**
- 集成测试：**未配置**
- 单元测试：**未配置**
- 单测（单个测试）运行命令：**未配置**

证据范围：`README.md`、`CMakeLists.txt`、`.github/workflows/*` 未定义 lint/test/ctest 入口。

## 高层架构（Big Picture）

### 启动与应用壳层
- `main.cpp`：初始化 `QApplication`、语言/翻译、应用版本，启动 `MainWindow`。
- `MainWindow`（`mainwindow.h/.cpp`）：侧边栏导航 + `QStackedWidget` 内容区；负责页面切换、主题切换、启动更新检查、退出保存草稿。

### 核心业务链路
- UI 页面（如 `widgets/videogen.*`、`widgets/imagegen.*`）发起生成请求。
- `network/videoapi.h`：统一封装创建视频、查询任务、下载视频；按模型分发到不同的方法（VEO3/Grok/WAN/Sora2 等）。
- `network/imageapi.h`：统一封装图生图、文生图等图片生成 API 请求。
- `network/imageuploader.h`：负责文件上传功能，支持图片上传至图床，音频上传到临时服务器。
- `network/taskpollmanager.h`：统一轮询异步任务状态、超时、下载进度与恢复。
- `database/dbmanager.h`：使用 SQLite 持久化 API Key、图床 Key、任务与历史、应用偏好/草稿数据（包含 ApiKey、GenerationHistory、VideoTask、ImagePreferences、ImageDraft 结构）。
- `network/updatemanager.h`：版本检查、下载通道回退、校验与安装流程。

主线关系：
- UI (例如 `VideoGenWidget` / `ImageGenWidget`) -> 收集参数调用对应的API类。
- 视频生成流程 -> `VideoAPI` 创建任务，`TaskPollManager` 轮询和处理下载。
- 附件上传流程 -> 依赖 `ImageUploader` 获取上传 URL。
- 持久化流 -> 所有的API调用结果/任务信息保存到 `DBManager`。
- 启动更新链路 -> `UpdateManager`（由 `MainWindow` 及 `AboutWidget` 触发）。

### 资源与发布链路
- `resources.qrc`：QSS（暗色/明亮主题样式表）与图片资源入口。
- `CMakeLists.txt`：目标定义、平台差异与打包相关设置。
- `.github/workflows/build-macos.yml`、`.github/workflows/build-windows.yml`：CI 构建与发布工件配置。

## 命名一致性与执行准则
- 当前命名一致：
  - README 运行示例：`open ChickenAI.app`
  - `CMakeLists.txt`：`project(ChickenAI ...)`
  - Windows 输出名：`OUTPUT_NAME "ChickenAI"`

执行准则：
- 排障与发布时，优先以 `CMakeLists.txt` + CI workflow 的产物命名为准。

## 维护约束
- 新增命令前，必须能在仓库文件中找到来源。
- CI 命令必须标注为 CI，不得冒充本地默认命令。
- 对未配置能力（lint/test/单测）保持“未配置”，禁止猜测。
- 更新命令或架构说明时，必须同步更新“事实来源”列表。
