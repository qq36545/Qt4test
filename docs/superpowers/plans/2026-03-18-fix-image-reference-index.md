# 图片引用索引修正实施计划

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复提示词中 `@图1` `@图片1` 引用错位问题（1-based → 0-based 转换）

**Architecture:** 在前端 `generateVideo()` 调用 API 前，预处理提示词，将用户友好的 1-based 索引转换为程序内部的 0-based 索引

**Tech Stack:** C++ Qt, 正则表达式

---

## 问题描述

**当前状态**：
- UI 显示："图片1"、"图片2"、"图片3"（1-based）
- 内部存储：`uploadedImagePaths[0]`, `[1]`, `[2]`（0-based）
- 用户提示词：`@图1` 或 `@图片1` 期望引用"图片1"
- **Bug**：后端 API 解析 `@图1` 为 `images[1]`，导致引用到"图片2"

**影响范围**：
- Grok3 视频生成
- Veo3 统一格式（图生视频）

**解决方案**：
前端预处理提示词，将 `@图N` 和 `@图片N` 转换为 `@图(N-1)` 和 `@图片(N-1)`

---

## 文件结构

**修改文件**：
- `widgets/videogen.cpp:1368-1579` - `VideoSingleTab::generateVideo()` 函数
- `widgets/videogen.h:58-76` - 添加私有辅助函数声明

**不创建新文件**

---

## Chunk 1: 核心实现

### Task 1: 添加辅助函数声明

**Files:**
- Modify: `widgets/videogen.h:58-76`

- [ ] **Step 1: 在 VideoSingleTab 私有方法区添加函数声明**

在 `videogen.h` 的 `VideoSingleTab` 类私有方法区（约第 75 行 `copyImagesToPersistentStorage` 后）添加：

```cpp
QString normalizeImageReferences(const QString &prompt) const;
```

- [ ] **Step 2: 验证编译**

Run: `cd /Users/wuyingyi/Downloads/code/test2/demos/Qt4test && qmake && make`
Expected: 编译通过（仅添加声明，未实现）

- [ ] **Step 3: Commit**

```bash
git add widgets/videogen.h
git commit -m "feat: 添加图片引用索引规范化函数声明"
```

---

### Task 2: 实现索引转换函数

**Files:**
- Modify: `widgets/videogen.cpp:1579` (在 `onVideoCreated` 函数后添加)

- [ ] **Step 1: 实现 normalizeImageReferences 函数**

在 `videogen.cpp` 约第 1617 行（`onVideoCreated` 函数结束后）添加：

```cpp
QString VideoSingleTab::normalizeImageReferences(const QString &prompt) const
{
    QString result = prompt;

    // 处理 @图N 和 @图片N 格式（N = 1,2,3）
    // 转换为 @图(N-1) 和 @图片(N-1)
    QRegularExpression re(R"(@(图片?)(\d+))");
    QRegularExpressionMatchIterator it = re.globalMatch(result);

    // 从后向前替换，避免位置偏移
    QList<QPair<int, int>> replacements;  // <start, length>
    QStringList newTexts;

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        int num = match.captured(2).toInt();
        if (num >= 1 && num <= 3) {
            int newNum = num - 1;
            QString prefix = match.captured(1);  // "图" 或 "图片"
            QString newText = QString("@%1%2").arg(prefix).arg(newNum);

            replacements.prepend(qMakePair(match.capturedStart(), match.capturedLength()));
            newTexts.prepend(newText);
        }
    }

    // 执行替换
    for (int i = 0; i < replacements.size(); ++i) {
        result.replace(replacements[i].first, replacements[i].second, newTexts[i]);
    }

    return result;
}
```

- [ ] **Step 2: 验证编译**

Run: `cd /Users/wuyingyi/Downloads/code/test2/demos/Qt4test && make`
Expected: 编译通过

- [ ] **Step 3: Commit**

```bash
git add widgets/videogen.cpp
git commit -m "feat: 实现图片引用 1-based 到 0-based 转换"
```

---

### Task 3: 集成到 generateVideo()

**Files:**
- Modify: `widgets/videogen.cpp:1368-1579`

- [ ] **Step 1: 在 generateVideo() 开头规范化提示词**

