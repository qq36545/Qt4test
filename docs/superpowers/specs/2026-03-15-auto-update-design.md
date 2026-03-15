# ChickenAI 自动升级功能设计文档

**日期**: 2026-03-15
**版本**: 1.0
**状态**: 已批准

## 1. 概述

为 ChickenAI 应用添加自动升级功能，支持启动时自动检查更新和手动检查更新，提供可选升级和强制升级两种模式，使用多渠道下载确保中国大陆用户的稳定访问。

### 1.1 目标

- 用户启动应用时自动检查新版本
- 支持可选升级（用户可拒绝）和强制升级（紧急bug修复）
- 保护用户数据（视频、数据库、配置）不被升级过程破坏
- 提供手动检查更新入口（AboutWidget）
- 使用3个稳定渠道确保中国大陆用户可访问

### 1.2 非目标

- 不实现应用内自动替换（使用系统标准安装流程）
- 不实现增量更新（使用完整安装包）
- 不实现自动静默安装（需要用户手动安装）

## 2. 架构设计

### 2.1 核心组件

**UpdateManager 类**（单例模式）
- 位置：`network/updatemanager.h` 和 `network/updatemanager.cpp`
- 职责：
  - 版本检查和比较
  - 下载管理和多渠道切换
  - UI 交互（弹窗提示）
  - 断点续传和文件校验

### 2.2 版本信息格式

版本信息存储为 JSON 格式，部署在3个渠道：

```json
{
  "version": "1.0.2",
  "versionCode": 102,
  "updateType": "optional",
  "forceReason": "",
  "releaseDate": "2026-03-15",
  "releaseNotes": "1. 新增xxx功能\n2. 修复xxx问题",
  "minSupportedVersion": "1.0.0",
  "downloadUrls": {
    "macos_arm64": {
      "github": "https://github.com/.../ChickenAI-1.0.2-macOS-arm64.dmg",
      "gitee": "https://gitee.com/.../ChickenAI-1.0.2-macOS-arm64.dmg",
      "cloudflare": "https://pub-xxx.r2.dev/ChickenAI-1.0.2-macOS-arm64.dmg"
    },
    "macos_intel": {
      "github": "https://github.com/.../ChickenAI-1.0.2-macOS-intel.dmg",
      "gitee": "https://gitee.com/.../ChickenAI-1.0.2-macOS-intel.dmg",
      "cloudflare": "https://pub-xxx.r2.dev/ChickenAI-1.0.2-macOS-intel.dmg"
    },
    "windows_x64": {
      "github": "https://github.com/.../ChickenAI-1.0.2-Windows-x64.exe",
      "gitee": "https://gitee.com/.../ChickenAI-1.0.2-Windows-x64.exe",
      "cloudflare": "https://pub-xxx.r2.dev/ChickenAI-1.0.2-Windows-x64.exe"
    }
  },
  "fileSize": {
    "macos_arm64": 52428800,
    "macos_intel": 54525952,
    "windows_x64": 48234496
  },
  "sha256": {
    "macos_arm64": "abc123...",
    "macos_intel": "def456...",
    "windows_x64": "ghi789..."
  }
}
```

**字段说明**：
- `version`: 版本号字符串（用于显示）
- `versionCode`: 版本号数值（用于比较，格式：主版本*100 + 次版本*10 + 修订版本）
- `updateType`: 升级类型（`"optional"` 或 `"mandatory"`）
- `forceReason`: 强制升级原因（仅当 `updateType` 为 `"mandatory"` 时显示）
- `releaseNotes`: 更新说明
- `minSupportedVersion`: 最低支持版本（低于此版本强制升级）
- `downloadUrls`: 各平台各渠道的下载链接
- `fileSize`: 各平台文件大小（字节）
- `sha256`: 各平台文件的 SHA256 校验值

### 2.3 支持的平台

- **macOS ARM64**: Apple Silicon (M1/M2/M3) 芯片
- **macOS Intel**: Intel 芯片
- **Windows x64**: Windows 10/11 64位

