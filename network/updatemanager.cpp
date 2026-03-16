#include "updatemanager.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QCryptographicHash>

UpdateManager* UpdateManager::m_instance = nullptr;

UpdateManager* UpdateManager::getInstance()
{
    if (!m_instance) {
        m_instance = new UpdateManager();
    }
    return m_instance;
}

UpdateManager::UpdateManager(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_checkTimeoutTimer(new QTimer(this))
{
    qRegisterMetaType<UpdateManager::ReleaseInfo>("UpdateManager::ReleaseInfo");

    m_checkTimeoutTimer->setSingleShot(true);
    connect(m_checkTimeoutTimer, &QTimer::timeout, this, &UpdateManager::onVersionCheckTimeout);

    m_channelOrder << "github" << "gitee" << "cloudflare";

    // 默认版本信息地址（可由外部 setVersionInfoUrls 覆盖）
    m_versionInfoUrls << "https://raw.githubusercontent.com/qq36545/Qt4test/master/releases/version.json";
}

UpdateManager::~UpdateManager()
{
    cleanupVersionCheckReplies();
    resetDownloadState();
}

void UpdateManager::setVersionInfoUrls(const QStringList& urls)
{
    if (!urls.isEmpty()) {
        m_versionInfoUrls = urls;
    }
}

void UpdateManager::setMandatoryHardBlockEnabled(bool enabled)
{
    m_mandatoryHardBlockEnabled = enabled;
}

void UpdateManager::checkForUpdates(bool isManual)
{
    if (m_checkInProgress) {
        return;
    }

    m_checkInProgress = true;
    m_checkManual = isManual;
    m_checkResolved = false;
    cleanupVersionCheckReplies();

    emit checkStarted(isManual);

    for (const QString& url : m_versionInfoUrls) {
        QNetworkRequest req{QUrl(url)};
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply* reply = m_networkManager->get(req);
        m_checkReplies.append(reply);
        connect(reply, &QNetworkReply::finished, this, &UpdateManager::onVersionCheckReplyFinished);
    }

    m_checkTimeoutTimer->start(5000);
}

void UpdateManager::onVersionCheckReplyFinished()
{
    if (m_checkResolved) {
        QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
        if (reply) {
            m_checkReplies.removeAll(reply);
            reply->deleteLater();
        }
        return;
    }

    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    const QByteArray body = reply->readAll();
    const bool handled = tryHandleVersionResponse(reply, body);

    m_checkReplies.removeAll(reply);
    reply->deleteLater();

    if (handled) {
        return;
    }

    if (m_checkReplies.isEmpty()) {
        finalizeVersionCheckFailed(m_checkManual, QStringLiteral("所有渠道均不可用或返回非法版本信息"));
    }
}

void UpdateManager::onVersionCheckTimeout()
{
    if (m_checkResolved) {
        return;
    }
    finalizeVersionCheckFailed(m_checkManual, QStringLiteral("检查更新超时，请稍后重试"));
}

bool UpdateManager::tryHandleVersionResponse(QNetworkReply* reply, const QByteArray& body)
{
    if (reply->error() != QNetworkReply::NoError) {
        return false;
    }

    ReleaseInfo info;
    QString parseError;
    if (!parseReleaseInfo(body, info, parseError)) {
        return false;
    }

    finalizeVersionCheckSuccess(info, m_checkManual);
    return true;
}

bool UpdateManager::parseReleaseInfo(const QByteArray& body, ReleaseInfo& outInfo, QString& error) const
{
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        error = parseErr.errorString();
        return false;
    }

    const QJsonObject obj = doc.object();

    outInfo.version = obj.value("version").toString();
    outInfo.versionCode = obj.value("versionCode").toInt();
    outInfo.updateType = obj.value("updateType").toString("optional").toLower();
    outInfo.forceReason = obj.value("forceReason").toString();
    outInfo.releaseDate = obj.value("releaseDate").toString();
    outInfo.releaseNotes = obj.value("releaseNotes").toString();
    outInfo.minSupportedVersion = obj.value("minSupportedVersion").toString();
    outInfo.downloadUrls = obj.value("downloadUrls").toObject();
    outInfo.sha256 = obj.value("sha256").toObject();
    outInfo.fileSize = obj.value("fileSize").toObject();

    if (!outInfo.isValid()) {
        error = QStringLiteral("version/versionCode 缺失");
        return false;
    }

    return true;
}

