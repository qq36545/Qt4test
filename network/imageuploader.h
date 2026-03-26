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

    // 上传图片到公共图床，返回 HTTP URL
    // imageUploadApiKey: 页面 API 密钥下拉值（用于 Bearer 鉴权）
    void uploadToImgbb(const QString &localPath, const QString &imageUploadApiKey);

    // 上传音频到 tmpfile.link，返回下载链接
    void uploadAudioToTmpfile(const QString &localPath);

signals:
    void uploadSuccess(const QString &ossUrl);
    void audioUploadSuccess(const QString &downloadUrl);
    void uploadError(const QString &error);
    void retryAttempt(int retryCount, int maxRetries);

private slots:
    void onGetPolicyFinished();
    void onUploadToOssFinished();
    void onPublicUploadFinished();
    void onTmpfileUploadFinished();
    void retryUpload();
    void cancelUpload();

private:
    void uploadToOss(const QJsonObject &policyData, const QString &filePath);
    void startPublicUploadRequest();

    QNetworkAccessManager *networkManager;
    QNetworkReply *currentReply;

    // 保存当前上传任务的状态
    QString pendingFilePath;

    // 图片上传重试状态
    int maxRetries = 3;
    int currentRetry = 0;
    QList<int> retryDelays {2000, 5000, 10000};
    QString currentFilePath;
    QString currentImageUploadApiKey;
    bool retryCanceled = false;
    QProgressDialog *progressDialog = nullptr;
};

#endif // IMAGEUPLOADER_H
