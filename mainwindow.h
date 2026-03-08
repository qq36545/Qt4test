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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void showVideoGen();
    void showImageGen();
    void showHistory();

private:
    void setupUI();
    void setupSidebar();
    void setupContentArea();
    void applyStyles();

    // UI 组件
    QWidget *centralWidget;
    QWidget *sidebar;
    QStackedWidget *contentArea;

    // 侧边栏按钮
    QPushButton *videoButton;
    QPushButton *imageButton;
    QPushButton *historyButton;

    // 内容页面
    VideoGenWidget *videoGenWidget;
    ImageGenWidget *imageGenWidget;
    QWidget *historyWidget;
};

#endif // MAINWINDOW_H
