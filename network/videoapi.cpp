#include "videoapi.h"
#include "imageuploader.h"
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QDebug>

VideoAPI::VideoAPI(QObject *parent)
    : QObject(parent)
    , networkManager(new QNetworkAccessManager(this))
    , imageUploader(new ImageUploader(this))
{
    connect(imageUploader, &ImageUploader::uploadSuccess, this, &VideoAPI::onImageUploadSuccess);
    connect(imageUploader, &ImageUploader::uploadError, this, &VideoAPI::onImageUploadError);
}

VideoAPI::~VideoAPI()
{
}

void VideoAPI::createVideo(const QString &apiKey,
                           const QString &baseUrl,
                           const QString &modelName,
                           const QString &model,
                           const QString &prompt,
                           const QStringList &imagePaths,
                           const QString &size,
                           const QString &seconds,
                           bool watermark,
                           const QString &aspectRatio,
                           const QString &imgbbApiKey)
{
    // 根据模型名称分发到不同的实现
    if (modelName.contains("Grok", Qt::CaseInsensitive)) {
        // Grok模型：需要先上传图片获取URL
        currentGrokRequest.apiKey = apiKey;
        currentGrokRequest.baseUrl = baseUrl;
        currentGrokRequest.model = model;
        currentGrokRequest.prompt = prompt;
        currentGrokRequest.aspectRatio = aspectRatio;
        currentGrokRequest.size = size;
        currentGrokRequest.imgbbApiKey = imgbbApiKey;
        currentGrokRequest.localImagePaths = imagePaths;
        currentGrokRequest.uploadedUrls.clear();
        currentGrokRequest.uploadIndex = 0;

        // 开始上传第一张图片到 imgbb
        if (!imagePaths.isEmpty()) {
            imageUploader->uploadToImgbb(imagePaths.first(), imgbbApiKey);
        } else {
            emit errorOccurred("Grok模型需要至少一张图片");
        }
    } else {
        // VEO3模型：直接上传文件
        createVeo3Video(apiKey, baseUrl, model, prompt, imagePaths, size, seconds, watermark);
    }
}

void VideoAPI::createVeo3Video(const QString &apiKey,
                               const QString &baseUrl,
                               const QString &model,
                               const QString &prompt,
                               const QStringList &imagePaths,
                               const QString &size,
                               const QString &seconds,
                               bool watermark)
{
    QUrl url(baseUrl + "/v1/videos");
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());

    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    // 添加模型参数
    QHttpPart modelPart;
    modelPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                        QVariant("form-data; name=\"model\""));
    modelPart.setBody(model.toUtf8());
    multiPart->append(modelPart);

    // 添加提示词
    QHttpPart promptPart;
    promptPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                         QVariant("form-data; name=\"prompt\""));
    promptPart.setBody(prompt.toUtf8());
    multiPart->append(promptPart);

    // 添加时长
    QHttpPart secondsPart;
    secondsPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                          QVariant("form-data; name=\"seconds\""));
    secondsPart.setBody(seconds.toUtf8());
    multiPart->append(secondsPart);

    // 添加尺寸
    QHttpPart sizePart;
    sizePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"size\""));
    sizePart.setBody(size.toUtf8());
    multiPart->append(sizePart);

    // 添加水印参数
    QHttpPart watermarkPart;
    watermarkPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                            QVariant("form-data; name=\"watermark\""));
    watermarkPart.setBody(watermark ? "true" : "false");
    multiPart->append(watermarkPart);

    // 添加图片文件
    for (const QString &imagePath : imagePaths) {
        QFile *file = new QFile(imagePath);
        if (!file->open(QIODevice::ReadOnly)) {
            emit errorOccurred(QString("无法打开图片文件: %1").arg(imagePath));
            delete file;
            continue;
        }

        QHttpPart imagePart;
        imagePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("image/jpeg"));
        imagePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                            QVariant(QString("form-data; name=\"input_reference\"; filename=\"%1\"")
                                     .arg(QFileInfo(imagePath).fileName())));
        imagePart.setBodyDevice(file);
        file->setParent(multiPart);
        multiPart->append(imagePart);
    }

    QNetworkReply *reply = networkManager->post(request, multiPart);
    multiPart->setParent(reply);
    replyMap[reply] = "create";

    connect(reply, &QNetworkReply::finished, this, &VideoAPI::onCreateVideoFinished);
}

