#pragma once

#include "password/passwordentry.h"

#include <QDialog>
#include <QStringList>

class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QProgressBar;
class QToolButton;

class PasswordEntryDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit PasswordEntryDialog(const QStringList &categories, const QStringList &availableTags, QWidget *parent = nullptr);

    void setEntry(const PasswordEntrySecrets &secrets);
    PasswordEntrySecrets entry() const;

private:
    void setupUi(const QStringList &categories, const QStringList &availableTags);
    void updateOkButtonState();
    void updateStrengthIndicator();
    void togglePasswordVisibility();
    void generatePassword();
    void addTagFromEdit();
    QStringList selectedTags() const;

    QLineEdit *titleEdit_ = nullptr;
    QLineEdit *usernameEdit_ = nullptr;
    QLineEdit *passwordEdit_ = nullptr;
    QToolButton *passwordToggleBtn_ = nullptr;
    QToolButton *passwordGenBtn_ = nullptr;
    QProgressBar *strengthBar_ = nullptr;
    QLabel *strengthLabel_ = nullptr;
    QLineEdit *urlEdit_ = nullptr;
    QComboBox *categoryCombo_ = nullptr;
    QListWidget *tagsList_ = nullptr;
    QLineEdit *newTagEdit_ = nullptr;
    QToolButton *tagAddBtn_ = nullptr;
    QPlainTextEdit *notesEdit_ = nullptr;
    QDialogButtonBox *buttonBox_ = nullptr;

    PasswordEntrySecrets data_;
    bool passwordVisible_ = false;
};
