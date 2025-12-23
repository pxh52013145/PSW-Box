#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSortFilterProxyModel;
class QTableView;
class QTimer;

class PasswordEntryModel;
class PasswordRepository;
class PasswordVault;

class PasswordManagerPage final : public QWidget
{
    Q_OBJECT

public:
    explicit PasswordManagerPage(QWidget *parent = nullptr);
    ~PasswordManagerPage() override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupUi();
    void wireSignals();
    void refreshAll();
    void refreshCategories();
    void updateUiState();

    void createVault();
    void unlockVault();
    void lockVault();
    void changeMasterPassword();

    void addEntry();
    void editSelectedEntry();
    void deleteSelectedEntry();
    void copySelectedUsername();
    void copySelectedPassword();

    void exportBackup();
    void importBackup();

    void resetAutoLockTimer();
    qint64 selectedEntryId() const;

    QLabel *statusLabel_ = nullptr;
    QPushButton *createBtn_ = nullptr;
    QPushButton *unlockBtn_ = nullptr;
    QPushButton *lockBtn_ = nullptr;
    QPushButton *changePwdBtn_ = nullptr;
    QPushButton *importBtn_ = nullptr;
    QPushButton *exportBtn_ = nullptr;

    QLineEdit *searchEdit_ = nullptr;
    QComboBox *categoryCombo_ = nullptr;
    QTableView *tableView_ = nullptr;
    QLabel *hintLabel_ = nullptr;

    QPushButton *addBtn_ = nullptr;
    QPushButton *editBtn_ = nullptr;
    QPushButton *deleteBtn_ = nullptr;
    QPushButton *copyUserBtn_ = nullptr;
    QPushButton *copyPwdBtn_ = nullptr;

    PasswordVault *vault_ = nullptr;
    PasswordRepository *repo_ = nullptr;
    PasswordEntryModel *model_ = nullptr;
    QSortFilterProxyModel *proxy_ = nullptr;

    QTimer *autoLockTimer_ = nullptr;
    QTimer *clipboardClearTimer_ = nullptr;
    QString lastClipboardSecret_;
};

