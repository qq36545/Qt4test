#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>

class VideoGenWidget;
class ImageGenWidget;
class ConfigWidget;
class AboutWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void showVideoGen();
    void showImageGen();
    void showConfig();
    void showAbout();
    void showHistory();

private:
    void setupUI();
    void setupSidebar();
    void setupContentArea();
    void setupFonts();
    void applyStyles();
    double calculateScaleFactor();

    // UI 组件
    QWidget *centralWidget;
    QWidget *sidebar;
    QStackedWidget *contentArea;

    // 侧边栏按钮
    QPushButton *videoButton;
    QPushButton *imageButton;
    QPushButton *configButton;
    QPushButton *aboutButton;
    QPushButton *historyButton;

    // 内容页面
    VideoGenWidget *videoGenWidget;
    ImageGenWidget *imageGenWidget;
    ConfigWidget *configWidget;
    AboutWidget *aboutWidget;
    QWidget *historyWidget;
};

#endif // MAINWINDOW_H
