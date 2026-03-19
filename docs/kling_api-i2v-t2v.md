# 可灵图生视频

## OpenAPI Specification

```yaml
openapi: 3.0.1
info:
  title: ''
  description: ''
  version: 1.0.0
paths:
  /kling/v1/videos/image2video:
    post:
      summary: 图生视频
      deprecated: false
      description: ''
      tags:
        - 可灵 Kling 平台/图生视频
      parameters:
        - name: Content-Type
          in: header
          description: ''
          required: false
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
                model_name:
                  type: string
                  description: >-
                    模型名称 枚举值：kling-v1, kling-v1-5, kling-v1-6, kling-v2-master,
                    kling-v2-1, kling-v2-1-master,
                    kling-v2-5-turbo,kling-v2-6,kling-v3
                image:
                  type: string
                  description: 参考图像 支持传入图片Base64编码或图片URL（确保可访问）
                image_tail:
                  type: string
                  description: 参考图像 - 尾帧控制 支持传入图片Base64编码或图片URL（确保可访问）
                prompt:
                  type: string
                  description: |-
                    正向文本提示词不能超过2500个字符
                    用<<<voice_1>>>来指定音色，序号同voice_list参数所引用音色的排列顺序
                    一次视频生成任务至多引用2个音色；指定音色时，sound参数值必须为on语法结构越简单越好，
                    如：男人<<<vocie_1>>>说：“你好”
                    当voice_list参数不为空且prompt参数中引用音色ID时，视频生成任务按“有指定音色”计量计费
                negative_prompt:
                  type: string
                  description: 负向文本提示词
                voice_list:
                  type: array
                  items:
                    type: object
                    properties:
                      voice_id:
                        type: string
                    x-apifox-orders:
                      - voice_id
                  description: |-
                    仅V2.6及后续版本模型支持当前参数生成视频时所引用的音色的列表
                    一次视频生成任务至多引用2个音色
                    当voice_list参数不为空且prompt参数中引用音色ID时，视频生成任务按“有指定音色”计量计费
                sound:
                  type: string
                  description: |-
                    仅V2.6及后续版本模型支持当前参数生成视频时是否同时生成声音
                    枚举值：on，off
                cfg_scale:
                  type: number
                  description: kling-v2.x模型不支持当前参数生成视频的自由度；值越大，模型自由度越小，与用户输入的提示词相关性越强
                mode:
                  type: string
                  description: |-
                    生成视频的模式
                    枚举值：std，pro
                    其中std：标准模式（标准），基础模式，性价比高
                    其中pro：专家模式（高品质），高表现模式，生成视频质量更佳
                static_mask:
                  type: string
                  description: 静态笔刷涂抹区域（用户通过运动笔刷涂抹的 mask 图片）
                multi_shot:
                  type: boolean
                  description: |-
                    是否生成多镜头视频
                    当前参数为true时，prompt参数无效
                    当前参数为false时，shot_type参数及multi_prompt参数无效
                shot_type:
                  type: string
                  description: |-
                    分镜方式
                    枚举值：customize
                    当multi_shot参数为true时，当前参数必填
                multi_prompt:
                  type: array
                  items:
                    type: object
                    properties:
                      index:
                        type: integer
                      prompt:
                        type: string
                      duration:
                        type: string
                    required:
                      - index
                      - prompt
                      - duration
                    x-apifox-orders:
                      - index
                      - prompt
                      - duration
                  description: "各分镜信息，如提示词、时长等\n● 通过index、prompt、duration参数定义分镜序号及相应提示词和时长，其中：\n\t○ 最多支持6个分镜，最小支持1个分镜\n\t○ 每个分镜相关内容的最大长度不超过512\n\t○ 每个分镜的时长不大于当前任务的总时长，不小于1\n\t○ 所有分镜的时长之和等于当前任务的总时长"
                dynamic_masks:
                  type: array
                  items:
                    type: object
                    properties:
                      mask:
                        type: string
                      trajectories:
                        type: array
                        items:
                          type: object
                          properties:
                            x:
                              type: integer
                            'y':
                              type: integer
                          required:
                            - x
                            - 'y'
                          x-apifox-orders:
                            - x
                            - 'y'
                    x-apifox-orders:
                      - mask
                      - trajectories
                  description: 动态笔刷配置列表
                camera_control:
                  type: object
                  properties:
                    type:
                      type: string
                    config:
                      type: object
                      properties:
                        horizontal:
                          type: integer
                        vertical:
                          type: integer
                        pan:
                          type: integer
                        tilt:
                          type: integer
                        roll:
                          type: integer
                        zoom:
                          type: integer
                      required:
                        - horizontal
                        - vertical
                        - pan
                        - tilt
                        - roll
                        - zoom
                      x-apifox-orders:
                        - horizontal
                        - vertical
                        - pan
                        - tilt
                        - roll
                        - zoom
                  required:
                    - type
                    - config
                  x-apifox-orders:
                    - type
                    - config
                  description: 控制摄像机运动的协议（如未指定，模型将根据输入的文本/图片进行智能匹配）
                duration:
                  type: string
                  description: |-
                    生成视频时长，单位s
                    枚举值：5，10
                watermark_info:
                  type: object
                  properties:
                    enabled:
                      type: boolean
                  required:
                    - enabled
                  x-apifox-orders:
                    - enabled
                  description: "是否同时生成含水印的结果\n● 通过enabled参数定义，用key:value承载，如下：：\n\"watermark_info\": {\n \t\"enabled\": boolean // true 为生成，false 为不生成\n}"
                callback_url:
                  type: string
                external_task_id:
                  type: string
              required:
                - model_name
                - duration
                - image
                - mode
              x-apifox-orders:
                - model_name
                - image
                - image_tail
                - prompt
                - negative_prompt
                - voice_list
                - sound
                - cfg_scale
                - mode
                - static_mask
                - multi_shot
                - shot_type
                - multi_prompt
                - dynamic_masks
                - camera_control
                - duration
                - watermark_info
                - callback_url
                - external_task_id
            example: "{\r\n    \"model_name\": \"kling-v1\",\r\n    \"image\": \"https://h2.inkwai.com/bs2/upload-ylab-stunt/se/ai_portal_queue_mmu_image_upscale_aiweb/3214b798-e1b4-4b00-b7af-72b5b0417420_raw_image_0.jpg\",\r\n    \"image_tail\": \"\",\r\n    \"prompt\": \"\",\r\n    \"negative_prompt\": \"\",\r\n    // \"voice_list\": [\r\n    //     {\r\n    //         \"voiceId\": \"\"\r\n    //     }\r\n    // ],\r\n    // \"sound\": \"\",\r\n    \"cfg_scale\": 0.5,\r\n    \"mode\": \"std\",\r\n    \"static_mask\": \"\",\r\n    \"dynamic_masks\": [\r\n        {\r\n            \"mask\": \"https://h2.inkwai.com/bs2/upload-ylab-stunt/se/ai_portal_queue_mmu_image_upscale_aiweb/3214b798-e1b4-4b00-b7af-72b5b0417420_raw_image_0.jpg\",\r\n            \"trajectories\": [\r\n                {\r\n                    \"x\": 0,\r\n                    \"y\": 0\r\n                },\r\n                {\r\n                    \"x\": 1,\r\n                    \"y\": 1\r\n                }\r\n            ],\r\n        }\r\n    ],\r\n    // \"camera_control\": {\r\n    //     \"type\": \"simple\",\r\n    //     \"config\": {\r\n    //         \"horizontal\": 1.0,\r\n    //         \"vertical\": 0,\r\n    //         \"pan\": 0,\r\n    //         \"tilt\": 0,\r\n    //         \"roll\": 0,\r\n    //         \"zoom\": 0\r\n    //     }\r\n    // },\r\n    \"duration\": 5,\r\n    \"callback_url\": \"\",\r\n    \"external_task_id\": \"\"\r\n}"
      responses:
        '200':
          description: ''
          content:
            application/json:
              schema:
                type: object
                properties: {}
                x-apifox-orders: []
          headers: {}
          x-apifox-name: 成功
      security: []
      x-apifox-folder: 可灵 Kling 平台/图生视频
      x-apifox-status: released
      x-run-in-apifox: https://app.apifox.com/web/project/7262944/apis/api-401792579-run
components:
  schemas: {}
  securitySchemes:
    bearer:
      type: http
      scheme: bearer
servers:
  - url: https://api.kuai.host
    description: 正式环境
security:
  - bearer: []

```

