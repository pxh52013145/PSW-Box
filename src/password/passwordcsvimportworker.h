#pragma once

#include "passwordentry.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

#include <atomic>

enum class PasswordCsvDuplicatePolicy : int
{
    Skip = 0,
    Update = 1,
    ImportAnyway = 2,
};

struct PasswordCsvImportOptions final
{
    PasswordCsvDuplicatePolicy duplicatePolicy = PasswordCsvDuplicatePolicy::Skip;
    bool createGroupsFromCategoryPath = false;
    PasswordEntryType defaultEntryType = PasswordEntryType::WebLogin;
};

class PasswordCsvImportWorker final : public QObject
{
    Q_OBJECT

public:
    explicit PasswordCsvImportWorker(QString csvPath,
                                     QString dbPath,
                                     QByteArray masterKey,
                                     qint64 defaultGroupId,
                                     QObject *parent = nullptr);
    ~PasswordCsvImportWorker() override;

    void setOptions(PasswordCsvImportOptions options);
    void requestCancel();

signals:
    void progressRangeChanged(int min, int max);
    void progressValueChanged(int value);
    void finished(int inserted, int updated, int skippedDuplicates, int skippedInvalid, const QStringList &warnings);
    void failed(const QString &error);

public slots:
    void run();

private:
    QString csvPath_;
    QString dbPath_;
    QByteArray masterKey_;
    qint64 defaultGroupId_ = 1;
    PasswordCsvImportOptions options_;
    std::atomic_bool cancelRequested_{false};
};
