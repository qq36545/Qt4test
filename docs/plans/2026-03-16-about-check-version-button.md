# 完善"关于"页面"检查新版本"按钮功能

## 概览
复用 UpdateManager 单例，实现手动版本检测功能。与启动时自动检测共享同一套机制。

## 当前状态
- ✅ UpdateManager 已实现完整的版本检测、下载、校验
- ✅ 启动时自动检查已工作（MainWindow::setupStartupUpdateCheck）
- ❌ AboutWidget::checkVersion() 只是占位符，显示"功能待完善"

## 目标
点击"检查新版本"按钮后：
1. 调用 UpdateManager::checkForUpdates(true)  // isManual=true
2. 显示"正在检查..."提示
3. 根据结果显示对应的对话框：
   - 有更新 → 显示版本信息，询问是否下载
   - 无更新 → 显示"已是最新版本"
   - 失败 → 显示错误原因

## 修改文件

### 1. widgets/aboutwidget.h
添加私有槽函数声明：
```cpp
private slots:
    void checkVersion();  // 已存在，保持不变
    void onUpdateCheckStarted(bool isManual);
    void onUpdateAvailable(const UpdateManager::ReleaseInfo& info,
                           bool isManual, bool mandatoryEffective);
    void onNoUpdateFound(bool isManual, const QString& currentVersion);
    void onCheckFailed(bool isManual, const QString& reason);
```

添加成员变量：
```cpp
private:
    bool m_checkingUpdate = false;  // 防止重复检查
```

### 2. widgets/aboutwidget.cpp

#### 2.1 构造函数中连接信号（一次性）
```cpp
AboutWidget::AboutWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();

    // 连接 UpdateManager 信号
    UpdateManager *updateManager = UpdateManager::getInstance();
    connect(updateManager, &UpdateManager::checkStarted,
            this, &AboutWidget::onUpdateCheckStarted);
    connect(updateManager, &UpdateManager::updateAvailable,
            this, &AboutWidget::onUpdateAvailable);
    connect(updateManager, &UpdateManager::noUpdateFound,
            this, &AboutWidget::onNoUpdateFound);
    connect(updateManager, &UpdateManager::checkFailed,
            this, &AboutWidget::onCheckFailed);
}
```

#### 2.2 实现 checkVersion()
```cpp
void AboutWidget::checkVersion()
{
    if (m_checkingUpdate) {
        return;  // 防止重复点击
    }

    UpdateManager::getInstance()->checkForUpdates(true);
}
```

#### 2.3 实现信号处理槽函数
```cpp
void AboutWidget::onUpdateCheckStarted(bool isManual)
{
    if (!isManual) return;  // 只处理手动检查

    m_checkingUpdate = true;
    checkVersionButton->setEnabled(false);
    checkVersionButton->setText("🔄 检查中...");
}

void AboutWidget::onUpdateAvailable(
    const UpdateManager::ReleaseInfo& info,
    bool isManual, bool mandatoryEffective)
{
    if (!isManual) return;

    m_checkingUpdate = false;
    checkVersionButton->setEnabled(true);
    checkVersionButton->setText("🔍 检测新版本");

    QString text = QString("发现新版本 %1\n\n"
                          "当前版本: v%2\n"
                          "最新版本: %3\n"
                          "发布日期: %4\n\n"
                          "更新说明:\n%5\n\n"
                          "是否立即下载？")
                      .arg(info.version)
                      .arg(QApplication::applicationVersion())
                      .arg(info.version)
                      .arg(info.releaseDate)
                      .arg(info.releaseNotes);

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("发现新版本");
    msgBox.setText(text);
    msgBox.setIcon(QMessageBox::Information);
    QPushButton *downloadBtn = msgBox.addButton("立即下载",
                                                QMessageBox::AcceptRole);
    msgBox.addButton("稍后", QMessageBox::RejectRole);
    msgBox.exec();

    if (msgBox.clickedButton() == downloadBtn) {
        UpdateManager::getInstance()->startDownloadAndInstall();
    }
}

void AboutWidget::onNoUpdateFound(bool isManual, const QString& currentVersion)
{
    if (!isManual) return;

    m_checkingUpdate = false;
    checkVersionButton->setEnabled(true);
    checkVersionButton->setText("🔍 检测新版本");

    QMessageBox::information(this, "检查更新",
        QString("当前已是最新版本\n\n版本: v%1").arg(currentVersion));
}

void AboutWidget::onCheckFailed(bool isManual, const QString& reason)
{
    if (!isManual) return;

    m_checkingUpdate = false;
    checkVersionButton->setEnabled(true);
    checkVersionButton->setText("🔍 检测新版本");

    QMessageBox::warning(this, "检查更新失败",
        QString("无法检查更新\n\n原因: %1\n\n"
               "请检查网络连接后重试").arg(reason));
}
```