void UpdateManager::finalizeVersionCheckSuccess(const ReleaseInfo& info, bool isManual)
{
    if (m_checkResolved) {
        return;
    }
    m_checkResolved = true;
    m_checkInProgress = false;
    m_checkTimeoutTimer->stop();

    const int localCode = currentVersionCode();
    int minSupportedCode = 0;
    if (!info.minSupportedVersion.isEmpty()) {
        minSupportedCode = versionStringToCode(info.minSupportedVersion);
    }

    const bool hasUpdate = info.versionCode > localCode;
    if (!hasUpdate) {
        m_hasPendingUpdate = false;
        if (isManual) {
            emit noUpdateFound(true, QApplication::applicationVersion());
        }
        cleanupVersionCheckReplies();
        return;
    }

    m_latestInfo = info;
    m_hasPendingUpdate = true;

    bool mandatoryEffective = info.isMandatory();
    if (minSupportedCode > 0 && localCode < minSupportedCode) {
        mandatoryEffective = true;
        if (m_latestInfo.forceReason.isEmpty()) {
            m_latestInfo.forceReason = QStringLiteral("当前版本过旧，必须升级到最新版本");
        }
    }

    if (mandatoryEffective && !m_mandatoryHardBlockEnabled) {
        mandatoryEffective = false;
    }

    emit updateAvailable(m_latestInfo, isManual, mandatoryEffective);
    cleanupVersionCheckReplies();
}

void UpdateManager::finalizeVersionCheckFailed(bool isManual, const QString& reason)
{
    if (m_checkResolved) {
        return;
    }
    m_checkResolved = true;
    m_checkInProgress = false;
    m_checkTimeoutTimer->stop();

    if (isManual) {
        emit checkFailed(true, reason);
    }

    cleanupVersionCheckReplies();
}

void UpdateManager::cleanupVersionCheckReplies()
{
    for (QNetworkReply* reply : m_checkReplies) {
        if (!reply) {
            continue;
        }
        disconnect(reply, nullptr, this, nullptr);
        if (reply->isRunning()) {
            reply->abort();
        }
        reply->deleteLater();
    }
    m_checkReplies.clear();
}

UpdateManager::ReleaseInfo UpdateManager::latestReleaseInfo() const
{
    return m_latestInfo;
}

bool UpdateManager::hasPendingUpdate() const
{
    return m_hasPendingUpdate;
}

void UpdateManager::startDownloadAndInstall()
{
    if (!m_hasPendingUpdate) {
        emit downloadFailed(QStringLiteral("当前无可下载更新"));
        return;
    }

    if (m_downloadReply) {
        return;
    }

    m_platformKey = detectPlatform();
    if (m_platformKey.isEmpty()) {
        emit downloadFailed(QStringLiteral("不支持当前平台"));
        return;
    }

    m_platformDownloadUrls = m_latestInfo.downloadUrls.value(m_platformKey).toObject();
    if (m_platformDownloadUrls.isEmpty()) {
        emit downloadFailed(QStringLiteral("版本信息缺少当前平台下载地址"));
        return;
    }

    m_channelIndex = -1;
    startNextChannelDownload();
}

void UpdateManager::startNextChannelDownload(const QString& reasonForFallback)
{
    const QString prev = currentChannelName();
    m_channelIndex += 1;

    while (m_channelIndex < m_channelOrder.size()) {
        const QString channel = m_channelOrder[m_channelIndex];
        const QString url = m_platformDownloadUrls.value(channel).toString();
        if (!url.isEmpty()) {
            if (!reasonForFallback.isEmpty() && !prev.isEmpty()) {
                emit downloadChannelFallback(prev, channel, reasonForFallback);
            }

            m_downloadFilePath = computeDownloadPath(url);
            QDir().mkpath(QFileInfo(m_downloadFilePath).absolutePath());

            resetDownloadState();
            m_downloadFile = new QFile(m_downloadFilePath, this);
            if (!m_downloadFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                QString err = QStringLiteral("无法写入下载文件: %1").arg(m_downloadFilePath);
                delete m_downloadFile;
                m_downloadFile = nullptr;
                m_channelIndex += 1;
                continue;
            }

            QNetworkRequest req{QUrl(url)};
            req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
            m_downloadReply = m_networkManager->get(req);

            connect(m_downloadReply, &QNetworkReply::downloadProgress, this, &UpdateManager::downloadProgress);
            connect(m_downloadReply, &QNetworkReply::finished, this, &UpdateManager::onDownloadReplyFinished);

            emit downloadStarted(channel, m_downloadFilePath);
            return;
        }

        m_channelIndex += 1;
    }

    emit downloadFailed(QStringLiteral("所有下载渠道均失败"));
}

