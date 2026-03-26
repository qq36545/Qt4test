#include "imageuploader.h"
#include <QSettings>
#include <QFile>
#include <QHttpMultiPart>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QDebug>
#include <QTimer>
#include <cstdint>

// 从环境变量或配置文件获取 API Key
static QString getDashscopeApiKey()
{
    QByteArray key = qgetenv("DASHSCOPE_API_KEY");
    if (!key.isEmpty()) {
        return QString::fromUtf8(key);
    }
    QSettings settings("ChickenAI", "VideoGen");
    return settings.value("dashscopeApiKey", "").toString();
}

static QString detectUploadMimeType(const QFileInfo &fileInfo)
{
    QMimeDatabase db;
    QString detected = db.mimeTypeForFile(fileInfo).name();
    if (detected.isEmpty() || detected == "application/octet-stream") {
        const QString suffix = fileInfo.suffix().toLower();
        if (suffix == "jpg" || suffix == "jpeg") detected = "image/jpeg";
        else if (suffix == "png") detected = "image/png";
        else if (suffix == "webp") detected = "image/webp";
        else if (suffix == "bmp") detected = "image/bmp";
        else detected = "image/jpeg";
    }
    return detected;
}

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
    if (progressDialog) {
        progressDialog->hide();
        progressDialog->deleteLater();
        progressDialog = nullptr;
    }
}

void ImageUploader::uploadImage(const QString &localPath,
                                const QString &apiKey,
                                const QString &modelName)
{
    if (!QFile::exists(localPath)) {
        emit uploadError("文件不存在: " + localPath);
        return;
    }

    pendingFilePath = localPath;

    Q_UNUSED(apiKey)
    const QString realKey = getDashscopeApiKey();
    if (realKey.isEmpty()) {
        emit uploadError("未配置 DashScope API Key，请设置环境变量 DASHSCOPE_API_KEY 或在设置中配置");
        return;
    }

    // 步骤1：获取阿里云 OSS 上传凭证
    QUrl url("https://dashscope.aliyuncs.com/api/v1/uploads");
    QUrlQuery query;
    query.addQueryItem("action", "getPolicy");
    query.addQueryItem("model", modelName);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QString("Bearer %1").arg(realKey).toUtf8());
    request.setRawHeader("Content-Type", "application/json");

    currentReply = networkManager->get(request);
    connect(currentReply, &QNetworkReply::finished, this, &ImageUploader::onGetPolicyFinished);
}

void ImageUploader::onGetPolicyFinished()
{
    if (!currentReply) return;

    if (currentReply->error() != QNetworkReply::NoError) {
        emit uploadError("获取上传凭证失败: " + currentReply->errorString());
        currentReply->deleteLater();
        currentReply = nullptr;
        return;
    }

    QByteArray response = currentReply->readAll();
    currentReply->deleteLater();
    currentReply = nullptr;

    qDebug() << "[ImageUploader] getPolicy response:" << response;

    QJsonDocument doc = QJsonDocument::fromJson(response);
    if (!doc.isObject()) {
        emit uploadError("获取上传凭证响应格式错误: " + QString(response));
        return;
    }

    QJsonObject root = doc.object();
    if (!root.contains("data")) {
        emit uploadError("获取上传凭证响应缺少data字段: " + QString(response));
        return;
    }

    QJsonObject policyData = root["data"].toObject();
    uploadToOss(policyData, pendingFilePath);
}

void ImageUploader::uploadToOss(const QJsonObject &policyData, const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit uploadError("无法打开文件: " + filePath);
        return;
    }

    const QByteArray fileData = file.readAll();
    file.close();

    QFileInfo fileInfo(filePath);
    const QString uploadMimeType = detectUploadMimeType(fileInfo);
    const QString uploadDir = policyData["upload_dir"].toString();
    const QString key = uploadDir + "/" + fileInfo.fileName();
    const QString uploadHost = policyData["upload_host"].toString();
    const QByteArray boundary = "----ClaudeQtBoundary7MA4YWxkTrZu0gW";

    auto appendTextPart = [&](QByteArray &body, const QString &name, const QString &value) {
        body += "--" + boundary + "\r\n";
        body += "Content-Disposition: form-data; name=\"" + name.toUtf8() + "\"\r\n\r\n";
        body += value.toUtf8();
        body += "\r\n";
    };

    QByteArray body;
    appendTextPart(body, "OSSAccessKeyId", policyData["oss_access_key_id"].toString());
    appendTextPart(body, "Signature", policyData["signature"].toString());
    appendTextPart(body, "policy", policyData["policy"].toString());
    appendTextPart(body, "x-oss-object-acl", policyData["x_oss_object_acl"].toString());
    appendTextPart(body, "x-oss-forbid-overwrite", policyData["x_oss_forbid_overwrite"].toString());
    appendTextPart(body, "key", key);
    appendTextPart(body, "success_action_status", "200");

    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"file\"; filename=\"" + fileInfo.fileName().toUtf8() + "\"\r\n";
    body += "Content-Type: " + uploadMimeType.toUtf8() + "\r\n\r\n";
    body += fileData;
    body += "\r\n";
    body += "--" + boundary + "--\r\n";

    QUrl ossUrl(uploadHost);
    QNetworkRequest request(ossUrl);
    request.setRawHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
    request.setHeader(QNetworkRequest::ContentLengthHeader, body.size());

    currentReply = networkManager->post(request, body);

    connect(currentReply, &QNetworkReply::finished, this, &ImageUploader::onUploadToOssFinished);

    // 上传成功后返回 oss:// URL
    currentReply->setProperty("ossKey", key);
}

