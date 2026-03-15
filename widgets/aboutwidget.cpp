#include "aboutwidget.h"
#include <QApplication>
#include <QMessageBox>
#include <QPixmap>

AboutWidget::AboutWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
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
    QMessageBox::information(this, "版本检测",
        QString("当前版本: v%1\n\n正在检测新版本...\n\n(功能待完善)").arg(QApplication::applicationVersion()));
}
