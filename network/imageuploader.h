#ifndef IMAGEUPLOADER_H
#define IMAGEUPLOADER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class ImageUploader : public QObject
{
    Q_OBJECT

public:
    explicit ImageUploader(QObject *parent = nullptr);
    ~ImageUploader();

    // 上传图片到 tmpfile.link，返回URL
    void uploadImage(const QString &localPath);

signals:
    void uploadSuccess(const QString &url);
    void uploadError(const QString &error);

private slots:
    void onUploadFinished();

private:
    QNetworkAccessManager *networkManager;
    QNetworkReply *currentReply;
};

#endif // IMAGEUPLOADER_H
