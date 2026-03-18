#ifndef IMAGEUPLOADER_H
#define IMAGEUPLOADER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QProgressDialog>

class ImageUploader : public QObject
{
    Q_OBJECT

public:
    explicit ImageUploader(QObject *parent = nullptr);
    ~ImageUploader();

    // 上传图片到阿里云临时存储，返回 oss:// URL
    // apiKey: 阿里云百炼 API Key
    // modelName: 文件将用于的模型名称（如 grok-video-3）
    void uploadImage(const QString &localPath,
                     const QString &apiKey,
                     const QString &modelName);

    // 上传图片到 imgbb，返回 HTTP URL
    void uploadToImgbb(const QString &localPath, const QString &imgbbApiKey);

signals:
    void uploadSuccess(const QString &ossUrl);
    void uploadError(const QString &error);
    void retryAttempt(int retryCount, int maxRetries);

private slots:
    void onGetPolicyFinished();
    void onUploadToOssFinished();
    void onImgbbUploadFinished();
    void retryUpload();
    void cancelUpload();

private:
    void uploadToOss(const QJsonObject &policyData, const QString &filePath);
    void startImgbbUploadRequest();

    QNetworkAccessManager *networkManager;
    QNetworkReply *currentReply;

    // 保存当前上传任务的状态
    QString pendingFilePath;

    // imgbb 上传重试状态
    int maxRetries = 3;
    int currentRetry = 0;
    QList<int> retryDelays {2000, 5000, 10000};
    QString currentFilePath;
    QString currentImgbbApiKey;
    bool retryCanceled = false;
    QProgressDialog *progressDialog = nullptr;
};

#endif // IMAGEUPLOADER_H
