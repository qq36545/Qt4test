#include "veo3api.h"
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

Veo3API::Veo3API(QObject *parent)
    : QObject(parent)
{
    networkManager = new QNetworkAccessManager(this);
}

Veo3API::~Veo3API()
{
}

void Veo3API::createVideo(const QString &apiKey,
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

    // 创建 multipart/form-data
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
        file->setParent(multiPart);  // 确保文件在 multiPart 删除时被删除
        multiPart->append(imagePart);
    }

    QNetworkReply *reply = networkManager->post(request, multiPart);
    multiPart->setParent(reply);  // 确保 multiPart 在 reply 删除时被删除
    replyMap[reply] = "create";

    connect(reply, &QNetworkReply::finished, this, &Veo3API::onCreateVideoFinished);
}

void Veo3API::queryTask(const QString &apiKey,
                        const QString &baseUrl,
                        const QString &taskId)
{
    QUrl url(baseUrl + "/v1/videos/" + taskId);
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    request.setRawHeader("Content-Type", "application/json");
    request.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = networkManager->get(request);
    replyMap[reply] = "query";

    connect(reply, &QNetworkReply::finished, this, &Veo3API::onQueryTaskFinished);
}

void Veo3API::downloadVideo(const QString &apiKey,
                            const QString &baseUrl,
                            const QString &taskId,
                            const QString &savePath)
{
    QUrl url(baseUrl + "/v1/videos/" + taskId + "/content");
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());

    QNetworkReply *reply = networkManager->get(request);
    replyMap[reply] = savePath;  // 保存路径

    connect(reply, &QNetworkReply::finished, this, &Veo3API::onDownloadFinished);
}

void Veo3API::onCreateVideoFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObj = jsonDoc.object();

        QString taskId = jsonObj["id"].toString();
        QString status = jsonObj["status"].toString();

        emit videoCreated(taskId, status);
    } else {
        emit errorOccurred(QString("创建视频失败: %1").arg(reply->errorString()));
    }

    replyMap.remove(reply);
    reply->deleteLater();
}

void Veo3API::onQueryTaskFinished()
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

void Veo3API::onDownloadFinished()
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
