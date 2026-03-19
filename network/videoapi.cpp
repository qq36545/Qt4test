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
                           const QString &imgbbApiKey,
                           bool enhancePrompt,
                           bool enableUpsample)
{
    // 根据模型名称分发到不同的实现
    if (modelName.contains("Grok", Qt::CaseInsensitive)) {
        // Grok模型：需要先上传图片获取URL
        currentRequest.apiKey = apiKey;
        currentRequest.baseUrl = baseUrl;
        currentRequest.model = model;
        currentRequest.prompt = prompt;
        currentRequest.aspectRatio = aspectRatio;
        currentRequest.size = size;
        currentRequest.imgbbApiKey = imgbbApiKey;
        currentRequest.localImagePaths = imagePaths;
        currentRequest.uploadedUrls.clear();
        currentRequest.uploadIndex = 0;
        currentRequest.enhancePrompt = enhancePrompt;
        currentRequest.enableUpsample = enableUpsample;
        currentRequest.targetMethod = "grok";
        currentRequest.duration = seconds.toInt();

        // 开始上传第一张图片到 imgbb；无图片则直接文生视频
        if (!imagePaths.isEmpty()) {
            imageUploader->uploadToImgbb(imagePaths.first(), imgbbApiKey);
        } else {
            createGrokVideo(apiKey, baseUrl, model, prompt, {}, aspectRatio, size, seconds.toInt());
        }
    } else if (model.startsWith("veo_")) {
        // VEO3 OpenAI格式：直接上传文件
        createVeo3Video(apiKey, baseUrl, model, prompt, imagePaths, size, seconds, watermark);
    } else if (model.contains("wan", Qt::CaseInsensitive)) {
        // WAN 视频：先上传图片到 imgbb
        currentRequest.apiKey = apiKey;
        currentRequest.baseUrl = baseUrl;
        currentRequest.model = model;
        currentRequest.prompt = prompt;
        currentRequest.imgbbApiKey = imgbbApiKey;
        currentRequest.localImagePaths = imagePaths;
        currentRequest.uploadedUrls.clear();
        currentRequest.uploadIndex = 0;
        currentRequest.targetMethod = "wan";
        // WAN 参数通过扩展参数传入，这里只处理图片上传
        if (!imagePaths.isEmpty()) {
            imageUploader->uploadToImgbb(imagePaths.first(), imgbbApiKey);
        } else {
            // 无图片直接调用（需要调用方传入完整参数）
            emit errorOccurred("WAN 模型需要上传图片");
        }
    } else {
        // VEO3 统一格式：需要先上传图片到 imgbb 获取 URL
        currentRequest.apiKey = apiKey;
        currentRequest.baseUrl = baseUrl;
        currentRequest.model = model;
        currentRequest.prompt = prompt;
        currentRequest.aspectRatio = aspectRatio;
        currentRequest.size = size;
        currentRequest.imgbbApiKey = imgbbApiKey;
        currentRequest.localImagePaths = imagePaths;
        currentRequest.uploadedUrls.clear();
        currentRequest.uploadIndex = 0;
        currentRequest.enhancePrompt = enhancePrompt;
        currentRequest.enableUpsample = enableUpsample;
        currentRequest.targetMethod = "veo3_unified";

        if (!imagePaths.isEmpty()) {
            imageUploader->uploadToImgbb(imagePaths.first(), imgbbApiKey);
        } else {
            // 无图片直接调用
            createVeo3UnifiedVideo(apiKey, baseUrl, model, prompt, QStringList(),
                                   aspectRatio, enhancePrompt, enableUpsample);
        }
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
    request.setTransferTimeout(60000);  // 60秒超时

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

void VideoAPI::createVeo3UnifiedVideo(const QString &apiKey,
                                      const QString &baseUrl,
                                      const QString &model,
                                      const QString &prompt,
                                      const QStringList &imageUrls,
                                      const QString &aspectRatio,
                                      bool enhancePrompt,
                                      bool enableUpsample)
{
    QUrl url(baseUrl + "/v1/video/create");
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    request.setRawHeader("Content-Type", "application/json");
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(60000);  // 60秒超时

    QJsonObject jsonObj;
    jsonObj["model"] = model;
    jsonObj["prompt"] = prompt;
    jsonObj["aspect_ratio"] = aspectRatio;
    jsonObj["enhance_prompt"] = enhancePrompt;
    jsonObj["enable_upsample"] = enableUpsample;

    QJsonArray imagesArray;
    for (const QString &imageUrl : imageUrls) {
        imagesArray.append(imageUrl);
    }
    jsonObj["images"] = imagesArray;

    QJsonDocument jsonDoc(jsonObj);
    QByteArray jsonData = jsonDoc.toJson();

    qDebug() << "[VideoAPI] VEO3 Unified API Request:";
    qDebug() << "  URL:" << url.toString();
    qDebug() << "  Model:" << model;
    qDebug() << "  Aspect Ratio:" << aspectRatio;
    qDebug() << "  enhance_prompt:" << enhancePrompt;
    qDebug() << "  enable_upsample:" << enableUpsample;
    qDebug() << "  Image count:" << imageUrls.size();

    QNetworkReply *reply = networkManager->post(request, jsonData);
    replyMap[reply] = "create";

    connect(reply, &QNetworkReply::finished, this, &VideoAPI::onCreateVideoFinished);
}


void VideoAPI::createGrokVideo(const QString &apiKey,
                                const QString &baseUrl,
                                const QString &model,
                                const QString &prompt,
                                const QStringList &imagePaths,
                                const QString &aspectRatio,
                                const QString &size,
                                int duration)
{
    QUrl url(baseUrl + "/v1/video/create");
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    request.setRawHeader("Content-Type", "application/json");
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(60000);  // 60秒超时
    // 阿里云 oss:// 临时URL必须加此请求头，否则服务端无法解析
    request.setRawHeader("X-DashScope-OssResourceResolve", "enable");

    // 构建JSON请求体
    QJsonObject jsonObj;
    jsonObj["model"] = model;
    jsonObj["prompt"] = prompt;
    jsonObj["aspect_ratio"] = aspectRatio;
    jsonObj["size"] = size;
    if (duration > 0) {
        jsonObj["duration"] = duration;
    }

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
    qDebug() << "  Duration:" << duration;
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
    } else if (modelName.contains("wan", Qt::CaseInsensitive)) {
        // WAN 查询接口
        url = QUrl(baseUrl + "/alibailian/api/v1/tasks/" + taskId);
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

void VideoAPI::createWanVideo(const QString &apiKey,
                              const QString &baseUrl,
                              const QString &model,
                              const QString &prompt,
                              const QString &negativePrompt,
                              const QString &imageUrl,
                              const QString &audioUrl,
                              const QString &templateName,
                              const QString &resolution,
                              int duration,
                              bool promptExtend,
                              bool watermark,
                              const QString &seed)
{
    QUrl url(baseUrl + "/alibailian/api/v1/services/aigc/video-generation/video-synthesis");
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    request.setRawHeader("Content-Type", "application/json");
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(60000);

    QJsonObject jsonObj;
    jsonObj["model"] = model;

    QJsonObject inputObj;
    inputObj["prompt"] = prompt;
    if (!negativePrompt.isEmpty()) {
        inputObj["negative_prompt"] = negativePrompt;
    }
    if (!imageUrl.isEmpty()) {
        inputObj["img_url"] = imageUrl;
    }
    if (!audioUrl.isEmpty()) {
        inputObj["audio_url"] = audioUrl;
    }
    if (!templateName.isEmpty() && templateName != "无") {
        inputObj["template"] = templateName;
    }
    jsonObj["input"] = inputObj;

    QJsonObject parametersObj;
    parametersObj["resolution"] = resolution;
    parametersObj["duration"] = duration;
    parametersObj["prompt_extend"] = promptExtend;
    parametersObj["watermark"] = watermark;
    if (!seed.isEmpty()) {
        parametersObj["seed"] = seed.toLongLong();
    }
    // audio 字段：仅 wan2.5 支持
    if (model.contains("wan2.5", Qt::CaseInsensitive) && !audioUrl.isEmpty()) {
        parametersObj["audio"] = true;
    }
    jsonObj["parameters"] = parametersObj;

    QJsonDocument jsonDoc(jsonObj);
    QByteArray jsonData = jsonDoc.toJson();

    qDebug() << "[VideoAPI] WAN API Request:";
    qDebug() << "  URL:" << url.toString();
    qDebug() << "  Model:" << model;
    qDebug() << "  Resolution:" << resolution;
    qDebug() << "  Duration:" << duration;
    qDebug() << "  Template:" << templateName;
    qDebug() << "  Prompt extend:" << promptExtend;
    qDebug() << "  Image URL:" << imageUrl;
    qDebug() << "  Audio URL:" << audioUrl;

    QNetworkReply *reply = networkManager->post(request, jsonData);
    replyMap[reply] = "create";

    connect(reply, &QNetworkReply::finished, this, &VideoAPI::onCreateVideoFinished);
}

void VideoAPI::onImageUploadSuccess(const QString &url)
{
    // 图片上传成功，添加到URL列表
    currentRequest.uploadedUrls.append(url);
    currentRequest.uploadIndex++;

    // 检查是否还有更多图片需要上传
    if (currentRequest.uploadIndex < currentRequest.localImagePaths.size()) {
        imageUploader->uploadToImgbb(
            currentRequest.localImagePaths[currentRequest.uploadIndex],
            currentRequest.imgbbApiKey);
    } else {
        // 所有图片上传完成，根据 targetMethod 分发
        if (currentRequest.targetMethod == "grok") {
            createGrokVideo(currentRequest.apiKey,
                            currentRequest.baseUrl,
                            currentRequest.model,
                            currentRequest.prompt,
                            currentRequest.uploadedUrls,
                            currentRequest.aspectRatio,
                            currentRequest.size,
                            currentRequest.duration);
        } else if (currentRequest.targetMethod == "wan") {
            // WAN：调用 createWanVideo（参数通过扩展接口传入，这里用已上传的图片URL）
            // 注意：WAN 参数需要在调用 createVideo 时通过额外机制传递
            createWanVideo(currentRequest.apiKey,
                           currentRequest.baseUrl,
                           currentRequest.model,
                           currentRequest.prompt,
                           currentRequest.negativePrompt,
                           currentRequest.uploadedUrls.isEmpty() ? QString() : currentRequest.uploadedUrls.first(),
                           currentRequest.audioUrl,
                           currentRequest.templateName,
                           currentRequest.resolution,
                           currentRequest.duration,
                           currentRequest.promptExtend,
                           currentRequest.watermark,
                           currentRequest.seed);
        } else {
            // veo3_unified
            createVeo3UnifiedVideo(currentRequest.apiKey,
                                   currentRequest.baseUrl,
                                   currentRequest.model,
                                   currentRequest.prompt,
                                   currentRequest.uploadedUrls,
                                   currentRequest.aspectRatio,
                                   currentRequest.enhancePrompt,
                                   currentRequest.enableUpsample);
        }
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

        // 检查是否是 WAN 返回格式 { request_id, output: { task_id, task_status } }
        if (jsonObj.contains("request_id") && jsonObj.contains("output")) {
            QString requestId = jsonObj["request_id"].toString();
            QJsonObject output = jsonObj["output"].toObject();
            QString taskId = output["task_id"].toString();
            QString status = output["task_status"].toString();
            qDebug() << "[VideoAPI] WAN video created:" << requestId << taskId << status;
            emit videoCreated(taskId, status);
        } else {
            // VEO3/Grok 格式 { id, status }
            QString taskId = jsonObj["id"].toString();
            QString status = jsonObj["status"].toString();
            emit videoCreated(taskId, status);
        }
    } else {
        // 检查是否是超时错误
        if (reply->error() == QNetworkReply::TimeoutError) {
            emit errorOccurred("任务提交超时，但任务可能已创建。请稍后在历史记录中右键选择'修复任务ID'来恢复。");
        } else {
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            qDebug() << "[VideoAPI] create video failed:" << statusCode << responseData;
            emit errorOccurred(QString("创建视频失败(HTTP %1): %2")
                               .arg(statusCode)
                               .arg(responseData.isEmpty() ? reply->errorString() : QString::fromUtf8(responseData)));
        }
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

        // 检查是否是 WAN 返回格式
        if (jsonObj.contains("output")) {
            QJsonObject output = jsonObj["output"].toObject();
            QString taskId = jsonObj["request_id"].toString(); // WAN 用 request_id
            QString status = output["task_status"].toString();
            QString videoUrl = output["video_url"].toString();
            int progress = output["progress"].toInt(0);

            emit taskStatusUpdated(taskId, status, videoUrl, progress);
        } else {
            // VEO3/Grok 格式
            QString taskId = jsonObj["id"].toString();
            QString status = jsonObj["status"].toString();
            QString videoUrl = jsonObj["video_url"].toString();
            int progress = jsonObj["progress"].toInt();

            emit taskStatusUpdated(taskId, status, videoUrl, progress);
        }
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