平台检测逻辑：
```cpp
QString UpdateManager::detectPlatform() {
    #ifdef Q_OS_WIN
        return "windows_x64";
    #elif defined(Q_OS_MACOS)
        #ifdef Q_PROCESSOR_ARM
            return "macos_arm64";
        #else
            return "macos_intel";
        #endif
    #endif
}
```

### 2.4 下载渠道

**优先级顺序**：
1. **GitHub Releases**: 国际标准，主渠道
2. **Gitee Releases**: 国内镜像，中国大陆用户优先
3. **Cloudflare R2**: 国际 CDN，备用渠道

**切换逻辑**：
- 并发请求3个渠道的 `version.json`
- 第一个成功响应的渠道用于获取版本信息
- 下载时按优先级顺序尝试，失败自动切换到下一个渠道

**版本信息存储位置**：
- GitHub: `https://raw.githubusercontent.com/<用户名>/<仓库名>/main/releases/version.json`
- Gitee: `https://gitee.com/<用户名>/<仓库名>/raw/main/releases/version.json`
- Cloudflare R2: `https://pub-<bucket>.r2.dev/releases/version.json`

## 3. 数据流设计

### 3.1 启动时检查流程

```
应用启动
  ↓
MainWindow::show()
  ↓
QTimer::singleShot(500ms) → UpdateManager::checkForUpdates(false)
  ↓
[并发请求3个渠道的 version.json，5秒超时]
  ↓
第一个成功的响应 → 解析 JSON
  ↓
版本比较（remoteVersionCode > currentVersionCode）
  ↓
┌─────────────────┬─────────────────┐
│ 无新版本        │ 有新版本        │
│ → 静默结束      │                 │
└─────────────────┴─────────────────┘
                  ↓
          检查 updateType
          ↓
    ┌─────────────┬─────────────┐
    │ optional    │ mandatory   │
    └─────────────┴─────────────┘
          ↓              ↓
    可选更新弹窗    强制更新弹窗
    [稍后提醒/立即更新]  [立即更新]
          ↓              ↓
    用户选择"立即更新"
          ↓
    二次确认弹窗
    [取消/开始下载]
          ↓
    开始下载（带进度条）
          ↓
    [尝试渠道1 → 失败 → 渠道2 → 失败 → 渠道3]
          ↓
    下载完成 → SHA256校验
          ↓
    ┌─────────────┬─────────────┐
    │ 校验成功    │ 校验失败    │
    └─────────────┴─────────────┘
          ↓              ↓
    下载完成弹窗    删除文件，尝试下一渠道
    [打开文件夹/关闭程序]
```

### 3.2 手动检查流程

```
用户点击"检测新版本"按钮（AboutWidget）
  ↓
显示"检查中..."加载提示
  ↓
UpdateManager::checkForUpdates(true)
  ↓
[同启动检查流程]
  ↓
如果无新版本 → 弹窗提示"当前已是最新版本 v1.0.1"
```

### 3.3 多渠道切换逻辑

```cpp
QStringList channels = {"github", "gitee", "cloudflare"};
for (const QString& channel : channels) {
    QString url = downloadUrls[platform][channel];
    if (tryDownload(url)) {
        break;  // 成功则停止
    }
    // 失败则自动尝试下一个渠道
    qDebug() << "Channel" << channel << "failed, trying next...";
}
```

## 4. 错误处理

### 4.1 网络错误

**超时设置**：
- 版本检查请求：5秒超时
- 下载请求：动态超时（文件大小 / 100KB/s，最少30秒）

**失败场景**：

**场景1：所有渠道都无法获取版本信息**
- 启动检查：静默失败，不阻塞启动，记录日志
- 手动检查：弹窗提示"网络连接失败，请稍后重试"

**场景2：版本信息 JSON 格式错误**
- 尝试下一个渠道
- 所有渠道都失败 → 同场景1

**场景3：下载过程中断**
- 保存已下载的部分到临时文件（`.part` 后缀）
- 下次启动时检测到未完成的下载，询问是否继续
- 使用 HTTP Range 请求实现断点续传

