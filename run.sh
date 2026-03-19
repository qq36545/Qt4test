#!/bin/bash
# ChickenAI 启动脚本 - 避免Qt库冲突

# 获取脚本所在目录
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
APP_PATH="${SCRIPT_DIR}/build/ChickenAI.app"

# 设置环境变量，优先使用应用包内的Qt库
export DYLD_LIBRARY_PATH="${APP_PATH}/Contents/Frameworks:${DYLD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="${APP_PATH}/Contents/PlugIns"

# 运行应用
"${APP_PATH}/Contents/MacOS/ChickenAI" "$@"
