# 图片生成 gemini-3-pro-image-preview 控制宽高比 +清晰度

## OpenAPI Specification

```yaml
openapi: 3.0.1
info:
  title: ''
  description: ''
  version: 1.0.0
paths:
  /v1beta/models/gemini-3-pro-image-preview:generateContent:
    post:
      summary: 图片生成 gemini-3-pro-image-preview 控制宽高比 +清晰度
      deprecated: false
      description: >-
        官方文档：https://ai.google.dev/gemini-api/docs/image-generation?hl=zh-cn#gemini-image-editing
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
                    imageConfig:
                      type: object
                      properties:
                        aspectRatio:
                          type: string
                          x-apifox-mock: '16:9'
                      x-apifox-orders:
                        - aspectRatio
                      required:
                        - aspectRatio
                  required:
                    - responseModalities
                    - imageConfig
                  x-apifox-orders:
                    - responseModalities
                    - imageConfig
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
                    - text: >-
                        'Hi, This is a picture of me. Can you add a llama next
                        to me
                    - inline_data:
                        mime_type: image/jpeg
                        data: //省略
              generationConfig:
                responseModalities:
                  - TEXT
                  - IMAGE
                imageConfig:
                  aspectRatio: '9:16'
                  imageSize: 1K
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
      x-run-in-apifox: https://app.apifox.com/web/project/7109750/apis/api-381740608-run
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