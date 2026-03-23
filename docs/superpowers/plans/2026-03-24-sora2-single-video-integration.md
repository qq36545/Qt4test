# Sora2 单视频接入（方案A独立页）Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在“AI视频生成-单个”新增“Sora2视频”独立页面，支持“统一格式/OpenAI格式”切换、5个变体、多图上传、历史入库与“单个记录”展示回填。

**Architecture:** 采用方案A：新增 `Sora2GenPage` 与现有 VEO/Grok/WAN 解耦；`VideoSingleTab` 仅负责页面挂载与分发。网络层在 `VideoAPI` 中新增 Sora2 专属创建/查询分支，保持现有模型路径不回归。历史记录复用 `video_history` 现有字段，不改表结构，通过 `model_variant` 统一写入“变体-文生视频/图生视频”。

**Tech Stack:** Qt Widgets (C++), QStackedWidget, QNetworkAccessManager, multipart/form-data, JSON API, SQLite(DBManager)

---

## 0. 范围与验收

### 功能范围（本计划内）
1. 单个模型下拉新增：`Sora2视频`
2. Sora2 页面支持两种 API 格式：
   - 统一格式：`/v1/video/create`（JSON）
   - OpenAI格式：`/v1/videos`（multipart）
3. 两种格式都支持 5 个变体：
   - `sora-2-all`
   - `sora-2`
   - `sora-2-vip-all`
   - `sora-2-pro`
   - `sora-2-pro-all`
4. 多图参考上传（不使用首帧/尾帧语义）
5. 历史记录进入“AI视频生成-单个记录”，视频类型显示：`变体模型-文生视频/图生视频`

### 非目标（本计划不做）
- 不改数据库 schema
- 不改 VEO/Grok/WAN 现有交互
- 不做批量页接入

---

## 1. 文件结构（按批次拆分，单任务≤3文件）

### 批次A：Sora2 页面骨架与路由
- Create: `widgets/sora2genpage.h`
- Create: `widgets/sora2genpage.cpp`
- Modify: `widgets/videogen.h`

### 批次B：主页面挂载与历史入口路由
- Modify: `widgets/videogen.cpp`
- Modify: `widgets/videogen.h`
- Modify: `widgets/sora2genpage.h`（信号/回填接口微调）

### 批次C：网络层 Sora2 请求分支
- Modify: `network/videoapi.h`
- Modify: `network/videoapi.cpp`
- Modify: `widgets/sora2genpage.cpp`（调用参数对齐）

### 批次D：历史入库与回填
- Modify: `widgets/sora2genpage.cpp`
- Modify: `widgets/videogen.cpp`
- Modify: `database/dbmanager.cpp`（仅在发现字段映射缺口时；优先不改）

---

## 2. 参数映射真值表（实现时按此编码）

### 2.1 统一格式（`/v1/video/create`）
Request(JSON):
- `model`: 5变体之一
- `prompt`: 文本
- `images`: URL数组（图生时可为空/非空按产品判定）
- `size`: `small` | `large`
- `orientation`: `portrait` | `landscape`
- `duration`: `5` | `10`
- `watermark`: bool
- `private`: bool

### 2.2 OpenAI格式（`/v1/videos`）
Request(multipart/form-data):
- `model`
- `prompt`
- `seconds`（按文档能力集合）
- `size`（按模型能力集合）
- `input_reference`（多图：多 part）
- `watermark`
- `private`
- `style`（可选，默认空，中文标签）

### 2.3 UI值到API值
- 分辨率（统一格式）
  - UI: `small(720p)` -> API: `small`
  - UI: `large(1080p)` -> API: `large`
- 方向
  - UI: `竖屏` -> `portrait`
  - UI: `横屏` -> `landscape`
- 视频类型（历史）
  - `imagePaths.isEmpty()` -> `文生视频`
  - 否则 -> `图生视频`
  - `modelVariant = <variant> + "-" + <文生/图生>`

---

## Chunk 1: 新增 Sora2GenPage（独立页，不耦合 VEO）

### Task 1: 创建页面类与最小可用 UI

**Files:**
- Create: `widgets/sora2genpage.h`
- Create: `widgets/sora2genpage.cpp`
- Modify: `widgets/videogen.h`

- [ ] **Step 1: 新建头文件，定义页面能力**

