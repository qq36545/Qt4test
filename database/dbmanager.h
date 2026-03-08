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

private:
    explicit DBManager(QObject *parent = nullptr);
    ~DBManager();

    bool initDatabase();
    bool createTables();

    QSqlDatabase db;
    static DBManager* m_instance;
};

#endif // DBMANAGER_H
