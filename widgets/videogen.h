#ifndef VIDEOGEN_H
#define VIDEOGEN_H

#include <QWidget>
#include <QTabWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QTableWidget>
#include <QListWidget>
#include <QCheckBox>
#include <QScrollArea>
#include <QSpinBox>
#include <QGridLayout>
#include <QShowEvent>
#include <QTimer>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QVariantMap>
#include <QList>
#include <QDialog>

// 前向声明
struct VideoTask;
class VideoAPI;
class VeoGenPage;
class GrokGenPage;
class WanGenPage;
class Sora2GenPage;

// 单个视频生成 Tab（调度器）
class VideoSingleTab : public QWidget
{
    Q_OBJECT

public:
    explicit VideoSingleTab(QWidget *parent = nullptr);
    void loadFromTask(const VideoTask& task);  // 从历史任务加载参数

signals:
    void apiKeySelectionChanged(const QString& apiKeyValue);

public slots:
    void refreshApiKeys();

private slots:
    void onModelChanged(int index);
    void onVeoSubmitSucceeded(const QString& modelName);
    void onVeoSubmitFailed(const QString& modelName, const QString& error);
    void onGrokSubmitSucceeded(const QString& modelName);
    void onGrokSubmitFailed(const QString& modelName, const QString& error);
    void onWanSubmitSucceeded(const QString& modelName);
    void onWanSubmitFailed(const QString& modelName, const QString& error);
    void onSora2CreateTaskRequested(const QVariantMap& payload);
    void onSora2VideoCreated(const QString& taskId, const QString& status);
    void onSora2ImageUploadProgress(int current, int total);
    void onVeoImageUploadProgress(int current, int total);
    void onGrokImageUploadProgress(int current, int total);
    void onWanImageUploadProgress(int current, int total);
    void onSora2ApiError(const QString& error);

private:
    struct SubmitDialogTheme {
        bool isDark = false;
        QString textColor;
        QString bgColor;
        QString borderColor;
        QString accentBgColor;
        QString accentTextColor;
        QString buttonBgColor;
        QString buttonHoverColor;
        QString buttonTextColor;
    };

    struct Sora2PendingContext {
        QString tempTaskId;
        QString apiKey;
        QString baseUrl;
    };

    void setupUI();
    void setSora2Submitting(bool submitting);
    SubmitDialogTheme resolveSubmitDialogTheme() const;
    void showGeneralSubmittingDialog(const QString& message = QStringLiteral("正在提交..."));
    void updateGeneralSubmittingDialog(int current, int total);
    void closeGeneralSubmittingDialog();
    void showSubmitSuccessToast();
    void showUnifiedSubmitResultDialog(bool success,
                                       const QString& title,
                                       const QString& message,
                                       const QString& detail = QString());

    QStackedWidget *stack;
    VeoGenPage *veoPage;
    GrokGenPage *grokPage;
    WanGenPage *wanPage;
    Sora2GenPage *sora2Page;
    QComboBox *modelCombo;
    VideoAPI *sora2Api;
    QDialog *generalSubmittingDialog = nullptr;
    QLabel *generalSubmittingLabel = nullptr;
    QList<Sora2PendingContext> sora2PendingTasks;
    bool sora2Submitting = false;
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

private:
    void setupUI();
    void loadApiKeys();

    QComboBox *apiKeyCombo;
    QPushButton *addKeyButton;
    QComboBox *serverCombo;
    QPushButton *generateButton;
};

// 单个视频历史记录 Tab
class VideoSingleHistoryTab : public QWidget
{
    Q_OBJECT

public:
    explicit VideoSingleHistoryTab(QWidget *parent = nullptr);

public slots:
    void refreshApiKeys();

protected:
    void showEvent(QShowEvent *event) override;  // tab显示时自动刷新
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void refreshHistory();
    void switchView();  // 切换列表/缩略图视图
    void onViewVideo(const QString& taskId);
    void onBrowseFile(const QString& taskId);
    void onRetryDownload(const QString& taskId);
    void onRetryQuery(const QString& taskId);
    void onRegenerate(const QString& taskId);
    void showContextMenu(const QPoint &pos);  // 显示右键菜单
    void onFixTaskId(const QString& taskId);  // 修复任务ID
    void onDeleteSelected();  // 删除选中的记录
    void onSelectAllChanged(int state);  // 全选/取消全选
    void onCheckBoxStateChanged();  // 单个勾选框状态变化
    void onTaskStatusUpdated(const QString& taskId, const QString& status, int progress);  // 任务状态更新（TaskPollManager）
    void onTaskTimeout(const QString& taskId);  // 任务超时处理
    void onTaskFailed(const QString& taskId, const QString& error);  // 任务失败处理
    void onApiTaskStatusUpdated(const QString& taskId, const QString& status, const QString& videoUrl, int progress);  // 任务状态更新（Veo3API）
    void onQueryError(const QString& error);  // 查询错误处理

private:
    void setupUI();
    void loadHistory(int offset = 0, int limit = 50);
    void setupListView();
    void setupThumbnailView();
    void updateHeaderCheckBoxPosition();  // 更新表头勾选框位置
    void loadApiKeys();
    void applySyncedQueryApiKeyFromSettings();

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
    QComboBox* apiKeyCombo;  // 密钥选择下拉列表
    QComboBox* serverCombo;  // 服务器选择下拉列表
    QSet<QString> selectedTaskIds;  // 存储选中的任务ID
    bool isListView;
    int currentOffset;
    class VideoAPI* veo3API;  // API实例，用于重新查询任务状态
    QString currentRefreshingTaskId;  // 当前正在刷新的任务ID
    QTimer* tooltipHideTimer;  // 状态tooltip 3秒自动消失计时器
    bool hasShownRecoveryPrompt;  // 是否已显示过恢复提示
    QSet<QString> notifiedTaskFailures;  // 已提示失败/超时的任务ID（防重复）
};

// 历史记录容器 Widget (4 tab)
class VideoHistoryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoHistoryWidget(QWidget *parent = nullptr);
    VideoSingleHistoryTab* getVideoSingleTab() const { return videoSingleTab; }

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
    VideoHistoryWidget* getHistoryWidget() const { return historyWidget; }

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