**场景4：SHA256 校验失败**
- 删除损坏的文件
- 自动尝试下一个渠道重新下载
- 所有渠道都失败 → 提示用户"下载失败，请立即重试"

**场景5：磁盘空间不足**
- 下载前检查可用空间（需要至少 2倍文件大小）
- 不足则提示用户清理磁盘空间

### 4.2 版本比较逻辑

```cpp
// 使用 versionCode 进行数值比较
// 例如：1.0.2 → versionCode = 102
//      1.1.0 → versionCode = 110
//      2.0.0 → versionCode = 200

bool hasUpdate = remoteVersionCode > currentVersionCode;

// 支持最低版本检查
if (currentVersionCode < minSupportedVersionCode) {
    updateType = "mandatory";
    forceReason = "当前版本过旧，必须升级到最新版本";
}
```

### 4.3 边界情况

**用户在下载过程中关闭应用**：
- 保存下载进度到 QSettings
- 下次启动时检测到未完成的下载，询问是否继续

**强制更新时用户强制退出**：
- 下次启动时再次检测，仍然是强制更新
- 无法绕过

**版本回退（服务器版本号变小）**：
- 不提示更新（只有版本号增大才提示）

**多个实例同时运行**：
- 使用 QLockFile 确保只有一个实例在下载
- 其他实例检测到锁文件，提示"正在下载更新，请稍候"

## 5. 用户数据保护

### 5.1 数据存储位置

**macOS**:
```
~/Library/Application Support/Qt4test/
├── database.db          # SQLite数据库
├── settings.conf        # QSettings配置
└── videos/              # 用户生成的视频
    └── outputs/
```

**Windows**:
```
C:\Users\<用户名>\AppData\Local\Qt4test\
├── database.db
├── settings.conf
└── videos\
    └── outputs\
```

### 5.2 为什么完整安装包不会破坏用户数据

**macOS (.dmg 安装)**：
- 用户拖拽 ChickenAI.app 到 Applications 文件夹时，只替换应用程序本身
- 用户数据存储在 `~/Library/Application Support/Qt4test/`，完全独立
- 系统不会触碰用户数据目录

