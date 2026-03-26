#include "taskpollmanager.h"
#include "../database/dbmanager.h"
#include "videoapi.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

TaskPollManager* TaskPollManager::m_instance = nullptr;

TaskPollManager* TaskPollManager::getInstance()
{
    if (!m_instance) {
        m_instance = new TaskPollManager();
    }
    return m_instance;
}

TaskPollManager::TaskPollManager(QObject *parent)
    : QObject(parent)
{
    networkManager = new QNetworkAccessManager(this);

    // 创建定时器，10 秒轮询一次
    pollTimer = new QTimer(this);
    pollTimer->setInterval(10000);  // 10 秒
    connect(pollTimer, &QTimer::timeout, this, &TaskPollManager::onPollTimer);
    pollTimer->start();
}

TaskPollManager::~TaskPollManager()
{
    if (pollTimer) {
        pollTimer->stop();
    }
}

void TaskPollManager::startPolling(const QString& taskId, const QString& taskType,
                                   const QString& apiKey, const QString& baseUrl,
                                   const QString& modelName)
{
    TaskInfo info;
    info.taskId = taskId;
    info.taskType = taskType;
    info.apiKey = apiKey;
    info.baseUrl = baseUrl;
    info.modelName = modelName;
    info.startTime = QDateTime::currentDateTime();
    info.retryCount = 0;
    info.downloadRetryCount = 0;
    info.videoUrl = "";

    activeTasks[taskId] = info;
    qDebug() << "Started polling for task:" << taskId;
}

void TaskPollManager::stopPolling(const QString& taskId)
{
    activeTasks.remove(taskId);
    qDebug() << "Stopped polling for task:" << taskId;
}

void TaskPollManager::onPollTimer()
{
    if (activeTasks.isEmpty()) {
        return;
    }

    qDebug() << "Polling" << activeTasks.size() << "tasks...";

    for (auto it = activeTasks.begin(); it != activeTasks.end(); ) {
        const TaskInfo& info = it.value();

        // 检查超时（20 分钟）
        if (isTaskTimeout(info.startTime)) {
            qDebug() << "Task timeout:" << info.taskId;
            DBManager::instance()->updateTaskStatus(info.taskId, "timeout", 0);
            emit taskTimeout(info.taskId);
            it = activeTasks.erase(it);
            continue;
        }

        // 查询任务状态（与 VideoAPI::queryTask 共用同一规则）
        QNetworkRequest request;
        const QUrl url = buildTaskQueryUrl(info.baseUrl, info.modelName, info.taskId);

        request.setUrl(url);
        request.setRawHeader("Authorization", QString("Bearer %1").arg(info.apiKey).toUtf8());
        request.setRawHeader("Content-Type", "application/json");
        request.setRawHeader("Accept", "application/json");

        QNetworkReply* reply = networkManager->get(request);
        replyTaskMap[reply] = info.taskId;
        connect(reply, &QNetworkReply::finished, this, &TaskPollManager::onQueryFinished);

        ++it;
    }
}

void TaskPollManager::onQueryFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    QString taskId = replyTaskMap.value(reply);
    replyTaskMap.remove(reply);

    if (reply->error() != QNetworkReply::NoError) {
        int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray errBody = reply->readAll();
        qWarning() << "[POLL] Query failed for task" << taskId
                   << "httpCode:" << httpCode
                   << "error:" << reply->errorString()
                   << "body:" << errBody.left(500);
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    // [DIAG] 打印原始响应，确认字段层级
    qDebug() << "[POLL] Raw response for task" << taskId << ":" << data.left(1000);

    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject obj = doc.object();

    QString status = obj["status"].toString();
    int progress = obj["progress"].toInt();
    QString videoUrl = obj["video_url"].toString();
    // OpenAI格式失败信息在 "message"，统一格式可能在 "error" 或 "message"
    QString errorMessage = obj["message"].toString();
    if (errorMessage.isEmpty()) errorMessage = obj["error"].toString();

    qDebug() << "[POLL] Task" << taskId
             << "status:" << status
             << "progress:" << progress
             << "video_url:" << (videoUrl.isEmpty() ? "(empty)" : videoUrl)
             << "message:" << (errorMessage.isEmpty() ? "(empty)" : errorMessage)
             << "progress_type_ok:" << !obj["progress"].isUndefined();

    // 更新数据库
    DBManager::instance()->updateTaskStatus(taskId, status, progress, videoUrl);

    // 如果有错误信息，存入数据库
    if (!errorMessage.isEmpty() && (status == "failed" || status.isEmpty())) {
        DBManager::instance()->updateTaskErrorMessage(taskId, errorMessage);
    }

    // 发出状态更新信号，通知UI刷新
    emit taskStatusUpdated(taskId, status, progress);

    if (status == "completed" && !videoUrl.isEmpty()) {
        // 任务完成，开始下载
        if (activeTasks.contains(taskId)) {
            TaskInfo& info = activeTasks[taskId];
            info.videoUrl = videoUrl;  // 保存 videoUrl 用于重试
            downloadVideo(taskId, videoUrl, info.apiKey, info.baseUrl);
            // 注意：不要在这里 stopPolling，等下载完成后再移除
        }
        emit taskCompleted(taskId, videoUrl);
    } else if (status == "failed") {
        stopPolling(taskId);
        emit taskFailed(taskId, errorMessage.isEmpty() ? obj["error"].toString() : errorMessage);
    }
}

