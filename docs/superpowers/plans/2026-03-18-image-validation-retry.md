# 图片格式校验与上传重试实施计划

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 添加图片格式校验（选择时）和上传重试机制（指数退避）

**Architecture:**
- 文件选择时立即校验格式和大小
- ImageUploader 添加重试逻辑（2s/5s/10s 指数退避）
- QProgressDialog 显示重试进度，支持用户取消

**Tech Stack:** C++ Qt, QImageReader, QProgressDialog, QTimer

---

## 需求描述

**当前问题**：
- 用户可以选择任意格式/大小的图片
- 上传失败后没有重试机制
- 用户不知道上传进度

**解决方案**：
1. 选择文件时校验：格式（jpg/png/webp）+ 大小（≤10MB）
2. 上传失败自动重试：3 次，间隔 2s/5s/10s
3. 显示进度对话框："正在重试第 N 次..."，带"取消"按钮
4. 超时时间：120 秒

**影响范围**：
- Grok3 视频：3 张图片上传
- Veo3 统一格式：首帧/中间帧/尾帧或 components

---

## 文件结构

**修改文件**：
- `widgets/videogen.cpp` - 添加文件选择时的校验逻辑
- `network/imageuploader.h` - 添加重试相关成员变量和信号
- `network/imageuploader.cpp` - 实现重试逻辑和进度对话框

**不创建新文件**

---

## Chunk 1: 文件选择时校验

### Task 1: 添加图片校验辅助函数

**Files:**
- Modify: `widgets/videogen.h:75` - 添加私有方法声明
- Modify: `widgets/videogen.cpp:1645` - 实现校验函数

- [ ] **Step 1: 在 videogen.h 添加函数声明**

在 `VideoSingleTab` 私有方法区添加：

```cpp
bool validateImageFile(const QString &filePath, QString &errorMsg) const;
```

- [ ] **Step 2: 实现校验函数**

在 `videogen.cpp` 约第 1645 行（`normalizeImageReferences` 函数后）添加：

```cpp
bool VideoSingleTab::validateImageFile(const QString &filePath, QString &errorMsg) const
{
    QFileInfo fileInfo(filePath);

    // 检查文件是否存在
    if (!fileInfo.exists()) {
        errorMsg = "文件不存在";
        return false;
    }

    // 检查文件大小（10MB = 10 * 1024 * 1024 bytes）
    qint64 fileSize = fileInfo.size();
    if (fileSize > 10 * 1024 * 1024) {
        errorMsg = QString("文件过大（%1 MB），请选择小于 10MB 的图片")
                   .arg(fileSize / 1024.0 / 1024.0, 0, 'f', 2);
        return false;
    }

    // 检查文件格式
    QImageReader reader(filePath);
    QString format = reader.format().toLower();
    if (format != "jpg" && format != "jpeg" && format != "png" && format != "webp") {
        errorMsg = QString("不支持的格式（%1），仅支持 JPG/PNG/WEBP").arg(format);
        return false;
    }

    return true;
}
```

- [ ] **Step 3: 验证编译**

Run: `make`
Expected: 编译通过

- [ ] **Step 4: Commit**

```bash
git add widgets/videogen.h widgets/videogen.cpp
git commit -m "feat: 添加图片文件校验函数（格式+大小）"
```

---

### Task 2: 集成校验到图片上传函数

**Files:**
- Modify: `widgets/videogen.cpp:975-1115` - 修改 5 个上传函数

- [ ] **Step 1: 修改 uploadImage() 添加校验**

在 `videogen.cpp:975` 的 `uploadImage()` 函数中，文件选择后立即校验：

```cpp
void VideoSingleTab::uploadImage()
{
    QString fileName = QFileDialog::getOpenFileName(this, "选择图片", "", "图片文件 (*.jpg *.jpeg *.png *.webp)");
    if (fileName.isEmpty()) return;

    // 校验文件
    QString errorMsg;
    if (!validateImageFile(fileName, errorMsg)) {
        QMessageBox::warning(this, "图片校验失败", errorMsg);
        return;
    }

    // ... 后续代码保持不变
```

- [ ] **Step 2: 修改 uploadGrokImage2() 添加校验**

在 `videogen.cpp:1072` 的 `uploadGrokImage2()` 函数中添加相同校验逻辑

- [ ] **Step 3: 修改 uploadGrokImage3() 添加校验**

在 `videogen.cpp:1109` 的 `uploadGrokImage3()` 函数中添加相同校验逻辑

- [ ] **Step 4: 修改 uploadEndFrameImage() 添加校验**

在相应函数中添加校验逻辑

- [ ] **Step 5: 修改 uploadMiddleFrameImage() 添加校验**

在相应函数中添加校验逻辑

- [ ] **Step 6: 验证编译**

Run: `make`
Expected: 编译通过

- [ ] **Step 7: Commit**

```bash
git add widgets/videogen.cpp
git commit -m "feat: 在图片选择时立即校验格式和大小"
```

---

## Chunk 2: 上传重试机制

### Task 3: 添加重试相关成员变量

**Files:**
- Modify: `network/imageuploader.h` - 添加成员变量和信号

- [ ] **Step 1: 查看当前 ImageUploader 类定义**

Run: `head -50 network/imageuploader.h`
Expected: 了解当前类结构

- [ ] **Step 2: 添加重试相关成员变量**

在 `ImageUploader` 类私有成员区添加：

```cpp
private:
    // 现有成员...

    // 重试相关
    int maxRetries = 3;
    int currentRetry = 0;
    QStringList retryDelays = {2000, 5000, 10000};  // 毫秒
    QString currentFilePath;
    QString currentApiKey;
    QProgressDialog *progressDialog = nullptr;
```

