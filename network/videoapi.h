#ifndef VIDEOAPI_H
#define VIDEOAPI_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QStringList>
#include <QJsonDocument>
#include <QJsonObject>

#include "imageuploader.h"

class VideoAPI : public QObject
{
    Q_OBJECT

public:
    explicit VideoAPI(QObject *parent = nullptr);
    ~VideoAPI();

    // 统一的创建视频接口
    void createVideo(const QString &apiKey,
                     const QString &baseUrl,
                     const QString &modelName,
                     const QString &model,
                     const QString &prompt,
                     const QStringList &imagePaths,
                     const QString &size,
                     const QString &seconds,
                     bool watermark,
                     const QString &aspectRatio = QString(),
                     const QString &imageUploadApiKey = QString(),
                     bool enhancePrompt = true,
                     bool enableUpsample = true,
                     const QString &apiFormat = QString(),
                     const QString &style = QString(),
                     bool privateMode = false,
                     const QString &orientation = QString(),
                     const QString &duration = QString());

    // 统一的查询任务接口
    void queryTask(const QString &apiKey,
                   const QString &baseUrl,
                   const QString &modelName,
                   const QString &taskId);

    static bool isQuotaInsufficientError(const QString &error);
    static QString normalizeUserFacingError(const QString &error);

    // 下载视频
    void downloadVideo(const QString &apiKey,
                       const QString &baseUrl,
                       const QString &taskId,
                       const QString &savePath);

signals:
    void videoCreated(const QString &taskId, const QString &status);
    void taskStatusUpdated(const QString &taskId, const QString &status, const QString &videoUrl, int progress);
    void videoDownloaded(const QString &savePath);
    void imageUploadProgress(int current, int total);
    void errorOccurred(const QString &error);

private slots:
    void onCreateVideoFinished();
    void onQueryTaskFinished();
    void onDownloadFinished();
    void onImageUploadSuccess(const QString &url);
    void onImageUploadError(const QString &error);

private:
    // VEO3 OpenAI格式方法
    void createVeo3Video(const QString &apiKey,
                         const QString &baseUrl,
                         const QString &model,
                         const QString &prompt,
                         const QStringList &imagePaths,
                         const QString &size,
                         const QString &seconds,
                         bool watermark,
                         const QString &style = QString(),
                         bool privateMode = false);

    // VEO3 统一格式方法（JSON POST /v1/video/create）
    void createVeo3UnifiedVideo(const QString &apiKey,
                                const QString &baseUrl,
                                const QString &model,
                                const QString &prompt,
                                const QStringList &imageUrls,
                                const QString &aspectRatio,
                                bool enhancePrompt,
                                bool enableUpsample,
                                const QString &size = QString(),
                                const QString &orientation = QString(),
                                const QString &duration = QString(),
                                bool watermark = false,
                                bool privateMode = false);

    // Grok 专用方法
    void createGrokVideo(const QString &apiKey,
                         const QString &baseUrl,
                         const QString &model,
                         const QString &prompt,
                         const QStringList &imagePaths,
                         const QString &aspectRatio,
                         const QString &size,
                         int duration);

    // WAN 专用方法
    void createWanVideo(const QString &apiKey,
                        const QString &baseUrl,
                        const QString &model,
                        const QString &prompt,
                        const QString &negativePrompt,
                        const QString &imageUrl,
                        const QString &audioUrl,
                        const QString &templateName,
                        const QString &resolution,
                        int duration,
                        bool promptExtend,
                        bool watermark,
                        const QString &seed);

    QNetworkAccessManager *networkManager;
    QMap<QNetworkReply*, QString> replyMap;
    ImageUploader *imageUploader;

    // 图片上传临时数据（内部使用）
    struct UnifiedFormatRequest {
        QString apiKey;
        QString baseUrl;
        QString model;
        QString prompt;
        QString aspectRatio;
        QString size;
        QString orientation;   // Sora2 unified: 方向
        QString durationText;  // Sora2 unified: 时长字符串
        QString imageUploadApiKey;
        QStringList localImagePaths;
        QStringList uploadedUrls;
        int uploadIndex;
        bool enhancePrompt;
        bool enableUpsample;
        bool privateMode;      // Sora2 openai/unified: private
        QString targetMethod;  // "grok" | "veo3_unified" | "wan"
        int duration;          // Grok/WAN: 秒数
        // WAN 专用字段
        QString negativePrompt;
        QString audioUrl;
        QString templateName;
        QString resolution;
        bool promptExtend;
        bool watermark;
        QString seed;
    };
    UnifiedFormatRequest currentRequest;

public:
    // 获取 ImageUploader 实例（只读访问）
    ImageUploader* getImageUploader() { return imageUploader; }
    
    // 公共方法：用于上传图片
    void uploadImageToImgbb(const QString &localPath, const QString &imageUploadApiKey) {
        imageUploader->uploadToImgbb(localPath, imageUploadApiKey);
    }
    
    // WAN 视频参数结构体
    struct WanVideoParams {
        QString apiKey;
        QString baseUrl;
        QString model;
        QString prompt;
        QString negativePrompt;
        QStringList localImagePaths;
        QString audioUrl;
        QString templateName;
        QString resolution;
        int duration;
        bool promptExtend;
        bool watermark;
        QString seed;
    };
    
    // 准备 WAN 请求（封装参数传递）
    void prepareWanRequest(const WanVideoParams &params);
};

#endif // VIDEOAPI_H

