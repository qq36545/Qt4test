#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QList>
#include <QDateTime>

struct ApiKey {
    int id;
    QString name;
    QString apiKey;
    QDateTime createdAt;
};

struct GenerationHistory {
    int id;
    QString date;
    QString type;  // "single" or "batch"
    QString modelType;  // "video" or "image"
    QString modelName;
    QString prompt;
    QString imagePath;
    QString parameters;
    QString status;
    QString resultPath;
    QDateTime createdAt;
};

struct VideoTask {
    int id;
    QString taskId;           // API 返回的任务 ID
    QString taskType;         // "video_single", "video_batch", "image_single", "image_batch"
    QString prompt;
    QString modelVariant;     // 模型变体："文生视频", "图生视频", "首尾帧生视频"
    QString status;           // "pending", "processing", "completed", "failed", "timeout"
    int progress;             // 0-100
    QString videoUrl;         // API 返回的视频 URL
    QString videoPath;        // 本地保存路径
    QString thumbnailPath;    // 缩略图路径
    QString downloadStatus;   // "not_started", "downloading", "completed", "failed"
    QDateTime createdAt;
    QDateTime completedAt;

    // 重新生成所需的完整参数
    QString modelName;        // 模型名称（如 "Veo 2.0"）
    QString apiKeyName;       // API密钥名称
    QString serverUrl;        // 服务器URL
    QString resolution;       // 分辨率（如 "1280x720"）
    int duration;             // 时长（秒）
    bool watermark;           // 是否添加水印
    QString imagePaths;       // 图片路径（JSON字符串）
    QString endFrameImagePath; // 尾帧图片路径
};

class DBManager : public QObject
{
    Q_OBJECT

public:
    static DBManager* instance();

    // API Keys
    bool addApiKey(const QString& name, const QString& apiKey);
    bool updateApiKey(int id, const QString& name, const QString& apiKey);
    bool deleteApiKey(int id);
    QList<ApiKey> getAllApiKeys();
    ApiKey getApiKey(int id);

    // Generation History
    int addGenerationHistory(const GenerationHistory& history);
    bool updateGenerationHistory(int id, const QString& status, const QString& resultPath);
    bool deleteGenerationHistory(int id);
    QList<GenerationHistory> getAllGenerationHistory();
    GenerationHistory getGenerationHistory(int id);

    // Video Tasks (异步轮询)
    int insertVideoTask(const VideoTask& task);
    bool updateTaskStatus(const QString& taskId, const QString& status, int progress, const QString& videoUrl = QString());
    bool updateVideoPath(const QString& taskId, const QString& videoPath, const QString& thumbnailPath, const QString& downloadStatus);
    QList<VideoTask> getTasksByType(const QString& taskType, int offset = 0, int limit = 50);
    VideoTask getTaskById(const QString& taskId);  // 根据 taskId 查询
    int getTaskCount(const QString& taskType);
    QList<VideoTask> getPendingTasks();  // 获取所有待轮询的任务
    bool deleteVideoTask(const QString& taskId);  // 删除单个任务
    int deleteVideoTasks(const QStringList& taskIds);  // 批量删除任务，返回实际删除的数量

private:
    explicit DBManager(QObject *parent = nullptr);
    ~DBManager();

    bool initDatabase();
    bool createTables();

    QSqlDatabase db;
    static DBManager* m_instance;
};

#endif // DBMANAGER_H
