#!/bin/bash
# ChickenAI 启动脚本 - 避免Qt库冲突

# 获取脚本所在目录
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# 设置环境变量，优先使用应用包内的Qt库
export DYLD_LIBRARY_PATH="${SCRIPT_DIR}/ChickenAI.app/Contents/Frameworks:${DYLD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="${SCRIPT_DIR}/ChickenAI.app/Contents/PlugIns"

# 运行应用
"${SCRIPT_DIR}/ChickenAI.app/Contents/MacOS/ChickenAI" "$@"
