#include "passwordgraphdialog.h"

#include "password/passwordgraph.h"
#include "password/passwordrepository.h"
#include "password/passwordvault.h"

#include <QDialogButtonBox>
#include <QGraphicsEllipseItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {

QColor baseColorForType(PasswordEntryType type)
{
    switch (type) {
    case PasswordEntryType::WebLogin:
        return QColor("#4F86F7");
    case PasswordEntryType::DesktopClient:
        return QColor("#7B68EE");
    case PasswordEntryType::ApiKeyToken:
        return QColor("#17A2B8");
    case PasswordEntryType::DatabaseCredential:
        return QColor("#28A745");
    case PasswordEntryType::ServerSsh:
        return QColor("#F39C12");
    case PasswordEntryType::DeviceWifi:
        return QColor("#E74C3C");
    default:
        return QColor("#4F86F7");
    }
}

QString labelWithCount(const PasswordGraph::Node &node)
{
    if (node.count <= 1)
        return node.label;
    return QString("%1 (%2)").arg(node.label).arg(node.count);
}

class PasswordGraphNodeItem final : public QGraphicsEllipseItem
{
public:
    PasswordGraphNodeItem(PasswordGraph::Node node,
                          qreal radius,
                          const QColor &fill,
                          std::function<void(const PasswordGraph::Node &)> onClick,
                          QGraphicsItem *parent = nullptr)
        : QGraphicsEllipseItem(parent), node_(std::move(node)), onClick_(std::move(onClick))
    {
        setRect(-radius, -radius, radius * 2, radius * 2);
        setBrush(fill);
        setPen(QPen(QColor("#222"), 1));
        setCursor(Qt::PointingHandCursor);
        setFlag(QGraphicsItem::ItemIsSelectable, true);

        auto *text = new QGraphicsSimpleTextItem(::labelWithCount(node_), this);
        text->setBrush(QBrush(QColor("#111")));
        text->setAcceptedMouseButtons(Qt::NoButton);
        const auto br = text->boundingRect();
        text->setPos(-br.width() / 2.0, -br.height() / 2.0);
    }

    const PasswordGraph::Node &node() const { return node_; }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override
    {
        if (onClick_)
            onClick_(node_);
        QGraphicsEllipseItem::mousePressEvent(event);
    }

private:
    PasswordGraph::Node node_;
    std::function<void(const PasswordGraph::Node &)> onClick_;
};

} // namespace

PasswordGraphDialog::PasswordGraphDialog(PasswordRepository *repo, PasswordVault *vault, QWidget *parent)
    : QDialog(parent), repo_(repo), vault_(vault)
{
    setupUi();
    rebuildGraph();
    fitViewToScene();
}

PasswordGraphDialog::~PasswordGraphDialog() = default;

