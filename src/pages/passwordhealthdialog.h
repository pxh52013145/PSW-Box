#pragma once

#include <QByteArray>
#include <QDialog>
#include <QString>

class QCheckBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QSortFilterProxyModel;
class QTableView;
class QThread;

class PasswordHealthModel;
class PasswordHealthWorker;

class PasswordHealthDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit PasswordHealthDialog(const QString &dbPath, const QByteArray &masterKey, QWidget *parent = nullptr);
    ~PasswordHealthDialog() override;

signals:
    void entryActivated(qint64 entryId);

private:
    void setupUi();
    void wireSignals();
    void startScan();
    void cancelScan();
    void updateUiState();

    QString dbPath_;
    QByteArray masterKey_;

    QLabel *statusLabel_ = nullptr;
    QLineEdit *searchEdit_ = nullptr;
    QCheckBox *onlyIssuesCheck_ = nullptr;
    QCheckBox *pwnedCheck_ = nullptr;
    QPushButton *scanBtn_ = nullptr;
    QPushButton *cancelBtn_ = nullptr;
    QProgressBar *progressBar_ = nullptr;
    QTableView *tableView_ = nullptr;

    PasswordHealthModel *model_ = nullptr;
    QSortFilterProxyModel *proxy_ = nullptr;

    QThread *thread_ = nullptr;
    PasswordHealthWorker *worker_ = nullptr;
    bool running_ = false;
    bool pwnedRequested_ = false;
};