**Windows (.exe 安装程序)**：
- 安装程序只更新 `C:\Program Files\ChickenAI\` 下的程序文件
- 用户数据存储在 `%LOCALAPPDATA%\Qt4test\`，完全独立
- 安装程序不会删除用户数据目录

### 5.3 数据库版本兼容性

```cpp
// 在 DBManager 中添加版本检查
void DBManager::checkDatabaseVersion() {
    int dbVersion = getDbVersion();  // 从 settings 表读取
    int appVersion = 102;  // 当前应用版本码
    
    if (dbVersion < appVersion) {
        // 执行数据库迁移（添加新表、新字段等）
        migrateDatabaseFrom(dbVersion, appVersion);
    }
}
```

### 5.4 升级后首次启动检查

```cpp
// MainWindow 构造函数中
void MainWindow::checkFirstRunAfterUpdate() {
    QSettings settings;
    QString lastVersion = settings.value("lastVersion", "1.0.0").toString();
    QString currentVersion = QApplication::applicationVersion();
    
    if (lastVersion != currentVersion) {
        // 首次运行新版本
        DBManager::instance()->checkDatabaseVersion();
        settings.setValue("lastVersion", currentVersion);
        
        // 可选：显示"更新成功"提示和新功能介绍
        showWhatsNewDialog();
    }
}
```

## 6. UI设计

### 6.1 可选更新弹窗

```
┌─────────────────────────────────────┐
│  🎉 发现新版本 v1.0.2               │
├─────────────────────────────────────┤
│  当前版本：v1.0.1                   │
│  最新版本：v1.0.2                   │
│  发布日期：2026-03-15               │
│                                     │
│  更新内容：                         │
│  • 新增xxx功能                      │
│  • 修复xxx问题                      │
│  • 优化xxx性能                      │
│                                     │
│  文件大小：50 MB                    │
├─────────────────────────────────────┤
│         [稍后提醒]    [立即更新]    │
└─────────────────────────────────────┘
```

### 6.2 强制更新弹窗

```
┌─────────────────────────────────────┐
│  ⚠️  必须更新才能继续使用            │
├─────────────────────────────────────┤
│  当前版本：v1.0.1                   │
│  最新版本：v1.0.2                   │
│                                     │
│  更新原因：                         │
│  修复严重安全漏洞，必须立即升级     │
│                                     │
│  更新内容：                         │
│  • 修复xxx严重bug                   │
│  • 修复xxx安全漏洞                  │
│                                     │
│  ⚠️ 请先保存当前正在生成的视频任务   │
├─────────────────────────────────────┤
│                    [立即更新]       │
└─────────────────────────────────────┘
```

**特点**：
- 模态对话框，无法关闭
- 只有一个按钮"立即更新"
- 用户必须升级才能继续使用

### 6.3 二次确认弹窗

```
┌─────────────────────────────────────┐
│  📦 准备下载更新                    │
├─────────────────────────────────────┤
│  即将下载 50 MB 的安装包            │
│                                     │
│  ✓ 您的数据（视频、数据库、配置）   │
│    不会被删除                       │
│                                     │
│  建议：                             │
│  • 保存当前正在生成的视频任务       │
│  • 确保网络连接稳定                 │
│                                     │
│  下载完成后，程序将关闭，           │
│  请手动安装新版本。                 │
├─────────────────────────────────────┤
│         [取消]        [开始下载]    │
└─────────────────────────────────────┘
```

### 6.4 下载进度窗口

```
┌─────────────────────────────────────┐
│  ⬇️  正在下载更新 v1.0.2             │
├─────────────────────────────────────┤
│  [████████████░░░░░░░░] 65%         │
│                                     │
│  已下载：32.5 MB / 50 MB            │
│  速度：2.5 MB/s                     │
│  剩余时间：约 7 秒                  │
│                                     │
│  当前渠道：Gitee                    │
├─────────────────────────────────────┤
│                    [取消下载]       │
└─────────────────────────────────────┘
```

**特点**：
- 实时更新进度条、速度、剩余时间
- 显示当前使用的下载渠道
- 如果渠道切换，显示"切换到 xxx 渠道"

### 6.5 下载完成弹窗

```
┌─────────────────────────────────────┐
│  ✅ 下载完成                        │
├─────────────────────────────────────┤
│  安装包已保存到：                   │
│  ~/Downloads/ChickenAI-1.0.2.dmg    │
│                                     │
│  请按以下步骤安装：                 │
│  1. 打开下载的安装包                │
│  2. 拖拽到 Applications 文件夹      │
│  3. 替换旧版本                      │
│                                     │
│  安装完成后，您的所有数据将保持不变 │
├─────────────────────────────────────┤
│    [打开文件夹]    [关闭程序]      │
└─────────────────────────────────────┘
```

**行为**：
- "打开文件夹"：macOS 打开 Finder 并选中 .dmg 文件，Windows 打开文件夹并选中 .exe 文件
- "关闭程序"：退出应用

### 6.6 下载失败弹窗

```
┌─────────────────────────────────────┐
│  ❌ 下载失败                        │
├─────────────────────────────────────┤
│  已尝试所有下载渠道：               │
│  • GitHub：连接超时                 │
│  • Gitee：连接超时                  │
│  • Cloudflare R2：连接超时          │
│                                     │
│  建议：                             │
│  • 检查网络连接                     │
│  • 立即重试                         │
│  • 或手动下载安装包                 │
├─────────────────────────────────────┤
│    [手动下载]    [立即重试]        │
└─────────────────────────────────────┘
```

**行为**：
- "手动下载"：打开浏览器到 Gitee Release 页面（国内稳定）
- "立即重试"：重新开始下载流程

## 7. 实现细节

### 7.1 UpdateManager 类结构

```cpp
// network/updatemanager.h
class UpdateManager : public QObject {
    Q_OBJECT
    
public:
    static UpdateManager* instance();
    void checkForUpdates(bool isManual = false);
    
signals:
    void updateCheckCompleted(bool hasUpdate);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadCompleted(const QString& filePath);
    void downloadFailed(const QString& reason);
    
private:
    UpdateManager(QObject *parent = nullptr);
    
