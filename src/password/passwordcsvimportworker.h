#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

#include <atomic>

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

    void requestCancel();

signals:
    void progressRangeChanged(int min, int max);
    void progressValueChanged(int value);
    void finished(int imported, int skippedDuplicates, int skippedInvalid, const QStringList &warnings);
    void failed(const QString &error);

public slots:
    void run();

private:
    QString csvPath_;
    QString dbPath_;
    QByteArray masterKey_;
    qint64 defaultGroupId_ = 1;
    std::atomic_bool cancelRequested_{false};
};