void UpdateManager::onDownloadReplyFinished()
{
    if (!m_downloadReply || !m_downloadFile) {
        return;
    }

    QNetworkReply* reply = m_downloadReply;
    QFile* file = m_downloadFile;
    const QString filePath = m_downloadFilePath;
    const QString channel = currentChannelName();

    const QByteArray payload = reply->readAll();
    if (!payload.isEmpty()) {
        file->write(payload);
    }
    file->flush();
    file->close();

    disconnect(reply, nullptr, this, nullptr);
    reply->deleteLater();

    m_downloadReply = nullptr;
    m_downloadFile = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        QFile::remove(filePath);
        startNextChannelDownload(reply->errorString());
        return;
    }

    const QString expectedSha = m_latestInfo.sha256.value(m_platformKey).toString();
    if (!expectedSha.isEmpty() && !verifySha256(filePath, expectedSha)) {
        QFile::remove(filePath);
        startNextChannelDownload(QStringLiteral("SHA256校验失败"));
        return;
    }

    emit downloadFinished(filePath);

    QMessageBox::information(nullptr,
                             QStringLiteral("下载完成"),
                             QStringLiteral("安装包已下载到:\n%1\n\n即将打开所在目录，请手动安装。")
                                 .arg(filePath));
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(filePath).absolutePath()));
}

void UpdateManager::resetDownloadState()
{
    if (m_downloadReply) {
        disconnect(m_downloadReply, nullptr, this, nullptr);
        if (m_downloadReply->isRunning()) {
            m_downloadReply->abort();
        }
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
    }

    if (m_downloadFile) {
        if (m_downloadFile->isOpen()) {
            m_downloadFile->close();
        }
        m_downloadFile->deleteLater();
        m_downloadFile = nullptr;
    }
}

QString UpdateManager::currentChannelName() const
{
    if (m_channelIndex >= 0 && m_channelIndex < m_channelOrder.size()) {
        return m_channelOrder[m_channelIndex];
    }
    return QString();
}

QString UpdateManager::currentChannelUrl() const
{
    const QString channel = currentChannelName();
    if (channel.isEmpty()) {
        return QString();
    }
    return m_platformDownloadUrls.value(channel).toString();
}

QString UpdateManager::computeDownloadPath(const QString& url) const
{
    const QString downloadDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/ChickenAI_Updates";
    const QString fallbackName = QStringLiteral("ChickenAI-%1-installer").arg(m_latestInfo.version);

    const QUrl qurl(url);
    QString fileName = QFileInfo(qurl.path()).fileName();
    if (fileName.isEmpty()) {
        fileName = fallbackName;
#ifdef Q_OS_MACOS
        fileName += ".dmg";
#elif defined(Q_OS_WIN)
        fileName += ".exe";
#endif
    }

    return QDir(downloadDir).filePath(fileName);
}

bool UpdateManager::verifySha256(const QString& filePath, const QString& expectedSha256) const
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!f.atEnd()) {
        hash.addData(f.read(1024 * 1024));
    }
    f.close();

    const QString actual = QString::fromLatin1(hash.result().toHex()).toLower();
    const QString expected = expectedSha256.trimmed().toLower();
    return actual == expected;
}

QString UpdateManager::detectPlatform() const
{
#ifdef Q_OS_WIN
    return QStringLiteral("windows_x64");
#elif defined(Q_OS_MACOS)
#ifdef Q_PROCESSOR_ARM
    return QStringLiteral("macos_arm64");
#else
    return QStringLiteral("macos_intel");
#endif
#else
    return QString();
#endif
}

int UpdateManager::versionStringToCode(const QString& version) const
{
    QString norm = version;
    if (norm.startsWith('v', Qt::CaseInsensitive)) {
        norm = norm.mid(1);
    }

    const QStringList parts = norm.split('.');
    if (parts.size() < 3 || parts.size() > 4) {
        return 0;
    }

    bool ok1 = false, ok2 = false, ok3 = false, ok4 = true;
    const int major = parts[0].toInt(&ok1);
    const int minor = parts[1].toInt(&ok2);
    const int patch = parts[2].toInt(&ok3);
    int build = 0;
    if (parts.size() == 4) {
        build = parts[3].toInt(&ok4);
    }

    if (!ok1 || !ok2 || !ok3 || !ok4) {
        return 0;
    }

    return major * 1000 + minor * 100 + patch * 10 + build;
}

int UpdateManager::currentVersionCode() const
{
    const QString ver = QApplication::applicationVersion();
    return versionStringToCode(ver);
}
