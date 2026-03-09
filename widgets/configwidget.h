#ifndef CONFIGWIDGET_H
#define CONFIGWIDGET_H

#include <QWidget>
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

private slots:
    void addApiKey();
    void editApiKey();
    void deleteApiKey();
    void viewApiKey();
    void refreshTable();

private:
    void setupUI();
    void loadApiKeys();
    void updateColumnWidths();
    void updateRowHeight();
    double calculateScaleFactor();
    QString maskApiKey(const QString& apiKey);

    QTableWidget *apiKeyTable;
    QPushButton *addButton;
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

#endif // CONFIGWIDGET_H