- [ ] **Step 3: 添加重试相关信号**

在 `ImageUploader` 类信号区添加：

```cpp
signals:
    void uploadSuccess(const QString &imageUrl);
    void uploadError(const QString &error);
    void retryAttempt(int retryCount, int maxRetries);  // 新增
```

- [ ] **Step 4: 验证编译**

Run: `make`
Expected: 编译通过

- [ ] **Step 5: Commit**

```bash
git add network/imageuploader.h
git commit -m "feat: 添加 ImageUploader 重试相关成员变量"
```

---

### Task 4: 实现重试逻辑

**Files:**
- Modify: `network/imageuploader.cpp` - 修改 uploadToImgbb 和错误处理

- [ ] **Step 1: 修改 uploadToImgbb 保存上传参数**

在 `uploadToImgbb()` 函数开头保存参数：

```cpp
void ImageUploader::uploadToImgbb(const QString &filePath, const QString &apiKey)
{
    currentFilePath = filePath;
    currentApiKey = apiKey;
    currentRetry = 0;

    // 创建进度对话框
    if (!progressDialog) {
        progressDialog = new QProgressDialog("正在上传图片...", "取消", 0, 0, nullptr);
        progressDialog->setWindowModality(Qt::ApplicationModal);
        progressDialog->setMinimumDuration(0);
        connect(progressDialog, &QProgressDialog::canceled, this, &ImageUploader::cancelUpload);
    }
    progressDialog->setLabelText("正在上传图片...");
    progressDialog->show();

    // ... 后续上传逻辑
```

- [ ] **Step 2: 添加重试函数**

在 `imageuploader.cpp` 末尾添加：

```cpp
void ImageUploader::retryUpload()
{
    if (currentRetry >= maxRetries) {
        if (progressDialog) progressDialog->hide();
        emit uploadError("上传失败，已重试 3 次");
        return;
    }

    currentRetry++;
    emit retryAttempt(currentRetry, maxRetries);

    if (progressDialog) {
        progressDialog->setLabelText(QString("上传失败，正在重试第 %1 次...").arg(currentRetry));
    }

    int delay = retryDelays[currentRetry - 1];
    QTimer::singleShot(delay, this, [this]() {
        uploadToImgbb(currentFilePath, currentApiKey);
    });
}
```

- [ ] **Step 3: 修改错误处理调用重试**

在上传失败的错误处理中调用 `retryUpload()` 而不是直接 emit uploadError

- [ ] **Step 4: 添加取消上传函数**

```cpp
void ImageUploader::cancelUpload()
{
    currentRetry = maxRetries;  // 阻止重试
    if (progressDialog) progressDialog->hide();
    emit uploadError("用户取消上传");
}
```

- [ ] **Step 5: 验证编译**

Run: `make`
Expected: 编译通过

- [ ] **Step 6: Commit**

```bash
git add network/imageuploader.h network/imageuploader.cpp
git commit -m "feat: 实现图片上传重试机制（指数退避）"
```

---

### Task 5: 手动测试验证

**Files:**
- Test: 启动应用，测试不同场景

- [ ] **Step 1: 测试格式校验**

测试场景：
1. 启动应用，进入"AI视频生成-单个"
2. 点击"上传图片"，选择一个 `.txt` 文件
3. Expected: 弹出警告"不支持的格式"

- [ ] **Step 2: 测试大小校验**

测试场景：
1. 准备一个 >10MB 的图片
2. 点击"上传图片"，选择该图片
3. Expected: 弹出警告"文件过大（XX MB），请选择小于 10MB 的图片"

- [ ] **Step 3: 测试正常上传**

测试场景：
1. 选择一个合格的图片（<10MB, jpg/png/webp）
2. Expected: 校验通过，图片预览显示

- [ ] **Step 4: 测试上传重试（模拟网络故障）**

测试场景：
1. 断开网络连接
2. 上传图片并点击生成
3. Expected: 显示"正在重试第 1 次..."、"正在重试第 2 次..."、"正在重试第 3 次..."
4. 最终显示"上传失败，已重试 3 次"

- [ ] **Step 5: 测试取消重试**

测试场景：
1. 断开网络连接
2. 上传图片并点击生成
3. 在重试对话框中点击"取消"按钮
4. Expected: 停止重试，显示"用户取消上传"

- [ ] **Step 6: Commit 测试通过标记**

```bash
git commit --allow-empty -m "test: 验证图片校验和上传重试功能"
```

---

## 可能的问题

### 问题 1: QImageReader 格式检测不准确
**症状**: 某些图片格式检测错误
**原因**: QImageReader 依赖文件扩展名和内容
**解决**: 使用 `reader.canRead()` 进一步验证

### 问题 2: 进度对话框阻塞 UI
**症状**: 重试期间 UI 无响应
**原因**: QProgressDialog 模态对话框
**解决**: 已设置 `Qt::ApplicationModal`，符合预期

### 问题 3: 重试期间用户关闭应用
**症状**: 应用崩溃
**原因**: progressDialog 野指针
**解决**: 在析构函数中清理 progressDialog

---

## 测试用例矩阵

| 场景 | 输入 | 预期输出 |
|------|------|---------|
| 格式校验-txt | 选择 .txt 文件 | 警告"不支持的格式" |
| 格式校验-bmp | 选择 .bmp 文件 | 警告"不支持的格式" |
| 大小校验 | 选择 15MB 图片 | 警告"文件过大" |
| 正常上传 | 选择 5MB jpg | 校验通过，显示预览 |
| 网络故障重试 | 断网上传 | 重试 3 次后失败 |
| 取消重试 | 重试时点击取消 | 停止重试 |

---
