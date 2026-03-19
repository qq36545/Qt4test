# wan生成视频

## OpenAPI Specification

```yaml
openapi: 3.0.1
info:
  title: ''
  description: ''
  version: 1.0.0
paths:
  /alibailian/api/v1/services/aigc/video-generation/video-synthesis:
    post:
      summary: 生成视频
      deprecated: false
      description: ''
      tags:
        - 视频模型/通义万象 视频生成
      parameters: []
      requestBody:
        content:
          application/json:
            schema:
              type: object
              properties:
                model:
                  description: 模型名称, (必选) 用于指定本次视频生成所用到的模型。示例值：wan2.5-i2v-preview。
                  type: string
                input:
                  type: object
                  properties:
                    prompt:
                      description: 提示词, (可选) 用来描述生成图像中期望包含的元素和视觉特点。
                      type: string
                    negative_prompt:
                      description: 反向提示词, (可选) 用来描述不希望在视频画面中看到的内容。
                      type: string
                    img_url:
                      description: 首帧图像, (必选) 首帧图像的URL或 Base64 编码数据。
                      type: string
                    audio_url:
                      description: 音频文件URL, (可选，仅wan2.5-i2v-preview支持) 模型将使用该音频生成视频。
                      type: string
                    template:
                      description: 视频特效模板, (可选) 视频特效模板的名称。若未填写，表示不使用任何视频特效。
                      type: string
                  required:
                    - prompt
                    - negative_prompt
                    - img_url
                    - audio_url
                    - template
                  description: 输入信息, (必选) 输入的基本信息，如提示词、图像等。
                  x-apifox-orders:
                    - prompt
                    - negative_prompt
                    - img_url
                    - audio_url
                    - template
                parameters:
                  type: object
                  properties:
                    resolution:
                      description: 视频分辨率, (可选) 指定生成的视频分辨率档位，不改变视频的宽高比。
                      type: string
                    duration:
                      description: 视频时长, (可选) 生成视频的时长，单位为秒。该参数的取值依赖于model参数。
                      type: integer
                    prompt_extend:
                      description: Prompt智能改写, (可选) 是否开启prompt智能改写。默认值为true。
                      type: boolean
                    watermark:
                      description: 水印标识, (可选) 是否添加“AI生成”水印标识。默认值为false。
                      type: boolean
                    audio:
                      description: >-
                        自动添加音频, (可选，仅wan2.5-i2v-preview支持)
                        控制是否自动为视频添加音频，仅在audio_url为空时生效。
                      type: boolean
                    seed:
                      description: 随机数种子, (可选) 固定seed值有助于提升生成结果的可复现性。
                      type: integer
                  required:
                    - resolution
                    - duration
                    - prompt_extend
                    - watermark
                    - audio
                    - seed
                  description: 视频处理参数, (可选) 视频的高级处理参数，如设置分辨率、时长、水印等。
                  x-apifox-orders:
                    - resolution
                    - duration
                    - prompt_extend
                    - watermark
                    - audio
                    - seed
              required:
                - model
                - input
                - parameters
              x-apifox-orders:
                - model
                - input
                - parameters
            example: "{\r\n    \"model\": \"wan2.5-i2v-preview\",\r\n    \"input\": {\r\n        \"prompt\": \"改变一下光线\",\r\n        \"img_url\": \"https://brainrot-yt-shorts.oss-cn-beijing.aliyuncs.com/images/cached/55a955b7e41723417281051d7bebdb45.png\"\r\n    },\r\n    \"parameters\": {\r\n        \"resolution\": \"480P\",\r\n        \"prompt_extend\": true,\r\n        // \"duration\": 5,\r\n        \"audio\": true\r\n    }\r\n}"
      responses:
        '200':
          description: ''
          content:
            application/json:
              schema:
                type: object
                properties:
                  request_id:
                    type: string
                  output:
                    type: object
                    properties:
                      task_id:
                        type: string
                      task_status:
                        type: string
                    required:
                      - task_id
                      - task_status
                    x-apifox-orders:
                      - task_id
                      - task_status
                required:
                  - request_id
                  - output
                x-apifox-orders:
                  - request_id
                  - output
              example:
                request_id: ccffb21f-c21e-4eba-861a-6c80e6db6e4d
                output:
                  task_id: a55bfe14-6e78-4b9d-b97d-128420399ed1
                  task_status: PENDING
          headers: {}
          x-apifox-name: 成功
      security:
        - bearer: []
      x-apifox-folder: 视频模型/通义万象 视频生成
      x-apifox-status: developing
      x-run-in-apifox: https://app.apifox.com/web/project/7109750/apis/api-360272407-run
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

# wan视频查询

## OpenAPI Specification

```yaml
openapi: 3.0.1
info:
  title: ''
  description: ''
  version: 1.0.0
