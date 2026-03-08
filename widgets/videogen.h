#ifndef VIDEOGEN_H
#define VIDEOGEN_H

#include <QWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

class VideoGenWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoGenWidget(QWidget *parent = nullptr);

private slots:
    void generateVideo();

private:
    void setupUI();

    QTextEdit *promptInput;
    QComboBox *resolutionCombo;
    QComboBox *durationCombo;
    QComboBox *styleCombo;
    QLabel *previewLabel;
    QPushButton *generateButton;
};

#endif // VIDEOGEN_H
