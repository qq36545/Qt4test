#ifndef IMAGEHISTORY_H
#define IMAGEHISTORY_H

#include <QWidget>
#include <QTableWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QScrollArea>
#include <QGridLayout>
#include <QSet>
#include <QShowEvent>
#include <QEvent>
#include <QResizeEvent>

// ImageTask 用于存储图片历史记录数据
struct ImageTask {
    int id;
    QString taskId;
    QString prompt;
    QString modelVariant;      // 模型变体：香蕉1/2/Pro
    QString status;           // generating/success/failed
    QString resultPath;        // 本地保存路径
    QString createdAt;        // 创建时间
    QString apiKeyName;        // API密钥名称
    QString serverUrl;        // 服务器URL
    QString imageSize;        // 尺寸：1K/2K/4K
    QString aspectRatio;      // 宽高比：1:1/16:9/9:16
    QString referenceCount;   // 参考图数量
    QString parameters;        // 完整参数JSON
};

// 图片历史记录单个记录Tab
class ImageSingleHistoryTab : public QWidget
{
    Q_OBJECT

public:
    explicit ImageSingleHistoryTab(QWidget *parent = nullptr);
    ~ImageSingleHistoryTab() = default;

signals:
    // 重新生成信号，携带完整参数
    void regenerateRequested(const QString& prompt,
                           const QString& modelVariant,
                           const QStringList& referencePaths,
                           int apiKeyId,
                           const QString& serverUrl,
                           const QString& imageSize,
                           const QString& aspectRatio);

public slots:
    void refreshHistory();

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void switchView();
    void onViewImage(const QString& taskId);
    void onBrowseFile(const QString& taskId);
    void onRegenerate(const QString& taskId);
    void onDeleteSelected();
    void onSelectAllChanged(int state);
    void onCheckBoxStateChanged();
    void onTimeFilterChanged(int index);

private:
    void setupUI();
    void setupListView();
    void setupThumbnailView();
    void loadHistory();
    void loadListViewData();
    void loadApiKeys();
    void updateHeaderCheckBoxPosition();
    void loadThumbnailCards();
    void absorbRightWhitespaceToActionColumn();

    // 视图切换
    bool isListView;
    QStackedWidget* viewStack;
    QWidget* listViewWidget;
    QWidget* thumbnailViewWidget;

    // 列表视图组件
    QTableWidget* historyTable;
    QCheckBox* headerCheckBox;

    // 缩略图视图组件
    QScrollArea* thumbnailScrollArea;
    QWidget* thumbnailContainer;
    QGridLayout* thumbnailLayout;

    // 工具栏组件
    QPushButton* switchViewButton;
    QPushButton* deleteButton;
    QComboBox* statusCombo;
    QComboBox* timeFilterCombo;

    // 选中状态
    QSet<int> selectedIds;

    // 时间筛选选项
    enum TimeFilter {
        All = 0,
        Today = 1,
        ThisWeek = 2,
        ThisMonth = 3
    };
    TimeFilter currentTimeFilter;
};

#endif // IMAGEHISTORY_H