```cpp
class Sora2GenPage : public QWidget {
    Q_OBJECT
public:
    explicit Sora2GenPage(QWidget *parent = nullptr);
    void loadFromTask(const VideoTask &task);

signals:
    void createTaskRequested(const QVariantMap &payload);

private slots:
    void onSubmitClicked();
    void onApiFormatChanged(int index);
    void onUploadImagesClicked();

private:
    QString currentApiFormat() const;   // "unified" | "openai"
    QString currentVariant() const;     // 5 variants
    QStringList currentImagePaths() const;
    bool isImageToVideo() const;
    QVariantMap buildUnifiedPayload() const;
    QVariantMap buildOpenAIPayload() const;
};
```

- [ ] **Step 2: 新建 cpp，先搭建控件与默认值**

必须包含：
1. API格式下拉：`统一格式` / `OpenAI格式`
2. 变体下拉：5变体
3. prompt 输入
4. 多图缩略图区域 + “上传参考图”
5. 统一格式参数区：size/orientation/duration
6. OpenAI参数区：seconds/size/style(可空)
7. watermark/private
8. “开始生成”按钮

- [ ] **Step 3: 编译验证（仅页面接入声明，不挂载）**

Run: `cmake --build build -j4`
Expected: 编译通过

- [ ] **Step 4: Commit**

```bash
git add widgets/sora2genpage.h widgets/sora2genpage.cpp widgets/videogen.h
git commit -m "feat: 新增 Sora2GenPage 独立页面骨架"
```

---

## Chunk 2: VideoSingleTab 挂载 Sora2 页面并切换

### Task 2: 页面路由接线

**Files:**
- Modify: `widgets/videogen.cpp`
- Modify: `widgets/videogen.h`
- Modify: `widgets/sora2genpage.h`

- [ ] **Step 1: 在 `VideoSingleTab` 成员中新增页面指针**

在 `widgets/videogen.h`：
```cpp
class Sora2GenPage;
Sora2GenPage *sora2Page = nullptr;
```

- [ ] **Step 2: 扩展模型下拉与 stack**

在 `widgets/videogen.cpp` 的初始化处：
```cpp
modelCombo->addItems({"VEO3视频", "Grok3视频", "WAN视频", "Sora2视频"});
stack->addWidget(sora2Page); // 新索引
```

- [ ] **Step 3: 扩展切页逻辑和 loadFromTask 分发**

要求：
1. 选择 `Sora2视频` 时切到 `sora2Page`
2. 历史回填时，如果任务属于 Sora2 变体，路由到 `sora2Page->loadFromTask(task)`

- [ ] **Step 4: 编译验证**

Run: `cmake --build build -j4`
Expected: 编译通过，模型下拉出现“Sora2视频”

- [ ] **Step 5: Commit**

```bash
git add widgets/videogen.h widgets/videogen.cpp widgets/sora2genpage.h
git commit -m "feat: VideoSingleTab 挂载并路由 Sora2 页面"
```

---

## Chunk 3: VideoAPI 新增 Sora2 请求分支

### Task 3: 网络层对齐两种协议

**Files:**
- Modify: `network/videoapi.h`
- Modify: `network/videoapi.cpp`
- Modify: `widgets/sora2genpage.cpp`

- [ ] **Step 1: 在 `videoapi.h` 增加 Sora2 创建接口（建议最小增量）**

```cpp
void createSora2UnifiedVideo(const QString &apiKey, const QString &baseUrl, const QVariantMap &payload);
void createSora2OpenAIVideo(const QString &apiKey, const QString &baseUrl, const QVariantMap &payload);
```

- [ ] **Step 2: 在 `videoapi.cpp` 实现统一格式请求**

关键点：
1. POST `baseUrl + "/v1/video/create"`
2. `Content-Type: application/json`
3. `Authorization: Bearer <apiKey>`
4. body 包含 `images/model/orientation/prompt/size/duration/watermark/private`

- [ ] **Step 3: 在 `videoapi.cpp` 实现 OpenAI multipart 请求**

关键点：
1. POST `baseUrl + "/v1/videos"`
2. multipart 字段包含 `model/prompt/seconds/size/watermark/private/style`
3. 多图时追加多个 `input_reference` part

- [ ] **Step 4: 查询接口沿用 `/v1/videos/{id}`，仅补充 Sora2 条件分支**

要求：
- 不影响 Grok/WAN 既有分支
- Sora2 创建后统一进入现有轮询信号链

- [ ] **Step 5: 页面提交逻辑改为调用上述新接口**

