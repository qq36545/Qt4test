#ifndef IMAGEGEN_H
#define IMAGEGEN_H

#include <QWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

class ImageGenWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ImageGenWidget(QWidget *parent = nullptr);

private slots:
    void generateImage();

private:
    void setupUI();

    QTextEdit *promptInput;
    QComboBox *resolutionCombo;
    QComboBox *aspectRatioCombo;
    QComboBox *styleCombo;
    QLabel *previewLabel;
    QPushButton *generateButton;
};

#endif // IMAGEGEN_H