# 可灵文生视频

## OpenAPI Specification

```yaml
openapi: 3.0.1
info:
  title: ''
  description: ''
  version: 1.0.0
paths:
  /kling/v1/videos/text2video:
    post:
      summary: 文生视频
      deprecated: false
      description: ''
      tags:
        - 可灵 Kling 平台/文生视频
      parameters:
        - name: Content-Type
          in: header
          description: ''
          required: false
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
                model_name:
                  type: string
                  description: ' 模型名称 枚举值：kling-v1,kling-v1-6, kling-v2-master, kling-v2-1-master, kling-v2-5-turbo,kling-v3'
                prompt:
                  type: string
                  description: 正向文本提示词不能超过2500个字符
                multi_shot:
                  type: boolean
                  description: |-
                    是否生成多镜头视频
                    当前参数为true时，prompt参数无效
                    当前参数为false时，shot_type参数及multi_prompt参数无效
                shot_type:
                  type: string
                  description: |-
                    分镜方式
                    枚举值：customize
                    当multi_shot参数为true时，当前参数必填 
                multi_prompt:
                  type: array
                  items:
                    type: object
                    properties:
                      index:
                        type: integer
                      prompt:
                        type: string
                      duration:
                        type: string
                    required:
                      - index
                      - prompt
                      - duration
                    x-apifox-orders:
                      - index
                      - prompt
                      - duration
                  description: "各分镜信息，如提示词、时长等\n● 通过index、prompt、duration参数定义分镜序号及相应提示词和时长，其中：\n\t○ 最多支持6个分镜，最小支持1个分镜\n\t○ 每个分镜相关内容的最大长度不超过512\n\t○ 每个分镜的时长不大于当前任务的总时长，不小于1\n\t○ 所有分镜的时长之和等于当前任务的总时长"
                negative_prompt:
                  type: string
                  description: 负向文本提示词不能超过2500个字符
                cfg_scale:
                  type: number
                  description: 生成视频的自由度；值越大，模型自由度越小，与用户输入的提示词相关性越强
                mode:
                  type: string
                  description: |-
                    生成视频的模式
                    枚举值：std，pro
                    其中std：标准模式（标准），基础模式，性价比高
                    其中pro：专家模式（高品质），高表现模式，生成视频质量更佳
                sound:
                  type: string
                  description: 生成视频时是否同时生成声音枚举值：on，off仅V2.6及后续版本模型支持当前参数
                aspect_ratio:
                  type: string
                  description: 生成视频的画面纵横比（宽:高）
                duration:
                  type: string
                  description: 生成视频时长，单位s
                callback_url:
                  type: string
                external_task_id:
                  type: string
                watermark_info:
                  type: object
                  properties:
                    enabled:
                      type: boolean
                  required:
                    - enabled
                  x-apifox-orders:
                    - enabled
                  description: "是否同时生成含水印的结果\n● 通过enabled参数定义，用key:value承载，如下：：\n\"watermark_info\": {\n \t\"enabled\": boolean // true 为生成，false 为不生成\n}"
                camera_control:
                  type: object
                  properties:
                    type:
                      type: string
                    config:
                      type: object
                      properties:
                        horizontal:
                          type: integer
                        vertical:
                          type: integer
                        pan:
                          type: integer
                        tilt:
                          type: integer
                        roll:
                          type: integer
                        zoom:
                          type: integer
                      required:
                        - horizontal
                        - vertical
                        - pan
                        - tilt
                        - roll
                        - zoom
                      x-apifox-orders:
                        - horizontal
                        - vertical
                        - pan
                        - tilt
                        - roll
                        - zoom
                  required:
                    - type
                    - config
                  x-apifox-orders:
                    - type
                    - config
                  description: 控制摄像机运动的协议（如未指定，模型将根据输入的文本/图片进行智能匹配）
              required:
                - model_name
                - mode
                - duration
                - multi_shot
              x-apifox-orders:
                - model_name
                - prompt
                - multi_shot
                - shot_type
                - multi_prompt
                - negative_prompt
                - cfg_scale
                - mode
                - sound
                - camera_control
                - aspect_ratio
                - duration
                - watermark_info
                - callback_url
                - external_task_id
            example:
              model_name: kling-v1
              prompt: 生成一个海边有一个人跳舞的视频
              negative_prompt: ''
              cfg_scale: 0.5
              mode: std
              sound: 'off'
              camera_control:
                type: simple
                config:
                  horizontal: 1
                  vertical: 0
                  pan: 0
                  tilt: 0
                  roll: 0
                  zoom: 0
              aspect_ratio: '16:9'
              duration: '5'
              callback_url: ''
              external_task_id: ''
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
                code: 0
                message: SUCCEED
                request_id: 603e2a28-fb89-4146-ae33-412d74012a6d
                data:
                  task_id: '831922345719271433'
                  task_status: submitted
                  task_info: {}
                  created_at: 1766374262370
                  updated_at: 1766374262370
          headers: {}
          x-apifox-name: 成功
      security:
        - bearer: []
      x-apifox-folder: 可灵 Kling 平台/文生视频
      x-apifox-status: released
      x-run-in-apifox: https://app.apifox.com/web/project/7262944/apis/api-401792577-run
components:
  schemas: {}
  securitySchemes:
    bearer:
      type: http
      scheme: bearer
servers:
  - url: https://api.kuai.host
    description: 正式环境
security:
  - bearer: []

```

