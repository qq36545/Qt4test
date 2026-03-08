#ifndef IMAGEGEN_H
#define IMAGEGEN_H

#include <QWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>

// 单个图片生成 Tab
class ImageSingleTab : public QWidget
{
    Q_OBJECT

public:
    explicit ImageSingleTab(QWidget *parent = nullptr);

private slots:
    void generateImage();

private:
    void setupUI();
    void loadApiKeys();

    QTextEdit *promptInput;
    QComboBox *apiKeyCombo;
    QPushButton *addKeyButton;
    QComboBox *resolutionCombo;
    QComboBox *aspectRatioCombo;
    QComboBox *styleCombo;
    QLabel *previewLabel;
    QPushButton *generateButton;
};

// 批量图片生成 Tab
class ImageBatchTab : public QWidget
{
    Q_OBJECT

public:
    explicit ImageBatchTab(QWidget *parent = nullptr);

private slots:
    void generateBatch();
    void importCSV();
    void deleteAll();

private:
    void setupUI();
    void loadApiKeys();

    QTextEdit *promptInput;
    QComboBox *apiKeyCombo;
    QPushButton *addKeyButton;
    QPushButton *importButton;
    QPushButton *deleteAllButton;
    QPushButton *generateButton;
};

// 历史记录 Tab
class ImageHistoryTab : public QWidget
{
    Q_OBJECT

public:
    explicit ImageHistoryTab(QWidget *parent = nullptr);

private slots:
    void refreshHistory();
    void onRowDoubleClicked(int row, int column);

private:
    void setupUI();
    void loadHistory();

    QTableWidget *historyTable;
    QPushButton *refreshButton;
};

// 主图片生成 Widget
class ImageGenWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ImageGenWidget(QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupUI();
    void updateTabWidths();

    QTabWidget *tabWidget;
    ImageSingleTab *singleTab;
    ImageBatchTab *batchTab;
    ImageHistoryTab *historyTab;
};

#endif // IMAGEGEN_H
