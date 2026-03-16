#ifndef ABOUTWIDGET_H
#define ABOUTWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include "../network/updatemanager.h"

class AboutWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AboutWidget(QWidget *parent = nullptr);

private slots:
    void checkVersion();
    void onUpdateCheckStarted(bool isManual);
    void onUpdateAvailable(const UpdateManager::ReleaseInfo& info,
                           bool isManual, bool mandatoryEffective);
    void onNoUpdateFound(bool isManual, const QString& currentVersion);
    void onCheckFailed(bool isManual, const QString& reason);

private:
    void setupUI();

    QLabel *logoLabel;
    QLabel *nameLabel;
    QLabel *versionLabel;
    QLabel *copyrightLabel;
    QPushButton *checkVersionButton;

    bool m_checkingUpdate = false;
};

#endif // ABOUTWIDGET_H
