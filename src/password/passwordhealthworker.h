#pragma once

#include "passwordhealth.h"

#include <QByteArray>
#include <QObject>
#include <QString>

#include <atomic>

class PasswordHealthWorker final : public QObject
{
    Q_OBJECT

public:
    explicit PasswordHealthWorker(QString dbPath,
                                  QByteArray masterKey,
                                  bool enablePwnedCheck = false,
                                  bool allowNetwork = true,
                                  QObject *parent = nullptr);
    ~PasswordHealthWorker() override;

    void requestCancel();

signals:
    void progressRangeChanged(int min, int max);
    void progressValueChanged(int value);
    void finished(const QVector<PasswordHealthItem> &items);
    void failed(const QString &error);

public slots:
    void run();

private:
    QString dbPath_;
    QByteArray masterKey_;
    bool enablePwnedCheck_ = false;
    bool allowNetwork_ = true;
    std::atomic_bool cancelRequested_{false};
};
