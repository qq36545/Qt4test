#include "dbmanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>

DBManager* DBManager::m_instance = nullptr;

DBManager* DBManager::instance()
{
    if (!m_instance) {
        m_instance = new DBManager();
    }
    return m_instance;
}

DBManager::DBManager(QObject *parent)
    : QObject(parent)
{
    initDatabase();
}

DBManager::~DBManager()
{
    if (db.isOpen()) {
        db.close();
    }
}

bool DBManager::initDatabase()
{
    // 获取应用数据目录
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir;
    if (!dir.exists(dataPath)) {
        dir.mkpath(dataPath);
    }

    QString dbPath = dataPath + "/chickenai.db";
    qDebug() << "Database path:" << dbPath;

    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qCritical() << "Failed to open database:" << db.lastError().text();
        return false;
    }

    return createTables();
}

bool DBManager::createTables()
{
    QSqlQuery query;

    // 创建 API Keys 表
    QString createApiKeysTable = R"(
        CREATE TABLE IF NOT EXISTS api_keys (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            api_key TEXT NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";

    if (!query.exec(createApiKeysTable)) {
        qCritical() << "Failed to create api_keys table:" << query.lastError().text();
        return false;
    }

    // 创建 Generation History 表
    QString createHistoryTable = R"(
        CREATE TABLE IF NOT EXISTS generation_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            date TEXT NOT NULL,
            type TEXT NOT NULL,
            model_type TEXT NOT NULL,
            model_name TEXT,
            prompt TEXT,
            image_path TEXT,
            parameters TEXT,
            status TEXT,
            result_path TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";

    if (!query.exec(createHistoryTable)) {
        qCritical() << "Failed to create generation_history table:" << query.lastError().text();
        return false;
    }

    // 创建 Video Tasks 表（异步轮询）
    QString createVideoTasksTable = R"(
        CREATE TABLE IF NOT EXISTS video_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            task_id TEXT NOT NULL UNIQUE,
            task_type TEXT NOT NULL,
            prompt TEXT,
            model_variant TEXT,
            status TEXT DEFAULT 'pending',
            progress INTEGER DEFAULT 0,
            video_url TEXT,
            video_path TEXT,
            thumbnail_path TEXT,
            download_status TEXT DEFAULT 'not_started',
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            completed_at TIMESTAMP
        )
    )";

    if (!query.exec(createVideoTasksTable)) {
        qCritical() << "Failed to create video_history table:" << query.lastError().text();
        return false;
    }

    // 为已存在的表添加 model_variant 列（兼容旧数据）
    query.exec("PRAGMA table_info(video_history)");
    bool hasModelVariant = false;
    while (query.next()) {
        if (query.value(1).toString() == "model_variant") {
            hasModelVariant = true;
            break;
        }
    }
    if (!hasModelVariant) {
        if (!query.exec("ALTER TABLE video_history ADD COLUMN model_variant TEXT")) {
            qCritical() << "Failed to add model_variant column:" << query.lastError().text();
        }
    }

    // 添加重新生成所需的完整参数列（兼容旧数据）
    QStringList newColumns = {
        "model_name TEXT",
        "api_key_name TEXT",
        "server_url TEXT",
        "resolution TEXT",
        "duration INTEGER",
        "watermark INTEGER DEFAULT 0",
        "image_paths TEXT",
        "end_frame_image_path TEXT",
        "aspect_ratio TEXT",
        "size TEXT",
        "error_message TEXT"
    };

    for (const QString& columnDef : newColumns) {
        QString columnName = columnDef.split(' ').first();
        query.exec("PRAGMA table_info(video_history)");
        bool hasColumn = false;
        while (query.next()) {
            if (query.value(1).toString() == columnName) {
                hasColumn = true;
                break;
            }
        }
        if (!hasColumn) {
            QString alterSql = QString("ALTER TABLE video_history ADD COLUMN %1").arg(columnDef);
            if (!query.exec(alterSql)) {
                qWarning() << "Failed to add column" << columnName << ":" << query.lastError().text();
            } else {
                qDebug() << "Added column:" << columnName;
            }
        }
    }

    // 插入默认 API Key（如果表为空）
    query.exec("SELECT COUNT(*) FROM api_keys");
    if (query.next() && query.value(0).toInt() == 0) {
        addApiKey("sora2密钥", "sk-***********jshfg");
    }

    // 创建 imgbb_keys 表
    QString createImgbbKeysTable = R"(
        CREATE TABLE IF NOT EXISTS imgbb_keys (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            api_key TEXT NOT NULL,
            is_active INTEGER DEFAULT 0,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";

    if (!query.exec(createImgbbKeysTable)) {
        qCritical() << "Failed to create imgbb_keys table:" << query.lastError().text();
        return false;
    }

    // 创建 Image Preferences 表
    QString createImagePrefsTable = R"(
        CREATE TABLE IF NOT EXISTS image_preferences (
            model_variant TEXT PRIMARY KEY,
            image_size TEXT,
            aspect_ratio TEXT,
            server_url TEXT,
            api_key_id INTEGER,
            prompt TEXT,
            reference_image_paths TEXT
        )
    )";

    if (!query.exec(createImagePrefsTable)) {
        qCritical() << "Failed to create image_preferences table:" << query.lastError().text();
        return false;
    }

    // 创建 App Preferences 表（全局应用偏好）
    QString createAppPrefsTable = R"(
        CREATE TABLE IF NOT EXISTS app_preferences (
            key TEXT PRIMARY KEY,
            value TEXT
        )
    )";

    if (!query.exec(createAppPrefsTable)) {
        qCritical() << "Failed to create app_preferences table:" << query.lastError().text();
        return false;
    }

    // 为已存在的表添加新列（兼容旧数据）
    QStringList newImagePrefColumns = {
        "prompt TEXT",
        "reference_image_paths TEXT"
    };

    for (const QString& columnDef : newImagePrefColumns) {
        QString columnName = columnDef.split(' ').first();
        query.exec("PRAGMA table_info(image_preferences)");
        bool hasColumn = false;
        while (query.next()) {
            if (query.value(1).toString() == columnName) {
                hasColumn = true;
                break;
            }
        }
        if (!hasColumn) {
            QString alterSql = QString("ALTER TABLE image_preferences ADD COLUMN %1").arg(columnDef);
            if (!query.exec(alterSql)) {
                qWarning() << "Failed to add column" << columnName << "to image_preferences:" << query.lastError().text();
            } else {
                qDebug() << "Added column" << columnName << "to image_preferences";
            }
        }
    }

    // 创建 Image Draft 表（草稿，按模型分别保存）
    // 检查旧表是否有 id 列，如有则删除重建（数据库版本升级）
    query.exec("PRAGMA table_info(image_draft)");
    bool hasIdColumn = false;
    while (query.next()) {
        if (query.value(1).toString() == "id") {
            hasIdColumn = true;
            break;
        }
    }
    if (hasIdColumn) {
        query.exec("DROP TABLE image_draft");
    }

    QString createImageDraftTable = R"(
        CREATE TABLE IF NOT EXISTS image_draft (
            model_variant TEXT PRIMARY KEY,
            api_key_id TEXT,
            server_url TEXT,
            image_size TEXT,
            aspect_ratio TEXT,
            prompt TEXT,
            reference_image_paths TEXT,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";

    if (!query.exec(createImageDraftTable)) {
        qCritical() << "Failed to create image_draft table:" << query.lastError().text();
        return false;
    }

    return true;
}

