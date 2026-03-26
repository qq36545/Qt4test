#ifndef SORA2GENPAGE_H
#define SORA2GENPAGE_H

#include <QWidget>
#include <QVariantMap>
#include <QEvent>
#include <QList>

class QComboBox;
class QTextEdit;
class QLabel;
class QCheckBox;
class QPushButton;
class QStackedWidget;
class QRadioButton;
class QButtonGroup;
class QGridLayout;
class QShowEvent;
struct VideoTask;

class Sora2GenPage : public QWidget
{
    Q_OBJECT

public:
    explicit Sora2GenPage(QWidget *parent = nullptr);
    void loadFromTask(const VideoTask &task);
    void setSubmitEnabled(bool enabled);

protected:
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void createTaskRequested(const QVariantMap &payload);
    void apiKeySelectionChanged(const QString &apiKeyValue);

public slots:
    void refreshApiKeys();
    void restoreDraftSettings();

private slots:
    void onSubmitClicked();
    void onApiFormatChanged();
    void onUploadImagesClicked();

private:
    void setupUI();
    void updateThumbnailGrid();
    void applyApiFormatRadioStyle();
    QString resolveAppTheme() const;

    QString currentApiFormat() const;
    QString currentVariant() const;
    QStringList currentImagePaths() const;
    bool isImageToVideo() const;
    QVariantMap buildUnifiedPayload() const;
    QVariantMap buildOpenAIPayload() const;

    void loadApiKeys();
    int maxReferenceImages() const;
    void removeReferenceImage(int index);
    void replaceReferenceImage(int index);

    QList<int> durationsForVariant(const QString &variant) const;
    void refreshDurationOptionsByVariant();
    void setDurationComboItems(QComboBox *combo, const QList<int> &values, int preferValue);
    int currentDurationValue(QComboBox *combo, bool *ok = nullptr) const;
    bool validateDurationEditor(QComboBox *combo);
    void syncDurationValueToBoth(int value, QComboBox *source = nullptr);

    void queueSaveSettings();
    void saveSettings();
    void loadSettings();
    QString buildSettingsSnapshot() const;

private:
    QRadioButton *unifiedFormatRadio;
    QRadioButton *openaiFormatRadio;
    QButtonGroup *apiFormatGroup;
    QLabel *apiFormatLabel;
    QComboBox *variantCombo;
    QComboBox *apiKeyCombo;
    QPushButton *addKeyButton;
    QComboBox *serverCombo;
    QTextEdit *promptInput;
    QPushButton *clearImagesButton;
    QLabel *referenceCountLabel;
    QWidget *thumbnailContainer;
    QGridLayout *thumbnailLayout;
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
    int lastValidDuration = 10;
    bool syncingDuration = false;
    bool suppressAutoSave = false;
    bool pendingSaveSettings = false;
    QString lastSavedSettingsSnapshot;
};

#endif // SORA2GENPAGE_H
