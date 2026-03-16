# 发布前门禁（preflight）SOP

目标：在发布 `releases/version.json` 前，阻断以下漂移：

- `tag`
- `version.txt`
- `releases/version.json.version`
- 产物文件名
- `sha256`
- `fileSize`

> 本门禁不改升级逻辑，仅做发布前一致性检查。

---

## 1. 脚本位置

- `scripts/release_preflight_check.sh`

参数：

- `--tag`：本次发布 tag（如 `v1.0.2.1`）
- `--repo`：GitHub 仓库（如 `owner/repo`）
- `--version-txt`：`version.txt` 路径
- `--version-json`：`releases/version.json` 路径
- `--asset-dir`：本地产物目录

---

## 2. 先决条件

- `python3`
- `curl`
- `shasum`

产物目录需包含以下文件（按 CI/NSIS 命名）：

- `ChickenAI-<V>-win64-setup.exe`
- `ChickenAI-<V>-macos-arm64.dmg`
- `ChickenAI-<V>-macos-x86_64.dmg`

其中 `<V>` = `version.txt` 全量版本（例如 `1.0.2.1`）。

---

## 3. 检查契约（阻断）

1. 版本一致：
   - `tag == v<version.txt>`
   - `releases/version.json.version == <version.txt 前三段>`

2. 文件名一致：
   - windows: `ChickenAI-<V>-win64-setup.exe`
   - macOS arm64: `ChickenAI-<V>-macos-arm64.dmg`
   - macOS intel: `ChickenAI-<V>-macos-x86_64.dmg`

3. metadata 一致：
   - 若 `downloadUrls.<platform>.github` 非空：
     - `sha256.<platform>` 必填
     - `fileSize.<platform>` 必填且 > 0
     - 且与本地产物计算结果一致

4. URL 规则：
   - GitHub：可达性阻断（`curl -I -L`，失败回退 `GET`，最终要求 HTTP 200）
   - `gitee/cloudflare`：
     - 空串放行
     - 非空仅做 URL 语法校验（不做可达性阻断）

---

## 4. 推荐发布顺序

1. 打 tag 触发 CI；
2. Release 产物就绪；
3. 回填 `releases/version.json`：下载链接 / sha256 / fileSize；
4. 运行 preflight；
5. 全部通过后，再发布 metadata。

---

## 5. 使用示例

```bash
bash scripts/release_preflight_check.sh \
  --tag v1.0.2.1 \
  --repo your-org/your-repo \
  --version-txt version.txt \
  --version-json releases/version.json \
  --asset-dir /absolute/path/to/assets
```

返回码：

- `0`：全部通过
- `1`：任一检查失败（阻断）

---

## 6. 失败回滚策略

任一检查失败（典型：404 / SHA 不一致 / size 不一致），立即将 `releases/version.json` 回滚到上一可用版本，再重新发布。

---

## 7. 反例验证（至少 3 条）

1. `tag` 与 `version.txt` 不一致：
   - 例：`--tag v1.0.2.2`，但 `version.txt=1.0.2.1`

2. 任一平台 GitHub URL 404：
   - 例：`downloadUrls.macos_arm64.github` 指向不存在资产

3. 任一平台 SHA 或 size 不一致：
   - 例：`sha256.windows_x64` 或 `fileSize.windows_x64` 与本地产物不一致
