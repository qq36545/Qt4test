#ifndef SORA2GENPAGE_H
#define SORA2GENPAGE_H

#include <QWidget>
#include <QVariantMap>
#include <QEvent>

class QComboBox;
class QTextEdit;
class QListWidget;
class QLabel;
class QCheckBox;
class QPushButton;
class QStackedWidget;
class QRadioButton;
class QButtonGroup;
class QShowEvent;
struct VideoTask;

class Sora2GenPage : public QWidget
{
    Q_OBJECT

public:
    explicit Sora2GenPage(QWidget *parent = nullptr);
    void loadFromTask(const VideoTask &task);

protected:
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;

signals:
    void createTaskRequested(const QVariantMap &payload);

private slots:
    void onSubmitClicked();
    void onApiFormatChanged();
    void onUploadImagesClicked();

private:
    void setupUI();
    void refreshImageList();
    void applyApiFormatRadioStyle();
    QString resolveAppTheme() const;

    QString currentApiFormat() const;
    QString currentVariant() const;
    QStringList currentImagePaths() const;
    bool isImageToVideo() const;
    QVariantMap buildUnifiedPayload() const;
    QVariantMap buildOpenAIPayload() const;

private:
    QRadioButton *unifiedFormatRadio;
    QRadioButton *openaiFormatRadio;
    QButtonGroup *apiFormatGroup;
    QLabel *apiFormatLabel;
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