    // 版本检查
    void fetchVersionInfo();
    void parseVersionInfo(const QJsonObject& json);
    bool compareVersions(int remoteCode, int currentCode);
    
    // 下载管理
    void startDownload();
    void downloadFromChannel(const QString& channel);
    bool verifyChecksum(const QString& filePath, const QString& expectedSha256);
    
    // UI交互
    void showOptionalUpdateDialog();
    void showMandatoryUpdateDialog();
    void showDownloadProgressDialog();
    void showDownloadCompleteDialog(const QString& filePath);
    void showDownloadFailedDialog();
    
    // 工具方法
    QString detectPlatform();
    QString getDownloadPath();
    qint64 getAvailableDiskSpace();
    
    // 成员变量
    QNetworkAccessManager* networkManager;
    QNetworkReply* currentReply;
    QFile* downloadFile;
    QJsonObject versionInfo;
    QString currentPlatform;
    bool isManualCheck;
    int currentChannelIndex;
    qint64 totalBytes;
    qint64 receivedBytes;
    QElapsedTimer downloadTimer;
};
```

### 7.2 并发请求版本信息

```cpp
void UpdateManager::fetchVersionInfo() {
    QStringList urls = {
        "https://raw.githubusercontent.com/.../version.json",
        "https://gitee.com/.../version.json",
        "https://pub-xxx.r2.dev/releases/version.json"
    };
    
    for (const QString& url : urls) {
        QNetworkRequest request(url);
        request.setTransferTimeout(5000);  // 5秒超时
        
        QNetworkReply* reply = networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray data = reply->readAll();
                QJsonDocument doc = QJsonDocument::fromJson(data);
                parseVersionInfo(doc.object());
                cancelOtherRequests();  // 取消其他请求
            }
            reply->deleteLater();
        });
    }
}
```

### 7.3 断点续传

```cpp
void UpdateManager::downloadFromChannel(const QString& channel) {
    QString url = versionInfo["downloadUrls"][currentPlatform][channel].toString();
    QNetworkRequest request(url);
    
    // 检查是否有未完成的下载
    QString tempPath = getDownloadPath() + ".part";
    if (QFile::exists(tempPath)) {
        QFile file(tempPath);
        qint64 existingSize = file.size();
        
        // 设置 Range 头实现断点续传
        request.setRawHeader("Range", QString("bytes=%1-").arg(existingSize).toUtf8());
        receivedBytes = existingSize;
    }
    
    currentReply = networkManager->get(request);
    // 连接信号槽...
}
```

### 7.4 SHA256 校验

```cpp
bool UpdateManager::verifyChecksum(const QString& filePath, const QString& expectedSha256) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return false;
    }
    
    QString actualSha256 = hash.result().toHex();
    return actualSha256 == expectedSha256;
}
```

## 8. 集成点

### 8.1 main.cpp

```cpp
#include "network/updatemanager.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // 设置应用信息
    app.setApplicationName("ChickenAI");
    app.setApplicationVersion("1.0.1");
    app.setOrganizationName("ChickenAI");
    
    // 创建 UpdateManager 实例
    UpdateManager::instance();
    
    MainWindow window;
    window.show();
    
    return app.exec();
}
```

### 8.2 MainWindow 构造函数

```cpp
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUI();
    
    // 延迟500ms后检查更新（不阻塞UI）
    QTimer::singleShot(500, []() {
        UpdateManager::instance()->checkForUpdates(false);
    });
    
    // 检查是否首次运行新版本
    checkFirstRunAfterUpdate();
}
```

### 8.3 AboutWidget

```cpp
void AboutWidget::checkVersion() {
    // 显示加载提示
    QProgressDialog *progress = new QProgressDialog(
        "正在检查新版本...", "取消", 0, 0, this);
    progress->setWindowModality(Qt::WindowModal);
    progress->show();
    
    // 手动检查更新
    UpdateManager::instance()->checkForUpdates(true);
}
```

## 9. 测试策略

### 9.1 单元测试

- 版本比较逻辑测试
- 平台检测测试
- JSON 解析测试
- SHA256 校验测试

### 9.2 集成测试

- 模拟服务器测试版本检查
- 模拟下载测试多渠道切换
- 断点续传测试

### 9.3 手动测试清单

**功能测试**：
- [ ] 启动时自动检查更新（无新版本）
- [ ] 启动时自动检查更新（有可选更新）
- [ ] 启动时自动检查更新（有强制更新）
- [ ] 手动点击"检测新版本"按钮
- [ ] 可选更新：点击"稍后提醒"
- [ ] 可选更新：点击"立即更新"
- [ ] 强制更新：只能点击"立即更新"
- [ ] 下载进度显示正确
- [ ] 下载过程中点击"取消下载"
- [ ] 下载完成后点击"打开文件夹"
- [ ] 下载完成后点击"关闭程序"
- [ ] 下载失败后点击"立即重试"
- [ ] 下载失败后点击"手动下载"

**网络测试**：
- [ ] GitHub 可访问时优先使用 GitHub
- [ ] GitHub 不可访问时自动切换到 Gitee
- [ ] Gitee 不可访问时自动切换到 Cloudflare R2
- [ ] 所有渠道都不可访问时显示失败提示
- [ ] 网络中断后恢复，断点续传功能正常

**平台测试**：
- [ ] macOS ARM64：下载正确的 .dmg 文件
- [ ] macOS Intel：下载正确的 .dmg 文件
- [ ] Windows 64-bit：下载正确的 .exe 文件

**数据保护测试**：
- [ ] 升级前后，数据库文件完整
- [ ] 升级前后，用户生成的视频文件完整
- [ ] 升级前后，配置文件完整
- [ ] 数据库版本迁移功能正常

## 10. 文件清单

**新增文件**：
- `network/updatemanager.h` (~100 行)
- `network/updatemanager.cpp` (~600 行)
- `releases/version.json` (版本信息文件，部署到3个渠道)

**修改文件**：
- `main.cpp` (添加 5 行)
- `mainwindow.h` (添加 1 个方法声明)
- `mainwindow.cpp` (添加 10 行)
- `widgets/aboutwidget.cpp` (修改 `checkVersion()` 方法)
- `CMakeLists.txt` (添加 updatemanager 到源文件列表)

## 11. 工作量估算

- **核心开发**: 2-3 天
  - UpdateManager 类实现：1.5 天
  - UI 弹窗设计和实现：0.5 天
  - 集成到现有代码：0.5 天
- **测试和调试**: 1-2 天
  - 单元测试：0.5 天
  - 集成测试：0.5 天
  - 手动测试：1 天
- **总计**: 3-5 天

## 12. 风险和缓解

**风险1：网络不稳定导致下载失败**
- 缓解：多渠道自动切换 + 断点续传

**风险2：用户在下载过程中强制关闭应用**
- 缓解：保存下载进度，下次启动时询问是否继续

**风险3：SHA256 校验失败**
- 缓解：自动尝试下一个渠道重新下载

**风险4：磁盘空间不足**
- 缓解：下载前检查可用空间，不足则提示用户

**风险5：版本信息 JSON 格式错误**
- 缓解：尝试下一个渠道，所有渠道都失败则静默失败

## 13. 未来扩展

- 支持增量更新（减少下载大小）
- 支持自动静默安装（需要处理权限问题）
- 支持更新日志的富文本显示（Markdown）
- 支持更新统计（收集用户升级数据）
- 支持灰度发布（部分用户先升级）

---

**文档结束**
