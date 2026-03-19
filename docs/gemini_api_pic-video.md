原生格式
图片理解
POST
/v1beta/models/gemini-2.5-pro:generateContent
官方文档：https://ai.google.dev/gemini-api/docs/image-understanding?hl=zh-cn


# 图片理解

## OpenAPI Specification

```yaml
openapi: 3.0.1
info:
  title: ''
  description: ''
  version: 1.0.0
paths:
  /v1beta/models/gemini-2.5-pro:generateContent:
    post:
      summary: 图片理解
      deprecated: false
      description: 官方文档：https://ai.google.dev/gemini-api/docs/image-understanding?hl=zh-cn
      tags:
        - 聊天(Chat)/谷歌Gemini 接口/原生格式
      parameters:
        - name: key
          in: query
          description: ''
          required: true
          example: '{{YOUR_API_KEY}}'
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
                contents:
                  type: array
                  items:
                    type: object
                    properties:
                      parts:
                        type: array
                        items:
                          type: object
                          properties:
                            text:
                              type: string
                            inline_data:
                              type: object
                              properties:
                                mime_type:
                                  type: string
                                data:
                                  type: string
                              required:
                                - mime_type
                                - data
                              x-apifox-orders:
                                - mime_type
                                - data
                          x-apifox-orders:
                            - text
                            - inline_data
                    x-apifox-orders:
                      - parts
                generationConfig:
                  type: object
                  properties:
                    responseModalities:
                      type: array
                      items:
                        type: string
                  required:
                    - responseModalities
                  x-apifox-orders:
                    - responseModalities
              required:
                - contents
                - generationConfig
              x-apifox-orders:
                - contents
                - generationConfig
            example:
              contents:
                - role: user
                  parts:
                    - inline_data:
                        mime_type: image/png
                        data: >-
                          (base64的图片数据，省略)
                    - text: Caption this image.
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
      x-apifox-folder: 聊天(Chat)/谷歌Gemini 接口/原生格式
      x-apifox-status: released
      x-run-in-apifox: https://app.apifox.com/web/project/7109750/apis/api-349239115-run
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

# 视频理解

## OpenAPI Specification

```yaml
openapi: 3.0.1
info:
  title: ''
  description: ''
  version: 1.0.0
paths:
  /v1beta/models/gemini-2.5-pro:generateContent:
    post:
      summary: 视频理解
      deprecated: false
      description: 官方文档：https://ai.google.dev/gemini-api/docs/video-understanding?hl=zh-cn
      tags:
        - 聊天(Chat)/谷歌Gemini 接口/原生格式
      parameters:
        - name: key
          in: query
          description: ''
          required: true
          example: '{{YOUR_API_KEY}}'
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
                contents:
                  type: array
                  items:
                    type: object
                    properties:
                      parts:
                        type: array
                        items:
                          type: object
                          properties:
                            inline_data:
                              type: object
                              properties:
                                mime_type:
                                  type: string
                                data:
                                  type: string
                              required:
                                - mime_type
                                - data
                              x-apifox-orders:
                                - mime_type
                                - data
                            text:
                              type: string
                          x-apifox-orders:
                            - inline_data
                            - text
                    x-apifox-orders:
                      - parts
              required:
                - contents
              x-apifox-orders:
                - contents
            example:
              contents:
                - role: user
                  parts:
                    - inline_data:
                        mime_type: video/mp4
                        data: >-
                          (base64的视频数据，省略)
                    - text: Please summarize the video in 3 sentences.
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
      x-apifox-folder: 聊天(Chat)/谷歌Gemini 接口/原生格式
      x-apifox-status: released
      x-run-in-apifox: https://app.apifox.com/web/project/7109750/apis/api-386534714-run
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