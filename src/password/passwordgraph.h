#pragma once

#include "passwordentry.h"

#include <QString>
#include <QVector>

namespace PasswordGraph {

enum class NodeKind : int
{
    Type = 0,
    Service = 1,
    Account = 2,
};

struct Node final
{
    int id = -1;
    NodeKind kind = NodeKind::Service;
    PasswordEntryType entryType = PasswordEntryType::WebLogin;
    QString label;
    QString serviceKey;
    QString username;
    int count = 0;
};

struct Edge final
{
    int from = -1;
    int to = -1;
    int count = 0;
};

struct Graph final
{
    QVector<Node> nodes;
    QVector<Edge> edges;
};

Graph build(const QVector<PasswordEntry> &entries);

} // namespace PasswordGraph
