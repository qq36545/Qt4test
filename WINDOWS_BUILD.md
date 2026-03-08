# Windows 安装包构建指南

## 文件清单

已创建/修改的文件：

1. **CMakeLists.txt** - 添加 Windows 平台支持
2. **.github/workflows/build-windows.yml** - GitHub Actions 自动构建配置
3. **installer.nsi** - NSIS 安装脚本
4. **resources/app.rc** - Windows 资源文件（图标引用）
5. **resources/app.ico.txt** - 图标占位说明
6. **LICENSE.txt** - MIT 许可证（NSIS 安装时显示）
7. **.gitignore** - Git 忽略规则

## 使用方法

### 方式 1：GitHub Actions 自动构建（推荐）

1. **创建 GitHub 仓库**
   ```bash
   # 在 GitHub 上创建新仓库，然后：
   git remote add origin https://github.com/你的用户名/Qt4test.git
   git add .
   git commit -m "Initial commit with Windows build support"
   git branch -M main
   git push -u origin main
   ```

2. **触发构建**
   - 推送代码后，GitHub Actions 自动开始构建
   - 访问仓库的 Actions 标签页查看进度

3. **下载安装包**
   - 构建完成后，在 Actions 页面下载 `Qt4test-Windows-Installer` artifact
   - 解压得到 `Qt4test-1.0.0-win64-setup.exe`

4. **发布版本（可选）**
   ```bash
   git tag v1.0.0
   git push origin v1.0.0
   ```
   - 推送 tag 后，安装包会自动发布到 GitHub Releases

### 方式 2：本地 Windows 构建

**环境要求**：
- Windows 10/11 64-bit
- Qt 6.10.2 (MSVC 2022)
- CMake 3.16+
- Visual Studio 2022
- NSIS 3.x

**构建步骤**：
```cmd
# 1. 配置
mkdir build
cd build
cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release

# 2. 编译
nmake

# 3. 准备部署目录
mkdir ..\deploy
copy Qt4test.exe ..\deploy\
copy ..\LICENSE.txt ..\deploy\

# 4. 收集 Qt 依赖
cd ..\deploy
windeployqt --release --no-translations Qt4test.exe

# 5. 打包安装程序
cd ..
makensis installer.nsi
```

## 安装包特性

- **文件名**：`Qt4test-1.0.0-win64-setup.exe`
- **大小**：约 50-80 MB（包含所有 Qt 依赖）
- **安装位置**：`C:\Program Files\Qt4test\`
- **快捷方式**：
  - 桌面快捷方式
  - 开始菜单文件夹
- **卸载**：通过"添加或删除程序"或开始菜单的卸载快捷方式

## 待完成事项

### 1. 应用图标（重要）

当前使用占位符，需要创建真实的 `.ico` 文件：

**方法 A：在线工具**
1. 访问 https://www.icoconverter.com/
2. 上传 PNG 图片（建议 512x512）
3. 下载生成的 `app.ico`
4. 替换 `resources/app.ico.txt` 为 `resources/app.ico`

**方法 B：ImageMagick**
```bash
# 从 PNG 生成多尺寸 ICO
convert icon.png -define icon:auto-resize=256,128,64,48,32,16 resources/app.ico
```

### 2. 代码签名（可选）

如果需要避免 Windows SmartScreen 警告：

1. 购买代码签名证书（如 DigiCert, Sectigo）
2. 在 GitHub Actions 中配置签名：
   ```yaml
   - name: Sign installer
     run: |
       signtool sign /f cert.pfx /p ${{ secrets.CERT_PASSWORD }} /t http://timestamp.digicert.com Qt4test-1.0.0-win64-setup.exe
   ```

### 3. 自定义安装界面（可选）

修改 `installer.nsi` 中的图标路径：
```nsis
!define MUI_ICON "resources\app.ico"
!define MUI_UNICON "resources\app.ico"
```

## 故障排查

### 问题 1：GitHub Actions 构建失败

**检查**：
- Actions 日志中的错误信息
- Qt 版本是否匹配（当前配置为 6.10.2）
- CMakeLists.txt 中的 Qt 路径

### 问题 2：安装包无法运行

**可能原因**：
- 缺少 Visual C++ Redistributable
- windeployqt 未正确收集依赖

**解决**：
```cmd
# 手动添加 VC++ 运行时
copy "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\14.xx\x64\Microsoft.VC143.CRT\*.dll" deploy\
```

### 问题 3：NSIS 找不到文件

**检查**：
- `deploy` 目录是否存在
- `LICENSE.txt` 是否在根目录

## 下一步

1. **创建应用图标** - 替换占位符
2. **推送到 GitHub** - 触发自动构建
3. **测试安装包** - 在 Windows 上验证
4. **发布版本** - 创建 GitHub Release

## 技术细节

### GitHub Actions 工作流程

```
1. Checkout 代码
2. 安装 Qt 6.10.2 (MSVC 2022)
3. 配置 MSVC 环境
4. CMake 配置 + NMake 编译
5. 创建 deploy 目录
6. windeployqt 收集依赖
7. 安装 NSIS
8. makensis 打包
9. 上传 artifact
10. (可选) 发布到 Releases
```

### NSIS 安装流程

```
1. 显示许可证 (LICENSE.txt)
2. 选择安装目录
3. 复制文件到安装目录
4. 写入注册表
5. 创建快捷方式
6. 生成卸载程序
```

## 参考资料

- [Qt 官方文档 - Windows 部署](https://doc.qt.io/qt-6/windows-deployment.html)
- [NSIS 官方文档](https://nsis.sourceforge.io/Docs/)
- [GitHub Actions - Qt 构建](https://github.com/jurplel/install-qt-action)
