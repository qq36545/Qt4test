#include "imageuploader.h"
#include <QFile>
#include <QHttpMultiPart>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

ImageUploader::ImageUploader(QObject *parent)
    : QObject(parent)
    , networkManager(new QNetworkAccessManager(this))
    , currentReply(nullptr)
{
}

ImageUploader::~ImageUploader()
{
    if (currentReply) {
        currentReply->abort();
        currentReply->deleteLater();
    }
}

void ImageUploader::uploadImage(const QString &localPath)
{
    QFile *file = new QFile(localPath);
    if (!file->open(QIODevice::ReadOnly)) {
        emit uploadError("无法打开文件: " + localPath);
        delete file;
        return;
    }

    // 检查文件大小（100MB限制）
    qint64 fileSize = file->size();
    if (fileSize > 100 * 1024 * 1024) {
        emit uploadError("文件超过100MB限制");
        file->close();
        delete file;
        return;
    }

    // 构建 multipart/form-data
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/octet-stream"));
    QFileInfo fileInfo(localPath);
    QString disposition = QString("form-data; name=\"file\"; filename=\"%1\"").arg(fileInfo.fileName());
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant(disposition));
    filePart.setBodyDevice(file);
    file->setParent(multiPart);

    multiPart->append(filePart);

    // 发送请求到 tmpfile.link
    QNetworkRequest request(QUrl("https://tmpfile.link/api/upload"));
    request.setRawHeader("Accept", "application/json");

    currentReply = networkManager->post(request, multiPart);
    multiPart->setParent(currentReply);

    connect(currentReply, &QNetworkReply::finished, this, &ImageUploader::onUploadFinished);
}

void ImageUploader::onUploadFinished()
{
    if (!currentReply) return;

    if (currentReply->error() != QNetworkReply::NoError) {
        emit uploadError("上传失败: " + currentReply->errorString());
        currentReply->deleteLater();
        currentReply = nullptr;
        return;
    }

    QByteArray response = currentReply->readAll();

    // 调试：打印服务器响应
    qDebug() << "[ImageUploader] Server response:" << response;

    QJsonDocument doc = QJsonDocument::fromJson(response);

    if (!doc.isObject()) {
        emit uploadError("服务器返回格式错误: " + QString(response));
        currentReply->deleteLater();
        currentReply = nullptr;
        return;
    }

    QJsonObject obj = doc.object();

    // 调试：打印JSON对象
    qDebug() << "[ImageUploader] JSON object:" << obj;

    // tmpfile.link返回的字段是downloadLink
    QString url = obj.value("downloadLink").toString();
    if (url.isEmpty()) {
        url = obj.value("url").toString();
    }
    if (url.isEmpty()) {
        url = obj.value("link").toString();
    }

    if (url.isEmpty()) {
        emit uploadError("未获取到图片URL，响应: " + QString(response));
    } else {
        emit uploadSuccess(url);
    }

    currentReply->deleteLater();
    currentReply = nullptr;
}
