#ifndef SORA2GENPAGE_H
#define SORA2GENPAGE_H

#include <QWidget>
#include <QVariantMap>

class QComboBox;
class QTextEdit;
class QListWidget;
class QLabel;
class QCheckBox;
class QPushButton;
class QStackedWidget;
struct VideoTask;

class Sora2GenPage : public QWidget
{
    Q_OBJECT

public:
    explicit Sora2GenPage(QWidget *parent = nullptr);
    void loadFromTask(const VideoTask &task);

signals:
    void createTaskRequested(const QVariantMap &payload);

private slots:
    void onSubmitClicked();
    void onApiFormatChanged(int index);
    void onUploadImagesClicked();

private:
    void setupUI();
    void refreshImageList();

    QString currentApiFormat() const;
    QString currentVariant() const;
    QStringList currentImagePaths() const;
    bool isImageToVideo() const;
    QVariantMap buildUnifiedPayload() const;
    QVariantMap buildOpenAIPayload() const;

private:
    QComboBox *apiFormatCombo;
    QComboBox *variantCombo;
    QTextEdit *promptInput;
    QPushButton *uploadImagesButton;
    QListWidget *imageList;

    QStackedWidget *paramsStack;

    // unified
    QComboBox *unifiedSizeCombo;
    QComboBox *unifiedOrientationCombo;
    QComboBox *unifiedDurationCombo;

    // openai
    QComboBox *openaiSecondsCombo;
    QComboBox *openaiSizeCombo;
    QComboBox *openaiStyleCombo;

    QCheckBox *watermarkCheckBox;
    QCheckBox *privateCheckBox;

    QPushButton *submitButton;

    QStringList uploadedImagePaths;
};

#endif // SORA2GENPAGE_H
