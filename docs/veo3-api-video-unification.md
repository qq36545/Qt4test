# 视频统一格式：创建视频

## OpenAPI Specification

```yaml
openapi: 3.0.1
info:
  title: ''
  description: ''
  version: 1.0.0
paths:
  /v1/video/create:
    post:
      summary: 创建视频
      deprecated: false
      description: ''
      tags:
        - 视频模型/veo 视频生成/视频统一格式
      parameters:
        - name: Content-Type
          in: header
          description: ''
          required: true
          example: application/json
          schema:
            type: string
        - name: Accept
          in: header
          description: ''
          required: true
          example: application/json
          schema:
            type: string
        - name: Authorization
          in: header
          description: ''
          required: false
          example: Bearer {{YOUR_API_KEY}}
          schema:
            type: string
      requestBody:
        content:
          application/json:
            schema:
              type: object
              properties:
                model:
                  type: string
                  enum:
                    - veo2
                    - veo2-fast
                    - veo2-fast-frames
                    - veo2-fast-components
                    - veo2-pro
                    - veo2-pro-components
                    - veo3
                    - veo3-fast
                    - veo3-fast-frames
                    - veo3-frames
                    - veo3-pro
                    - veo3-pro-frames
                    - veo3.1
                    - veo3.1-fast
                    - veo3.1-pro
                    - veo3.1-4k
                    - veo3.1-pro-4k
                  x-apifox-enum:
                    - value: veo2
                      name: ''
                      description: Google最新的高级人工智能模型, veo2 fast 模式，质量好速度快
                    - value: veo2-fast
                      name: ''
                      description: Google最新的高级人工智能模型, veo2 fast 模式，质量好速度快
                    - value: veo2-fast-frames
                      name: ''
                      description: Google最新的高级人工智能模型, veo2 fast 模式，支持首尾帧
                    - value: veo2-fast-components
                      name: ''
                      description: >-
                        Google最新的高级人工智能模型, veo2 fast 模式,
                        支持上传图片素材，最终生成的视频会将所有图片合并成视频中的一部分，比如，图片 1 变成背景，图片 2
                        笼罩天空等等
                    - value: veo2-pro
                      name: ''
                      description: Google最新的高级人工智能模型, veo2 高质量 模式，质量很高，同时价格也很昂贵，使用需注意
                    - value: veo2-pro-components
                      name: ''
                      description: >-
                        Google最新的高级人工智能模型, veo2 pro模式,
                        支持上传图片素材，最终生成的视频会将所有图片合并成视频中的一部分，比如，图片 1 变成背景，图片 2
                        笼罩天空等等
                    - value: veo3
                      name: ''
                      description: veo3 谷歌官方最新的视频生成模型，生成的视频带有声音，目前全球独一家带有声音的视频模型
                    - value: veo3-fast
                      name: ''
                      description: >-
                        Google最新的高级人工智能模型, veo3 快速
                        模式，支持视频自动配套音频生成，质量高价格很低，性价比最高的选择
                    - value: veo3-fast-frames
                      name: ''
                      description: >-
                        Google最新的高级人工智能模型, veo3 快速
                        模式，支持视频自动配套音频生成，支持首帧传递，质量高价格很低，性价比最高的选择
                    - value: veo3-frames
                      name: ''
                      description: >-
                        Google最新的高级人工智能模型, veo3 快速
                        模式，支持视频自动配套音频生成，支持首帧传递，质量高价格很低，性价比最高的选择
                    - value: veo3-pro
                      name: ''
                      description: >-
                        Google最新的高级人工智能模型, veo3 高质量
                        模式，支持视频自动配套音频生成，质量超高，价格也超高，使用需注意
                    - value: veo3-pro-frames
                      name: ''
                      description: >-
                        Google最新的高级人工智能模型, veo3 高质量
                        模式，支持视频自动配套音频生成，支持首帧传递，不支持尾帧，质量超高，价格也超高，使用需注意
                    - value: veo3.1
                      name: ''
                      description: >-
                        Google最新的高级人工智能模型, veo3.1 快速
                        模式，支持视频自动配套音频生成，质量高价格很低，性价比最高的选择, 自适应首尾帧和文生视频
                    - value: veo3.1-fast
                      name: ''
                      description: >-
                        Google最新的高级人工智能模型, veo3.1 快速
                        模式，支持视频自动配套音频生成，质量高价格很低，性价比最高的选择, 自适应首尾帧和文生视频
                    - value: veo3.1-pro
                      name: ''
                      description: >-
                        Google最新的高级人工智能模型, veo3.1 高质量
                        模式，支持视频自动配套音频生成，质量超高，价格也超高，使用需注意, 自适应首尾帧和文生视频
                    - value: veo3.1-4k
                      name: ''
                      description: >-
                        Google最新的高级人工智能模型, veo3.1 快速
                        +4k模式，支持视频自动配套音频生成，质量高价格很低，性价比最高的选择, 自适应首尾帧和文生视频
                    - value: veo3.1-pro-4k
                      name: ''
                      description: >-
                        Google最新的高级人工智能模型, veo3.1 高质量
                        +4k模式，支持视频自动配套音频生成，质量超高，价格也超高，使用需注意, 自适应首尾帧和文生视频
                prompt:
                  type: string
                  description: 提示词
                enhance_prompt:
                  type: boolean
                  description: 由于 veo 只支持英文提示词，所以如果需要中文自动转成英文提示词，可以开启此开关
                enable_upsample:
                  type: boolean
                images:
                  type: array
                  items:
                    type: string
                  description: >-
                    当模型是带 veo2-fast-frames 最多支持两个，分别是首尾帧，当模型是 veo3-pro-frames
                    最多支持一个首帧，当模型是 veo2-fast-components 最多支持 3 个，此时图片为视频中的元素
                aspect_ratio:
                  type: string
                  description: ⚠️仅veo3支持，“16:9”或“9:16”
              required:
                - prompt
                - model
              x-apifox-orders:
                - enable_upsample
                - enhance_prompt
                - images
                - model
                - prompt
                - aspect_ratio
            example:
              enable_upsample: true
              enhance_prompt: true
              images:
                - >-
                  https://filesystem.site/cdn/20250702/w8AauvxxPhYoqqkFWdMippJpb9zBxN.png
              model: veo3.1-fast
              prompt: make animate
              aspect_ratio: '16:9'
      responses:
        '200':
          description: ''
          content:
            application/json:
              schema:
                type: object
                properties:
                  id:
                    type: string
                  status:
                    type: string
                  status_update_time:
                    type: integer
                  enhanced_prompt:
                    type: string
                required:
                  - id
                  - status
                  - status_update_time
                  - enhanced_prompt
                x-apifox-orders:
                  - id
                  - status
                  - status_update_time
                  - enhanced_prompt
              example:
                id: veo3-fast-frames:1757555257-PORrVn9sa9
                status: pending
                status_update_time: 1757555257582
          headers: {}
          x-apifox-name: 成功
      security:
        - bearer: []
      x-apifox-folder: 视频模型/veo 视频生成/视频统一格式
      x-apifox-status: released
      x-run-in-apifox: https://app.apifox.com/web/project/7109750/apis/api-349239166-run
components:
  schemas: {}
  securitySchemes:
    bearer:
      type: http
      scheme: bearer
servers:
  - url: https://api.vectorengine.ai
    description: 正式环境
security:
  - bearer: []

```