void TaskPollManager::downloadVideo(const QString& taskId, const QString& videoUrl,
                                    const QString& apiKey, const QString& baseUrl)
{
    // 创建输出目录
    QString outputDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                       + "/outputs/videos/veo3_one_by_one";
    QDir dir;
    if (!dir.exists(outputDir)) {
        dir.mkpath(outputDir);
    }

    // 生成文件名
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString videoPath = outputDir + "/veo3_" + timestamp + ".mp4";

    qDebug() << "Downloading video to:" << videoPath;

    // 更新下载状态
    DBManager::instance()->updateVideoPath(taskId, videoPath, "", "downloading");

    // 发起下载请求
    QNetworkRequest request;
    request.setUrl(QUrl(videoUrl));
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());

    QNetworkReply* reply = networkManager->get(request);
    replyTaskMap[reply] = taskId;

    connect(reply, &QNetworkReply::downloadProgress, this, &TaskPollManager::onDownloadProgress);
    connect(reply, &QNetworkReply::finished, this, &TaskPollManager::onDownloadFinished);
}

void TaskPollManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply || bytesTotal <= 0) return;

    QString taskId = replyTaskMap.value(reply);
    int progress = (bytesReceived * 100) / bytesTotal;

    emit downloadProgress(taskId, progress);
}

void TaskPollManager::onDownloadFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    QString taskId = replyTaskMap.value(reply);
    replyTaskMap.remove(reply);

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Download failed for task" << taskId << ":" << reply->errorString();

        // 检查是否需要重试
        if (activeTasks.contains(taskId)) {
            TaskInfo& info = activeTasks[taskId];
            info.downloadRetryCount++;

            if (info.downloadRetryCount < 5) {
                // 重试下载
                qDebug() << "Retrying download for task" << taskId << "attempt" << info.downloadRetryCount + 1;
                reply->deleteLater();
                downloadVideo(taskId, info.videoUrl, info.apiKey, info.baseUrl);
                return;
            } else {
                // 达到最大重试次数
                QString errorMsg = QString("下载失败，已重试 5 次。\n错误: %1\n请检查网络连接。")
                    .arg(reply->errorString());
                qWarning() << errorMsg;
                DBManager::instance()->updateVideoPath(taskId, "", "", "failed");
                activeTasks.remove(taskId);
            }
        } else {
            DBManager::instance()->updateVideoPath(taskId, "", "", "failed");
        }

        reply->deleteLater();
        return;
    }

    // 获取视频路径（按 taskId 直查）
    QString videoPath = getSavedVideoPath(taskId);

    if (videoPath.isEmpty()) {
        qWarning() << "Video path not found for task" << taskId;
        reply->deleteLater();
        return;
    }

    // 保存视频文件
    QFile file(videoPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(reply->readAll());
        file.close();
        qDebug() << "Video downloaded successfully:" << videoPath;

        // 生成缩略图
        QString thumbnailPath = videoPath;
        thumbnailPath.replace(".mp4", "_thumb.jpg");
        generateThumbnail(videoPath, thumbnailPath);

        // 更新数据库
        DBManager::instance()->updateVideoPath(taskId, videoPath, thumbnailPath, "completed");

        // 从活动任务中移除
        activeTasks.remove(taskId);
    } else {
        qWarning() << "Failed to save video file:" << videoPath;
        DBManager::instance()->updateVideoPath(taskId, "", "", "failed");
    }

    reply->deleteLater();
}