void ImageUploader::onUploadToOssFinished()
{
    if (!currentReply) return;

    int statusCode = currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (currentReply->error() != QNetworkReply::NoError || statusCode != 200) {
        QByteArray body = currentReply->readAll();
        emit uploadError(QString("上传文件到OSS失败(HTTP %1): %2")
                         .arg(statusCode)
                         .arg(QString(body)));
        currentReply->deleteLater();
        currentReply = nullptr;
        return;
    }

    QString ossKey = currentReply->property("ossKey").toString();
    currentReply->deleteLater();
    currentReply = nullptr;

    QString ossUrl = "oss://" + ossKey;
    qDebug() << "[ImageUploader] Upload success, oss url:" << ossUrl;
    emit uploadSuccess(ossUrl);
}

void ImageUploader::uploadToImgbb(const QString &localPath, const QString &imageUploadApiKey)
{
    if (!QFile::exists(localPath)) {
        emit uploadError("文件不存在: " + localPath);
        return;
    }

    currentFilePath = localPath;
    currentImageUploadApiKey = imageUploadApiKey;
    currentRetry = 0;
    retryCanceled = false;

    startPublicUploadRequest();
}

void ImageUploader::startPublicUploadRequest()
{
    // Capture old reply before it is overwritten by the new post() below.
    // Qt re-enters the event loop during network operations, so we must ensure
    // the old reply's stale finished() signal cannot reach onPublicUploadFinished
    // via the new currentReply pointer.
    QNetworkReply *oldReply = currentReply;
    if (oldReply) {
        disconnect(oldReply, &QNetworkReply::finished, this, &ImageUploader::onPublicUploadFinished);
        // Deferred lambda: abort+delete the old reply after this function returns
        // and the event loop is re-entered (post() below resets currentReply first).
        QTimer::singleShot(0, this, [oldReply]() {
            oldReply->abort();
            oldReply->deleteLater();
        });
    }

    QFile *file = new QFile(currentFilePath);
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        if (progressDialog) progressDialog->hide();
        emit uploadError("无法打开文件: " + currentFilePath);
        return;
    }

    QFileInfo fi(currentFilePath);
    const QString uploadMimeType = detectUploadMimeType(fi);

    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant(QString("form-data; name=\"file\"; filename=\"%1\"").arg(fi.fileName())));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(uploadMimeType));
    filePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(filePart);

    QUrl url("https://imageproxy.zhongzhuan.chat/api/upload");
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QString("Bearer %1").arg(currentImageUploadApiKey).toUtf8());
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(120000);

    currentReply = networkManager->post(request, multiPart);
    multiPart->setParent(currentReply);
    connect(currentReply, &QNetworkReply::finished, this, &ImageUploader::onPublicUploadFinished);
}

