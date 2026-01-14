#include "passwordgroupmodel.h"

#include <QApplication>
#include <QStyle>

#include <algorithm>

struct PasswordGroupModel::Node final
{
    PasswordGroup group;
    Node *parent = nullptr;
    QVector<Node *> children;

    ~Node()
    {
        for (auto *child : children)
            delete child;
    }
};

PasswordGroupModel::PasswordGroupModel(QObject *parent) : QAbstractItemModel(parent)
{
    root_ = new Node();
    root_->group.id = 0;
    nodesById_.insert(0, root_);
}

PasswordGroupModel::~PasswordGroupModel()
{
    delete root_;
    root_ = nullptr;
}

QModelIndex PasswordGroupModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column != 0 || row < 0)
        return {};

    const auto *parentNode = nodeFromIndex(parent);
    if (!parentNode)
        return {};

    if (row >= parentNode->children.size())
        return {};

    auto *child = parentNode->children.at(row);
    return createIndex(row, column, child);
}

QModelIndex PasswordGroupModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return {};

    const auto *node = nodeFromIndex(child);
    if (!node || !node->parent || node->parent == root_)
        return {};

    const auto *parentNode = node->parent;
    if (!parentNode->parent)
        return {};

    const auto row = parentNode->parent->children.indexOf(const_cast<Node *>(parentNode));
    if (row < 0)
        return {};

    return createIndex(row, 0, const_cast<Node *>(parentNode));
}

int PasswordGroupModel::rowCount(const QModelIndex &parent) const
{
    const auto *node = nodeFromIndex(parent);
    if (!node)
        return 0;
    return node->children.size();
}

int PasswordGroupModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QVariant PasswordGroupModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};

    const auto *node = nodeFromIndex(index);
    if (!node)
        return {};

    if (role == Qt::DisplayRole)
        return node->group.name;

    if (role == Qt::DecorationRole)
        return QApplication::style()->standardIcon(QStyle::SP_DirIcon);

    if (role == GroupIdRole)
        return node->group.id;

    return {};
}

QVariant PasswordGroupModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};

    if (section == 0)
        return "分组";

    return {};
}

PasswordGroupModel::Node *PasswordGroupModel::nodeFromIndex(const QModelIndex &index) const
{
    if (!index.isValid())
        return root_;

    return static_cast<Node *>(index.internalPointer());
}

void PasswordGroupModel::setGroups(const QVector<PasswordGroup> &groups)
{
    beginResetModel();

    delete root_;
    root_ = new Node();
    root_->group.id = 0;

    nodesById_.clear();
    nodesById_.insert(0, root_);

    QVector<Node *> nodes;
    nodes.reserve(groups.size());
    for (const auto &group : groups) {
        auto *node = new Node();
        node->group = group;
        nodes.push_back(node);
        nodesById_.insert(group.id, node);
    }

    for (auto *node : nodes) {
        const auto parentId = node->group.parentId;
        auto *parentNode = nodesById_.value(parentId, root_);
        node->parent = parentNode;
        parentNode->children.push_back(node);
    }

    const auto sortTree = [&](auto &&self, Node *node) -> void {
        if (!node)
            return;

        std::sort(node->children.begin(), node->children.end(), [](const auto *a, const auto *b) {
            return a->group.name.toLower() < b->group.name.toLower();
        });

        for (auto *child : node->children)
            self(self, child);
    };

    sortTree(sortTree, root_);
    endResetModel();
}

qint64 PasswordGroupModel::groupIdForIndex(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;
    const auto *node = nodeFromIndex(index);
    if (!node)
        return 0;
    return node->group.id;
}

QModelIndex PasswordGroupModel::indexForGroupId(qint64 groupId) const
{
    const auto *node = nodesById_.value(groupId, nullptr);
    if (!node || node == root_ || !node->parent)
        return {};

    const auto row = node->parent->children.indexOf(const_cast<Node *>(node));
    if (row < 0)
        return {};

    return createIndex(row, 0, const_cast<Node *>(node));
}

void PasswordGroupModel::collectDescendants(const Node *node, QVector<qint64> &out) const
{
    if (!node)
        return;

    if (node != root_)
        out.push_back(node->group.id);

    for (const auto *child : node->children)
        collectDescendants(child, out);
}

QVector<qint64> PasswordGroupModel::descendantGroupIds(qint64 groupId) const
{
    QVector<qint64> out;
    const auto *node = nodesById_.value(groupId, nullptr);
    if (!node)
        return out;

    collectDescendants(node, out);
    return out;
}
