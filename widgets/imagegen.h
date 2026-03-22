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
#include <QStackedWidget>
#include <QGridLayout>
#include <QEvent>

class ImageAPI;
struct GenerationHistory;

// Gemini 图片生成页
class GeminiImagePage : public QWidget
{
    Q_OBJECT

public:
    explicit GeminiImagePage(QWidget *parent = nullptr);

signals:
    void imageGeneratedSuccessfully();
    void apiKeySelectionChanged(const QString &apiKeyValue);

public slots:
    void refreshApiKeys();

private slots:
    void onVariantChanged(int index);
    void savePreferences();
    void uploadReferenceImage();
    void clearReferenceImage();
    void clearPrompt();
    void resetForm();
    void generateImage();
    void onApiImageGenerated(const QByteArray &imageBytes, const QString &mimeType);
    void onApiError(const QString &error);

private:
    void setupUI();
    void loadApiKeys();
    void updateImageSizeVisibility();
    void rebuildImageSizeOptions();
    void rebuildAspectRatioOptions();
    void restorePreferences();
    QString currentModelValue() const;
    QString currentVariantLabel() const;
    QString saveGeneratedImage(const QByteArray &imageBytes, const QString &mimeType, QString &error);
    bool isMultiImageMode() const;
    void updateThumbnailGrid();
    void removeReferenceImage(int index);
    void replaceReferenceImage(int index);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

    ImageAPI *imageApi;

    QComboBox *variantCombo;
    QComboBox *apiKeyCombo;
    QPushButton *addKeyButton;
    QComboBox *serverCombo;
    QWidget *imageSizeWidget;
    QLabel *imageSizeLabel;
    QComboBox *imageSizeCombo;
    QComboBox *aspectRatioCombo;
    QTextEdit *promptInput;
    QLabel *referenceImagePreviewLabel;
    QPushButton *uploadReferenceButton;
    QPushButton *clearReferenceButton;
    QLabel *resultPreviewLabel;
    QPushButton *generateButton;
    QPushButton *resetButton;

    QStringList referenceImagePaths;
    int currentHistoryId;
    QLabel *referenceCountLabel;
    QWidget *thumbnailContainer;
    QGridLayout *thumbnailLayout;
    QLabel *referenceHintLabel;
};

// 单个图片生成 Tab（路由容器）
class ImageSingleTab : public QWidget
{
    Q_OBJECT

public:
    explicit ImageSingleTab(QWidget *parent = nullptr);

signals:
    void imageGeneratedSuccessfully();
    void apiKeySelectionChanged(const QString &apiKeyValue);

public slots:
    void refreshApiKeys();

private slots:
    void onModelChanged(int index);

private:
    void setupUI();

    QComboBox *modelCombo;
    QStackedWidget *stack;
    GeminiImagePage *geminiPage;
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

public slots:
    void refreshHistory();
    void refreshApiKeys();

private slots:
    void onRowDoubleClicked(int row, int column);
    void openSelectedFolder();

private:
    void setupUI();
    void loadHistory();

    QTableWidget *historyTable;
    QPushButton *refreshButton;
    QPushButton *openFolderButton;
};

// 主图片生成 Widget
class ImageGenWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ImageGenWidget(QWidget *parent = nullptr);
    ImageSingleTab* getSingleTab() const { return singleTab; }

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
