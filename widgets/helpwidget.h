#ifndef HELPWIDGET_H
#define HELPWIDGET_H

#include <QWidget>

class QTextBrowser;
class QLabel;
class QResizeEvent;
class QEvent;

class HelpWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HelpWidget(QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void setupUI();
    void updateThemeStyles();
    QTextBrowser *textBrowser;
    QLabel *titleLabel;
    QWidget *contentWidget;
};

#endif // HELPWIDGET_H
