#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

enum class CatalogItemType
{
    Folder,
    Tool,
    Doc
};

enum class ToolId
{
    Translator,
    PasswordPlaceholder,
    Readme
};

enum class IconKind
{
    Folder,
    Tool,
    Doc
};

struct CatalogItem final
{
    QString id;
    QString name;
    QString description;
    CatalogItemType type = CatalogItemType::Folder;
    IconKind icon = IconKind::Folder;
    ToolId toolId = ToolId::Translator;
    QString docContent;
    QVector<const CatalogItem *> children;
};

class ToolCatalog final
{
public:
    static const CatalogItem *root();

private:
    static ToolCatalog &instance();
    ToolCatalog();
    CatalogItem *createItem(const QString &id, const QString &name, CatalogItemType type, IconKind icon);
    const CatalogItem *root_ = nullptr;
    QVector<CatalogItem *> owned_;
};
