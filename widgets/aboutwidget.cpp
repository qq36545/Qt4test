#include "aboutwidget.h"
#include <QApplication>
#include <QMessageBox>
#include <QPixmap>

AboutWidget::AboutWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();

    // 连接 UpdateManager 信号
    UpdateManager *updateManager = UpdateManager::getInstance();
    connect(updateManager, &UpdateManager::checkStarted,
            this, &AboutWidget::onUpdateCheckStarted);
    connect(updateManager, &UpdateManager::updateAvailable,
            this, &AboutWidget::onUpdateAvailable);
    connect(updateManager, &UpdateManager::noUpdateFound,
            this, &AboutWidget::onNoUpdateFound);
    connect(updateManager, &UpdateManager::checkFailed,
            this, &AboutWidget::onCheckFailed);
}

void AboutWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(40, 40, 40, 40);
    mainLayout->setSpacing(30);
    mainLayout->setAlignment(Qt::AlignCenter);

    // Logo
    logoLabel = new QLabel();
    QPixmap logo(":/resources/logo.png");
    if (!logo.isNull()) {
        logoLabel->setPixmap(logo.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        logoLabel->setText("🐔");
        logoLabel->setStyleSheet("font-size: 120px;");
    }
    logoLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(logoLabel);

    // 项目名称
    nameLabel = new QLabel("鸡你太美ai 图片/视频批量生成器");
    nameLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #F8FAFC;");
    nameLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(nameLabel);

    // 版本号
    versionLabel = new QLabel(QString("版本: v%1").arg(QApplication::applicationVersion()));
    versionLabel->setStyleSheet("font-size: 18px; color: #94A3B8;");
    versionLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(versionLabel);

    // 检测版本按钮
    checkVersionButton = new QPushButton("🔍 检测新版本");
    checkVersionButton->setFixedHeight(45);
    checkVersionButton->setFixedWidth(200);
    checkVersionButton->setCursor(Qt::PointingHandCursor);
    connect(checkVersionButton, &QPushButton::clicked, this, &AboutWidget::checkVersion);
    mainLayout->addWidget(checkVersionButton, 0, Qt::AlignCenter);

    mainLayout->addSpacing(20);

    // 版权信息
    copyrightLabel = new QLabel("©️ 版权所有：科哥玩AI\n微信：312088415");
    copyrightLabel->setStyleSheet("font-size: 16px; color: #64748B;");
    copyrightLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(copyrightLabel);

    mainLayout->addStretch();
}

void AboutWidget::checkVersion()
{
    if (m_checkingUpdate) {
        return;  // 防止重复点击
    }

    UpdateManager::getInstance()->checkForUpdates(true);
}

void AboutWidget::onUpdateCheckStarted(bool isManual)
{
    if (!isManual) return;  // 只处理手动检查

    m_checkingUpdate = true;
    checkVersionButton->setEnabled(false);
    checkVersionButton->setText("🔄 检查中...");
}

void AboutWidget::onUpdateAvailable(
    const UpdateManager::ReleaseInfo& info,
    bool isManual, bool mandatoryEffective)
{
    if (!isManual) return;

    m_checkingUpdate = false;
    checkVersionButton->setEnabled(true);
    checkVersionButton->setText("🔍 检测新版本");

    QString text = QString("发现新版本 %1\n\n"
                          "当前版本: v%2\n"
                          "最新版本: %3\n"
                          "发布日期: %4\n\n"
                          "更新说明:\n%5\n\n"
                          "是否立即下载？")
                      .arg(info.version)
                      .arg(QApplication::applicationVersion())
                      .arg(info.version)
                      .arg(info.releaseDate)
                      .arg(info.releaseNotes);

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("发现新版本");
    msgBox.setText(text);
    msgBox.setIcon(QMessageBox::Information);
    QPushButton *downloadBtn = msgBox.addButton("立即下载",
                                                QMessageBox::AcceptRole);
    msgBox.addButton("稍后", QMessageBox::RejectRole);
    msgBox.exec();

    if (msgBox.clickedButton() == downloadBtn) {
        UpdateManager::getInstance()->startDownloadAndInstall();
    }
}

void AboutWidget::onNoUpdateFound(bool isManual, const QString& currentVersion)
{
    if (!isManual) return;

    m_checkingUpdate = false;
    checkVersionButton->setEnabled(true);
    checkVersionButton->setText("🔍 检测新版本");

    QMessageBox::information(this, "检查更新",
        QString("当前已是最新版本\n\n版本: v%1").arg(currentVersion));
}

void AboutWidget::onCheckFailed(bool isManual, const QString& reason)
{
    if (!isManual) return;

    m_checkingUpdate = false;
    checkVersionButton->setEnabled(true);
    checkVersionButton->setText("🔍 检测新版本");

    QMessageBox::warning(this, "检查更新失败",
        QString("无法检查更新\n\n原因: %1\n\n"
               "请检查网络连接后重试").arg(reason));
}
