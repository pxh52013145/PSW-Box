#pragma once

#include "password/passwordgenerator.h"

#include <QDialog>

class QCheckBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;

class PasswordGeneratorDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit PasswordGeneratorDialog(QWidget *parent = nullptr);

    QString password() const;

private:
    void setupUi();
    void wireSignals();
    void regenerate();
    PasswordGeneratorOptions optionsFromUi() const;

    QSpinBox *lengthSpin_ = nullptr;

    QCheckBox *upperCheck_ = nullptr;
    QCheckBox *lowerCheck_ = nullptr;
    QCheckBox *digitCheck_ = nullptr;
    QCheckBox *symbolCheck_ = nullptr;
    QCheckBox *excludeAmbiguousCheck_ = nullptr;
    QCheckBox *requireEachCheck_ = nullptr;

    QLineEdit *previewEdit_ = nullptr;
    QPushButton *regenBtn_ = nullptr;
    QPushButton *copyBtn_ = nullptr;
    QLabel *errorLabel_ = nullptr;

    QProgressBar *strengthBar_ = nullptr;
    QLabel *strengthLabel_ = nullptr;

    QDialogButtonBox *buttonBox_ = nullptr;

    QString password_;
};

