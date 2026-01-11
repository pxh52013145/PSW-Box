#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSortFilterProxyModel;
class QTableView;
class QTimer;
class QToolButton;
class QTreeView;

class PasswordEntryModel;
class PasswordFaviconService;
class PasswordGroupModel;
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
    void refreshGroups();
    void updateUiState();

    void createVault();
    void unlockVault();
    void lockVault();
    void changeMasterPassword();
    void showHealthReport();
    void showWebAssistant();
    void showGraph();

    void addGroup();
    void renameSelectedGroup();
    void deleteSelectedGroup();

    void addEntry();
    void editSelectedEntry();
    void editEntryById(qint64 id);
    void deleteSelectedEntry();
    void moveSelectedEntryToGroup();
    void copySelectedUsername();
    void copySelectedPassword();

    void exportBackup();
    void importBackup();
    void exportCsv();
    void importCsv();

    void resetAutoLockTimer();
    qint64 selectedEntryId() const;
    qint64 selectedGroupId() const;

    QLabel *statusLabel_ = nullptr;
    QPushButton *createBtn_ = nullptr;
    QPushButton *unlockBtn_ = nullptr;
    QPushButton *lockBtn_ = nullptr;
    QPushButton *changePwdBtn_ = nullptr;
    QPushButton *importBtn_ = nullptr;
    QPushButton *exportBtn_ = nullptr;
    QPushButton *importCsvBtn_ = nullptr;
    QPushButton *exportCsvBtn_ = nullptr;
    QPushButton *healthBtn_ = nullptr;
    QPushButton *webAssistantBtn_ = nullptr;
    QPushButton *graphBtn_ = nullptr;

    QLineEdit *searchEdit_ = nullptr;
    QLineEdit *tagFilterEdit_ = nullptr;
    QComboBox *typeCombo_ = nullptr;
    QComboBox *categoryCombo_ = nullptr;
    QToolButton *groupAddBtn_ = nullptr;
    QToolButton *groupRenameBtn_ = nullptr;
    QToolButton *groupDeleteBtn_ = nullptr;
    QTreeView *groupView_ = nullptr;
    QTableView *tableView_ = nullptr;
    QLabel *hintLabel_ = nullptr;

    QPushButton *addBtn_ = nullptr;
    QPushButton *editBtn_ = nullptr;
    QPushButton *deleteBtn_ = nullptr;
    QPushButton *moveBtn_ = nullptr;
    QPushButton *copyUserBtn_ = nullptr;
    QPushButton *copyPwdBtn_ = nullptr;

    PasswordVault *vault_ = nullptr;
    PasswordRepository *repo_ = nullptr;
    PasswordEntryModel *model_ = nullptr;
    PasswordFaviconService *faviconService_ = nullptr;
    PasswordGroupModel *groupModel_ = nullptr;
    QSortFilterProxyModel *proxy_ = nullptr;

    QTimer *autoLockTimer_ = nullptr;
    QTimer *clipboardClearTimer_ = nullptr;
    QString lastClipboardSecret_;
};
