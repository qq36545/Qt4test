#ifndef GROKGENPAGE_H
#define GROKGENPAGE_H

#include <QWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QCheckBox>
#include <QScrollArea>
#include <QTimer>
#include <QEvent>

class VideoAPI;

class GrokGenPage : public QWidget
{
    Q_OBJECT

public:
    explicit GrokGenPage(QWidget *parent = nullptr);
    ~GrokGenPage() = default;

    void loadFromTask(const struct VideoTask& task);

signals:
    void apiKeySelectionChanged(const QString& apiKeyValue);
    void submitSucceeded(const QString& modelName);
    void submitFailed(const QString& modelName, const QString& error);

public slots:
    void refreshApiKeys();

private slots:
    void generateVideo();
    void resetForm();
    void onModelVariantChanged(int index);
    void onVideoCreated(const QString &taskId, const QString &status);
    void onTaskStatusUpdated(const QString &taskId, const QString &status, const QString &videoUrl, int progress);
    void onImageUploadProgress(int current, int total);
    void onApiError(const QString &error);
    void onAnyParameterChanged();

private:
    void setupUI();
    void connectSignals();
    void loadApiKeys();
    void onUploadImagesClicked();
    void updateThumbnailGrid();
    void removeReferenceImage(int index);
    void replaceReferenceImage(int index);
    int maxReferenceImages() const;
    void queueSaveSettings();
    void saveSettings();
    void loadSettings();
    QString buildSettingsSnapshot() const;
    QString calculateParamsHash() const;
    bool checkDuplicateSubmission();
    bool validateImageFile(const QString &filePath, QString &errorMsg) const;
    void setSubmitting(bool submitting);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

    QTextEdit *promptInput;
    QComboBox *apiKeyCombo;
    QPushButton *addKeyButton;
    QComboBox *serverCombo;
    QComboBox *modelVariantCombo;
    QComboBox *resolutionCombo;
    QLabel *resolutionLabel;
    QComboBox *sizeCombo;
    QLabel *sizeLabel;
    QLabel *promptLabel;
    QLabel *previewLabel;
    QPushButton *generateButton;
    QPushButton *resetButton;

    QLabel *imageLabel;
    QLabel *referenceCountLabel;
    QPushButton *clearImagesButton;
    QWidget *thumbnailContainer;
    QGridLayout *thumbnailLayout;
    QStringList uploadedImagePaths;
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

#endif // GROKGENPAGE_H