void VideoAPI::createGrokVideo(const QString &apiKey,
                                const QString &baseUrl,
                                const QString &model,
                                const QString &prompt,
                                const QStringList &imagePaths,
                                const QString &aspectRatio,
                                const QString &size)
{
    QUrl url(baseUrl + "/v1/video/create");
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    request.setRawHeader("Content-Type", "application/json");
    request.setRawHeader("Accept", "application/json");
    // 阿里云 oss:// 临时URL必须加此请求头，否则服务端无法解析
    request.setRawHeader("X-DashScope-OssResourceResolve", "enable");

    // 构建JSON请求体
    QJsonObject jsonObj;
    jsonObj["model"] = model;
    jsonObj["prompt"] = prompt;
    jsonObj["aspect_ratio"] = aspectRatio;
    jsonObj["size"] = size;

    QJsonArray imagesArray;
    for (const QString &imageUrl : imagePaths) {
        imagesArray.append(imageUrl);
    }
    jsonObj["images"] = imagesArray;

    QJsonDocument jsonDoc(jsonObj);
    QByteArray jsonData = jsonDoc.toJson();

    // 调试：打印请求信息
    qDebug() << "[VideoAPI] Grok API Request:";
    qDebug() << "  URL:" << url.toString();
    qDebug() << "  Model:" << model;
    qDebug() << "  Prompt:" << prompt;
    qDebug() << "  Aspect Ratio:" << aspectRatio;
    qDebug() << "  Size:" << size;
    qDebug() << "  Image count:" << imagePaths.size();
    qDebug() << "  JSON Body size:" << jsonData.size();

    QNetworkReply *reply = networkManager->post(request, jsonData);
    replyMap[reply] = "create";

    connect(reply, &QNetworkReply::finished, this, &VideoAPI::onCreateVideoFinished);
}

void VideoAPI::queryTask(const QString &apiKey,
                         const QString &baseUrl,
                         const QString &modelName,
                         const QString &taskId)
{
    QUrl url;
    if (modelName.contains("Grok", Qt::CaseInsensitive)) {
        // Grok查询接口
        url = QUrl(baseUrl + "/v1/video/query");
        QUrlQuery query;
        query.addQueryItem("id", taskId);
        url.setQuery(query);
    } else {
        // VEO3查询接口
        url = QUrl(baseUrl + "/v1/videos/" + taskId);
    }

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    request.setRawHeader("Content-Type", "application/json");
    request.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = networkManager->get(request);
    replyMap[reply] = "query";

    connect(reply, &QNetworkReply::finished, this, &VideoAPI::onQueryTaskFinished);
}

void VideoAPI::downloadVideo(const QString &apiKey,
                             const QString &baseUrl,
                             const QString &taskId,
                             const QString &savePath)
{
    QUrl url(baseUrl + "/v1/videos/" + taskId + "/content");
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());

    QNetworkReply *reply = networkManager->get(request);
    replyMap[reply] = savePath;

    connect(reply, &QNetworkReply::finished, this, &VideoAPI::onDownloadFinished);
}

void VideoAPI::onImageUploadSuccess(const QString &url)
{
    // 图片上传成功，添加到URL列表
    currentGrokRequest.uploadedUrls.append(url);
    currentGrokRequest.uploadIndex++;

    // 检查是否还有更多图片需要上传
    if (currentGrokRequest.uploadIndex < currentGrokRequest.localImagePaths.size()) {
        imageUploader->uploadToImgbb(
            currentGrokRequest.localImagePaths[currentGrokRequest.uploadIndex],
            currentGrokRequest.imgbbApiKey);
    } else {
        // 所有图片上传完成，调用Grok API（传 uploadedUrls）
        createGrokVideo(currentGrokRequest.apiKey,
                        currentGrokRequest.baseUrl,
                        currentGrokRequest.model,
                        currentGrokRequest.prompt,
                        currentGrokRequest.uploadedUrls,
                        currentGrokRequest.aspectRatio,
                        currentGrokRequest.size);
    }
}

void VideoAPI::onImageUploadError(const QString &error)
{
    emit errorOccurred("图片上传失败: " + error);
}

void VideoAPI::onCreateVideoFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    QByteArray responseData = reply->readAll();

    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObj = jsonDoc.object();

        QString taskId = jsonObj["id"].toString();
        QString status = jsonObj["status"].toString();

        emit videoCreated(taskId, status);
    } else {
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug() << "[VideoAPI] create video failed:" << statusCode << responseData;
        emit errorOccurred(QString("创建视频失败(HTTP %1): %2")
                           .arg(statusCode)
                           .arg(responseData.isEmpty() ? reply->errorString() : QString::fromUtf8(responseData)));
    }

    replyMap.remove(reply);
    reply->deleteLater();
}

void VideoAPI::onQueryTaskFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObj = jsonDoc.object();

        QString taskId = jsonObj["id"].toString();
        QString status = jsonObj["status"].toString();
        QString videoUrl = jsonObj["video_url"].toString();
        int progress = jsonObj["progress"].toInt();

        emit taskStatusUpdated(taskId, status, videoUrl, progress);
    } else {
        emit errorOccurred(QString("查询任务失败: %1").arg(reply->errorString()));
    }

    replyMap.remove(reply);
    reply->deleteLater();
}

void VideoAPI::onDownloadFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    QString savePath = replyMap[reply];

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray videoData = reply->readAll();
        QFile file(savePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(videoData);
            file.close();
            emit videoDownloaded(savePath);
        } else {
            emit errorOccurred(QString("无法保存视频文件: %1").arg(savePath));
        }
    } else {
        emit errorOccurred(QString("下载视频失败: %1").arg(reply->errorString()));
    }

    replyMap.remove(reply);
    reply->deleteLater();
}