# 创建视频，带图片

## OpenAPI Specification

```yaml
openapi: 3.0.1
info:
  title: ''
  description: ''
  version: 1.0.0
paths:
  /v1/video/create:
    post:
      summary: 创建视频，带图片
      deprecated: false
      description: ''
      tags:
        - 视频模型/veo 视频生成/视频统一格式
      parameters:
        - name: Content-Type
          in: header
          description: ''
          required: true
          example: application/json
          schema:
            type: string
        - name: Accept
          in: header
          description: ''
          required: true
          example: application/json
          schema:
            type: string
        - name: Authorization
          in: header
          description: ''
          required: false
          example: Bearer {{YOUR_API_KEY}}
          schema:
            type: string
      requestBody:
        content:
          application/json:
            schema:
              type: object
              properties:
                model:
                  type: string
                  description: |-
                    枚举值:
                    veo2
                    veo2-fast
                    veo2-fast-frames
                    veo2-fast-components
                    veo2-pro
                    veo3
                    veo3-fast
                    veo3-pro
                    veo3-pro-frames
                    veo3-fast-frames
                    veo3-frames
                prompt:
                  type: string
                  description: 提示词
                images:
                  type: array
                  items:
                    type: string
                  description: >
                    当模型是带 veo2-fast-frames 最多支持两个，分别是首尾帧，当模型是 veo3-pro-frames
                    最多支持一个首帧，当模型是 veo2-fast-components 最多支持 3 个，此时图片为视频中的元素
                enhance_prompt:
                  type: boolean
                  description: |
                    由于 veo 只支持英文提示词，所以如果需要中文自动转成英文提示词，可以开启此开关
                enable_upsample:
                  type: string
                  description: 超分
                aspect_ratio:
                  type: string
                  description: |
                    ⚠️仅veo3支持，“16:9”或“9:16”
              required:
                - model
                - prompt
                - enhance_prompt
                - enable_upsample
                - aspect_ratio
              x-apifox-orders:
                - model
                - prompt
                - images
                - enhance_prompt
                - enable_upsample
                - aspect_ratio
            example:
              prompt: 牛飞上天了
              model: veo3-fast-frames
              images:
                - >-
                  https://filesystem.site/cdn/20250612/VfgB5ubjInVt8sG6rzMppxnu7gEfde.png
                - >-
                  https://filesystem.site/cdn/20250612/998IGmUiM2koBGZM3UnZeImbPBNIUL.png
              enhance_prompt: true
              enable_upsample: true
              aspect_ratio: '16:9'
      responses:
        '200':
          description: ''
          content:
            application/json:
              schema:
                type: object
                properties:
                  id:
                    type: string
                  object:
                    type: string
                  created:
                    type: integer
                  choices:
                    type: array
                    items:
                      type: object
                      properties:
                        index:
                          type: integer
                        message:
                          type: object
                          properties:
                            role:
                              type: string
                            content:
                              type: string
                          required:
                            - role
                            - content
                          x-apifox-orders:
                            - role
                            - content
                        finish_reason:
                          type: string
                      x-apifox-orders:
                        - index
                        - message
                        - finish_reason
                  usage:
                    type: object
                    properties:
                      prompt_tokens:
                        type: integer
                      completion_tokens:
                        type: integer
                      total_tokens:
                        type: integer
                    required:
                      - prompt_tokens
                      - completion_tokens
                      - total_tokens
                    x-apifox-orders:
                      - prompt_tokens
                      - completion_tokens
                      - total_tokens
                required:
                  - id
                  - object
                  - created
                  - choices
                  - usage
                x-apifox-orders:
                  - id
                  - object
                  - created
                  - choices
                  - usage
              example:
                id: veo3-fast-frames:1762010543-twr7BEQ5wO
                status: pending
                status_update_time: 1762010543957
          headers: {}
          x-apifox-name: OK
      security:
        - bearer: []
      x-apifox-folder: 视频模型/veo 视频生成/视频统一格式
      x-apifox-status: released
      x-run-in-apifox: https://app.apifox.com/web/project/7109750/apis/api-349239168-run
components:
  schemas: {}
  securitySchemes:
    bearer:
      type: http
      scheme: bearer
servers:
  - url: https://api.vectorengine.ai
    description: 正式环境
security:
  - bearer: []

```

