#include "toolcatalog.h"

ToolCatalog &ToolCatalog::instance()
{
    static ToolCatalog catalog;
    return catalog;
}

CatalogItem *ToolCatalog::createItem(const QString &id,
                                     const QString &name,
                                     CatalogItemType type,
                                     IconKind icon)
{
    auto *item = new CatalogItem;
    item->id = id;
    item->name = name;
    item->type = type;
    item->icon = icon;
    owned_.push_back(item);
    return item;
}

ToolCatalog::ToolCatalog()
{
    auto *root = createItem("root", "ROOT", CatalogItemType::Folder, IconKind::Folder);

    auto *apps = createItem("apps", "APPLICATIONS", CatalogItemType::Folder, IconKind::Folder);
    apps->description = "Executable modules";

    auto *docs = createItem("docs", "DOCUMENTS", CatalogItemType::Folder, IconKind::Folder);
    docs->description = "System manuals";

    auto *system = createItem("system", "SYSTEM", CatalogItemType::Folder, IconKind::Folder);
    system->description = "Core files";

    root->children = {apps, docs, system};

    auto *translator = createItem("tool_translator", "TRANSLATOR.EXE", CatalogItemType::Tool, IconKind::Tool);
    translator->description = "System-wide Translation";
    translator->toolId = ToolId::Translator;

    auto *password = createItem("tool_password", "PASSWORD_VAULT.DAT", CatalogItemType::Tool, IconKind::Tool);
    password->description = "Password Manager (WIP)";
    password->toolId = ToolId::PasswordPlaceholder;

    apps->children = {translator, password};

    auto *readme = createItem("doc_readme", "README.TXT", CatalogItemType::Doc, IconKind::Doc);
    readme->description = "Read Me First";
    readme->toolId = ToolId::Readme;
    readme->docContent = R"doc(
WELCOME TO TOOLBOX // CASSETTE UI
==============================

NAVIGATION:
1) Click FOLDERS to navigate
2) Click FILES to launch tools
3) Use the BREADCRUMB bar to go back

STATUS:
SYSTEM_READY // V.0.1

NOTE:
This is a course design project. All code must be explainable in defense.
)doc";
    docs->children = {readme};

    root_ = root;
}

const CatalogItem *ToolCatalog::root()
{
    return ToolCatalog::instance().root_;
}
