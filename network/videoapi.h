#ifndef VIDEOAPI_H
#define VIDEOAPI_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QStringList>

class ImageUploader;

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
                     const QString &imgbbApiKey = QString(),
                     bool enhancePrompt = true,
                     bool enableUpsample = true);

    // 统一的查询任务接口
    void queryTask(const QString &apiKey,
                   const QString &baseUrl,
                   const QString &modelName,
                   const QString &taskId);

    // 下载视频
    void downloadVideo(const QString &apiKey,
                       const QString &baseUrl,
                       const QString &taskId,
                       const QString &savePath);

signals:
    void videoCreated(const QString &taskId, const QString &status);
    void taskStatusUpdated(const QString &taskId, const QString &status, const QString &videoUrl, int progress);
    void videoDownloaded(const QString &savePath);
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
                         bool watermark);

    // VEO3 统一格式方法（JSON POST /v1/video/create）
    void createVeo3UnifiedVideo(const QString &apiKey,
                                const QString &baseUrl,
                                const QString &model,
                                const QString &prompt,
                                const QStringList &imageUrls,
                                const QString &aspectRatio,
                                bool enhancePrompt,
                                bool enableUpsample);

    // Grok 专用方法
    void createGrokVideo(const QString &apiKey,
                         const QString &baseUrl,
                         const QString &model,
                         const QString &prompt,
                         const QStringList &imagePaths,
                         const QString &aspectRatio,
                         const QString &size,
                         int duration);

    QNetworkAccessManager *networkManager;
    QMap<QNetworkReply*, QString> replyMap;
    ImageUploader *imageUploader;

    // 图片上传临时数据（Grok 和 VEO3统一格式共用）
    struct UnifiedFormatRequest {
        QString apiKey;
        QString baseUrl;
        QString model;
        QString prompt;
        QString aspectRatio;
        QString size;
        QString imgbbApiKey;
        QStringList localImagePaths;
        QStringList uploadedUrls;
        int uploadIndex;
        bool enhancePrompt;
        bool enableUpsample;
        QString targetMethod;  // "grok" | "veo3_unified"
        int duration;          // Grok: 6/10/15
    };
    UnifiedFormatRequest currentRequest;
};

#endif // VIDEOAPI_H

