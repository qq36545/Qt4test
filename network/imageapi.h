#ifndef IMAGEAPI_H
#define IMAGEAPI_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class ImageAPI : public QObject
{
    Q_OBJECT

public:
    explicit ImageAPI(QObject *parent = nullptr);
    ~ImageAPI();

    void generateImage(const QString &apiKey,
                       const QString &baseUrl,
                       const QString &model,
                       const QString &prompt,
                       const QString &aspectRatio,
                       const QString &imageSize,
                       const QString &referenceImagePath = QString());

signals:
    void imageGenerated(const QByteArray &imageBytes, const QString &mimeType);
    void errorOccurred(const QString &error);

private:
    bool buildReferenceInlineData(const QString &filePath, QString &mimeType, QString &base64Data, QString &error) const;
    bool parseImageFromResponse(const QByteArray &response, QByteArray &imageBytes, QString &mimeType, QString &error) const;

    QNetworkAccessManager *networkManager;
};

#endif // IMAGEAPI_H
