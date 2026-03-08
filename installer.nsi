; Qt4test NSIS 安装脚本
; 用于创建 Windows 安装包

;--------------------------------
; 包含现代 UI
!include "MUI2.nsh"

;--------------------------------
; 基本信息
!define APP_NAME "Qt4test"
!define APP_VERSION "1.0.0"
!define APP_PUBLISHER "Qt4test Team"
!define APP_DESCRIPTION "AI 视频/图片生成演示应用"
!define APP_EXE "Qt4test.exe"

; 安装包输出名称
OutFile "Qt4test-${APP_VERSION}-win64-setup.exe"

; 默认安装目录
InstallDir "$PROGRAMFILES64\${APP_NAME}"

; 从注册表获取上次安装目录
InstallDirRegKey HKLM "Software\${APP_NAME}" "Install_Dir"

; 请求管理员权限
RequestExecutionLevel admin

; 压缩方式
SetCompressor /SOLID lzma

;--------------------------------
; 界面设置
!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

;--------------------------------
; 安装页面
!insertmacro MUI_PAGE_LICENSE "LICENSE.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

; 卸载页面
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; 语言
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"

;--------------------------------
; 版本信息
VIProductVersion "${APP_VERSION}.0"
VIAddVersionKey /LANG=${LANG_SIMPCHINESE} "ProductName" "${APP_NAME}"
VIAddVersionKey /LANG=${LANG_SIMPCHINESE} "CompanyName" "${APP_PUBLISHER}"
VIAddVersionKey /LANG=${LANG_SIMPCHINESE} "FileDescription" "${APP_DESCRIPTION}"
VIAddVersionKey /LANG=${LANG_SIMPCHINESE} "FileVersion" "${APP_VERSION}"

;--------------------------------
; 安装部分
Section "主程序" SecMain
    SectionIn RO

    ; 设置输出路径
    SetOutPath "$INSTDIR"

    ; 复制所有文件（GitHub Actions 会准备好）
    File /r "deploy\*.*"

    ; 写入注册表
    WriteRegStr HKLM "Software\${APP_NAME}" "Install_Dir" "$INSTDIR"

    ; 写入卸载信息
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayName" "${APP_NAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayIcon" "$INSTDIR\${APP_EXE}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "Publisher" "${APP_PUBLISHER}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayVersion" "${APP_VERSION}"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "NoRepair" 1

    ; 创建卸载程序
    WriteUninstaller "$INSTDIR\uninstall.exe"

    ; 创建开始菜单快捷方式
    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\卸载.lnk" "$INSTDIR\uninstall.exe"

    ; 创建桌面快捷方式
    CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"
SectionEnd

;--------------------------------
; 卸载部分
Section "Uninstall"
    ; 删除注册表键
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"
    DeleteRegKey HKLM "Software\${APP_NAME}"

    ; 删除快捷方式
    Delete "$SMPROGRAMS\${APP_NAME}\*.*"
    RMDir "$SMPROGRAMS\${APP_NAME}"
    Delete "$DESKTOP\${APP_NAME}.lnk"

    ; 删除安装目录
    RMDir /r "$INSTDIR"
SectionEnd
