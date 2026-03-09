#ifndef VEO3API_H
#define VEO3API_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QStringList>

class Veo3API : public QObject
{
    Q_OBJECT

public:
    explicit Veo3API(QObject *parent = nullptr);
    ~Veo3API();

    // 创建视频生成任务
    void createVideo(const QString &apiKey,
                     const QString &baseUrl,
                     const QString &model,
                     const QString &prompt,
                     const QStringList &imagePaths,
                     const QString &size,
                     const QString &seconds,
                     bool watermark);

    // 查询任务状态
    void queryTask(const QString &apiKey,
                   const QString &baseUrl,
                   const QString &taskId);

    // 下载视频
    void downloadVideo(const QString &apiKey,
                       const QString &baseUrl,
                       const QString &taskId,
                       const QString &savePath);

signals:
    void videoCreated(const QString &taskId, const QString &status);
    void taskStatusUpdated(const QString &taskId, const QString &status, const QString &videoUrl, int progress);
    void videoDownloaded(const QString &savePath);
    void errorOccurred(const QString &error);

private slots:
    void onCreateVideoFinished();
    void onQueryTaskFinished();
    void onDownloadFinished();

private:
    QNetworkAccessManager *networkManager;
    QMap<QNetworkReply*, QString> replyMap;  // 用于跟踪请求类型
};

#endif // VEO3API_H
