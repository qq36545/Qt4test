#ifndef VIDEOGEN_H
#define VIDEOGEN_H

#include <QWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QListWidget>
#include <QCheckBox>
#include <QScrollArea>

// 单个视频生成 Tab
class VideoSingleTab : public QWidget
{
    Q_OBJECT

public:
    explicit VideoSingleTab(QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void generateVideo();
    void onModelChanged(int index);
    void onModelVariantChanged(int index);
    void uploadImage();
    void uploadEndFrameImage();
    void removeImage(int index);

private:
    void setupUI();
    void loadApiKeys();
    void updateResolutionOptions(bool is4K);
    void updateImageUploadUI(const QString &modelName);
    void updateImagePreview();
    void onVideoCreated(const QString &taskId, const QString &status);
    void onTaskStatusUpdated(const QString &taskId, const QString &status, const QString &videoUrl, int progress);
    void onApiError(const QString &error);

    QTextEdit *promptInput;
    QComboBox *modelCombo;
    QComboBox *apiKeyCombo;
    QPushButton *addKeyButton;
    QComboBox *serverCombo;
    QComboBox *modelVariantCombo;
    QComboBox *resolutionCombo;
    QComboBox *durationCombo;
    QCheckBox *watermarkCheckBox;
    QLabel *imageLabel;
    QLabel *imagePreviewLabel;
    QPushButton *uploadImageButton;
    QStringList uploadedImagePaths;  // 改为支持多图
    QWidget *endFrameWidget;
    QLabel *endFrameLabel;
    QLabel *endFramePreviewLabel;
    QPushButton *uploadEndFrameButton;
    QString uploadedEndFrameImagePath;
    QLabel *previewLabel;
    QPushButton *generateButton;

    class Veo3API *veo3API;  // API 实例
    QString currentTaskId;   // 当前任务 ID
};

// 批量视频生成 Tab
class VideoBatchTab : public QWidget
{
    Q_OBJECT

public:
    explicit VideoBatchTab(QWidget *parent = nullptr);

private slots:
    void generateBatch();
    void importCSV();
    void deleteSelected();
    void deleteAll();
    void onModelChanged(int index);
    void onModelVariantChanged(int index);

private:
    void setupUI();
    void loadApiKeys();
    void updateResolutionOptions(bool is4K);
    void updateImageUploadUI(const QString &modelName);

    QTextEdit *promptInput;
    QLabel *imageDropArea;
    QListWidget *imageList;
    QComboBox *modelCombo;
    QComboBox *apiKeyCombo;
    QPushButton *addKeyButton;
    QComboBox *serverCombo;
    QComboBox *modelVariantCombo;
    QComboBox *resolutionCombo;
    QComboBox *durationCombo;
    QCheckBox *watermarkCheckBox;
    QLabel *imageLabel;
    QWidget *endFrameWidget;
    QLabel *endFrameLabel;
    QPushButton *importButton;
    QPushButton *deleteButton;
    QPushButton *deleteAllButton;
    QPushButton *generateButton;
};

// 历史记录 Tab
class VideoHistoryTab : public QWidget
{
    Q_OBJECT

public:
    explicit VideoHistoryTab(QWidget *parent = nullptr);

private slots:
    void refreshHistory();
    void onRowDoubleClicked(int row, int column);

private:
    void setupUI();
    void loadHistory();

    QTableWidget *historyTable;
    QPushButton *refreshButton;
};

// 主视频生成 Widget
class VideoGenWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoGenWidget(QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupUI();
    void updateTabWidths();

    QTabWidget *tabWidget;
    VideoSingleTab *singleTab;
    VideoBatchTab *batchTab;
    VideoHistoryTab *historyTab;
};

#endif // VIDEOGEN_H