在 `Sora2GenPage::onSubmitClicked()`：
```cpp
if (currentApiFormat() == "unified") {
    emit createTaskRequested(buildUnifiedPayload());
} else {
    emit createTaskRequested(buildOpenAIPayload());
}
```

- [ ] **Step 6: 编译验证**

Run: `cmake --build build -j4`
Expected: 编译通过，点击生成发出请求（可抓包确认 endpoint）

- [ ] **Step 7: Commit**

```bash
git add network/videoapi.h network/videoapi.cpp widgets/sora2genpage.cpp
git commit -m "feat: VideoAPI 新增 Sora2 统一/OpenAI 双协议创建"
```

---

## Chunk 4: 历史记录写入与“视频类型”展示

### Task 4: 入库字段与展示规则

**Files:**
- Modify: `widgets/sora2genpage.cpp`
- Modify: `widgets/videogen.cpp`
- Modify: `database/dbmanager.cpp`（仅必要）

- [ ] **Step 1: 创建任务时构造 `modelVariant` 显示值**

规则：
```cpp
QString taskKind = imagePaths.isEmpty() ? "文生视频" : "图生视频";
QString modelVariantForHistory = variant + "-" + taskKind;
```

并写入 `VideoTask.modelVariant`。

- [ ] **Step 2: `taskType` 统一走单个页链路**

要求：
- 进入“生成历史记录 -> AI视频生成-单个记录”
- 与当前单个页历史筛选条件兼容

- [ ] **Step 3: 回填逻辑支持 Sora2 专属参数**

`loadFromTask` 至少回填：
- prompt
- variant
- API格式（可通过 detail/extra 或默认策略）
- 多图路径
- unified/openai 对应参数

- [ ] **Step 4: 编译验证**

Run: `cmake --build build -j4`
Expected: 编译通过

- [ ] **Step 5: Commit**

```bash
git add widgets/sora2genpage.cpp widgets/videogen.cpp
# 如有 db 映射修改再追加 database/dbmanager.cpp
git commit -m "feat: Sora2 历史记录写入与单个记录页回填"
```

---

## Chunk 5: 回归验证（手工）

### Task 5: 端到端场景验证

**Files:**
- Test: 手工 UI + 接口联调

- [ ] **Step 1: 统一格式文生**

操作：Sora2 -> 统一格式 -> 不上传图 -> 提交

Expected:
- 请求到 `/v1/video/create`
- 历史 `modelVariant` 类似 `sora-2-pro-文生视频`
- 在“AI视频生成-单个记录”可见

- [ ] **Step 2: 统一格式图生（多图）**

操作：上传 2~6 张图提交

Expected:
- `images` 数组包含全部上传图 URL
- 历史显示 `...-图生视频`

- [ ] **Step 3: OpenAI格式文生 + style空值**

Expected:
- 请求到 `/v1/videos`
- 未选 style 时不传或传空（按实现约定一致）

- [ ] **Step 4: OpenAI格式图生（多图）**

Expected:
- multipart 包含多个 `input_reference`

- [ ] **Step 5: 历史回填后不自动生成**

Expected:
- 点击历史项只回填参数，不触发 submit

- [ ] **Step 6: 回归旧模型**

Expected:
- VEO/Grok/WAN 行为与改动前一致

- [ ] **Step 7: Commit（测试记录）**

```bash
git commit --allow-empty -m "test: 验证 Sora2 单视频双协议与历史链路"
```

---

## 3. 可能问题与覆盖建议

1. **文档示例与枚举值存在不一致（seconds/size）**
   - 风险：UI可选值与后端校验冲突
   - 覆盖：每个变体各发一条最小请求，记录 200/4xx

2. **OpenAI多图 `input_reference` 兼容性**
   - 风险：后端只收单文件或字段名差异
   - 覆盖：1图/2图/6图三档抓包与返回对比

3. **历史回填缺失 API格式信息**
   - 风险：重新生成落到错误参数区
   - 覆盖：统一/OpenAI 各生成1条后分别回填检查

4. **旧模型回归风险**
   - 风险：`VideoSingleTab` 路由改动导致索引错位
   - 覆盖：VEO/Grok/WAN 各跑一条最小流程

---

## 4. 执行守则

- 严格按 Chunk 顺序执行；每个 Task 完成即提交
- 任一 Task 若预计改动 >3 文件，立即拆分为新 Task
- 不新增抽象层，不复用 VEO 内部私有实现，保持 Sora2 独立

---

## 未决问题（如无则写“无”）

- 无