paths:
  /alibailian/api/v1/tasks/{task_id}:
    get:
      summary: 视频查询
      deprecated: false
      description: ''
      tags:
        - 视频模型/通义万象 视频生成
      parameters:
        - name: task_id
          in: path
          description: ''
          required: true
          example: a55bfe14-6e78-4b9d-b97d-128420399ed1
          schema:
            type: string
      responses:
        '200':
          description: ''
          content:
            application/json:
              schema:
                type: object
                properties:
                  usage:
                    type: object
                    properties:
                      SR:
                        type: integer
                      duration:
                        type: integer
                      video_count:
                        type: integer
                    required:
                      - SR
                      - duration
                      - video_count
                    x-apifox-orders:
                      - SR
                      - duration
                      - video_count
                  output:
                    type: object
                    properties:
                      task_id:
                        type: string
                      end_time:
                        type: string
                      video_url:
                        type: string
                      orig_prompt:
                        type: string
                      submit_time:
                        type: string
                      task_status:
                        type: string
                      actual_prompt:
                        type: string
                      scheduled_time:
                        type: string
                    required:
                      - task_id
                      - end_time
                      - video_url
                      - orig_prompt
                      - submit_time
                      - task_status
                      - actual_prompt
                      - scheduled_time
                    x-apifox-orders:
                      - task_id
                      - end_time
                      - video_url
                      - orig_prompt
                      - submit_time
                      - task_status
                      - actual_prompt
                      - scheduled_time
                  request_id:
                    type: string
                required:
                  - usage
                  - output
                  - request_id
                x-apifox-orders:
                  - usage
                  - output
                  - request_id
              example:
                usage:
                  SR: 480
                  duration: 5
                  video_count: 1
                output:
                  task_id: a55bfe14-6e78-4b9d-b97d-128420399ed1
                  end_time: '2025-10-11 19:02:27.297'
                  video_url: >-
                    https://dashscope-result-bj.oss-cn-beijing.aliyuncs.com/1d/21/20251011/9b1c82b0/a55bfe14-6e78-4b9d-b97d-128420399ed1.mp4?Expires=1760266939&OSSAccessKeyId=LTAI5tDUB1cEqFCYhEwWry26&Signature=Fjpx1uAlmLmeYGWx0Ye7n%2F%2F9Isw%3D
                  orig_prompt: 改变一下光线
                  submit_time: '2025-10-11 18:55:56.952'
                  task_status: SUCCEEDED
                  actual_prompt: >-
                    橙衣男性骑着扫帚从画面右侧向左侧高速飞过，身体前倾，双手紧握扫帚杆，头部朝前，嘴巴张开并大喊：'嘿！你们别拦我！'。扫帚在空中快速移动，扫帚毛因速度产生模糊效果。下方三人站在人行道上，手持魔杖指向空中人物，头部随其飞行方向轻微转动，注视其动作。背景中持续传来城市街道的车流声、行人交谈声、风声以及扫帚划过空气的呼啸声。
                  scheduled_time: '2025-10-11 18:55:57.908'
                request_id: ec8f059e-f2e5-4887-bb43-3306cd38f403
          headers: {}
          x-apifox-name: 成功
      security:
        - bearer: []
      x-apifox-folder: 视频模型/通义万象 视频生成
      x-apifox-status: developing
      x-run-in-apifox: https://app.apifox.com/web/project/7109750/apis/api-360272408-run
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
