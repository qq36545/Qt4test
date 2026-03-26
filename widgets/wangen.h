#ifndef WANGENPAGE_H
#define WANGENPAGE_H

#include <QWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QScrollArea>
#include <QSpinBox>
#include <QTimer>
#include <QEvent>

class VideoAPI;

class WanGenPage : public QWidget
{
    Q_OBJECT

public:
    explicit WanGenPage(QWidget *parent = nullptr);
    ~WanGenPage() = default;

    void loadFromTask(const struct VideoTask& task);

signals:
    void apiKeySelectionChanged(const QString& apiKeyValue);

public slots:
    void refreshApiKeys();

private slots:
    void generateVideo();
    void resetForm();
    void onModelVariantChanged(int index);
    void uploadImage();
    void uploadAudio();
    void clearAudio();
    void onVideoCreated(const QString &taskId, const QString &status);
    void onTaskStatusUpdated(const QString &taskId, const QString &status, const QString &videoUrl, int progress);
    void onImageUploadProgress(int current, int total);
    void onApiError(const QString &error);
    void onAnyParameterChanged();

private:
    void setupUI();
    void connectSignals();
    void loadApiKeys();
    void updateAudioWidgetVisibility(const QString &modelVariant);
    void updateImagePreview();
    void queueSaveSettings();
    void saveSettings();
    void loadSettings();
    QString buildSettingsSnapshot() const;
    QString calculateParamsHash() const;
    bool checkDuplicateSubmission();
    bool validateImgbbKey(QString &errorMsg) const;
    QString selectAndValidateImageFile(const QString &dialogTitle);
    bool validateImageFile(const QString &filePath, QString &errorMsg) const;
    void setSubmitting(bool submitting);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

    QTextEdit *promptInput;
    QTextEdit *negativePromptInput;
    QComboBox *apiKeyCombo;
    QPushButton *addKeyButton;
    QComboBox *serverCombo;
    QComboBox *modelVariantCombo;
    QComboBox *durationCombo;
    QLabel *durationLabel;
    QComboBox *resolutionCombo;
    QLabel *resolutionLabel;
    QComboBox *templateCombo;
    QLabel *templateLabel;
    QCheckBox *promptExtendCheckBox;
    QSpinBox *seedInput;
    QLabel *seedLabel;
    QCheckBox *audioCheckBox;
    QWidget *audioUploadWidget;
    QPushButton *audioUploadButton;
    QLabel *audioFileLabel;
    QPushButton *clearAudioButton;
    QCheckBox *watermarkCheckBox;
    QLabel *previewLabel;
    QPushButton *generateButton;
    QPushButton *resetButton;

    // 图片上传
    QLabel *imageLabel;
    QLabel *imagePreviewLabel;
    QPushButton *uploadImageButton;
    QPushButton *clearImageButton;
    QString uploadedImagePath;

    VideoAPI *api;
    QString currentTaskId;
    QString lastSubmittedParamsHash;
    bool suppressDuplicateWarning;
    bool parametersModified;
    bool pendingSaveSettings;
    bool suppressAutoSave;
    bool audioUploading;
    bool isSubmitting;
    QString lastSavedSettingsSnapshot;
    QString uploadedAudioPath;
    QString uploadedAudioUrl;
};

#endif // WANGENPAGE_H
