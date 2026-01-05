#pragma once

#include "passwordentry.h"

#include <QAbstractTableModel>
#include <QVector>

class PasswordEntryModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Roles
    {
        GroupIdRole = Qt::UserRole + 1,
        TagsRole,
    };

    explicit PasswordEntryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setFaviconService(class PasswordFaviconService *service);

    void reload();
    PasswordEntry itemAt(int row) const;

private:
    QVector<PasswordEntry> items_;
    class PasswordFaviconService *faviconService_ = nullptr;
};