## 实现步骤

### Step 1: 修改 aboutwidget.h
- [ ] 添加 4 个槽函数声明
- [ ] 添加 `m_checkingUpdate` 成员变量
- [ ] 添加 `#include "network/updatemanager.h"`

### Step 2: 修改 aboutwidget.cpp
- [ ] 在构造函数中连接 UpdateManager 信号（4 个）
- [ ] 重写 `checkVersion()` - 调用 `checkForUpdates(true)`
- [ ] 实现 `onUpdateCheckStarted()` - 禁用按钮，显示"检查中"
- [ ] 实现 `onUpdateAvailable()` - 显示新版本信息对话框
- [ ] 实现 `onNoUpdateFound()` - 显示"已是最新版本"
- [ ] 实现 `onCheckFailed()` - 显示错误信息

### Step 3: 编译测试
- [ ] 编译项目，确保无错误
- [ ] 运行程序，点击"检查新版本"按钮
- [ ] 验证按钮状态变化（禁用 → "检查中..." → 恢复）

### Step 4: 功能测试
- [ ] 测试场景 1: 有新版本 → 显示版本信息，点击"立即下载"
- [ ] 测试场景 2: 无新版本 → 显示"已是最新版本"
- [ ] 测试场景 3: 网络错误 → 显示错误信息
- [ ] 测试场景 4: 快速多次点击 → 防重复检查生效

## 技术要点

### 1. 信号连接时机
在构造函数中一次性连接，避免每次点击重复连接。

### 2. isManual 参数过滤
所有槽函数都检查 `isManual` 参数，只处理手动检查（true）。
启动时的自动检查（false）由 MainWindow 处理。

### 3. 防重复检查
使用 `m_checkingUpdate` 标志位 + 按钮禁用双重保护。
UpdateManager 内部也有 `m_checkInProgress` 保护。

### 4. 按钮状态管理
- 检查开始: 禁用 + 文字改为"检查中..."
- 检查结束: 恢复启用 + 文字恢复原样
- 无论成功/失败都要恢复状态

### 5. 下载流程复用
点击"立即下载"后，调用 `UpdateManager::startDownloadAndInstall()`。
下载进度、完成、失败的信号由 UpdateManager 发出，无需额外处理。

## 与启动检查的对比

| 特性 | 启动检查 | 手动检查 |
|------|---------|---------|
| 触发 | MainWindow 构造后 500ms | 用户点击按钮 |
| isManual | false | true |
| 信号处理 | MainWindow lambda | AboutWidget 槽函数 |
| 失败提示 | 静默（qDebug） | 显示错误对话框 |
| 无更新提示 | 不提示 | 显示"已是最新版本" |
| 有更新提示 | 弹出对话框 | 弹出对话框（更详细） |

## 可能的问题

### Q1: 重复连接信号？
A: 在构造函数中连接，只会连接一次。

### Q2: 并发检查冲突？
A: UpdateManager 有 `m_checkInProgress` 保护，AboutWidget 有 `m_checkingUpdate` 保护。

### Q3: 按钮状态未恢复？
A: 所有结果分支（成功/失败/无更新）都要恢复按钮状态。

### Q4: 下载进度如何显示？
A: UpdateManager 已有 `downloadProgress` 信号，可选择性连接显示进度条。
当前实现：下载完成后自动打开文件夹，无需额外 UI。

## 未来优化（可选）

1. **下载进度条**
   - 连接 `UpdateManager::downloadProgress` 信号
   - 在 AboutWidget 中显示进度条对话框

2. **更新日志格式化**
   - 解析 Markdown 格式的 releaseNotes
   - 使用 QTextBrowser 显示富文本

3. **自动检查开关**
   - 在"关于"页面添加"启动时自动检查更新"复选框
   - 保存到 QSettings

4. **检查频率控制**
   - 记录上次检查时间
   - 24 小时内不重复检查（可配置）

## 验收标准

- [x] 点击按钮后能正确检测版本
- [x] 按钮状态正确变化（禁用 → 检查中 → 恢复）
- [x] 有新版本时显示详细信息
- [x] 无新版本时显示"已是最新版本"
- [x] 检查失败时显示错误原因
- [x] 防止快速重复点击
- [x] 下载流程与启动检查一致
- [x] 不影响启动时的自动检查

## 预计工作量
- 编码: 30 分钟
- 测试: 15 分钟
- 总计: 45 分钟
