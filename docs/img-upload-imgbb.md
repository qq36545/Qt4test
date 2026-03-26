# 图片上传接口说明（已切换 imageproxy）

> 历史：旧版曾使用 imgbb v1。当前业务链路（Sora2 unified / Grok / WAN 复用上传）统一走 imageproxy。

## 当前协议

- 方法：`POST`
- 地址：`https://imageproxy.zhongzhuan.chat/api/upload`
- Header：`Authorization: Bearer <页面 API 密钥下拉值>`
- Content-Type：`multipart/form-data`
- 文件字段：`file`
- 成功取值：优先 `data.url`

---

## 最小回归验收模板（手工）

### Case 1：Sora2 unified + 有图
- 操作：在 Sora2 unified 模式选择一张参考图并提交。
- 预期：
  - 先请求 `POST https://imageproxy.zhongzhuan.chat/api/upload`
  - 请求头含 `Authorization: Bearer <页面下拉 API Key>`
  - 请求体为 `multipart/form-data` 且包含 `file`
  - 上传成功后，创建视频请求 `images` 使用上传返回 URL（优先 `data.url`）

### Case 2：Sora2 unified + 无图
- 操作：不选图直接提交。
- 预期：
  - 不触发上传接口
  - 直接进入创建视频请求与轮询链路

### Case 3：无效 token / 401
- 操作：使用无效 API Key 提交有图任务。
- 预期：
  - 上传请求失败，可见重试
  - 最终上抛错误前缀为：`图片上传失败:`
  - 提交流程结束后按钮恢复可点击（不锁死）

### Case 4：响应缺少 `data.url`
- 操作：构造上传接口返回 200 但不含 `data.url`。
- 预期：
  - 最终上抛：`图片上传响应缺少data.url字段:`
  - 不进入后续创建视频请求

---

## 验收记录（可复制）

- 日期：
- 提交分支：
- 构建结果：
- Case 1：通过 / 失败（备注）
- Case 2：通过 / 失败（备注）
- Case 3：通过 / 失败（备注）
- Case 4：通过 / 失败（备注）
- 结论：可发布 / 需修复