在 `videogen.cpp:1370` 行（获取 prompt 后）添加转换调用：

```cpp
void VideoSingleTab::generateVideo()
{
    QString prompt = promptInput->toPlainText().trimmed();
    if (prompt.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入视频生成提示词");
        return;
    }

    // 规范化图片引用：@图1 → @图0, @图片2 → @图片1
    prompt = normalizeImageReferences(prompt);

    // ... 后续代码保持不变
```

- [ ] **Step 2: 验证编译**

Run: `cd /Users/wuyingyi/Downloads/code/test2/demos/Qt4test && make`
Expected: 编译通过

- [ ] **Step 3: Commit**

```bash
git add widgets/videogen.cpp
git commit -m "feat: 在视频生成前规范化图片引用索引"
```

---

### Task 4: 手动测试验证

**Files:**
- Test: 启动应用，测试不同场景

- [ ] **Step 1: 测试基本转换**

测试场景：
1. 上传 3 张图片
2. 提示词输入：`让 @图1 飞起来，@图2 在地上，@图3 在天上`
3. 点击生成，检查数据库中保存的 prompt 是否为：`让 @图0 飞起来，@图1 在地上，@图2 在天上`

验证方法：
```bash
sqlite3 ~/.local/share/ChickenAI/chicken_ai.db "SELECT prompt FROM video_tasks ORDER BY id DESC LIMIT 1;"
```

Expected: 提示词中的索引已转换为 0-based

- [ ] **Step 2: 测试 @图片N 格式**

测试场景：
1. 提示词输入：`@图片1 变成背景，@图片2 在前景`
2. 点击生成

Expected: 转换为 `@图片0 变成背景，@图片1 在前景`

- [ ] **Step 3: 测试混合格式**

测试场景：
1. 提示词输入：`@图1 和 @图片3 一起飞`
2. 点击生成

Expected: 转换为 `@图0 和 @图片2 一起飞`

- [ ] **Step 4: 测试边界情况**

测试场景：
1. 提示词输入：`@图4 不存在，@图0 已经是0`
2. 点击生成

Expected: 保持不变（@图4 超出范围，@图0 不转换）

- [ ] **Step 5: 测试无引用场景**

测试场景：
1. 提示词输入：`一只猫在跑步`（无图片引用）
2. 点击生成

Expected: 提示词不变

- [ ] **Step 6: Commit 测试通过标记**

```bash
git commit --allow-empty -m "test: 验证图片引用索引转换功能"
```

---

## 可能的问题

### 问题 1: 正则表达式匹配范围
**症状**: `@图10` 被错误转换
**原因**: 正则 `\d+` 匹配多位数字
**解决**: 当前实现限制 `num >= 1 && num <= 3`，超出范围不转换

### 问题 2: 繁体字"圖"
**症状**: `@圖1` 不被转换
**原因**: 正则只匹配简体"图"
**解决**: 如需支持繁体，修改正则为 `@([图圖]片?)`

### 问题 3: 替换顺序导致位置偏移
**症状**: 多个引用时转换错误
**原因**: 从前向后替换导致后续位置偏移
**解决**: 当前实现从后向前替换（`prepend`）

---

## 测试用例矩阵

| 输入 | 预期输出 | 场景 |
|------|---------|------|
| `@图1` | `@图0` | 基本转换 |
| `@图2` | `@图1` | 基本转换 |
| `@图3` | `@图2` | 基本转换 |
| `@图片1` | `@图片0` | 带"片"字 |
| `@图片2` | `@图片1` | 带"片"字 |
| `@图1 @图2` | `@图0 @图1` | 多个引用 |
| `@图1和@图片3` | `@图0和@图片2` | 混合格式 |
| `@图4` | `@图4` | 超出范围 |
| `@图0` | `@图0` | 已是0-based |
| `一只猫` | `一只猫` | 无引用 |
| `@圖1` | `@圖1` | 繁体字（不支持）|

---

## 未解决的问题

1. **是否需要支持繁体字"圖"？**
   - 当前不支持，如需支持需修改正则

2. **是否需要支持超过 3 张图片？**
   - 当前限制 1-3，如需扩展需修改条件

3. **是否需要向后兼容旧提示词？**
   - 用户确认：全新功能，无需兼容

---