void ImageUploader::onPublicUploadFinished()
{
    if (!currentReply) return;

    QByteArray response = currentReply->readAll();
    int statusCode = currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QNetworkReply::NetworkError networkError = currentReply->error();
    QString errorString = currentReply->errorString();
    currentReply->deleteLater();
    currentReply = nullptr;

    if (retryCanceled) {
        if (progressDialog) progressDialog->hide();
        return;
    }

    const bool success = (networkError == QNetworkReply::NoError && statusCode == 200);
    if (!success) {
        if (currentRetry < maxRetries) {
            retryUpload();
            return;
        }

        if (progressDialog) progressDialog->hide();
        emit uploadError(QString("图片上传失败(HTTP %1): %2")
                         .arg(statusCode)
                         .arg(response.isEmpty() ? errorString : QString::fromUtf8(response)));
        return;
    }

    QString imageUrl;
    QJsonDocument doc = QJsonDocument::fromJson(response);
    if (doc.isObject()) {
        QJsonObject root = doc.object();
        QJsonObject dataObj = root["data"].toObject();
        imageUrl = dataObj["url"].toString();
        if (imageUrl.isEmpty()) {
            imageUrl = root["url"].toString();
        }
        if (imageUrl.isEmpty()) {
            imageUrl = dataObj["display_url"].toString();
        }
    }

    if (imageUrl.isEmpty()) {
        if (currentRetry < maxRetries) {
            retryUpload();
            return;
        }

        if (progressDialog) progressDialog->hide();
        emit uploadError("图片上传响应缺少data.url字段: " + QString::fromUtf8(response));
        return;
    }

    if (progressDialog) progressDialog->hide();
    qDebug() << "[ImageUploader] public upload success:" << imageUrl;
    emit uploadSuccess(imageUrl);
}

void ImageUploader::retryUpload()
{
    if (retryCanceled) return;

    currentRetry++;
    emit retryAttempt(currentRetry, maxRetries);

    if (progressDialog) {
        progressDialog->setLabelText(QString("图片上传失败，正在重试第 %1/%2 次...")
                                     .arg(currentRetry)
                                     .arg(maxRetries));
        progressDialog->show();
    }

    int delayMs = retryDelays.value(currentRetry - 1, 10000);
    QTimer::singleShot(delayMs, this, [this]() {
        if (retryCanceled) {
            if (progressDialog) progressDialog->hide();
            return;
        }
        startPublicUploadRequest();
    });
}

void ImageUploader::cancelUpload()
{
    retryCanceled = true;

    if (currentReply) {
        currentReply->abort();
        currentReply->deleteLater();
        currentReply = nullptr;
    }

    if (progressDialog) {
        progressDialog->hide();
    }

    emit uploadError("用户取消重试");
}

void ImageUploader::uploadAudioToTmpfile(const QString &localPath)
{
    if (!QFile::exists(localPath)) {
        emit uploadError("音频文件不存在: " + localPath);
        return;
    }

    QFileInfo fi(localPath);
    // 检查文件大小限制 (100MB)
    if (fi.size() > 100 * 1024 * 1024) {
        emit uploadError("音频文件超过100MB限制: " + localPath);
        return;
    }

    QFile *file = new QFile(localPath);
    if (!file->open(QIODevice::ReadOnly)) {
        emit uploadError("无法打开音频文件: " + localPath);
        delete file;
        return;
    }

    // curl -F "file=@/path/to/file.ext" => multipart/form-data，字段名 "file"
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                      QVariant(QString("form-data; name=\"file\"; filename=\"%1\"").arg(fi.fileName())));
    filePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(filePart);

    QUrl url("https://tmpfile.link/api/upload");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QString("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36"));
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(120000);

    currentReply = networkManager->post(request, multiPart);
    multiPart->setParent(currentReply);

    connect(currentReply, &QNetworkReply::finished, this, &ImageUploader::onTmpfileUploadFinished);
}

void ImageUploader::onTmpfileUploadFinished()
{
    if (!currentReply) return;

    QByteArray response = currentReply->readAll();
    int statusCode = currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QNetworkReply::NetworkError networkError = currentReply->error();
    currentReply->deleteLater();
    currentReply = nullptr;

    if (networkError != QNetworkReply::NoError || statusCode != 200) {
        emit uploadError(QString("tmpfile上传失败(HTTP %1): %2")
                        .arg(statusCode)
                        .arg(QString::fromUtf8(response)));
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(response);
    QJsonObject root = doc.object();

    // tmpfile.link 返回格式：downloadLink 可能在根层级，也可能在 data 下
    QString downloadLink;
    if (!root["downloadLink"].toString().isEmpty()) {
        downloadLink = root["downloadLink"].toString();
    } else if (!root["data"].toObject()["downloadLink"].toString().isEmpty()) {
        downloadLink = root["data"].toObject()["downloadLink"].toString();
    } else if (!root["url"].toString().isEmpty()) {
        downloadLink = root["url"].toString();
    }

    if (downloadLink.isEmpty()) {
        emit uploadError("tmpfile响应缺少downloadLink字段: " + QString(response));
        return;
    }

    qDebug() << "[ImageUploader] tmpfile upload success:" << downloadLink;
    emit audioUploadSuccess(downloadLink);
}