# 创建视频（参考图）

## OpenAPI Specification

```yaml
openapi: 3.0.1
info:
  title: ''
  description: ''
  version: 1.0.0
paths:
  /v1/video/create:
    post:
      summary: 创建视频（参考图）
      deprecated: false
      description: ''
      tags:
        - 视频模型/veo 视频生成/视频统一格式
      parameters:
        - name: Content-Type
          in: header
          description: ''
          required: true
          example: application/json
          schema:
            type: string
        - name: Accept
          in: header
          description: ''
          required: true
          example: application/json
          schema:
            type: string
        - name: Authorization
          in: header
          description: ''
          required: false
          example: Bearer {{YOUR_API_KEY}}
          schema:
            type: string
      requestBody:
        content:
          application/json:
            schema:
              type: object
              properties:
                model:
                  type: string
                  description: |-
                    枚举值:
                    veo2
                    veo2-fast
                    veo2-fast-frames
                    veo2-fast-components
                    veo2-pro
                    veo3
                    veo3-fast
                    veo3-pro
                    veo3-pro-frames
                    veo3-fast-frames
                    veo3-frames
                prompt:
                  type: string
                  description: 提示词
                images:
                  type: array
                  items:
                    type: string
                  description: >
                    当模型是带 veo2-fast-frames 最多支持两个，分别是首尾帧，当模型是 veo3-pro-frames
                    最多支持一个首帧，当模型是 veo2-fast-components、veo3.1-components 最多支持 3
                    个，此时图片为视频中的元素
                enhance_prompt:
                  type: boolean
                  description: |
                    由于 veo 只支持英文提示词，所以如果需要中文自动转成英文提示词，可以开启此开关
                enable_upsample:
                  type: string
                  description: 超分
                aspect_ratio:
                  type: string
                  description: |
                    ⚠️仅veo3支持，“16:9”或“9:16”
              required:
                - model
                - prompt
                - enhance_prompt
                - enable_upsample
                - aspect_ratio
              x-apifox-orders:
                - model
                - prompt
                - images
                - enhance_prompt
                - enable_upsample
                - aspect_ratio
            example:
              prompt: 牛飞上天了
              model: veo3.1-components
              images:
                - >-
                  https://filesystem.site/cdn/20250612/VfgB5ubjInVt8sG6rzMppxnu7gEfde.png
                - >-
                  https://filesystem.site/cdn/20250612/998IGmUiM2koBGZM3UnZeImbPBNIUL.png
                - >-
                  https://iknow-pic.cdn.bcebos.com/5882b2b7d0a20cf4ced1ab5f64094b36adaf99e9
              enhance_prompt: true
              enable_upsample: true
              aspect_ratio: '16:9'
      responses:
        '200':
          description: ''
          content:
            application/json:
              schema:
                type: object
                properties:
                  id:
                    type: string
                  object:
                    type: string
                  created:
                    type: integer
                  choices:
                    type: array
                    items:
                      type: object
                      properties:
                        index:
                          type: integer
                        message:
                          type: object
                          properties:
                            role:
                              type: string
                            content:
                              type: string
                          required:
                            - role
                            - content
                          x-apifox-orders:
                            - role
                            - content
                        finish_reason:
                          type: string
                      x-apifox-orders:
                        - index
                        - message
                        - finish_reason
                  usage:
                    type: object
                    properties:
                      prompt_tokens:
                        type: integer
                      completion_tokens:
                        type: integer
                      total_tokens:
                        type: integer
                    required:
                      - prompt_tokens
                      - completion_tokens
                      - total_tokens
                    x-apifox-orders:
                      - prompt_tokens
                      - completion_tokens
                      - total_tokens
                required:
                  - id
                  - object
                  - created
                  - choices
                  - usage
                x-apifox-orders:
                  - id
                  - object
                  - created
                  - choices
                  - usage
              example:
                id: veo3.1-components:1762241017-xTL0P9HvGF
                status: pending
                status_update_time: 1762241017286
          headers: {}
          x-apifox-name: OK
      security:
        - bearer: []
      x-apifox-folder: 视频模型/veo 视频生成/视频统一格式
      x-apifox-status: released
      x-run-in-apifox: https://app.apifox.com/web/project/7109750/apis/api-360308586-run
components:
  schemas: {}
  securitySchemes:
    bearer:
      type: http
      scheme: bearer
servers:
  - url: https://api.vectorengine.ai
    description: 正式环境
security:
  - bearer: []

```

