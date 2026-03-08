#ifndef ABOUTWIDGET_H
#define ABOUTWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

class AboutWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AboutWidget(QWidget *parent = nullptr);

private slots:
    void checkVersion();

private:
    void setupUI();

    QLabel *logoLabel;
    QLabel *nameLabel;
    QLabel *versionLabel;
    QLabel *copyrightLabel;
    QPushButton *checkVersionButton;
};

#endif // ABOUTWIDGET_H
