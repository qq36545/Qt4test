# 创建视频

## OpenAPI Specification

```yaml
openapi: 3.0.1
info:
  title: ''
  description: ''
  version: 1.0.0
paths:
  /v1/videos:
    post:
      summary: 创建视频
      deprecated: false
      description: ''
      tags:
        - 视频模型/sora 视频生成/OpenAI官方视频格式
      parameters: []
      requestBody:
        content:
          multipart/form-data:
            schema:
              type: object
              properties:
                model:
                  description: 要使用的视频生成模型（允许的值：sora-2, sora-2-pro）。
                  example: sora-2
                  type: string
                prompt:
                  description: 描述要生成的视频的文本提示。
                  example: A calico cat playing a piano on stage
                  type: string
                seconds:
                  description: 剪辑时长（秒）（允许的值：4, 8, 12）。默认为 4 秒。
                  example: '15'
                  type: string
                size:
                  description: >+
                    输出分辨率格式为宽度 x
                    高度（允许的值：720x1280，1280x720，1024x1792，1792x1024）。默认值为
                    720x1280。

                    sora-2 支持 720x1280，1280x720

                    sora-2-pro 支持 1024x1792，1792x1024

                  example: 720x1280
                  type: string
                input_reference:
                  format: binary
                  type: string
                  description: 可选的图像参考，用于指导生成。
                  example: >-
                    file://C:\Users\Administrator\Desktop\fuji-mountain-kawaguchiko-lake-morning-autumn-seasons-fuji-mountain-yamanachi-japan_副本.jpg
                watermark:
                  description: '-ALL模型可用参数'
                  example: 'false'
                  type: string
                private:
                  description: '-ALL模型可用参数'
                  example: 'false'
                  type: string
                style:
                  description: >-
                    -ALL模型可用参数 风格只支持：thanksgiving, comic, news, selfie,
                    nostalgic, anime
                  example: anime
                  type: string
              required:
                - model
                - prompt
            examples: {}
      responses:
        '200':
          description: ''
          content:
            application/json:
              schema:
                type: object
                properties: {}
                x-apifox-orders: []
              example:
                id: video_5c6a605a-30c0-4a6a-9dbd-d1d6cfdd9980
                object: video
                model: sora-2
                status: queued
                progress: 0
                created_at: 1761622232
                seconds: '10'
                size: 1280x720
          headers: {}
          x-apifox-name: 成功
      security:
        - bearer: []
      x-apifox-folder: 视频模型/sora 视频生成/OpenAI官方视频格式
      x-apifox-status: released
      x-run-in-apifox: https://app.apifox.com/web/project/7109750/apis/api-373137989-run
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

# 创建一个来自上传视频的角色

## OpenAPI Specification

```yaml
openapi: 3.0.1
info:
  title: ''
  description: ''
  version: 1.0.0
paths:
  /v1/videos/characters:
    post:
      summary: 创建一个来自上传视频的角色
      deprecated: false
      description: ''
      tags:
        - 视频模型/sora 视频生成/OpenAI官方视频格式
      parameters: []
      requestBody:
        content:
          multipart/form-data:
            schema:
              type: object
              properties:
                name:
                  description: |-
                    定义角色名称。

                    最大长度80  
                    最小长度1
                  example: 可爱的小鱼
                  type: string
                video:
                  format: binary
                  type: string
                  description: >+
                    用于创建角色的视频文件。目前角色上传时，2 到 4 秒的短片段效果最佳 16：9 或 9：16，分辨率为 720p 到
                    1080p。角色源视频在匹配要求输出的画面比例时效果最佳。如果宽高比不同，角色可能会显得拉伸或变形。一个视频最多可包含两个角色。

                  example: file://C:\Users\Administrator\Desktop\下载.mp4
              required:
                - name
                - video
            examples: {}
      responses:
        '200':
          description: ''
          content:
            application/json:
              schema:
                type: object
                properties: {}
                x-apifox-orders: []
              example:
                id: video_5c6a605a-30c0-4a6a-9dbd-d1d6cfdd9980
                object: video
                model: sora-2
                status: queued
                progress: 0
                created_at: 1761622232
                seconds: '10'
                size: 1280x720
          headers: {}
          x-apifox-name: 成功
      security:
        - bearer: []
      x-apifox-folder: 视频模型/sora 视频生成/OpenAI官方视频格式
      x-apifox-status: released
      x-run-in-apifox: https://app.apifox.com/web/project/7109750/apis/api-429932154-run
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

# openai 编辑视频

## OpenAPI Specification

```yaml
openapi: 3.0.1
info:
  title: ''
  description: ''
  version: 1.0.0
paths:
  /v1/videos/{id}/remix:
    post:
      summary: openai 编辑视频
      deprecated: false
      description: ''
      tags:
        - 视频模型/sora 视频生成/OpenAI官方视频格式
      parameters:
        - name: id
          in: path
          description: ''
          required: true
          example: video_099c5197-abfd-4e16-88ff-1e162f2a5c77
          schema:
            type: string
        - name: Content-Type
          in: header
          description: ''
          required: true
          example: application/json
          schema:
            type: string
      requestBody:
        content:
          application/json:
            schema:
              type: object
              properties:
                prompt:
                  type: string
              required:
                - prompt
              x-apifox-orders:
                - prompt
            example:
              prompt: 画面更精细一些
              size: 1280x720
      responses:
        '401':
          description: ''
          content:
            application/json:
              schema:
                type: object
                properties:
                  error:
                    type: object
                    properties:
                      message:
                        type: string
                      message_zh:
                        type: string
                      type:
                        type: string
                    required:
                      - message
                      - message_zh
                      - type
                    x-apifox-orders:
                      - message
                      - message_zh
                      - type
                required:
                  - error
                x-apifox-orders:
                  - error
              example:
                id: video_8a60610c-3a5e-4ca8-be05-405f0dc635d0
                object: video
                model: sora_video2
                status: queued
                progress: 0
                created_at: 1766374127
                seconds: '10'
                size: 1280x720
                remixed_from_video_id: e3215864-dd3c-49b1-8475-e4669b6d5c33
                detail: {}
          headers: {}
          x-apifox-name: 成功
      security:
        - bearer: []
      x-apifox-folder: 视频模型/sora 视频生成/OpenAI官方视频格式
      x-apifox-status: released
      x-run-in-apifox: https://app.apifox.com/web/project/7109750/apis/api-366376772-run
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

# openai 下载视频

## OpenAPI Specification

```yaml
openapi: 3.0.1
info:
  title: ''
  description: ''
  version: 1.0.0
