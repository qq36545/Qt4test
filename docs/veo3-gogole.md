# Google Gemini API - Veo 3.1 视频生成

> **文档来源**: https://ai.google.dev/gemini-api/docs/video?hl=zh-cn
>
> **更新日期**: 2026-03-09

## 概述

Veo 3.1 是 Google 最先进的视频生成模型，可生成高保真 8 秒视频，具有惊人的逼真效果和原生生成的音频。通过 Gemini API 可以编程访问此模型。

## 核心特性

### 1. 视频质量
- **分辨率**: 720p、1080p 或 4K
- **时长**: 8 秒
- **音频**: 原生生成的音频
- **画质**: 高保真、惊人的逼真效果

### 2. 新功能

#### 竖屏视频
- 横屏 (16:9) - 默认
- 竖屏 (9:16)

#### 视频扩展
- 扩展之前使用 Veo 生成的视频

#### 指定帧生成
- 通过指定第一帧和/或最后一帧来生成视频

#### 基于图片的指导
- 使用最多 3 张参考图片来指导生成的视频内容

## API 调用方式

### 基础信息
- **Base URL**: `https://generativelanguage.googleapis.com`
- **认证方式**: Google API Key (通过 `x-goog-api-key` header)
- **模型名称**: `veo-3.1-generate-preview`

### 文生视频 (Text-to-Video)

#### Python 示例
```python
import time
from google import genai
from google.genai import types

client = genai.Client()

prompt = """A close up of two people staring at a cryptic drawing
on a wall, torchlight flickering."""

# 生成视频
response = client.models.generate_video(
    model="veo-3.1-generate-preview",
    prompt=prompt,
)

# 轮询操作状态直到视频准备就绪
# (需要实现轮询逻辑)
```

#### JavaScript 示例
```javascript
import { GoogleGenAI } from "@google/genai";

const ai = new GoogleGenAI({});

const prompt = `A close up of two people staring at a cryptic drawing
on a wall, torchlight flickering.`;

const response = await ai.models.generateVideo({
    model: "veo-3.1-generate-preview",
    prompt: prompt,
});

// 轮询操作状态直到视频准备就绪
```

#### cURL 示例
```bash
BASE_URL="https://generativelanguage.googleapis.com"

curl "${BASE_URL}/v1/models/veo-3.1-generate-preview:predictLongRunning" \
  -H "x-goog-api-key: $GEMINI_API_KEY" \
  -H "Content-Type: application/json" \
  -X "POST" \
  -d '{
    "instances": [{
      "prompt": "A close up of two people staring at a cryptic drawing on a wall"
    }]
  }'
```

### 控制宽高比

使用 `aspect_ratio` 参数控制视频比例：

#### Python 示例
```python
prompt = """A montage of pizza making: a chef tossing and flattening
the floury dough, ladling rich red tomato sauce in a spiral."""

response = client.models.generate_video(
    model="veo-3.1-generate-preview",
    prompt=prompt,
    config=types.GenerateVideoConfig(
        aspect_ratio="9:16"  # 竖屏视频
    )
)
```

#### JavaScript 示例
```javascript
const response = await ai.models.generateVideo({
    model: "veo-3.1-generate-preview",
    prompt: prompt,
    config: {
        aspectRatio: "9:16"  // 竖屏视频
    }
});
```

### 异步处理

视频生成是异步操作，需要：
1. 发起生成请求，获得 `operation_name`
2. 轮询操作状态（建议间隔 5-10 秒）
3. 检查 `done` 字段判断是否完成
4. 从响应中获取 `video_uri`
5. 使用 API Key 下载视频

#### 轮询示例 (Bash)
```bash
while true; do
    status_response=$(curl -s -H "x-goog-api-key: $GEMINI_API_KEY" \
        "${BASE_URL}/${operation_name}")

    done=$(echo "$status_response" | jq -r '.done')

    if [ "$done" = "true" ]; then
        video_uri=$(echo "$status_response" | jq -r '.response.uri')
        echo "Downloading video from: ${video_uri}"

        curl -L -o output.mp4 \
            -H "x-goog-api-key: $GEMINI_API_KEY" \
            "${video_uri}"
        break
    fi

    sleep 10
done
```

## API 参数

### 必需参数
| 参数 | 类型 | 说明 |
|------|------|------|
| `model` | string | 模型名称：`veo-3.1-generate-preview` |
| `prompt` | string | 视频生成提示词 |

### 可选参数
| 参数 | 类型 | 可选值 | 默认值 | 说明 |
|------|------|--------|--------|------|
| `aspect_ratio` | string | `16:9`, `9:16` | `16:9` | 视频宽高比 |
| `first_frame` | image | - | - | 指定第一帧图片 |
| `last_frame` | image | - | - | 指定最后一帧图片 |
| `reference_images` | image[] | - | - | 参考图片（最多3张）|

## 与独立 Veo 3 API 的对比

| 特性 | Veo 3 (vectorengine.ai) | Veo 3.1 (Gemini API) |
|------|-------------------------|----------------------|
| 模型版本 | veo_3_1, veo_3_1-fast | veo-3.1-generate-preview |
| 认证方式 | Bearer Token | Google API Key |
| API 端点 | `/v1/videos` | `/v1/models/veo-3.1-generate-preview` |
| 分辨率 | 16x9, 9x16, 1x1 | 720p/1080p/4K (16:9 或 9:16) |
| 时长 | 8/10/15 秒 | 8 秒 |
| 垫图 | 必需 | 可选（参考图片）|
| 水印 | 可选 | 未提及 |
| 音频 | 未提及 | 原生生成 |
| 帧控制 | 不支持 | 支持首尾帧指定 |
| 视频扩展 | 不支持 | 支持 |

## 提示词指南

详细的提示词编写指南请参考：[Veo 提示指南](https://ai.google.dev/gemini-api/docs/prompting/veo)

## 模型版本

Gemini API 提供多个 Veo 模型变体，详情请参考官方文档的"模型版本"部分。

## 使用建议

### 适合使用 Gemini API Veo 3.1 的场景
- 需要高质量视频（720p/1080p/4K）
- 需要原生音频生成
- 需要帧控制或视频扩展功能
- 已在使用 Gemini API 生态系统
- 需要参考图片指导（最多3张）

### 适合使用独立 Veo 3 API 的场景
- 需要快速模式（veo_3_1-fast）
- 需要更灵活的时长选择（8/10/15秒）
- 已有 vectorengine.ai 的集成
- 需要强制垫图功能

## 定价和配额

请访问 [Gemini API 定价页面](https://ai.google.dev/pricing) 了解详细信息。

## 参考资源

- [官方文档](https://ai.google.dev/gemini-api/docs/video)
- [视频理解指南](https://ai.google.dev/gemini-api/docs/video-understanding)
- [Veo 提示指南](https://ai.google.dev/gemini-api/docs/prompting/veo)
- [Google AI Studio](https://aistudio.google.com/)

---

**文档状态**: ✅ 已完成（基于官方文档）
