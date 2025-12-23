#pragma once

#include "password/passwordentry.h"

#include <QDialog>
#include <QStringList>

class QComboBox;
class QDialogButtonBox;
class QLineEdit;
class QPlainTextEdit;
class QToolButton;

class PasswordEntryDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit PasswordEntryDialog(const QStringList &categories, QWidget *parent = nullptr);

    void setEntry(const PasswordEntrySecrets &secrets);
    PasswordEntrySecrets entry() const;

private:
    void setupUi(const QStringList &categories);
    void updateOkButtonState();
    void togglePasswordVisibility();
    void generatePassword();

    QLineEdit *titleEdit_ = nullptr;
    QLineEdit *usernameEdit_ = nullptr;
    QLineEdit *passwordEdit_ = nullptr;
    QToolButton *passwordToggleBtn_ = nullptr;
    QToolButton *passwordGenBtn_ = nullptr;
    QLineEdit *urlEdit_ = nullptr;
    QComboBox *categoryCombo_ = nullptr;
    QPlainTextEdit *notesEdit_ = nullptr;
    QDialogButtonBox *buttonBox_ = nullptr;

    PasswordEntrySecrets data_;
    bool passwordVisible_ = false;
};