paths:
  /v1/videos/{id}/content:
    get:
      summary: openai 下载视频
      deprecated: false
      description: |+
        给定一个提示，该模型将返回一个或多个预测的完成，并且还可以返回每个位置的替代标记的概率。

        为提供的提示和参数创建完成

      tags:
        - 视频模型/sora 视频生成/OpenAI官方视频格式
      parameters:
        - name: id
          in: path
          description: ''
          required: true
          example: video_099c5197-abfd-4e16-88ff-1e162f2a5c77
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
          headers: {}
          x-apifox-name: 成功
      security:
        - bearer: []
      x-apifox-folder: 视频模型/sora 视频生成/OpenAI官方视频格式
      x-apifox-status: released
      x-run-in-apifox: https://app.apifox.com/web/project/7109750/apis/api-373137987-run
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

# openai 查询任务

## OpenAPI Specification

```yaml
openapi: 3.0.1
info:
  title: ''
  description: ''
  version: 1.0.0
paths:
  /v1/videos/{id}:
    get:
      summary: openai 查询任务
      deprecated: false
      description: |+
        给定一个提示，该模型将返回一个或多个预测的完成，并且还可以返回每个位置的替代标记的概率。

        为提供的提示和参数创建完成

      tags:
        - 视频模型/sora 视频生成/OpenAI官方视频格式
      parameters:
        - name: id
          in: path
          description: ''
          required: true
          example: sora-2:task_01k81e7r1mf0qtvp3ett3mr4jm
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
              examples:
                '1':
                  summary: pending
                  value:
                    id: sora-2:task_01k6x15vhrff09dkkqjrzwhm60
                    detail:
                      id: task_01k6x15vhrff09dkkqjrzwhm60
                      input:
                        size: small
                        model: sy_ore
                        images:
                          - >-
                            https://filesystem.site/cdn/20250612/VfgB5ubjInVt8sG6rzMppxnu7gEfde.png
                          - >-
                            https://filesystem.site/cdn/20250612/998IGmUiM2koBGZM3UnZeImbPBNIUL.png
                        prompt: make animate
                        orientation: portrait
                      status: pending
                      pending_info:
                        id: task_01k6x15vhrff09dkkqjrzwhm60
                        seed: null
                        type: video_gen
                        user: user-a1sGDUIOXV32hNITe59LEkHa
                        model: sy_8
                        title: New Video
                        width: 352
                        height: 640
                        prompt: make animate
                        sdedit: null
                        status: processing
                        actions: null
                        guidance: null
                        n_frames: 450
                        priority: 2
                        operation: simple_compose
                        preset_id: null
                        created_at: '2025-10-06T15:10:26.875729Z'
                        n_variants: 1
                        project_id: null
                        request_id: null
                        generations: []
                        tracking_id: null
                        progress_pct: 0.9302178175176704
                        remix_config: null
                        inpaint_items:
                          - type: image
                            preset_id: null
                            crop_bounds: null
                            frame_index: 0
                            cameo_file_id: null
                            generation_id: null
                            upload_media_id: media_01k6x15tnzezbst05sth2qgd8r
                            source_end_frame: 0
                            uploaded_file_id: null
                            source_start_frame: 0
                          - type: image
                            preset_id: null
                            crop_bounds: null
                            frame_index: 0
                            cameo_file_id: null
                            generation_id: null
                            upload_media_id: media_01k6x15tzafwztz74t018v3enw
                            source_end_frame: 0
                            uploaded_file_id: null
                            source_start_frame: 0
                        interpolation: null
                        is_storyboard: null
                        failure_reason: null
                        organization_id: null
                        moderation_result:
                          code: null
                          type: passed
                          task_id: task_01k6x15vhrff09dkkqjrzwhm60
                          is_output_rejection: false
                          results_by_frame_index: {}
                        needs_user_review: false
                        queue_status_message: null
                        progress_pos_in_queue: null
                        num_unsafe_generations: 0
                        estimated_queue_wait_time: null
                    status: pending
                    status_update_time: 1759763621142
                '2':
                  summary: 成功示例
                  value:
                    id: video_5c6a605a-30c0-4a6a-9dbd-d1d6cfdd9980
                    size: 1280x720
                    model: sora-2
                    object: video
                    status: completed
                    seconds: '10'
                    progress: 100
                    video_url: >-
                      https://midjourney-plus.oss-us-west-1.aliyuncs.com/sora/cc4fb429-22a5-4747-a6f9-a6badccf8f42.mp4
                    created_at: 1761622232
                    completed_at: 1761622385
          headers: {}
          x-apifox-name: 成功
      security:
        - bearer: []
      x-apifox-folder: 视频模型/sora 视频生成/OpenAI官方视频格式
      x-apifox-status: released
      x-run-in-apifox: https://app.apifox.com/web/project/7109750/apis/api-366376769-run
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