#include "translationhistorymodel.h"

#include "translatedatabase.h"

#include <QSqlQuery>

TranslationHistoryModel::TranslationHistoryModel(QObject *parent) : QAbstractTableModel(parent)
{
    reload();
}

int TranslationHistoryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return items_.size();
}

int TranslationHistoryModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return 5;
}

QVariant TranslationHistoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= items_.size())
        return {};

    const auto &item = items_[index.row()];
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0:
            return item.createdAt.toString("MM-dd HH:mm");
        case 1:
            return item.sourceLang.isEmpty() ? "auto" : item.sourceLang;
        case 2:
            return item.targetLang.isEmpty() ? "zh-CN" : item.targetLang;
        case 3:
            return item.sourceText;
        case 4:
            return item.translatedText;
        default:
            return {};
        }
    }

    if (role == Qt::ToolTipRole) {
        return QString("%1\n%2").arg(item.sourceText, item.translatedText);
    }

    return {};
}

QVariant TranslationHistoryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};

    switch (section) {
    case 0:
        return "时间";
    case 1:
        return "源语言";
    case 2:
        return "目标语言";
    case 3:
        return "原文";
    case 4:
        return "译文";
    default:
        return {};
    }
}

void TranslationHistoryModel::reload()
{
    beginResetModel();
    items_.clear();

    auto database = TranslateDatabase::db();
    if (!database.isOpen()) {
        endResetModel();
        return;
    }

    QSqlQuery query(database);
    query.prepare(R"sql(
        SELECT id, source_text, translated_text, source_lang, target_lang, provider, created_at
        FROM translation_history
        ORDER BY created_at DESC
        LIMIT 200
    )sql");

    if (query.exec()) {
        while (query.next()) {
            TranslationHistoryItem item;
            item.id = query.value(0).toLongLong();
            item.sourceText = query.value(1).toString();
            item.translatedText = query.value(2).toString();
            item.sourceLang = query.value(3).toString();
            item.targetLang = query.value(4).toString();
            item.provider = query.value(5).toString();
            const auto ts = query.value(6).toLongLong();
            item.createdAt = QDateTime::fromSecsSinceEpoch(ts);
            items_.push_back(item);
        }
    }

    endResetModel();
}

void TranslationHistoryModel::addEntry(const TranslationHistoryItem &item)
{
    auto database = TranslateDatabase::db();
    if (!database.isOpen())
        return;

    QSqlQuery query(database);
    query.prepare(R"sql(
        INSERT INTO translation_history(source_text, translated_text, source_lang, target_lang, provider, created_at)
        VALUES(?, ?, ?, ?, ?, ?)
    )sql");
    query.addBindValue(item.sourceText);
    query.addBindValue(item.translatedText);
    query.addBindValue(item.sourceLang);
    query.addBindValue(item.targetLang);
    query.addBindValue(item.provider);
    query.addBindValue(item.createdAt.toSecsSinceEpoch());
    query.exec();

    beginInsertRows(QModelIndex(), 0, 0);
    items_.prepend(item);
    endInsertRows();
}

TranslationHistoryItem TranslationHistoryModel::itemAt(int row) const
{
    if (row < 0 || row >= items_.size())
        return {};
    return items_[row];
}
