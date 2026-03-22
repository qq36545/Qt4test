#include "imageapi.h"

#include <QNetworkRequest>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

ImageAPI::ImageAPI(QObject *parent)
    : QObject(parent),
      networkManager(new QNetworkAccessManager(this))
{
}

ImageAPI::~ImageAPI()
{
}

void ImageAPI::generateImage(const QString &apiKey,
                             const QString &baseUrl,
                             const QString &model,
                             const QString &prompt,
                             const QString &aspectRatio,
                             const QString &imageSize,
                             const QStringList &referenceImagePaths)
{
    const QString endpoint = QString("%1/v1beta/models/%2:generateContent")
                                 .arg(baseUrl, model);

    QNetworkRequest request{QUrl(endpoint)};
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    request.setRawHeader("Content-Type", "application/json");
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(60000);

    QJsonObject body;
    QJsonArray contents;
    QJsonObject userContent;
    userContent["role"] = "user";

    QJsonArray parts;
    QJsonObject textPart;
    textPart["text"] = prompt;
    parts.append(textPart);

    if (!referenceImagePaths.isEmpty()) {
        for (const QString &path : referenceImagePaths) {
            QString mimeType;
            QString base64Data;
            QString error;
            if (!buildReferenceInlineData(path, mimeType, base64Data, error)) {
                emit errorOccurred(error);
                return;
            }

            QJsonObject imagePart;
            QJsonObject inlineData;
            inlineData["mime_type"] = mimeType;
            inlineData["data"] = base64Data;
            imagePart["inline_data"] = inlineData;
            parts.append(imagePart);
        }
    }

    userContent["parts"] = parts;
    contents.append(userContent);
    body["contents"] = contents;

    QJsonObject generationConfig;
    QJsonArray responseModalities;
    responseModalities.append("TEXT");
    responseModalities.append("IMAGE");
    generationConfig["responseModalities"] = responseModalities;

    QJsonObject imageConfig;
    imageConfig["aspectRatio"] = aspectRatio;
    if (!imageSize.isEmpty()) {
        imageConfig["imageSize"] = imageSize;
    }
    generationConfig["imageConfig"] = imageConfig;
    body["generationConfig"] = generationConfig;

    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QNetworkReply *reply = networkManager->post(request, payload);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray responseBytes = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            QString error = QString("请求失败（HTTP %1）：%2")
                                .arg(statusCode)
                                .arg(reply->errorString());
            if (!responseBytes.isEmpty()) {
                error += "\n" + QString::fromUtf8(responseBytes);
            }
            emit errorOccurred(error);
            reply->deleteLater();
            return;
        }

        QByteArray imageBytes;
        QString mimeType;
        QString parseError;
        if (!parseImageFromResponse(responseBytes, imageBytes, mimeType, parseError)) {
            emit errorOccurred(parseError);
            reply->deleteLater();
            return;
        }

        emit imageGenerated(imageBytes, mimeType);
        reply->deleteLater();
    });
}

bool ImageAPI::buildReferenceInlineData(const QString &filePath,
                                        QString &mimeType,
                                        QString &base64Data,
                                        QString &error) const
{
    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        error = "参考图不存在";
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        error = "参考图不可读";
        return false;
    }

    const QByteArray bytes = file.readAll();
    file.close();

    if (bytes.isEmpty()) {
        error = "参考图内容为空";
        return false;
    }

    QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForFile(info);
    QString detected = mime.name();
    if (detected.isEmpty() || detected == "application/octet-stream") {
        const QString suffix = info.suffix().toLower();
        if (suffix == "jpg" || suffix == "jpeg") detected = "image/jpeg";
        else if (suffix == "png") detected = "image/png";
        else if (suffix == "webp") detected = "image/webp";
        else detected = "image/jpeg";
    }

    mimeType = detected;
    base64Data = QString::fromUtf8(bytes.toBase64());
    return true;
}

bool ImageAPI::parseImageFromResponse(const QByteArray &response,
                                      QByteArray &imageBytes,
                                      QString &mimeType,
                                      QString &error) const
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(response, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        error = "响应不是有效 JSON";
        return false;
    }

    const QJsonObject root = doc.object();
    const QJsonArray candidates = root.value("candidates").toArray();
    for (const QJsonValue &candidateValue : candidates) {
        const QJsonObject candidateObj = candidateValue.toObject();
        const QJsonObject contentObj = candidateObj.value("content").toObject();
        const QJsonArray parts = contentObj.value("parts").toArray();
        for (const QJsonValue &partValue : parts) {
            const QJsonObject partObj = partValue.toObject();

            QJsonObject inlineDataObj;
            if (partObj.contains("inline_data")) {
                inlineDataObj = partObj.value("inline_data").toObject();
            } else if (partObj.contains("inlineData")) {
                inlineDataObj = partObj.value("inlineData").toObject();
            }

            if (inlineDataObj.isEmpty()) {
                continue;
            }

            QString data = inlineDataObj.value("data").toString();
            if (data.isEmpty()) {
                data = inlineDataObj.value("dataBase64").toString();
            }

            if (data.isEmpty()) {
                continue;
            }

            QString mt = inlineDataObj.value("mime_type").toString();
            if (mt.isEmpty()) {
                mt = inlineDataObj.value("mimeType").toString();
            }
            if (mt.isEmpty()) {
                mt = "image/png";
            }

            QByteArray decoded = QByteArray::fromBase64(data.toUtf8());
            if (decoded.isEmpty()) {
                continue;
            }

            imageBytes = decoded;
            mimeType = mt;
            return true;
        }
    }

    error = "响应中未找到图片数据（candidates.content.parts.inline_data.data）";
    return false;
}
