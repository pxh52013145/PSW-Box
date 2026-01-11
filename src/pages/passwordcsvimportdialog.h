#pragma once

#include "password/passwordcsvimportworker.h"

#include <QDialog>

class QLabel;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QStandardItemModel;
class QTableView;

class PasswordCsvImportDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit PasswordCsvImportDialog(qint64 baseGroupId, const QString &baseGroupName, QWidget *parent = nullptr);
    ~PasswordCsvImportDialog() override;

    QString csvPath() const;
    PasswordCsvImportOptions options() const;

private:
    void setupUi();
    void wireSignals();
    void browseCsv();
    void reloadPreview();
    void setPreviewError(const QString &error);
    void setOkEnabled(bool enabled);

    qint64 baseGroupId_ = 1;
    QString baseGroupName_;

    QLineEdit *pathEdit_ = nullptr;
    QPushButton *browseBtn_ = nullptr;

    QLabel *formatLabel_ = nullptr;
    QLabel *summaryLabel_ = nullptr;

    QCheckBox *createGroupsCheck_ = nullptr;
    QComboBox *dupPolicyCombo_ = nullptr;
    QComboBox *typeCombo_ = nullptr;

    QTableView *previewView_ = nullptr;
    QStandardItemModel *previewModel_ = nullptr;
};

