#include "passwordgraph.h"

#include "passwordurl.h"

#include <QHash>

namespace PasswordGraph {

namespace {

QString normalizeKey(const QString &text)
{
    return text.trimmed().toLower();
}

QString serviceLabelForEntry(const PasswordEntry &entry)
{
    if (entry.type == PasswordEntryType::WebLogin) {
        const auto host = PasswordUrl::hostFromUrl(entry.url);
        if (!host.isEmpty())
            return host;
    }

    const auto title = entry.title.trimmed();
    if (!title.isEmpty())
        return title;

    const auto url = entry.url.trimmed();
    if (!url.isEmpty())
        return url;

    return "未命名";
}

QString usernameLabelForEntry(const PasswordEntry &entry)
{
    const auto user = entry.username.trimmed();
    if (!user.isEmpty())
        return user;
    return "<无账号>";
}

QString makeKey(const QStringList &parts)
{
    return parts.join('\n');
}

} // namespace

Graph build(const QVector<PasswordEntry> &entries)
{
    Graph graph;

    QHash<QString, int> typeNodeIdByType;
    QHash<QString, int> serviceNodeIdByKey;
    QHash<QString, int> accountNodeIdByKey;
    QHash<QString, int> edgeIndexByKey;

    const auto ensureNode = [&](auto &map, const QString &key, Node node) -> int {
        const auto it = map.constFind(key);
        if (it != map.constEnd())
            return it.value();
        node.id = graph.nodes.size();
        graph.nodes.push_back(node);
        map.insert(key, node.id);
        return node.id;
    };

    const auto addEdge = [&](int from, int to) {
        const auto key = makeKey({QString::number(from), QString::number(to)});
        const auto it = edgeIndexByKey.constFind(key);
        if (it != edgeIndexByKey.constEnd()) {
            graph.edges[it.value()].count++;
            return;
        }
        Edge e;
        e.from = from;
        e.to = to;
        e.count = 1;
        const auto idx = graph.edges.size();
        graph.edges.push_back(e);
        edgeIndexByKey.insert(key, idx);
    };

    for (const auto &entry : entries) {
        const auto typeInt = static_cast<int>(entry.type);

        const auto typeNodeKey = QString::number(typeInt);
        Node typeNode;
        typeNode.kind = NodeKind::Type;
        typeNode.entryType = entry.type;
        typeNode.label = passwordEntryTypeLabel(entry.type);
        const auto typeNodeId = ensureNode(typeNodeIdByType, typeNodeKey, typeNode);

        const auto serviceLabel = serviceLabelForEntry(entry);
        const auto serviceKey = normalizeKey(serviceLabel);
        const auto serviceNodeKey = makeKey({typeNodeKey, serviceKey});
        Node serviceNode;
        serviceNode.kind = NodeKind::Service;
        serviceNode.entryType = entry.type;
        serviceNode.label = serviceLabel;
        serviceNode.serviceKey = serviceKey;
        const auto serviceNodeId = ensureNode(serviceNodeIdByKey, serviceNodeKey, serviceNode);

        const auto usernameLabel = usernameLabelForEntry(entry);
        const auto userKey = normalizeKey(usernameLabel);
        const auto accountNodeKey = makeKey({typeNodeKey, serviceKey, userKey});
        Node accountNode;
        accountNode.kind = NodeKind::Account;
        accountNode.entryType = entry.type;
        accountNode.label = usernameLabel;
        accountNode.serviceKey = serviceKey;
        accountNode.username = usernameLabel;
        const auto accountNodeId = ensureNode(accountNodeIdByKey, accountNodeKey, accountNode);

        graph.nodes[typeNodeId].count++;
        graph.nodes[serviceNodeId].count++;
        graph.nodes[accountNodeId].count++;

        addEdge(typeNodeId, serviceNodeId);
        addEdge(serviceNodeId, accountNodeId);
    }

    return graph;
}

} // namespace PasswordGraph
