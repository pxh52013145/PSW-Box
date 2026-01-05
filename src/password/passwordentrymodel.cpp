#include "passwordentrymodel.h"

#include "passworddatabase.h"
#include "passwordfaviconservice.h"

#include <QSqlQuery>
#include <QUrl>

PasswordEntryModel::PasswordEntryModel(QObject *parent) : QAbstractTableModel(parent)
{
    reload();
}

void PasswordEntryModel::setFaviconService(PasswordFaviconService *service)
{
    if (faviconService_ == service)
        return;

    if (faviconService_)
        disconnect(faviconService_, nullptr, this, nullptr);

    faviconService_ = service;
    if (!faviconService_)
        return;

    connect(faviconService_, &PasswordFaviconService::iconUpdated, this, [this](const QString &host) {
        if (items_.isEmpty())
            return;

        auto hostMatchesRow = [&](int row) {
            if (row < 0 || row >= items_.size())
                return false;

            const auto urlText = items_.at(row).url.trimmed();
            if (urlText.isEmpty())
                return false;

            QUrl u(urlText);
            if (u.scheme().isEmpty() && urlText.contains('.') && !urlText.contains("://"))
                u = QUrl(QString("https://%1").arg(urlText));
            if (u.scheme() != "http" && u.scheme() != "https")
                return false;
            return u.host().trimmed().compare(host, Qt::CaseInsensitive) == 0;
        };

        int first = -1;
        int last = -1;
        for (int i = 0; i < items_.size(); ++i) {
            if (!hostMatchesRow(i))
                continue;
            if (first < 0)
                first = i;
            last = i;
        }

        if (first < 0)
            return;

        emit dataChanged(index(first, 0), index(last, 0), {Qt::DecorationRole});
    });
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
    return 6;
}

QVariant PasswordEntryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= items_.size())
        return {};

    const auto &item = items_[index.row()];
    if (role == GroupIdRole)
        return item.groupId;

    if (role == TagsRole)
        return item.tags;

    if (role == Qt::DecorationRole && index.column() == 0 && faviconService_) {
        const auto icon = faviconService_->iconForUrl(item.url);
        if (!icon.isNull())
            return icon;
    }

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
            return item.tags.join(", ");
        case 5:
            return item.updatedAt.toString("MM-dd HH:mm");
        default:
            return {};
        }
    }

    if (role == Qt::ToolTipRole) {
        return QString("标题：%1\n账号：%2\n网址：%3\n分类：%4\n标签：%5")
            .arg(item.title,
                 item.username,
                 item.url,
                 item.category.isEmpty() ? "未分类" : item.category,
                 item.tags.isEmpty() ? "-" : item.tags.join(", "));
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
        return "标签";
    case 5:
        return "更新";
    default:
        return {};
    }
}

void PasswordEntryModel::reload()
{
    beginResetModel();
    items_.clear();

    auto database = PasswordDatabase::db();
    if (!database.isOpen()) {
        endResetModel();
        return;
    }

    QSqlQuery query(database);
    query.prepare(R"sql(
        SELECT
            e.id,
            e.group_id,
            e.title,
            e.username,
            e.url,
            e.category,
            e.created_at,
            e.updated_at,
            GROUP_CONCAT(t.name, ',') AS tags_csv
        FROM password_entries e
        LEFT JOIN entry_tags et ON et.entry_id = e.id
        LEFT JOIN tags t ON t.id = et.tag_id
        GROUP BY e.id
        ORDER BY e.updated_at DESC
    )sql");

    if (query.exec()) {
        while (query.next()) {
            PasswordEntry entry;
            entry.id = query.value(0).toLongLong();
            entry.groupId = query.value(1).toLongLong();
            entry.title = query.value(2).toString();
            entry.username = query.value(3).toString();
            entry.url = query.value(4).toString();
            entry.category = query.value(5).toString();
            entry.createdAt = QDateTime::fromSecsSinceEpoch(query.value(6).toLongLong());
            entry.updatedAt = QDateTime::fromSecsSinceEpoch(query.value(7).toLongLong());
            const auto tagsCsv = query.value(8).toString();
            if (!tagsCsv.trimmed().isEmpty()) {
                for (const auto &tag : tagsCsv.split(',', Qt::SkipEmptyParts))
                    entry.tags.push_back(tag.trimmed());
            }
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
