#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;
class QTimer;

class UpdateManager : public QObject
{
    Q_OBJECT

public:
    struct ReleaseInfo {
        QString version;
        int versionCode = 0;
        QString updateType;      // optional | mandatory
        QString forceReason;
        QString releaseDate;
        QString releaseNotes;
        QString minSupportedVersion;
        QJsonObject downloadUrls;
        QJsonObject sha256;
        QJsonObject fileSize;

        bool isValid() const { return versionCode > 0 && !version.isEmpty(); }
        bool isMandatory() const { return updateType == "mandatory"; }
    };

    static UpdateManager* getInstance();

    void setVersionInfoUrls(const QStringList& urls);
    void setMandatoryHardBlockEnabled(bool enabled);

    void checkForUpdates(bool isManual);
    void startDownloadAndInstall();

    ReleaseInfo latestReleaseInfo() const;
    bool hasPendingUpdate() const;

signals:
    void checkStarted(bool isManual);
    void updateAvailable(const UpdateManager::ReleaseInfo& info, bool isManual, bool mandatoryEffective);
    void noUpdateFound(bool isManual, const QString& currentVersion);
    void checkFailed(bool isManual, const QString& reason);

    void downloadStarted(const QString& channel, const QString& filePath);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadChannelFallback(const QString& fromChannel, const QString& toChannel, const QString& reason);
    void downloadFinished(const QString& filePath);
    void downloadFailed(const QString& reason);

private slots:
    void onVersionCheckReplyFinished();
    void onVersionCheckTimeout();
    void onDownloadReplyFinished();

private:
    explicit UpdateManager(QObject *parent = nullptr);
    ~UpdateManager();

    QString detectPlatform() const;
    int versionStringToCode(const QString& version) const;
    int currentVersionCode() const;

    bool tryHandleVersionResponse(QNetworkReply* reply, const QByteArray& body);
    bool parseReleaseInfo(const QByteArray& body, ReleaseInfo& outInfo, QString& error) const;

    void finalizeVersionCheckSuccess(const ReleaseInfo& info, bool isManual);
    void finalizeVersionCheckFailed(bool isManual, const QString& reason);
    void cleanupVersionCheckReplies();

    void resetDownloadState();
    void startNextChannelDownload(const QString& reasonForFallback = QString());
    QString currentChannelName() const;
    QString currentChannelUrl() const;
    QString computeDownloadPath(const QString& url) const;
    bool verifySha256(const QString& filePath, const QString& expectedSha256) const;

private:
    static UpdateManager* m_instance;

    QNetworkAccessManager* m_networkManager;

    QStringList m_versionInfoUrls;
    QList<QNetworkReply*> m_checkReplies;
    QTimer* m_checkTimeoutTimer = nullptr;
    bool m_checkInProgress = false;
    bool m_checkManual = false;
    bool m_checkResolved = false;

    ReleaseInfo m_latestInfo;
    bool m_hasPendingUpdate = false;

    bool m_mandatoryHardBlockEnabled = false;

    QNetworkReply* m_downloadReply = nullptr;
    QFile* m_downloadFile = nullptr;
    QString m_downloadFilePath;

    QString m_platformKey;
    QJsonObject m_platformDownloadUrls;
    QStringList m_channelOrder;
    int m_channelIndex = -1;
};

Q_DECLARE_METATYPE(UpdateManager::ReleaseInfo)

#endif // UPDATEMANAGER_H
