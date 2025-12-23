#include "passwordentrymodel.h"

#include "core/database.h"

#include <QSqlQuery>

PasswordEntryModel::PasswordEntryModel(QObject *parent) : QAbstractTableModel(parent)
{
    reload();
}

int PasswordEntryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return items_.size();
}

int PasswordEntryModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return 5;
}

QVariant PasswordEntryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= items_.size())
        return {};

    const auto &item = items_[index.row()];
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0:
            return item.title;
        case 1:
            return item.username;
        case 2:
            return item.url;
        case 3:
            return item.category.isEmpty() ? "未分类" : item.category;
        case 4:
            return item.updatedAt.toString("MM-dd HH:mm");
        default:
            return {};
        }
    }

    if (role == Qt::ToolTipRole) {
        return QString("标题：%1\n账号：%2\n网址：%3\n分类：%4")
            .arg(item.title, item.username, item.url, item.category.isEmpty() ? "未分类" : item.category);
    }

    return {};
}

QVariant PasswordEntryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};

    switch (section) {
    case 0:
        return "标题";
    case 1:
        return "账号";
    case 2:
        return "网址";
    case 3:
        return "分类";
    case 4:
        return "更新";
    default:
        return {};
    }
}

void PasswordEntryModel::reload()
{
    beginResetModel();
    items_.clear();

    auto database = Database::db();
    if (!database.isOpen()) {
        endResetModel();
        return;
    }

    QSqlQuery query(database);
    query.prepare(R"sql(
        SELECT id, title, username, url, category, created_at, updated_at
        FROM password_entries
        ORDER BY updated_at DESC
    )sql");

    if (query.exec()) {
        while (query.next()) {
            PasswordEntry entry;
            entry.id = query.value(0).toLongLong();
            entry.title = query.value(1).toString();
            entry.username = query.value(2).toString();
            entry.url = query.value(3).toString();
            entry.category = query.value(4).toString();
            entry.createdAt = QDateTime::fromSecsSinceEpoch(query.value(5).toLongLong());
            entry.updatedAt = QDateTime::fromSecsSinceEpoch(query.value(6).toLongLong());
            items_.push_back(entry);
        }
    }

    endResetModel();
}

PasswordEntry PasswordEntryModel::itemAt(int row) const
{
    if (row < 0 || row >= items_.size())
        return {};
    return items_[row];
}