void TaskPollManager::generateThumbnail(const QString& videoPath, const QString& thumbnailPath)
{
    // TODO: 使用 Qt Multimedia 提取第一帧
    // 这里先留空，后续实现
    qDebug() << "TODO: Generate thumbnail for" << videoPath;
}

QString TaskPollManager::getSavedVideoPath(const QString& taskId) const
{
    VideoTask task = DBManager::instance()->getTaskById(taskId);
    return task.videoPath;
}

bool TaskPollManager::isTaskTimeout(const QDateTime& startTime)
{
    QDateTime now = QDateTime::currentDateTime();
    qint64 elapsedSeconds = startTime.secsTo(now);
    return elapsedSeconds > (20 * 60);  // 20 分钟
}

void TaskPollManager::triggerDownload(const QString& taskId, const QString& videoUrl,
                                      const QString& apiKey, const QString& baseUrl,
                                      const QString& taskType)
{
    // 如果已在下载中，忽略
    if (activeTasks.contains(taskId)) {
        qDebug() << "Task" << taskId << "already in active tasks, skipping triggerDownload";
        return;
    }

    // 添加到活动任务（仅用于下载重试跟踪）
    TaskInfo info;
    info.taskId = taskId;
    info.taskType = taskType;
    info.apiKey = apiKey;
    info.baseUrl = baseUrl;
    info.startTime = QDateTime::currentDateTime();
    info.retryCount = 0;
    info.downloadRetryCount = 0;
    info.videoUrl = videoUrl;
    activeTasks[taskId] = info;

    downloadVideo(taskId, videoUrl, apiKey, baseUrl);
}

void TaskPollManager::recoverPendingTasks()
{
    const QList<VideoTask> pendingTasks = DBManager::instance()->getPendingTasks();
    const QList<ApiKey> allKeys = DBManager::instance()->getAllApiKeys();

    auto resolveApiKeyValue = [&allKeys](const QString& keyName) -> QString {
        for (const ApiKey& key : allKeys) {
            if (key.name == keyName) {
                return key.apiKey;
            }
        }
        return QString();
    };

    for (const VideoTask& task : pendingTasks) {
        if (task.taskId.trimmed().isEmpty()) {
            qWarning() << "[Recover] skip task with empty task_id, db id:" << task.id;
            continue;
        }

        if (isTaskTimeout(task.createdAt)) {
            qWarning() << "[Recover] task timeout on startup:" << task.taskId;
            DBManager::instance()->updateTaskStatus(task.taskId, "timeout", 0);
            continue;
        }

        const QString apiKey = resolveApiKeyValue(task.apiKeyName);
        if (apiKey.trimmed().isEmpty()) {
            const QString msg = QString("恢复失败：未找到 API 密钥（%1）").arg(task.apiKeyName);
            qWarning() << "[Recover]" << msg << "task:" << task.taskId;
            DBManager::instance()->updateTaskErrorMessage(task.taskId, msg);
            emit taskFailed(task.taskId, msg);
            continue;
        }

        const QString baseUrl = task.serverUrl.trimmed();
        if (baseUrl.isEmpty()) {
            const QString msg = "恢复失败：缺少服务器地址";
            qWarning() << "[Recover]" << msg << "task:" << task.taskId;
            DBManager::instance()->updateTaskErrorMessage(task.taskId, msg);
            emit taskFailed(task.taskId, msg);
            continue;
        }

        const bool canResumeDownload =
            !task.videoUrl.trimmed().isEmpty() &&
            task.downloadStatus != "completed";

        if (canResumeDownload) {
            qDebug() << "[Recover] resume download task:" << task.taskId;
            triggerDownload(task.taskId,
                            task.videoUrl,
                            apiKey,
                            baseUrl,
                            task.taskType);
            continue;
        }

        qDebug() << "[Recover] resume polling task:" << task.taskId
                 << "status:" << task.status
                 << "model:" << task.modelName;
        startPolling(task.taskId,
                     task.taskType,
                     apiKey,
                     baseUrl,
                     task.modelName);
    }
}