void PasswordGraphDialog::setupUi()
{
    setWindowTitle("关系图谱（演示）");
    resize(980, 720);

    auto *root = new QVBoxLayout(this);

    auto *tip = new QLabel("点击节点可应用筛选并返回列表：类型 / 服务(host) / 账号(username)。", this);
    tip->setWordWrap(true);
    root->addWidget(tip);

    auto *toolbar = new QHBoxLayout();
    refreshBtn_ = new QPushButton("刷新", this);
    fitBtn_ = new QPushButton("适配视图", this);
    toolbar->addWidget(refreshBtn_);
    toolbar->addWidget(fitBtn_);
    toolbar->addStretch(1);
    root->addLayout(toolbar);

    scene_ = new QGraphicsScene(this);
    view_ = new QGraphicsView(scene_, this);
    view_->setRenderHint(QPainter::Antialiasing, true);
    view_->setDragMode(QGraphicsView::ScrollHandDrag);
    view_->setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    root->addWidget(view_, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText("关闭");
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(refreshBtn_, &QPushButton::clicked, this, [this]() {
        rebuildGraph();
        fitViewToScene();
    });
    connect(fitBtn_, &QPushButton::clicked, this, &PasswordGraphDialog::fitViewToScene);
}

void PasswordGraphDialog::fitViewToScene()
{
    if (!view_ || !scene_)
        return;
    const auto rect = scene_->itemsBoundingRect().adjusted(-40, -40, 40, 40);
    scene_->setSceneRect(rect);
    if (!rect.isEmpty())
        view_->fitInView(rect, Qt::KeepAspectRatio);
}

void PasswordGraphDialog::rebuildGraph()
{
    if (!scene_)
        return;

    scene_->clear();

    if (!repo_ || !vault_) {
        QMessageBox::warning(this, "失败", "图谱未初始化");
        return;
    }

    if (!vault_->isUnlocked()) {
        QMessageBox::information(this, "提示", "请先解锁 Vault 再查看图谱。");
        return;
    }

    const auto graph = PasswordGraph::build(repo_->listEntries());
    if (graph.nodes.isEmpty()) {
        scene_->addText("暂无条目");
        return;
    }

    QHash<int, QVector<int>> servicesByType;
    QHash<int, QVector<int>> accountsByService;
    for (const auto &e : graph.edges) {
        if (e.from < 0 || e.from >= graph.nodes.size() || e.to < 0 || e.to >= graph.nodes.size())
            continue;
        const auto &from = graph.nodes.at(e.from);
        const auto &to = graph.nodes.at(e.to);
        if (from.kind == PasswordGraph::NodeKind::Type && to.kind == PasswordGraph::NodeKind::Service) {
            servicesByType[from.id].push_back(to.id);
        } else if (from.kind == PasswordGraph::NodeKind::Service && to.kind == PasswordGraph::NodeKind::Account) {
            accountsByService[from.id].push_back(to.id);
        }
    }

    const auto byLabel = [&](int a, int b) {
        return graph.nodes.at(a).label.compare(graph.nodes.at(b).label, Qt::CaseInsensitive) < 0;
    };

    QVector<int> typeNodeIds;
    typeNodeIds.reserve(graph.nodes.size());
    for (const auto &n : graph.nodes) {
        if (n.kind == PasswordGraph::NodeKind::Type)
            typeNodeIds.push_back(n.id);
    }
    std::sort(typeNodeIds.begin(), typeNodeIds.end(), [&](int a, int b) {
        return static_cast<int>(graph.nodes.at(a).entryType) < static_cast<int>(graph.nodes.at(b).entryType);
    });

    const auto onClick = [this](const PasswordGraph::Node &node) {
        QString search;
        switch (node.kind) {
        case PasswordGraph::NodeKind::Type:
            search.clear();
            break;
        case PasswordGraph::NodeKind::Service:
            search = node.label;
            break;
        case PasswordGraph::NodeKind::Account:
            search = node.username;
            break;
        default:
            break;
        }

        emit filterRequested(static_cast<int>(node.entryType), search);
        accept();
    };

    const qreal typeXSpacing = 320.0;
    const qreal typeRadius = 34.0;
    const qreal serviceRadius = 26.0;
    const qreal accountRadius = 20.0;
    const qreal serviceYSpacing = 92.0;
    const qreal accountXOffset = 190.0;
    const qreal accountYSpacing = 44.0;

    QHash<int, PasswordGraphNodeItem *> itemsById;

    for (int col = 0; col < typeNodeIds.size(); ++col) {
        const auto typeId = typeNodeIds.at(col);
        const auto &typeNode = graph.nodes.at(typeId);
        const auto base = baseColorForType(typeNode.entryType);
        const auto typeFill = base.lighter(135);

        auto *typeItem = new PasswordGraphNodeItem(typeNode, typeRadius, typeFill, onClick);
        typeItem->setPos(col * typeXSpacing, 0);
        typeItem->setZValue(10);
        scene_->addItem(typeItem);
        itemsById.insert(typeId, typeItem);

        auto services = servicesByType.value(typeId);
        std::sort(services.begin(), services.end(), byLabel);
        services.erase(std::unique(services.begin(), services.end()), services.end());

        for (int i = 0; i < services.size(); ++i) {
            const auto serviceId = services.at(i);
            const auto &serviceNode = graph.nodes.at(serviceId);
            auto *serviceItem = new PasswordGraphNodeItem(serviceNode, serviceRadius, base.lighter(165), onClick);
            const qreal sx = col * typeXSpacing;
            const qreal sy = (i + 1) * serviceYSpacing;
            serviceItem->setPos(sx, sy);
            serviceItem->setZValue(10);
            scene_->addItem(serviceItem);
            itemsById.insert(serviceId, serviceItem);

            auto accounts = accountsByService.value(serviceId);
            std::sort(accounts.begin(), accounts.end(), byLabel);
            accounts.erase(std::unique(accounts.begin(), accounts.end()), accounts.end());

            const qreal startY = sy - ((accounts.size() - 1) * accountYSpacing) / 2.0;
            for (int j = 0; j < accounts.size(); ++j) {
                const auto accountId = accounts.at(j);
                const auto &accountNode = graph.nodes.at(accountId);
                auto *accountItem = new PasswordGraphNodeItem(accountNode, accountRadius, QColor("#FFFFFF"), onClick);
                accountItem->setPen(QPen(QColor("#999"), 1));
                accountItem->setPos(sx + accountXOffset, startY + j * accountYSpacing);
                accountItem->setZValue(10);
                scene_->addItem(accountItem);
                itemsById.insert(accountId, accountItem);
            }
        }
    }

    for (const auto &e : graph.edges) {
        auto *from = itemsById.value(e.from, nullptr);
        auto *to = itemsById.value(e.to, nullptr);
        if (!from || !to)
            continue;
        QPen pen(QColor("#A0A0A0"), 1);
        if (e.count >= 2)
            pen.setWidthF(1.5);
        auto *line = scene_->addLine(QLineF(from->pos(), to->pos()), pen);
        line->setZValue(0);
    }
}
