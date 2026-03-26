#ifndef VEOGENPAGE_H
#define VEOGENPAGE_H

#include <QWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QCheckBox>
#include <QScrollArea>
#include <QSpinBox>
#include <QGridLayout>
#include <QTimer>
#include <QEvent>

class VideoAPI;

class VeoGenPage : public QWidget
{
    Q_OBJECT

public:
    explicit VeoGenPage(QWidget *parent = nullptr);
    ~VeoGenPage() = default;

    void loadFromTask(const struct VideoTask& task);

signals:
    void apiKeySelectionChanged(const QString& apiKeyValue);

public slots:
    void refreshApiKeys();

private slots:
    void generateVideo();
    void resetForm();
    void onVariantTypeChanged();
    void onModelVariantChanged(int index);
    void uploadImage();
    void uploadEndFrameImage();
    void uploadMiddleFrameImage();
    void clearImage(int index);
    void onVideoCreated(const QString &taskId, const QString &status);
    void onTaskStatusUpdated(const QString &taskId, const QString &status, const QString &videoUrl, int progress);
    void onApiError(const QString &error);
    void onAnyParameterChanged();

private:
    void setupUI();
    void connectSignals();
    void loadApiKeys();
    void updateResolutionOptions(bool is4K, bool isVariant2 = false);
    void updateImageUploadUI(const QString &modelName);
    void updateImagePreview();
    QString selectAndValidateImageFile(const QString &dialogTitle, bool warnIfEmpty);
    bool validateImageFile(const QString &filePath, QString &errorMsg) const;
    QString normalizeImageReferences(const QString &prompt) const;
    void queueSaveSettings();
    void saveSettings();
    void loadSettings();
    QString buildSettingsSnapshot() const;
    QString calculateParamsHash() const;
    bool checkDuplicateSubmission();
    bool validateImgbbKey(QString &errorMsg) const;
    void setSubmitting(bool submitting);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

    // 控件
    QTextEdit *promptInput;
    QComboBox *apiKeyCombo;
    QPushButton *addKeyButton;
    QComboBox *serverCombo;
    QComboBox *modelVariantCombo;
    QComboBox *resolutionCombo;
    QLabel *resolutionLabel;
    QComboBox *durationCombo;
    QLabel *durationLabel;
    QCheckBox *watermarkCheckBox;
    QLabel *watermarkLabel;
    QLabel *promptLabel;
    QWidget *variantTypeWidget;
    QRadioButton *variantType1Radio;
    QRadioButton *variantType2Radio;
    QCheckBox *enhancePromptCheckBox;
    QLabel *enhancePromptLabel;
    QCheckBox *enableUpsampleCheckBox;
    QLabel *enableUpsampleLabel;
    QComboBox *aspectRatioCombo;
    QLabel *aspectRatioLabel;
    QLabel *previewLabel;
    QPushButton *generateButton;
    QPushButton *resetButton;

    // 图片上传
    QLabel *imageLabel;
    QLabel *imagePreviewLabel;
    QPushButton *uploadImageButton;
    QPushButton *clearImageButton;
    QPushButton *clearEndFrameButton;
    QPushButton *clearMiddleFrameButton;
    QStringList uploadedImagePaths;
    QWidget *endFrameWidget;
    QLabel *endFrameLabel;
    QLabel *endFramePreviewLabel;
    QPushButton *uploadEndFrameButton;
    QString uploadedEndFrameImagePath;
    QWidget *middleFrameWidget;
    QLabel *middleFrameLabel;
    QLabel *middleFramePreviewLabel;
    QPushButton *uploadMiddleFrameButton;
    QString uploadedMiddleFrameImagePath;
    QLabel *imageUploadHintLabel;

    VideoAPI *api;
    QString currentTaskId;
    QString lastSubmittedParamsHash;
    bool suppressDuplicateWarning;
    bool parametersModified;
    bool pendingSaveSettings;
    bool suppressAutoSave;
    bool isSubmitting = false;
    QString lastSavedSettingsSnapshot;
};

#endif // VEOGENPAGE_H
