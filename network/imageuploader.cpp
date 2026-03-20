#include "imageuploader.h"
#include <QSettings>
#include <QFile>
#include <QHttpMultiPart>
#include <QFileInfo>
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
    body += "Content-Type: application/octet-stream\r\n\r\n";
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

void ImageUploader::uploadToImgbb(const QString &localPath, const QString &imgbbApiKey)
{
    if (!QFile::exists(localPath)) {
        emit uploadError("文件不存在: " + localPath);
        return;
    }

    QFileInfo fi(localPath);
    if (fi.size() > 32 * 1024 * 1024) {
        emit uploadError("文件超过imgbb 32MB限制: " + localPath);
        return;
    }

    currentFilePath = localPath;
    currentImgbbApiKey = imgbbApiKey;
    currentRetry = 0;
    retryCanceled = false;

    if (!progressDialog) {
        progressDialog = new QProgressDialog(QString(), "取消重试", 0, 0);
        progressDialog->setAutoClose(false);
        progressDialog->setAutoReset(false);
        progressDialog->setMinimumDuration(0);
        progressDialog->setWindowModality(Qt::ApplicationModal);
        connect(progressDialog, &QProgressDialog::canceled, this, &ImageUploader::cancelUpload);
    }

    progressDialog->setLabelText("正在上传图片...");
    progressDialog->show();

    startImgbbUploadRequest();
}

void ImageUploader::startImgbbUploadRequest()
{
    QFile file(currentFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (progressDialog) progressDialog->hide();
        emit uploadError("无法打开文件: " + currentFilePath);
        return;
    }

    QByteArray base64Data = file.readAll().toBase64();
    file.close();

    QUrl url(QString("https://api.imgbb.com/1/upload?key=%1").arg(currentImgbbApiKey));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setTransferTimeout(120000);

    QByteArray body = "image=" + QUrl::toPercentEncoding(QString::fromLatin1(base64Data));

    currentReply = networkManager->post(request, body);
    connect(currentReply, &QNetworkReply::finished, this, &ImageUploader::onImgbbUploadFinished);
}

void ImageUploader::onImgbbUploadFinished()
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
        emit uploadError(QString("imgbb上传失败(HTTP %1): %2")
                         .arg(statusCode)
                         .arg(response.isEmpty() ? errorString : QString::fromUtf8(response)));
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(response);
    QJsonObject root = doc.object();
    QString imageUrl = root["data"].toObject()["url"].toString();
    if (imageUrl.isEmpty()) {
        if (currentRetry < maxRetries) {
            retryUpload();
            return;
        }

        if (progressDialog) progressDialog->hide();
        emit uploadError("imgbb响应缺少url字段: " + QString(response));
        return;
    }

    if (progressDialog) progressDialog->hide();
    qDebug() << "[ImageUploader] imgbb upload success:" << imageUrl;
    emit uploadSuccess(imageUrl);
}

void ImageUploader::retryUpload()
{
    if (retryCanceled) return;

    currentRetry++;
    emit retryAttempt(currentRetry, maxRetries);

    if (progressDialog) {
        progressDialog->setLabelText(QString("API Error，正在重试第 %1/%2 次...")
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
        startImgbbUploadRequest();
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