# 查询任务

## OpenAPI Specification

```yaml
openapi: 3.0.1
info:
  title: ''
  description: ''
  version: 1.0.0
paths:
  /v1/video/query:
    get:
      summary: 查询任务
      deprecated: false
      description: |+
        给定一个提示，该模型将返回一个或多个预测的完成，并且还可以返回每个位置的替代标记的概率。

        为提供的提示和参数创建完成

      tags:
        - 视频模型/veo 视频生成/视频统一格式
      parameters:
        - name: id
          in: query
          description: |
            任务ID
          required: true
          example: veo3.1-fast:1770350082-trii1OXZc3
          schema:
            type: string
        - name: Content-Type
          in: header
          description: ''
          required: true
          example: application/json
          schema:
            type: string
        - name: Accept
          in: header
          description: ''
          required: true
          example: application/json
          schema:
            type: string
        - name: Authorization
          in: header
          description: ''
          required: false
          example: Bearer {{YOUR_API_KEY}}
          schema:
            type: string
        - name: X-Forwarded-Host
          in: header
          description: ''
          required: false
          example: localhost:5173
          schema:
            type: string
      requestBody:
        content:
          multipart/form-data:
            schema:
              type: object
              properties: {}
            examples: {}
      responses:
        '200':
          description: ''
          content:
            application/json:
              schema:
                type: object
                properties:
                  id:
                    type: string
                  status:
                    type: string
                  video_url:
                    type: 'null'
                  enhanced_prompt:
                    type: string
                  status_update_time:
                    type: integer
                required:
                  - id
                  - status
                  - video_url
                  - enhanced_prompt
                  - status_update_time
                x-apifox-orders:
                  - id
                  - status
                  - video_url
                  - enhanced_prompt
                  - status_update_time
              example:
                id: veo3.1-fast:1770350082-trii1OXZc3
                detail:
                  id: veo3.1-fast:1770350082-trii1OXZc3
                  req:
                    model: veo3.1-fast
                    images:
                      - >-
                        https://filesystem.site/cdn/20250702/w8AauvxxPhYoqqkFWdMippJpb9zBxN.png
                    prompt: make animate
                    aspect_ratio: '16:9'
                    enhance_prompt: true
                    enable_upsample: true
                  images:
                    - url: >-
                        https://filesystem.site/cdn/20250702/w8AauvxxPhYoqqkFWdMippJpb9zBxN.png
                      status: completed
                      mediaId: >-
                        CAMaJGUwOTE4MTA4LWQ3MTgtNDk0OS05ZDNiLTIxMjBkM2M5ZGFkYyIDQ0FFKiQzYzY4MjJhZi05ZTVkLTRhNjktOGQ5NC1jYmNjYWVjNDhkMjM
                  status: completed
                  running: false
                  video_url: >-
                    https://pro.filesystem.site/cdn/20260206/9ddbd3bdc67f75a02f766363ebf8eb.mp4
                  created_at: 1770350082099
                  error_sleep: 5000
                  max_retries: 3
                  retry_count: 3
                  completed_at: 1770350328994
                  error_message: >-
                    [403] Request failed with status code 403 | Message:
                    reCAPTCHA evaluation failed | Code: 403 | Status:
                    PERMISSION_DENIED | Reason:
                    PUBLIC_ERROR_SOMETHING_WENT_WRONG | Type:
                    type.googleapis.com/google.rpc.ErrorInfo | URL:
                    /v1/video:batchAsyncGenerateVideoStartImage
                  video_media_id: >-
                    CAUSJDU1YzFjNTliLWYxNzctNGYwYy04YTJkLTI3MGFkMWE4YjkyMxokMTZjYjk1NzUtYWY4Yy00MzQzLWI4NjMtNGQ5YWFiZTZmNzMzIgNDQUUqJGQxZjBhOGExLTUxYjYtNDk5Yy04YWJlLWFhZTU0YzZiMjIxZQ
                  enhanced_prompt: >-
                    Create a vibrant and whimsical animation that features a
                    joyful scene of colorful animals playing together in a sunny
                    meadow, surrounded by blooming flowers and butterflies
                    fluttering by.
                  upsample_status: MEDIA_GENERATION_STATUS_SUCCESSFUL
                  startImageMediaId: >-
                    CAMaJGUwOTE4MTA4LWQ3MTgtNDk0OS05ZDNiLTIxMjBkM2M5ZGFkYyIDQ0FFKiQzYzY4MjJhZi05ZTVkLTRhNjktOGQ5NC1jYmNjYWVjNDhkMjM
                  status_update_time: 1770350329098
                  upsample_video_url: >-
                    https://pro.filesystem.site/cdn/20260206/1b50395bad4c6c7bc22e6ada144951.mp4
                  video_generation_id: 608c00e8de85d8856554201ae115b7c2
                  veo3StartImageMediaId: >-
                    CAMaJGUwOTE4MTA4LWQ3MTgtNDk0OS05ZDNiLTIxMjBkM2M5ZGFkYyIDQ0FFKiQzYzY4MjJhZi05ZTVkLTRhNjktOGQ5NC1jYmNjYWVjNDhkMjM
                  needs_safe_enhancement: true
                  upsample_generation_id: d1f0a8a1-51b6-499c-8abe-aae54c6b221e_upsampled
                  video_generation_error: >-
                    your request contains unsafe prompt or images, such as porn,
                    violence, minors etc., has been rejected by google, please
                    change your prompt or images and try again
                  video_generation_status: MEDIA_GENERATION_STATUS_SUCCESSFUL
                status: completed
                video_url: >-
                  https://pro.filesystem.site/cdn/20260206/1b50395bad4c6c7bc22e6ada144951.mp4
                status_update_time: 1770350329098
          headers: {}
          x-apifox-name: 成功
      security:
        - bearer: []
      x-apifox-folder: 视频模型/veo 视频生成/视频统一格式
      x-apifox-status: released
      x-run-in-apifox: https://app.apifox.com/web/project/7109750/apis/api-349239167-run
components:
  schemas: {}
  securitySchemes:
    bearer:
      type: http
      scheme: bearer
servers:
  - url: https://api.vectorengine.ai
    description: 正式环境
security:
  - bearer: []

```

# 状态码

// VEO任务状态常量
const (
  VeoStatusPending                  = "pending"
  VeoStatusImageDownloading         = "image_downloading"
  VeoStatusVideoGenerating          = "video_generating"
  VeoStatusVideoGenerationCompleted = "video_generation_completed"
  VeoStatusVideoGenerationFailed    = "video_generation_failed"
  VeoStatusVideoUpsampling          = "video_upsampling"
  VeoStatusVideoUpsamplingCompleted = "video_upsampling_completed"
  VeoStatusVideoUpsamplingFailed    = "video_upsampling_failed"
  VeoStatusCompleted                = "completed"
  VeoStatusFailed                   = "failed"
  VeoStatusError                    = "error" // 错误（用于错误响应）
)

