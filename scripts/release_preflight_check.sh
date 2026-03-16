#!/usr/bin/env bash

set -euo pipefail

TAG=""
REPO=""
VERSION_TXT=""
VERSION_JSON=""
ASSET_DIR=""

PASS_COUNT=0
FAIL_COUNT=0

log_pass() {
    echo "[PASS] $*"
    PASS_COUNT=$((PASS_COUNT + 1))
}

log_fail() {
    echo "[FAIL] $*"
    FAIL_COUNT=$((FAIL_COUNT + 1))
}

usage() {
    cat <<'EOF'
用法:
  scripts/release_preflight_check.sh \
    --tag v1.0.2.1 \
    --repo owner/repo \
    --version-txt version.txt \
    --version-json releases/version.json \
    --asset-dir /path/to/assets
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --tag)
            TAG="${2:-}"
            shift 2
            ;;
        --repo)
            REPO="${2:-}"
            shift 2
            ;;
        --version-txt)
            VERSION_TXT="${2:-}"
            shift 2
            ;;
        --version-json)
            VERSION_JSON="${2:-}"
            shift 2
            ;;
        --asset-dir)
            ASSET_DIR="${2:-}"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "未知参数: $1"
            usage
            exit 1
            ;;
    esac
done

if [[ -z "$TAG" || -z "$REPO" || -z "$VERSION_TXT" || -z "$VERSION_JSON" || -z "$ASSET_DIR" ]]; then
    echo "参数不完整"
    usage
    exit 1
fi

if [[ ! -f "$VERSION_TXT" ]]; then
    echo "version.txt 不存在: $VERSION_TXT"
    exit 1
fi

if [[ ! -f "$VERSION_JSON" ]]; then
    echo "version.json 不存在: $VERSION_JSON"
    exit 1
fi

if [[ ! -d "$ASSET_DIR" ]]; then
    echo "asset 目录不存在: $ASSET_DIR"
    exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
    echo "缺少 python3"
    exit 1
fi

if ! command -v curl >/dev/null 2>&1; then
    echo "缺少 curl"
    exit 1
fi

if ! command -v shasum >/dev/null 2>&1; then
    echo "缺少 shasum"
    exit 1
fi

VERSION_RAW="$(tr -d '[:space:]' < "$VERSION_TXT")"
if [[ -z "$VERSION_RAW" ]]; then
    echo "version.txt 为空"
    exit 1
fi

if [[ "$VERSION_RAW" != *.*.* ]]; then
    echo "version.txt 不是合法版本号(至少三段): $VERSION_RAW"
    exit 1
fi

VERSION_3="$(echo "$VERSION_RAW" | awk -F. '{print $1 "." $2 "." $3}')"
EXPECTED_TAG="v${VERSION_RAW}"

if [[ "$TAG" == "$EXPECTED_TAG" ]]; then
    log_pass "tag 与 version.txt 一致: $TAG"
else
    log_fail "tag 不一致，期望 ${EXPECTED_TAG}，实际 ${TAG}"
fi

JSON_DATA="$(python3 - "$VERSION_JSON" <<'PY'
import json, sys

with open(sys.argv[1], 'r', encoding='utf-8') as f:
    d = json.load(f)

platforms = ("windows_x64", "macos_arm64", "macos_intel")
channels = ("github", "gitee", "cloudflare")

print("JSON_VERSION\t" + str(d.get('version', '') or ''))

for p in platforms:
    for c in channels:
        v = str(d.get('downloadUrls', {}).get(p, {}).get(c, '') or '')
        print(f"URL_{p}_{c}\t{v}")

for p in platforms:
    sha = str(d.get('sha256', {}).get(p, '') or '')
    size = str(d.get('fileSize', {}).get(p, '') or '')
    print(f"SHA_{p}\t{sha}")
    print(f"SIZE_{p}\t{size}")
PY
)"

json_get() {
    local key="$1"
    printf '%s\n' "$JSON_DATA" | awk -F'\t' -v k="$key" '$1==k { sub(/^[^\t]*\t/, ""); print; found=1; exit } END { if (!found) print "" }'
}

JSON_VERSION="$(json_get "JSON_VERSION")"

if [[ "$JSON_VERSION" == "$VERSION_3" ]]; then
    log_pass "version.json.version 与 version.txt 前三段一致: $JSON_VERSION"
else
    log_fail "version.json.version 不一致，期望 ${VERSION_3}，实际 ${JSON_VERSION}"
fi

