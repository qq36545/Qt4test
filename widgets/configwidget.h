#ifndef CONFIGWIDGET_H
#define CONFIGWIDGET_H

#include <QWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialog>
#include <QLineEdit>

class ConfigWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ConfigWidget(QWidget *parent = nullptr);

signals:
    void apiKeysChanged();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void addApiKey();
    void editApiKey();
    void deleteApiKey();
    void viewApiKey();
    void refreshTable();

    void addImgbbKey();
    void editImgbbKey();
    void deleteImgbbKey();
    void applyImgbbKey();
    void refreshImgbbTable();

private:
    void setupUI();
    void setupApiKeyTab(QWidget *tab);
    void setupImgbbTab(QWidget *tab);
    void setupTableStyle(QTableWidget *table);
    void loadApiKeys();
    void loadImgbbKeys();
    void updateColumnWidths();
    void updateRowHeight();
    void updateThemeStyles();
    double calculateScaleFactor();
    QString maskApiKey(const QString& apiKey);

    QTabWidget *tabWidget;
    QTableWidget *apiKeyTable;
    QTableWidget *imgbbKeyTable;
    QPushButton *addButton;
    QPushButton *addImgbbButton;
    QLabel *hint1Label;
    QLabel *hint2Label;
    QLabel *hint3Label;
    double scaleFactor;
};

// 添加/编辑密钥对话框
class ApiKeyDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ApiKeyDialog(QWidget *parent = nullptr, int keyId = -1);

    QString getName() const { return nameEdit->text(); }
    QString getApiKey() const { return apiKeyEdit->text(); }

private:
    void setupUI();
    void loadApiKey(int id);

    int keyId;
    QLineEdit *nameEdit;
    QLineEdit *apiKeyEdit;
    QPushButton *saveButton;
    QPushButton *cancelButton;
};

// 添加/编辑 imgbb 密钥对话框
class ImgbbKeyDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImgbbKeyDialog(QWidget *parent = nullptr, int keyId = -1);

    QString getName() const { return nameEdit->text(); }
    QString getApiKey() const { return apiKeyEdit->text(); }

private:
    void setupUI();
    void loadKey(int id);

    int keyId;
    QLineEdit *nameEdit;
    QLineEdit *apiKeyEdit;
};

#endif // CONFIGWIDGET_H
