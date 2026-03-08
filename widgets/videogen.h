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

// 单个视频生成 Tab
class VideoSingleTab : public QWidget
{
    Q_OBJECT

public:
    explicit VideoSingleTab(QWidget *parent = nullptr);

private slots:
    void generateVideo();
    void onModelChanged(int index);

private:
    void setupUI();
    void loadApiKeys();

    QTextEdit *promptInput;
    QComboBox *modelCombo;
    QComboBox *apiKeyCombo;
    QPushButton *addKeyButton;
    QComboBox *resolutionCombo;
    QComboBox *durationCombo;
    QComboBox *styleCombo;
    QLabel *previewLabel;
    QPushButton *generateButton;
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

private:
    void setupUI();
    void loadApiKeys();

    QTextEdit *promptInput;
    QLabel *imageDropArea;
    QListWidget *imageList;
    QComboBox *modelCombo;
    QComboBox *apiKeyCombo;
    QPushButton *addKeyButton;
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