WIN_FILE="ChickenAI-${VERSION_RAW}-win64-setup.exe"
MAC_ARM_FILE="ChickenAI-${VERSION_RAW}-macos-arm64.dmg"
MAC_INTEL_FILE="ChickenAI-${VERSION_RAW}-macos-x86_64.dmg"

check_file_exists() {
    local f="$1"
    if [[ -f "$ASSET_DIR/$f" ]]; then
        log_pass "产物存在: $f"
    else
        log_fail "产物缺失: $f"
    fi
}

check_file_exists "$WIN_FILE"
check_file_exists "$MAC_ARM_FILE"
check_file_exists "$MAC_INTEL_FILE"

is_valid_url() {
    local u="$1"
    [[ "$u" =~ ^https?://[^[:space:]]+$ ]]
}

check_http_200() {
    local url="$1"
    local code
    code="$(curl -sS -L -I -o /dev/null -w '%{http_code}' "$url" || true)"

    if [[ "$code" == "200" ]]; then
        return 0
    fi

    code="$(curl -sS -L -o /dev/null -w '%{http_code}' "$url" || true)"
    [[ "$code" == "200" ]]
}

platforms=("windows_x64" "macos_arm64" "macos_intel")
channels=("github" "gitee" "cloudflare")

platform_file_for() {
    local platform="$1"
    case "$platform" in
        windows_x64) echo "$WIN_FILE" ;;
        macos_arm64) echo "$MAC_ARM_FILE" ;;
        macos_intel) echo "$MAC_INTEL_FILE" ;;
        *) echo "" ;;
    esac
}

lower_hex() {
    echo "$1" | tr '[:upper:]' '[:lower:]'
}

for platform in "${platforms[@]}"; do
    file_name="$(platform_file_for "$platform")"

    for channel in "${channels[@]}"; do
        url="$(json_get "URL_${platform}_${channel}")"

        if [[ "$channel" == "github" ]]; then
            if [[ -z "$url" ]]; then
                log_fail "${platform}.github 不能为空"
                continue
            fi

            if ! is_valid_url "$url"; then
                log_fail "${platform}.github URL 非法: $url"
                continue
            fi

            expected_url="https://github.com/${REPO}/releases/download/${TAG}/${file_name}"
            if [[ "$url" == "$expected_url" ]]; then
                log_pass "${platform}.github URL 命名匹配"
            else
                log_fail "${platform}.github URL 不匹配，期望 ${expected_url}，实际 ${url}"
            fi

            if check_http_200 "$url"; then
                log_pass "${platform}.github 可达 (HTTP 200)"
            else
                log_fail "${platform}.github 不可达 (非 200): $url"
            fi

            sha_value="$(json_get "SHA_${platform}")"
            size_value="$(json_get "SIZE_${platform}")"

            if [[ -z "$sha_value" ]]; then
                log_fail "${platform}.github 非空时 sha256.${platform} 必填"
            fi
            if [[ -z "$size_value" || "$size_value" == "0" ]]; then
                log_fail "${platform}.github 非空时 fileSize.${platform} 必填且>0"
            fi

            if [[ -f "$ASSET_DIR/$file_name" ]]; then
                actual_sha="$(shasum -a 256 "$ASSET_DIR/$file_name" | awk '{print $1}')"
                actual_size="$(wc -c < "$ASSET_DIR/$file_name" | tr -d '[:space:]')"

                if [[ -n "$sha_value" ]]; then
                    if [[ "$(lower_hex "$sha_value")" == "$(lower_hex "$actual_sha")" ]]; then
                        log_pass "sha256.${platform} 匹配"
                    else
                        log_fail "sha256.${platform} 不匹配，json=${sha_value} actual=${actual_sha}"
                    fi
                fi

                if [[ -n "$size_value" && "$size_value" != "0" ]]; then
                    if [[ "$size_value" == "$actual_size" ]]; then
                        log_pass "fileSize.${platform} 匹配"
                    else
                        log_fail "fileSize.${platform} 不匹配，json=${size_value} actual=${actual_size}"
                    fi
                fi
            else
                log_fail "缺少本地产物用于完整性校验: $ASSET_DIR/$file_name"
            fi
        else
            if [[ -z "$url" ]]; then
                log_pass "${platform}.${channel} 为空，按策略放行"
            else
                if is_valid_url "$url"; then
                    log_pass "${platform}.${channel} URL 语法合法"
                else
                    log_fail "${platform}.${channel} URL 语法非法: $url"
                fi
            fi
        fi
    done
done

echo
echo "检查完成: PASS=${PASS_COUNT}, FAIL=${FAIL_COUNT}"

if [[ $FAIL_COUNT -gt 0 ]]; then
    exit 1
fi

exit 0
