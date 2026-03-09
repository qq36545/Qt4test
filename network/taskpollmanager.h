#ifndef TASKPOLLMANAGER_H
#define TASKPOLLMANAGER_H

#include <QObject>
#include <QTimer>
#include <QMap>
#include <QString>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>

struct TaskInfo {
    QString taskId;
    QString taskType;
    QString apiKey;
    QString baseUrl;
    QDateTime startTime;
    int retryCount;
    int downloadRetryCount;  // 下载重试次数
    QString videoUrl;        // 视频 URL（用于重试下载）
};

class TaskPollManager : public QObject
{
    Q_OBJECT

public:
    static TaskPollManager* getInstance();

    // 启动/停止轮询
    void startPolling(const QString& taskId, const QString& taskType,
                     const QString& apiKey, const QString& baseUrl);
    void stopPolling(const QString& taskId);

    // 程序重启恢复
    void recoverPendingTasks();

signals:
    void taskCompleted(const QString& taskId, const QString& videoUrl);
    void taskFailed(const QString& taskId, const QString& error);
    void taskTimeout(const QString& taskId);
    void downloadProgress(const QString& taskId, int progress);

private slots:
    void onPollTimer();
    void onQueryFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();

private:
    explicit TaskPollManager(QObject *parent = nullptr);
    ~TaskPollManager();

    void downloadVideo(const QString& taskId, const QString& videoUrl,
                      const QString& apiKey, const QString& baseUrl);
    void generateThumbnail(const QString& videoPath, const QString& thumbnailPath);
    bool isTaskTimeout(const QDateTime& startTime);

    static TaskPollManager* m_instance;
    QTimer* pollTimer;
    QMap<QString, TaskInfo> activeTasks;
    QNetworkAccessManager* networkManager;
    QMap<QNetworkReply*, QString> replyTaskMap;  // 映射 reply 到 taskId
};

#endif // TASKPOLLMANAGER_H
