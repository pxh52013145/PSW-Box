#pragma once

#include "passwordhealth.h"

#include <QAbstractTableModel>
#include <QVector>

class PasswordHealthModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Columns
    {
        TitleColumn = 0,
        UsernameColumn,
        UrlColumn,
        IssuesColumn,
        ScoreColumn,
        UpdatedColumn,
        ColumnCount
    };

    explicit PasswordHealthModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setItems(const QVector<PasswordHealthItem> &items);
    PasswordHealthItem itemAt(int row) const;

private:
    QVector<PasswordHealthItem> items_;
};

