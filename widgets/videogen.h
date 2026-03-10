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
#include <QStackedWidget>
#include <QGridLayout>
#include <QShowEvent>

// 单个视频生成 Tab
class VideoSingleTab : public QWidget
{
    Q_OBJECT

public:
    explicit VideoSingleTab(QWidget *parent = nullptr);

public slots:
    void refreshApiKeys();

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
    void connectSignals();
    void loadApiKeys();
    void updateResolutionOptions(bool is4K);
    void updateImageUploadUI(const QString &modelName);
    void updateImagePreview();
    void onVideoCreated(const QString &taskId, const QString &status);
    void onTaskStatusUpdated(const QString &taskId, const QString &status, const QString &videoUrl, int progress);
    void onApiError(const QString &error);
    void saveSettings();
    void loadSettings();
    QString calculateParamsHash() const;
    bool checkDuplicateSubmission();
    void onAnyParameterChanged();
    QString copyImagesToPersistentStorage(const QString &taskId);

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
    QString lastSubmittedParamsHash;  // 上次提交的参数哈希
    bool suppressDuplicateWarning;    // 本次会话是否抑制重复提交警告
    bool parametersModified;          // 参数是否被修改过
};

// 批量视频生成 Tab
class VideoBatchTab : public QWidget
{
    Q_OBJECT

public:
    explicit VideoBatchTab(QWidget *parent = nullptr);

public slots:
    void refreshApiKeys();

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

// 单个视频历史记录 Tab
class VideoSingleHistoryTab : public QWidget
{
    Q_OBJECT

public:
    explicit VideoSingleHistoryTab(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;  // tab显示时自动刷新

private slots:
    void refreshHistory();
    void switchView();  // 切换列表/缩略图视图
    void onViewVideo(const QString& taskId);
    void onBrowseFile(const QString& taskId);
    void onRetryQuery(const QString& taskId);
    void onRegenerate(const QString& taskId);
    void showContextMenu(const QPoint &pos);  // 显示右键菜单
    void onDeleteSelected();  // 删除选中的记录
    void onSelectAllChanged(int state);  // 全选/取消全选
    void onCheckBoxStateChanged();  // 单个勾选框状态变化
    void onTaskStatusUpdated(const QString& taskId, const QString& status, int progress);  // 任务状态更新

private:
    void setupUI();
    void loadHistory(int offset = 0, int limit = 50);
    void setupListView();
    void setupThumbnailView();
    void updateHeaderCheckBoxPosition();  // 更新表头勾选框位置

    QStackedWidget* viewStack;
    QWidget* listViewWidget;
    QWidget* thumbnailViewWidget;
    QTableWidget* historyTable;
    QScrollArea* thumbnailScrollArea;
    QWidget* thumbnailContainer;
    QGridLayout* thumbnailLayout;
    QPushButton* switchViewButton;
    QPushButton* refreshButton;
    QPushButton* deleteButton;  // 删除按钮
    QCheckBox* headerCheckBox;  // 表头全选勾选框
    QSet<QString> selectedTaskIds;  // 存储选中的任务ID
    bool isListView;
    int currentOffset;
};

// 历史记录容器 Widget (4 tab)
class VideoHistoryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoHistoryWidget(QWidget *parent = nullptr);

private:
    void setupUI();

    QTabWidget* tabWidget;
    VideoSingleHistoryTab* videoSingleTab;
    // VideoBatchHistoryTab* videoBatchTab;  // 后续实现
    // ImageSingleHistoryTab* imageSingleTab;  // 后续实现
    // ImageBatchHistoryTab* imageBatchTab;  // 后续实现
};

// 旧的 VideoHistoryTab 保留作为兼容（后续删除）
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
    VideoSingleTab* getSingleTab() const { return singleTab; }
    VideoBatchTab* getBatchTab() const { return batchTab; }

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupUI();
    void updateTabWidths();

    QTabWidget *tabWidget;
    VideoSingleTab *singleTab;
    VideoBatchTab *batchTab;
    VideoHistoryWidget *historyWidget;  // 改为 VideoHistoryWidget
};

#endif // VIDEOGEN_H
