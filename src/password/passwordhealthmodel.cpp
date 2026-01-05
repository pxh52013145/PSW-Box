#include "passwordhealthmodel.h"

#include <QBrush>
#include <QColor>

PasswordHealthModel::PasswordHealthModel(QObject *parent) : QAbstractTableModel(parent) {}

int PasswordHealthModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return items_.size();
}

int PasswordHealthModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return ColumnCount;
}

QVariant PasswordHealthModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= items_.size())
        return {};

    const auto &item = items_.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case TitleColumn:
            return item.title;
        case UsernameColumn:
            return item.username;
        case UrlColumn:
            return item.url;
        case IssuesColumn:
            return item.issuesText();
        case ScoreColumn:
            return item.strengthScore;
        case UpdatedColumn:
            return item.daysSinceUpdate > 0 ? QString("%1 天").arg(item.daysSinceUpdate) : QString("0 天");
        default:
            return {};
        }
    }

    if (role == Qt::ToolTipRole) {
        return QString("标题：%1\n账号：%2\n网址：%3\n分类：%4\n标签：%5\n问题：%6")
            .arg(item.title,
                 item.username,
                 item.url,
                 item.category.isEmpty() ? "未分类" : item.category,
                 item.tags.isEmpty() ? "-" : item.tags.join(", "),
                 item.issuesText().isEmpty() ? "-" : item.issuesText());
    }

    if (role == Qt::BackgroundRole) {
        if (item.corrupted)
            return QBrush(QColor("#ffe3e3"));
        if (item.pwned)
            return QBrush(QColor("#ffd6d6"));
        if (item.weak)
            return QBrush(QColor("#fff1d6"));
        if (item.reused)
            return QBrush(QColor("#fff7cc"));
        if (item.stale)
            return QBrush(QColor("#f1f1f1"));
    }

    return {};
}

QVariant PasswordHealthModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};

    switch (section) {
    case TitleColumn:
        return "标题";
    case UsernameColumn:
        return "账号";
    case UrlColumn:
        return "网址";
    case IssuesColumn:
        return "问题";
    case ScoreColumn:
        return "评分";
    case UpdatedColumn:
        return "距更新";
    default:
        return {};
    }
}

void PasswordHealthModel::setItems(const QVector<PasswordHealthItem> &items)
{
    beginResetModel();
    items_ = items;
    endResetModel();
}

PasswordHealthItem PasswordHealthModel::itemAt(int row) const
{
    if (row < 0 || row >= items_.size())
        return {};
    return items_.at(row);
}
