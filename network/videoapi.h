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
                     const QString &aspectRatio = QString());

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
    // VEO3 专用方法
    void createVeo3Video(const QString &apiKey,
                         const QString &baseUrl,
                         const QString &model,
                         const QString &prompt,
                         const QStringList &imagePaths,
                         const QString &size,
                         const QString &seconds,
                         bool watermark);

    // Grok 专用方法
    void createGrokVideo(const QString &apiKey,
                         const QString &baseUrl,
                         const QString &model,
                         const QString &prompt,
                         const QStringList &imageUrls,
                         const QString &aspectRatio,
                         const QString &size);

    QNetworkAccessManager *networkManager;
    QMap<QNetworkReply*, QString> replyMap;
    ImageUploader *imageUploader;

    // Grok图片上传临时数据
    struct GrokVideoRequest {
        QString apiKey;
        QString baseUrl;
        QString model;
        QString prompt;
        QString aspectRatio;
        QString size;
        QStringList localImagePaths;
        QStringList uploadedUrls;
        int uploadIndex;
    };
    GrokVideoRequest currentGrokRequest;
};

#endif // VIDEOAPI_H

