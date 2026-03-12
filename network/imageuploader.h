#ifndef IMAGEUPLOADER_H
#define IMAGEUPLOADER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>

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

private slots:
    void onGetPolicyFinished();
    void onUploadToOssFinished();
    void onImgbbUploadFinished();

private:
    void uploadToOss(const QJsonObject &policyData, const QString &filePath);

    QNetworkAccessManager *networkManager;
    QNetworkReply *currentReply;

    // 保存当前上传任务的状态
    QString pendingFilePath;
};

#endif // IMAGEUPLOADER_H
