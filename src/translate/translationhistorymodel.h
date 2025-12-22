#pragma once

#include <QAbstractTableModel>
#include <QDateTime>
#include <QString>
#include <QVector>

struct TranslationHistoryItem final
{
    qint64 id = 0;
    QString sourceText;
    QString translatedText;
    QString sourceLang;
    QString targetLang;
    QString provider;
    QDateTime createdAt;
};

class TranslationHistoryModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit TranslationHistoryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void reload();
    void addEntry(const TranslationHistoryItem &item);
    TranslationHistoryItem itemAt(int row) const;

private:
    QVector<TranslationHistoryItem> items_;
};

