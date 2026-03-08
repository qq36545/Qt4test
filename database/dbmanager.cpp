#include "dbmanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>

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

    // 插入默认 API Key（如果表为空）
    query.exec("SELECT COUNT(*) FROM api_keys");
    if (query.next() && query.value(0).toInt() == 0) {
        addApiKey("sora2密钥", "sk-***********jshfg");
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
