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
#include <QTimer>

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
    void saveCurrentModelPreferences();  // 保存当前模型的参数
    void saveDraftOnClose();            // 关闭时保存草稿

public:
    void restoreDraft();                // 恢复草稿

private slots:
    void onVariantChanged(int index);
    void savePreferences(int modelIndex = -1);
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
    void restoreLastModelVariant();      // 恢复上次选择的模型
    QString currentModelValue() const;
    QString currentVariantLabel() const;
    QString saveGeneratedImage(const QByteArray &imageBytes, const QString &mimeType, QString &error);
    bool isMultiImageMode() const;
    int maxReferenceImages() const;
    void updateThumbnailGrid();
    void removeReferenceImage(int index);
    void replaceReferenceImage(int index);
    void saveDraft();                  // 保存草稿

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void hideEvent(QHideEvent *event) override;

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
    bool m_initializing = true;  // 初始化标志，避免首次 setupUI 时覆盖已保存的参数
    int m_previousVariantIndex = 0;  // 追踪切换前的模型索引
    bool m_imageSizeVisible = false;  // 追踪清晰度选择器的可见性状态（同步更新，避免 setVisible 事件延迟）

    // 草稿自动保存定时器
    QTimer *m_comboSaveTimer;      // 下拉框延迟1秒保存
    QTimer *m_promptSaveTimer;     // 提示词3秒空闲保存
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
    void regenerateRequested(const QString& prompt,
                         const QString& modelVariant,
                         const QStringList& referencePaths,
                         const QString& apiKeyName,
                         const QString& serverUrl,
                         const QString& imageSize,
                         const QString& aspectRatio);

public slots:
    void refreshApiKeys();
    void saveCurrentModelPreferences();
    void saveDraftOnClose();  // 关闭时保存草稿
    void onRegenerateRequest(const QString& prompt,
                            const QString& modelVariant,
                            const QStringList& referencePaths,
                            const QString& apiKeyName,
                            const QString& serverUrl,
                            const QString& imageSize,
                            const QString& aspectRatio);

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

// 前向声明
class ImageSingleHistoryTab;

// 历史记录 Tab (容器，支持双Tab)
class ImageHistoryTab : public QWidget
{
    Q_OBJECT

public:
    explicit ImageHistoryTab(QWidget *parent = nullptr);

public slots:
    void refreshHistory();
    void refreshApiKeys();

signals:
    void regenerateRequested(const QString& prompt,
                           const QString& modelVariant,
                           const QStringList& referencePaths,
                           const QString& apiKeyName,
                           const QString& serverUrl,
                           const QString& imageSize,
                           const QString& aspectRatio);

private:
    void setupUI();

    QTabWidget* historyTabWidget;
    ImageSingleHistoryTab* imageSingleTab;
    QWidget* imageBatchPlaceholder;  // "待开发"占位符
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