# 可灵查询任务（单个）

## OpenAPI Specification

```yaml
openapi: 3.0.1
info:
  title: ''
  description: ''
  version: 1.0.0
paths:
  /kling/v1/videos/text2video/{id}:
    get:
      summary: 查询任务（单个）
      deprecated: false
      description: ''
      tags:
        - 可灵 Kling 平台/文生视频
      parameters:
        - name: id
          in: path
          description: ''
          required: true
          example: '827996385395363846'
          schema:
            type: string
        - name: Content-Type
          in: header
          description: ''
          required: false
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
              properties: {}
              x-apifox-orders: []
            example: "// {\r\n//     \"task_id\": \"kling-v1825380683199176793\",      // 文生视频的任务ID 请求路径参数，直接将值填写在请求路径中，与external_task_id两种查询方式二选一\r\n//     \"external_task_id\": \"\",    // 文生视频的自定义任务ID 创建任务时填写的external_task_id，与task_id两种查询方式二选一\r\n// }"
      responses:
        '200':
          description: ''
          content:
            application/json:
              schema:
                type: object
                properties: {}
                x-apifox-orders: []
          headers: {}
          x-apifox-name: 成功
      security:
        - bearer: []
      x-apifox-folder: 可灵 Kling 平台/文生视频
      x-apifox-status: released
      x-run-in-apifox: https://app.apifox.com/web/project/7262944/apis/api-401792578-run
components:
  schemas: {}
  securitySchemes:
    bearer:
      type: http
      scheme: bearer
servers:
  - url: https://api.kuai.host
    description: 正式环境
security:
  - bearer: []

```