// API Keys CRUD
bool DBManager::addApiKey(const QString& name, const QString& apiKey)
{
    QSqlQuery query;
    query.prepare("INSERT INTO api_keys (name, api_key) VALUES (:name, :api_key)");
    query.bindValue(":name", name);
    query.bindValue(":api_key", apiKey);

    if (!query.exec()) {
        qCritical() << "Failed to add API key:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DBManager::updateApiKey(int id, const QString& name, const QString& apiKey)
{
    QSqlQuery query;
    query.prepare("UPDATE api_keys SET name = :name, api_key = :api_key WHERE id = :id");
    query.bindValue(":id", id);
    query.bindValue(":name", name);
    query.bindValue(":api_key", apiKey);

    if (!query.exec()) {
        qCritical() << "Failed to update API key:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DBManager::deleteApiKey(int id)
{
    QSqlQuery query;
    query.prepare("DELETE FROM api_keys WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        qCritical() << "Failed to delete API key:" << query.lastError().text();
        return false;
    }
    return true;
}

QList<ApiKey> DBManager::getAllApiKeys()
{
    QList<ApiKey> keys;
    QSqlQuery query("SELECT id, name, api_key, created_at FROM api_keys ORDER BY id");

    while (query.next()) {
        ApiKey key;
        key.id = query.value(0).toInt();
        key.name = query.value(1).toString();
        key.apiKey = query.value(2).toString();
        key.createdAt = query.value(3).toDateTime();
        keys.append(key);
    }

    return keys;
}

ApiKey DBManager::getApiKey(int id)
{
    ApiKey key;
    QSqlQuery query;
    query.prepare("SELECT id, name, api_key, created_at FROM api_keys WHERE id = :id");
    query.bindValue(":id", id);

    if (query.exec() && query.next()) {
        key.id = query.value(0).toInt();
        key.name = query.value(1).toString();
        key.apiKey = query.value(2).toString();
        key.createdAt = query.value(3).toDateTime();
    }

    return key;
}

// Generation History CRUD
int DBManager::addGenerationHistory(const GenerationHistory& history)
{
    QSqlQuery query;
    query.prepare(R"(
        INSERT INTO generation_history
        (date, type, model_type, model_name, prompt, image_path, parameters, status, result_path)
        VALUES (:date, :type, :model_type, :model_name, :prompt, :image_path, :parameters, :status, :result_path)
    )");

    query.bindValue(":date", history.date);
    query.bindValue(":type", history.type);
    query.bindValue(":model_type", history.modelType);
    query.bindValue(":model_name", history.modelName);
    query.bindValue(":prompt", history.prompt);
    query.bindValue(":image_path", history.imagePath);
    query.bindValue(":parameters", history.parameters);
    query.bindValue(":status", history.status);
    query.bindValue(":result_path", history.resultPath);

    if (!query.exec()) {
        qCritical() << "Failed to add generation history:" << query.lastError().text();
        return -1;
    }

    return query.lastInsertId().toInt();
}

bool DBManager::updateGenerationHistory(int id, const QString& status, const QString& resultPath)
{
    QSqlQuery query;
    query.prepare("UPDATE generation_history SET status = :status, result_path = :result_path WHERE id = :id");
    query.bindValue(":id", id);
    query.bindValue(":status", status);
    query.bindValue(":result_path", resultPath);

    if (!query.exec()) {
        qCritical() << "Failed to update generation history:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DBManager::deleteGenerationHistory(int id)
{
    QSqlQuery query;
    query.prepare("DELETE FROM generation_history WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        qCritical() << "Failed to delete generation history:" << query.lastError().text();
        return false;
    }
    return true;
}

QList<GenerationHistory> DBManager::getAllGenerationHistory()
{
    QList<GenerationHistory> histories;
    QSqlQuery query("SELECT * FROM generation_history ORDER BY created_at DESC");

    while (query.next()) {
        GenerationHistory history;
        history.id = query.value(0).toInt();
        history.date = query.value(1).toString();
        history.type = query.value(2).toString();
        history.modelType = query.value(3).toString();
        history.modelName = query.value(4).toString();
        history.prompt = query.value(5).toString();
        history.imagePath = query.value(6).toString();
        history.parameters = query.value(7).toString();
        history.status = query.value(8).toString();
        history.resultPath = query.value(9).toString();
        history.createdAt = query.value(10).toDateTime();
        histories.append(history);
    }

    return histories;
}

QList<GenerationHistory> DBManager::getGenerationHistoryByModelAndType(const QString& modelType, const QString& type)
{
    QList<GenerationHistory> histories;
    QSqlQuery query;
    query.prepare(R"(
        SELECT * FROM generation_history
        WHERE model_type = :model_type AND type = :type
        ORDER BY created_at DESC
    )");
    query.bindValue(":model_type", modelType);
    query.bindValue(":type", type);

    if (!query.exec()) {
        qCritical() << "Failed to get generation history by model/type:" << query.lastError().text();
        return histories;
    }

    while (query.next()) {
        GenerationHistory history;
        history.id = query.value(0).toInt();
        history.date = query.value(1).toString();
        history.type = query.value(2).toString();
        history.modelType = query.value(3).toString();
        history.modelName = query.value(4).toString();
        history.prompt = query.value(5).toString();
        history.imagePath = query.value(6).toString();
        history.parameters = query.value(7).toString();
        history.status = query.value(8).toString();
        history.resultPath = query.value(9).toString();
        history.createdAt = query.value(10).toDateTime();
        histories.append(history);
    }

    return histories;
}

GenerationHistory DBManager::getGenerationHistory(int id)
{
    GenerationHistory history;
    QSqlQuery query;
    query.prepare("SELECT * FROM generation_history WHERE id = :id");
    query.bindValue(":id", id);

    if (query.exec() && query.next()) {
        history.id = query.value(0).toInt();
        history.date = query.value(1).toString();
        history.type = query.value(2).toString();
        history.modelType = query.value(3).toString();
        history.modelName = query.value(4).toString();
        history.prompt = query.value(5).toString();
        history.imagePath = query.value(6).toString();
        history.parameters = query.value(7).toString();
        history.status = query.value(8).toString();
        history.resultPath = query.value(9).toString();
        history.createdAt = query.value(10).toDateTime();
    }

    return history;
}

// Video Tasks CRUD
int DBManager::insertVideoTask(const VideoTask& task)
{
    QSqlQuery query;
    query.prepare(R"(
        INSERT INTO video_history
        (task_id, task_type, prompt, model_variant, status, progress, video_url, video_path, thumbnail_path, download_status, completed_at,
         model_name, api_key_name, server_url, resolution, duration, watermark, image_paths, end_frame_image_path)
        VALUES (:task_id, :task_type, :prompt, :model_variant, :status, :progress, :video_url, :video_path, :thumbnail_path, :download_status, :completed_at,
                :model_name, :api_key_name, :server_url, :resolution, :duration, :watermark, :image_paths, :end_frame_image_path)
    )");

    query.bindValue(":task_id", task.taskId);
    query.bindValue(":task_type", task.taskType);
    query.bindValue(":prompt", task.prompt);
    query.bindValue(":model_variant", task.modelVariant);
    query.bindValue(":status", task.status);
    query.bindValue(":progress", task.progress);
    query.bindValue(":video_url", task.videoUrl);
    query.bindValue(":video_path", task.videoPath);
    query.bindValue(":thumbnail_path", task.thumbnailPath);
    query.bindValue(":download_status", task.downloadStatus);
    query.bindValue(":completed_at", task.completedAt);
    query.bindValue(":model_name", task.modelName);
    query.bindValue(":api_key_name", task.apiKeyName);
    query.bindValue(":server_url", task.serverUrl);
    query.bindValue(":resolution", task.resolution);
    query.bindValue(":duration", task.duration);
    query.bindValue(":watermark", task.watermark ? 1 : 0);
    query.bindValue(":image_paths", task.imagePaths);
    query.bindValue(":end_frame_image_path", task.endFrameImagePath);

    if (!query.exec()) {
        qCritical() << "Failed to insert video task:" << query.lastError().text();
        return -1;
    }

    return query.lastInsertId().toInt();
}

bool DBManager::updateTaskStatus(const QString& taskId, const QString& status, int progress, const QString& videoUrl)
{
    QSqlQuery query;

    if (videoUrl.isEmpty()) {
        query.prepare("UPDATE video_history SET status = :status, progress = :progress WHERE task_id = :task_id");
        query.bindValue(":status", status);
        query.bindValue(":progress", progress);
        query.bindValue(":task_id", taskId);
    } else {
        query.prepare(R"(
            UPDATE video_history
            SET status = :status, progress = :progress, video_url = :video_url,
                completed_at = CURRENT_TIMESTAMP
            WHERE task_id = :task_id
        )");
        query.bindValue(":status", status);
        query.bindValue(":progress", progress);
        query.bindValue(":video_url", videoUrl);
        query.bindValue(":task_id", taskId);
    }

    if (!query.exec()) {
        qCritical() << "Failed to update task status:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DBManager::updateVideoPath(const QString& taskId, const QString& videoPath, const QString& thumbnailPath, const QString& downloadStatus)
{
    QSqlQuery query;
    query.prepare(R"(
        UPDATE video_history
        SET video_path = :video_path, thumbnail_path = :thumbnail_path, download_status = :download_status
        WHERE task_id = :task_id
    )");

    query.bindValue(":video_path", videoPath);
    query.bindValue(":thumbnail_path", thumbnailPath);
    query.bindValue(":download_status", downloadStatus);
    query.bindValue(":task_id", taskId);

    if (!query.exec()) {
        qCritical() << "Failed to update video path:" << query.lastError().text();
        return false;
    }
    return true;
}

QList<VideoTask> DBManager::getTasksByType(const QString& taskType, int offset, int limit)
{
    QList<VideoTask> tasks;
    QSqlQuery query;
    query.prepare(R"(
        SELECT id, task_id, task_type, prompt, model_variant, status, progress, video_url, video_path,
               thumbnail_path, download_status, created_at, completed_at,
               model_name, api_key_name, server_url, resolution, duration, watermark, image_paths, end_frame_image_path,
               error_message
        FROM video_history
        WHERE task_type = :task_type
        ORDER BY created_at DESC
        LIMIT :limit OFFSET :offset
    )");

    query.bindValue(":task_type", taskType);
    query.bindValue(":limit", limit);
    query.bindValue(":offset", offset);

    if (!query.exec()) {
        qCritical() << "Failed to get tasks by type:" << query.lastError().text();
        return tasks;
    }

    while (query.next()) {
        VideoTask task;
        task.id = query.value(0).toInt();
        task.taskId = query.value(1).toString();
        task.taskType = query.value(2).toString();
        task.prompt = query.value(3).toString();
        task.modelVariant = query.value(4).toString();
        task.status = query.value(5).toString();
        task.progress = query.value(6).toInt();
        task.videoUrl = query.value(7).toString();
        task.videoPath = query.value(8).toString();
        task.thumbnailPath = query.value(9).toString();
        task.downloadStatus = query.value(10).toString();
        task.createdAt = query.value(11).toDateTime();
        task.completedAt = query.value(12).toDateTime();
        task.modelName = query.value(13).toString();
        task.apiKeyName = query.value(14).toString();
        task.serverUrl = query.value(15).toString();
        task.resolution = query.value(16).toString();
        task.duration = query.value(17).toInt();
        task.watermark = query.value(18).toInt() == 1;
        task.imagePaths = query.value(19).toString();
        task.endFrameImagePath = query.value(20).toString();
        task.errorMessage = query.value(21).toString();
        tasks.append(task);
    }

    return tasks;
}

bool DBManager::deleteVideoTask(const QString& taskId)
{
    QSqlQuery query;
    query.prepare("DELETE FROM video_history WHERE task_id = :task_id");
    query.bindValue(":task_id", taskId);

    if (!query.exec()) {
        qCritical() << "Failed to delete video task:" << query.lastError().text();
        return false;
    }
    return true;
}

int DBManager::deleteVideoTasks(const QStringList& taskIds)
{
    if (taskIds.isEmpty()) {
        return 0;
    }

    int deletedCount = 0;
    QSqlQuery query;

    // 开启事务以提高批量删除性能
    db.transaction();

    for (const QString& taskId : taskIds) {
        query.prepare("DELETE FROM video_history WHERE task_id = :task_id");
        query.bindValue(":task_id", taskId);

        if (query.exec()) {
            deletedCount++;
        } else {
            qCritical() << "Failed to delete video task" << taskId << ":" << query.lastError().text();
        }
    }

    db.commit();
    return deletedCount;
}

VideoTask DBManager::getTaskById(const QString& taskId)
{
    VideoTask task;
    QSqlQuery query;
    query.prepare(R"(
        SELECT id, task_id, task_type, prompt, model_variant, status, progress, video_url, video_path,
               thumbnail_path, download_status, created_at, completed_at,
               model_name, api_key_name, server_url, resolution, duration, watermark, image_paths, end_frame_image_path,
               error_message
        FROM video_history
        WHERE task_id = :task_id
    )");

    query.bindValue(":task_id", taskId);

    if (query.exec() && query.next()) {
        task.id = query.value(0).toInt();
        task.taskId = query.value(1).toString();
        task.taskType = query.value(2).toString();
        task.prompt = query.value(3).toString();
        task.modelVariant = query.value(4).toString();
        task.status = query.value(5).toString();
        task.progress = query.value(6).toInt();
        task.videoUrl = query.value(7).toString();
        task.videoPath = query.value(8).toString();
        task.thumbnailPath = query.value(9).toString();
        task.downloadStatus = query.value(10).toString();
        task.createdAt = query.value(11).toDateTime();
        task.completedAt = query.value(12).toDateTime();
        task.modelName = query.value(13).toString();
        task.apiKeyName = query.value(14).toString();
        task.serverUrl = query.value(15).toString();
        task.resolution = query.value(16).toString();
        task.duration = query.value(17).toInt();
        task.watermark = query.value(18).toInt() == 1;
        task.imagePaths = query.value(19).toString();
        task.endFrameImagePath = query.value(20).toString();
        task.errorMessage = query.value(21).toString();
    }

    return task;
}

int DBManager::getTaskCount(const QString& taskType)
{
    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM video_history WHERE task_type = :task_type");
    query.bindValue(":task_type", taskType);

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }

    return 0;
}

QList<VideoTask> DBManager::getPendingTasks()
{
    QList<VideoTask> tasks;
    QSqlQuery query(R"(
        SELECT id, task_id, task_type, prompt, model_variant, status, progress, video_url, video_path,
               thumbnail_path, download_status, created_at, completed_at
        FROM video_history
        WHERE status IN ('pending', 'processing', 'video_generating')
        ORDER BY created_at ASC
    )");

    if (!query.exec()) {
        qCritical() << "Failed to get pending tasks:" << query.lastError().text();
        return tasks;
    }

    while (query.next()) {
        VideoTask task;
        task.id = query.value(0).toInt();
        task.taskId = query.value(1).toString();
        task.taskType = query.value(2).toString();
        task.prompt = query.value(3).toString();
        task.modelVariant = query.value(4).toString();
        task.status = query.value(5).toString();
        task.progress = query.value(6).toInt();
        task.videoUrl = query.value(7).toString();
        task.videoPath = query.value(8).toString();
        task.thumbnailPath = query.value(9).toString();
        task.downloadStatus = query.value(10).toString();
        task.createdAt = query.value(11).toDateTime();
        task.completedAt = query.value(12).toDateTime();
        tasks.append(task);
    }

    return tasks;
}

bool DBManager::updateTaskId(const QString& oldTaskId, const QString& newTaskId)
{
    QSqlQuery query;
    query.prepare("UPDATE video_history SET task_id = :new_id WHERE task_id = :old_id");
    query.bindValue(":new_id", newTaskId);
    query.bindValue(":old_id", oldTaskId);

    if (!query.exec()) {
        qCritical() << "Failed to update task ID:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DBManager::updateTaskErrorMessage(const QString& taskId, const QString& errorMessage)
{
    QSqlQuery query;
    query.prepare("UPDATE video_history SET error_message = :error WHERE task_id = :task_id");
    query.bindValue(":error", errorMessage);
    query.bindValue(":task_id", taskId);

    if (!query.exec()) {
        qCritical() << "Failed to update task error message:" << query.lastError().text();
        return false;
    }
    return true;
}

// Imgbb Keys CRUD
bool DBManager::addImgbbKey(const QString& name, const QString& apiKey)
{
    QSqlQuery query;
    query.prepare("INSERT INTO imgbb_keys (name, api_key) VALUES (:name, :api_key)");
    query.bindValue(":name", name);
    query.bindValue(":api_key", apiKey);
    if (!query.exec()) {
        qCritical() << "Failed to add imgbb key:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DBManager::updateImgbbKey(int id, const QString& name, const QString& apiKey)
{
    QSqlQuery query;
    query.prepare("UPDATE imgbb_keys SET name = :name, api_key = :api_key WHERE id = :id");
    query.bindValue(":id", id);
    query.bindValue(":name", name);
    query.bindValue(":api_key", apiKey);
    if (!query.exec()) {
        qCritical() << "Failed to update imgbb key:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DBManager::deleteImgbbKey(int id)
{
    QSqlQuery query;
    query.prepare("DELETE FROM imgbb_keys WHERE id = :id");
    query.bindValue(":id", id);
    if (!query.exec()) {
        qCritical() << "Failed to delete imgbb key:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DBManager::setActiveImgbbKey(int id)
{
    db.transaction();
    QSqlQuery query;
    query.exec("UPDATE imgbb_keys SET is_active = 0");
    query.prepare("UPDATE imgbb_keys SET is_active = 1 WHERE id = :id");
    query.bindValue(":id", id);
    if (!query.exec()) {
        db.rollback();
        qCritical() << "Failed to set active imgbb key:" << query.lastError().text();
        return false;
    }
    db.commit();
    return true;
}

QList<ImgbbKey> DBManager::getAllImgbbKeys()
{
    QList<ImgbbKey> keys;
    QSqlQuery query("SELECT id, name, api_key, is_active FROM imgbb_keys ORDER BY id");
    while (query.next()) {
        ImgbbKey key;
        key.id = query.value(0).toInt();
        key.name = query.value(1).toString();
        key.apiKey = query.value(2).toString();
        key.isActive = query.value(3).toInt() == 1;
        keys.append(key);
    }
    return keys;
}

ImgbbKey DBManager::getActiveImgbbKey()
{
    ImgbbKey key;
    QSqlQuery query("SELECT id, name, api_key, is_active FROM imgbb_keys WHERE is_active = 1 LIMIT 1");
    if (query.next()) {
        key.id = query.value(0).toInt();
        key.name = query.value(1).toString();
        key.apiKey = query.value(2).toString();
        key.isActive = true;
    }
    return key;
}

// Image Preferences CRUD
bool DBManager::saveImagePreferences(const ImagePreferences& prefs)
{
    QSqlQuery query;
    query.prepare(R"(
        INSERT INTO image_preferences (model_variant, image_size, aspect_ratio, server_url, api_key_id, prompt, reference_image_paths)
        VALUES (:model_variant, :image_size, :aspect_ratio, :server_url, :api_key_id, :prompt, :reference_image_paths)
        ON CONFLICT(model_variant) DO UPDATE SET
            image_size = excluded.image_size,
            aspect_ratio = excluded.aspect_ratio,
            server_url = excluded.server_url,
            api_key_id = excluded.api_key_id,
            prompt = excluded.prompt,
            reference_image_paths = excluded.reference_image_paths
    )");

    query.bindValue(":model_variant", prefs.modelVariant);
    query.bindValue(":image_size", prefs.imageSize);
    query.bindValue(":aspect_ratio", prefs.aspectRatio);
    query.bindValue(":server_url", prefs.serverUrl);
    query.bindValue(":api_key_id", prefs.apiKeyId);
    query.bindValue(":prompt", prefs.prompt);
    query.bindValue(":reference_image_paths", prefs.referenceImagePaths);

    if (!query.exec()) {
        qCritical() << "Failed to save image preferences:" << query.lastError().text();
        return false;
    }
    return true;
}

ImagePreferences DBManager::loadImagePreferences(const QString& modelVariant)
{
    ImagePreferences prefs;
    prefs.modelVariant = modelVariant;
    prefs.imageSize = "1K";
    prefs.aspectRatio = "1:1";
    prefs.serverUrl = "";
    prefs.apiKeyId = -1;
    prefs.prompt = "";
    prefs.referenceImagePaths = "";

    QSqlQuery query;
    query.prepare("SELECT image_size, aspect_ratio, server_url, api_key_id, prompt, reference_image_paths FROM image_preferences WHERE model_variant = :model_variant");
    query.bindValue(":model_variant", modelVariant);

    if (query.exec() && query.next()) {
        prefs.imageSize = query.value(0).toString();
        prefs.aspectRatio = query.value(1).toString();
        prefs.serverUrl = query.value(2).toString();
        prefs.apiKeyId = query.value(3).toInt();
        prefs.prompt = query.value(4).toString();
        prefs.referenceImagePaths = query.value(5).toString();
    }

    return prefs;
}

// App Preferences CRUD
bool DBManager::saveAppPreference(const QString &key, const QString &value)
{
    QSqlQuery query;
    query.prepare(R"(
        INSERT INTO app_preferences (key, value) VALUES (:key, :value)
        ON CONFLICT(key) DO UPDATE SET value = excluded.value
    )");

    query.bindValue(":key", key);
    query.bindValue(":value", value);

    if (!query.exec()) {
        qCritical() << "Failed to save app preference:" << query.lastError().text();
        return false;
    }
    // 强制提交事务，确保数据写入
    db.commit();
    db.transaction();
    return true;
}

QString DBManager::loadAppPreference(const QString &key, const QString &defaultValue)
{
    QSqlQuery query;
    query.prepare("SELECT value FROM app_preferences WHERE key = :key");
    query.bindValue(":key", key);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return defaultValue;
}

// Image Draft CRUD
bool DBManager::saveImageDraft(const ImageDraft& draft)
{
    QSqlQuery query;
    query.prepare(R"(
        INSERT INTO image_draft (model_variant, api_key_id, server_url, image_size, aspect_ratio, prompt, reference_image_paths)
        VALUES (?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(model_variant) DO UPDATE SET
            api_key_id = excluded.api_key_id,
            server_url = excluded.server_url,
            image_size = excluded.image_size,
            aspect_ratio = excluded.aspect_ratio,
            prompt = excluded.prompt,
            reference_image_paths = excluded.reference_image_paths,
            updated_at = CURRENT_TIMESTAMP
    )");

    query.addBindValue(draft.modelVariant);
    query.addBindValue(draft.apiKeyId);
    query.addBindValue(draft.serverUrl);
    query.addBindValue(draft.imageSize);
    query.addBindValue(draft.aspectRatio);
    query.addBindValue(draft.prompt);
    query.addBindValue(draft.referenceImagePaths);

    if (!query.exec()) {
        qCritical() << "Failed to save image draft:" << query.lastError().text();
        return false;
    }
    // 强制提交事务
    db.commit();
    db.transaction();
    return true;
}

ImageDraft DBManager::loadImageDraft(const QString& modelVariant)
{
    ImageDraft draft;
    draft.modelVariant = modelVariant;
    QSqlQuery query;
    query.prepare("SELECT api_key_id, server_url, image_size, aspect_ratio, prompt, reference_image_paths FROM image_draft WHERE model_variant = :model_variant");
    query.bindValue(":model_variant", modelVariant);

    if (query.exec() && query.next()) {
        draft.apiKeyId = query.value(0).toString();
        draft.serverUrl = query.value(1).toString();
        draft.imageSize = query.value(2).toString();
        draft.aspectRatio = query.value(3).toString();
        draft.prompt = query.value(4).toString();
        draft.referenceImagePaths = query.value(5).toString();
    }
    return draft;
}

