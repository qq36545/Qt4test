#ifndef GROKGENPAGE_H
#define GROKGENPAGE_H

#include <QWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
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

public slots:
    void refreshApiKeys();

private slots:
    void generateVideo();
    void resetForm();
    void onModelVariantChanged(int index);
    void uploadImage1();
    void uploadImage2();
    void uploadImage3();
    void clearImage(int index);
    void onVideoCreated(const QString &taskId, const QString &status);
    void onTaskStatusUpdated(const QString &taskId, const QString &status, const QString &videoUrl, int progress);
    void onApiError(const QString &error);
    void onAnyParameterChanged();

private:
    void setupUI();
    void connectSignals();
    void loadApiKeys();
    void updateImagePreview();
    void queueSaveSettings();
    void saveSettings();
    void loadSettings();
    QString buildSettingsSnapshot() const;
    QString calculateParamsHash() const;
    bool checkDuplicateSubmission();
    QString selectAndValidateImageFile(const QString &dialogTitle);
    bool validateImageFile(const QString &filePath, QString &errorMsg) const;

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

    // 图片上传（3张）
    QLabel *imageLabel;
    QLabel *imagePreviewLabel;
    QPushButton *uploadImage1Button;
    QPushButton *clearImage1Button;

    QWidget *image2Widget;
    QLabel *image2PreviewLabel;
    QPushButton *uploadImage2Button;
    QPushButton *clearImage2Button;

    QWidget *image3Widget;
    QLabel *image3PreviewLabel;
    QPushButton *uploadImage3Button;
    QPushButton *clearImage3Button;

    QStringList uploadedImagePaths;
    QLabel *imageUploadHintLabel;

    VideoAPI *api;
    QString currentTaskId;
    QString lastSubmittedParamsHash;
    bool suppressDuplicateWarning;
    bool parametersModified;
    bool pendingSaveSettings;
    bool suppressAutoSave;
    QString lastSavedSettingsSnapshot;
};

#endif // GROKGENPAGE_H
