#include "passwordmanagerpage.h"

#include "core/apppaths.h"
#include "core/crypto.h"
#include "pages/passwordcsvimportdialog.h"
#include "pages/passwordentrydialog.h"
#include "pages/passwordgraphdialog.h"
#include "pages/passwordhealthdialog.h"
#include "password/passwordcsv.h"
#include "password/passwordcsvimportworker.h"
#include "password/passwordentrymodel.h"
#include "password/passwordfaviconservice.h"
#include "password/passwordgroupmodel.h"
#include "password/passwordrepository.h"
#include "password/passwordvault.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QDir>
#include <QHash>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QSaveFile>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QSet>
#include <QTableView>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

#include <algorithm>

#ifdef TBX_HAS_WEBENGINE
#include "pages/passwordwebassistantdialog.h"
#endif

namespace {

class PasswordFilterProxyModel final : public QSortFilterProxyModel
{
public:
    explicit PasswordFilterProxyModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {}

    void setSearchText(const QString &text)
    {
        searchText_ = text.trimmed();
        invalidateFilter();
    }

    void setCategory(const QString &category)
    {
        category_ = category.trimmed();
        invalidateFilter();
    }

    void setRequiredTags(const QStringList &tags)
    {
        requiredTags_ = tags;
        invalidateFilter();
    }

    void setGroupIds(const QVector<qint64> &groupIds)
    {
        groupIds_ = groupIds;
        invalidateFilter();
    }

    void setEntryType(int entryType)
    {
        entryType_ = entryType;
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        const auto model = sourceModel();
        if (!model)
            return true;

        const auto groupId = model->index(sourceRow, 0, sourceParent).data(PasswordEntryModel::GroupIdRole).toLongLong();
        if (!groupIds_.isEmpty() && !groupIds_.contains(groupId))
            return false;

        const auto entryType = model->index(sourceRow, 0, sourceParent).data(PasswordEntryModel::EntryTypeRole).toInt();
        if (entryType_ >= 0 && entryType != entryType_)
            return false;

        const auto title = model->index(sourceRow, 0, sourceParent).data().toString();
        const auto username = model->index(sourceRow, 1, sourceParent).data().toString();
        const auto url = model->index(sourceRow, 2, sourceParent).data().toString();
        const auto category = model->index(sourceRow, 3, sourceParent).data().toString();
        const auto tagsText = model->index(sourceRow, 4, sourceParent).data().toString();

        if (!category_.isEmpty() && category_ != "全部" && category_ != category)
            return false;

        if (!requiredTags_.isEmpty()) {
            const auto rowTags = model->index(sourceRow, 0, sourceParent).data(PasswordEntryModel::TagsRole).toStringList();
            for (const auto &required : requiredTags_) {
                bool found = false;
                for (const auto &tag : rowTags) {
                    if (tag.compare(required, Qt::CaseInsensitive) == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found)
                    return false;
            }
        }

        if (searchText_.isEmpty())
            return true;

        const auto typeLabel = passwordEntryTypeLabel(passwordEntryTypeFromInt(entryType));
        const auto haystack = QString("%1 %2 %3 %4 %5 %6").arg(title, username, url, typeLabel, category, tagsText);
        return haystack.contains(searchText_, Qt::CaseInsensitive);
    }

private:
    QString searchText_;
    QString category_ = "全部";
    QStringList requiredTags_;
    QVector<qint64> groupIds_;
    int entryType_ = -1;
};

QString promptPassword(QWidget *parent, const QString &title, const QString &label)
{
    bool ok = false;
    const auto text = QInputDialog::getText(parent, title, label, QLineEdit::Password, "", &ok);
    if (!ok)
        return {};
    return text;
}

QString promptPasswordWithConfirm(QWidget *parent, const QString &title, const QString &label)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(title);

    auto *root = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);

    auto *pwd1 = new QLineEdit(&dlg);
    pwd1->setEchoMode(QLineEdit::Password);
    auto *pwd2 = new QLineEdit(&dlg);
    pwd2->setEchoMode(QLineEdit::Password);
    form->addRow(label, pwd1);
    form->addRow("确认密码：", pwd2);
    root->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    buttons->button(QDialogButtonBox::Ok)->setText("确定");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    root->addWidget(buttons);

    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, [&]() {
        if (pwd1->text().isEmpty()) {
            QMessageBox::warning(&dlg, "提示", "密码不能为空");
            return;
        }
        if (pwd1->text() != pwd2->text()) {
            QMessageBox::warning(&dlg, "提示", "两次输入不一致");
            return;
        }
        dlg.accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return {};
    return pwd1->text();
}

} // namespace

PasswordManagerPage::PasswordManagerPage(QWidget *parent) : QWidget(parent)
{
    vault_ = new PasswordVault(this);
    repo_ = new PasswordRepository(vault_);
    faviconService_ = new PasswordFaviconService(this);
    model_ = new PasswordEntryModel(this);
    model_->setFaviconService(faviconService_);
    groupModel_ = new PasswordGroupModel(this);
    proxy_ = new PasswordFilterProxyModel(this);
    proxy_->setSourceModel(model_);

    autoLockTimer_ = new QTimer(this);
    autoLockTimer_->setSingleShot(true);
    autoLockTimer_->setInterval(5 * 60 * 1000);

    clipboardClearTimer_ = new QTimer(this);
    clipboardClearTimer_->setSingleShot(true);
    clipboardClearTimer_->setInterval(15 * 1000);

    setupUi();
    wireSignals();
    refreshGroups();
    refreshAll();

    qApp->installEventFilter(this);
}

PasswordManagerPage::~PasswordManagerPage()
{
    qApp->removeEventFilter(this);
}

bool PasswordManagerPage::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched);

    if (vault_->isUnlocked()) {
        switch (event->type()) {
        case QEvent::KeyPress:
        case QEvent::MouseButtonPress:
        case QEvent::Wheel:
        case QEvent::MouseMove:
            resetAutoLockTimer();
            break;
        default:
            break;
        }
    }

    return false;
}

void PasswordManagerPage::setupUi()
{
    auto *root = new QVBoxLayout(this);

    auto *topRow = new QHBoxLayout();
    statusLabel_ = new QLabel(this);
    topRow->addWidget(statusLabel_);
    topRow->addStretch(1);

    createBtn_ = new QPushButton("设置主密码", this);
    unlockBtn_ = new QPushButton("解锁", this);
    lockBtn_ = new QPushButton("锁定", this);
    changePwdBtn_ = new QPushButton("修改主密码", this);
    importBtn_ = new QPushButton("导入备份", this);
    exportBtn_ = new QPushButton("导出备份", this);
    importCsvBtn_ = new QPushButton("导入 CSV", this);
    exportCsvBtn_ = new QPushButton("导出 CSV", this);
    healthBtn_ = new QPushButton("安全报告", this);
    webAssistantBtn_ = new QPushButton("Web 助手", this);
    graphBtn_ = new QPushButton("图谱", this);

    topRow->addWidget(createBtn_);
    topRow->addWidget(unlockBtn_);
    topRow->addWidget(lockBtn_);
    topRow->addWidget(changePwdBtn_);
    topRow->addSpacing(12);
    topRow->addWidget(importBtn_);
    topRow->addWidget(exportBtn_);
    topRow->addWidget(importCsvBtn_);
    topRow->addWidget(exportCsvBtn_);
    topRow->addWidget(healthBtn_);
    topRow->addWidget(webAssistantBtn_);
    topRow->addWidget(graphBtn_);
    root->addLayout(topRow);

#ifndef TBX_HAS_WEBENGINE
    if (webAssistantBtn_) {
        webAssistantBtn_->setEnabled(false);
        webAssistantBtn_->setToolTip("未安装 Qt WebEngine 模块，Web 助手不可用。");
    }
#endif

    auto *filterRow = new QHBoxLayout();
    searchEdit_ = new QLineEdit(this);
    searchEdit_->setPlaceholderText("搜索标题/账号/网址/类型/分类/标签…");

    tagFilterEdit_ = new QLineEdit(this);
    tagFilterEdit_->setPlaceholderText("标签（逗号分隔）");

    typeCombo_ = new QComboBox(this);
    typeCombo_->addItem("全部", -1);
    typeCombo_->addItem(passwordEntryTypeLabel(PasswordEntryType::WebLogin), static_cast<int>(PasswordEntryType::WebLogin));
    typeCombo_->addItem(passwordEntryTypeLabel(PasswordEntryType::DesktopClient),
                        static_cast<int>(PasswordEntryType::DesktopClient));
    typeCombo_->addItem(passwordEntryTypeLabel(PasswordEntryType::ApiKeyToken), static_cast<int>(PasswordEntryType::ApiKeyToken));
    typeCombo_->addItem(passwordEntryTypeLabel(PasswordEntryType::DatabaseCredential),
                        static_cast<int>(PasswordEntryType::DatabaseCredential));
    typeCombo_->addItem(passwordEntryTypeLabel(PasswordEntryType::ServerSsh), static_cast<int>(PasswordEntryType::ServerSsh));
    typeCombo_->addItem(passwordEntryTypeLabel(PasswordEntryType::DeviceWifi), static_cast<int>(PasswordEntryType::DeviceWifi));

    categoryCombo_ = new QComboBox(this);
    categoryCombo_->addItem("全部");
    categoryCombo_->addItem("未分类");

    filterRow->addWidget(new QLabel("搜索：", this));
    filterRow->addWidget(searchEdit_, 1);
    filterRow->addSpacing(12);
    filterRow->addWidget(new QLabel("标签：", this));
    filterRow->addWidget(tagFilterEdit_);
    filterRow->addSpacing(12);
    filterRow->addWidget(new QLabel("类型：", this));
    filterRow->addWidget(typeCombo_);
    filterRow->addSpacing(12);
    filterRow->addWidget(new QLabel("分类：", this));
    filterRow->addWidget(categoryCombo_);
    root->addLayout(filterRow);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    auto *groupPanel = new QWidget(splitter);
    auto *groupLayout = new QVBoxLayout(groupPanel);
    groupLayout->setContentsMargins(0, 0, 0, 0);

    auto *groupToolbar = new QHBoxLayout();
    groupToolbar->addWidget(new QLabel("分组", groupPanel));
    groupToolbar->addStretch(1);

    groupAddBtn_ = new QToolButton(groupPanel);
    groupAddBtn_->setText("新建");
    groupRenameBtn_ = new QToolButton(groupPanel);
    groupRenameBtn_->setText("重命名");
    groupDeleteBtn_ = new QToolButton(groupPanel);
    groupDeleteBtn_->setText("删除");
    groupToolbar->addWidget(groupAddBtn_);
    groupToolbar->addWidget(groupRenameBtn_);
    groupToolbar->addWidget(groupDeleteBtn_);
    groupLayout->addLayout(groupToolbar);

    groupView_ = new QTreeView(groupPanel);
    groupView_->setModel(groupModel_);
    groupView_->setHeaderHidden(true);
    groupView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    groupView_->setUniformRowHeights(true);
    groupLayout->addWidget(groupView_, 1);

    groupPanel->setMinimumWidth(220);
    splitter->addWidget(groupPanel);

    tableView_ = new QTableView(splitter);
    tableView_->setModel(proxy_);
    tableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView_->setSortingEnabled(true);
    tableView_->setIconSize(QSize(16, 16));
    tableView_->horizontalHeader()->setStretchLastSection(true);
    tableView_->verticalHeader()->setVisible(false);
    tableView_->setWordWrap(false);
    splitter->addWidget(tableView_);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    root->addWidget(splitter, 1);

    auto *actionRow = new QHBoxLayout();
    addBtn_ = new QPushButton("新增", this);
    editBtn_ = new QPushButton("编辑", this);
    deleteBtn_ = new QPushButton("删除", this);
    moveBtn_ = new QPushButton("移动分组", this);
    copyUserBtn_ = new QPushButton("复制账号", this);
    copyPwdBtn_ = new QPushButton("复制密码", this);

    actionRow->addWidget(addBtn_);
    actionRow->addWidget(editBtn_);
    actionRow->addWidget(deleteBtn_);
    actionRow->addWidget(moveBtn_);
    actionRow->addSpacing(12);
    actionRow->addWidget(copyUserBtn_);
    actionRow->addWidget(copyPwdBtn_);
    actionRow->addStretch(1);
    root->addLayout(actionRow);

    hintLabel_ = new QLabel(this);
    hintLabel_->setText("提示：复制密码后会在 15 秒后自动清空剪贴板；解锁状态 5 分钟无操作会自动锁定。");
    root->addWidget(hintLabel_);
}

void PasswordManagerPage::wireSignals()
{
    connect(vault_, &PasswordVault::stateChanged, this, &PasswordManagerPage::updateUiState);

    connect(createBtn_, &QPushButton::clicked, this, &PasswordManagerPage::createVault);
    connect(unlockBtn_, &QPushButton::clicked, this, &PasswordManagerPage::unlockVault);
    connect(lockBtn_, &QPushButton::clicked, this, &PasswordManagerPage::lockVault);
    connect(changePwdBtn_, &QPushButton::clicked, this, &PasswordManagerPage::changeMasterPassword);
    connect(importBtn_, &QPushButton::clicked, this, &PasswordManagerPage::importBackup);
    connect(exportBtn_, &QPushButton::clicked, this, &PasswordManagerPage::exportBackup);
    connect(importCsvBtn_, &QPushButton::clicked, this, &PasswordManagerPage::importCsv);
    connect(exportCsvBtn_, &QPushButton::clicked, this, &PasswordManagerPage::exportCsv);
    connect(healthBtn_, &QPushButton::clicked, this, &PasswordManagerPage::showHealthReport);
    connect(webAssistantBtn_, &QPushButton::clicked, this, &PasswordManagerPage::showWebAssistant);
    connect(graphBtn_, &QPushButton::clicked, this, &PasswordManagerPage::showGraph);

    connect(addBtn_, &QPushButton::clicked, this, &PasswordManagerPage::addEntry);
    connect(editBtn_, &QPushButton::clicked, this, &PasswordManagerPage::editSelectedEntry);
    connect(deleteBtn_, &QPushButton::clicked, this, &PasswordManagerPage::deleteSelectedEntry);
    connect(moveBtn_, &QPushButton::clicked, this, &PasswordManagerPage::moveSelectedEntryToGroup);
    connect(copyUserBtn_, &QPushButton::clicked, this, &PasswordManagerPage::copySelectedUsername);
    connect(copyPwdBtn_, &QPushButton::clicked, this, &PasswordManagerPage::copySelectedPassword);

    connect(groupAddBtn_, &QToolButton::clicked, this, &PasswordManagerPage::addGroup);
    connect(groupRenameBtn_, &QToolButton::clicked, this, &PasswordManagerPage::renameSelectedGroup);
    connect(groupDeleteBtn_, &QToolButton::clicked, this, &PasswordManagerPage::deleteSelectedGroup);

    connect(tableView_, &QTableView::doubleClicked, this, [this]() { editSelectedEntry(); });
    connect(tableView_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() { updateUiState(); });

    connect(groupView_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this]() {
        static_cast<PasswordFilterProxyModel *>(proxy_)->setGroupIds(groupModel_->descendantGroupIds(selectedGroupId()));
        updateUiState();
    });

    connect(searchEdit_, &QLineEdit::textChanged, this, [this](const QString &text) {
        static_cast<PasswordFilterProxyModel *>(proxy_)->setSearchText(text);
    });

    connect(tagFilterEdit_, &QLineEdit::textChanged, this, [this](const QString &text) {
        auto normalized = text;
        normalized.replace("，", ",");

        QStringList tags;
        for (const auto &part : normalized.split(',', Qt::SkipEmptyParts)) {
            const auto t = part.trimmed();
            if (!t.isEmpty())
                tags.push_back(t);
        }

        static_cast<PasswordFilterProxyModel *>(proxy_)->setRequiredTags(tags);
    });

    connect(typeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        const auto typeValue = typeCombo_ ? typeCombo_->currentData().toInt() : -1;
        static_cast<PasswordFilterProxyModel *>(proxy_)->setEntryType(typeValue);
    });

    connect(categoryCombo_, &QComboBox::currentTextChanged, this, [this](const QString &category) {
        static_cast<PasswordFilterProxyModel *>(proxy_)->setCategory(category);
    });

    connect(autoLockTimer_, &QTimer::timeout, this, [this]() {
        if (vault_->isUnlocked()) {
            vault_->lock();
            hintLabel_->setText("已自动锁定");
        }
    });

    connect(clipboardClearTimer_, &QTimer::timeout, this, [this]() {
        auto *cb = QApplication::clipboard();
        if (cb->text() == lastClipboardSecret_) {
            cb->clear();
        }
        lastClipboardSecret_.clear();
    });
}

void PasswordManagerPage::showWebAssistant()
{
#ifdef TBX_HAS_WEBENGINE
    PasswordWebAssistantDialog dlg(repo_, vault_, this);
    dlg.exec();
    refreshAll();
#else
    QMessageBox::information(
        this,
        "提示",
        "当前 Qt Kit 未安装 Qt WebEngine 模块，无法使用 Web 助手。\n"
        "如需启用：使用 Qt Maintenance Tool 安装 Qt WebEngine（webenginewidgets + webchannel）后重新构建。");
#endif
}

void PasswordManagerPage::showGraph()
{
    if (!vault_->isUnlocked()) {
        QMessageBox::information(this, "提示", "请先解锁");
        return;
    }

    PasswordGraphDialog dlg(repo_, vault_, this);
    connect(&dlg, &PasswordGraphDialog::filterRequested, this, [this](int entryType, const QString &searchText) {
        if (typeCombo_) {
            const auto idx = typeCombo_->findData(entryType);
            typeCombo_->setCurrentIndex(idx >= 0 ? idx : 0);
        }
        if (searchEdit_)
            searchEdit_->setText(searchText);
    });
    dlg.exec();
}

void PasswordManagerPage::refreshAll()
{
    model_->reload();
    refreshCategories();
    updateUiState();
}

void PasswordManagerPage::refreshCategories()
{
    const auto current = categoryCombo_->currentText();
    const auto categories = repo_->listCategories();

    categoryCombo_->blockSignals(true);
    categoryCombo_->clear();
    categoryCombo_->addItem("全部");
    categoryCombo_->addItem("未分类");
    categoryCombo_->addItems(categories);
    const auto idx = categoryCombo_->findText(current);
    categoryCombo_->setCurrentIndex(idx >= 0 ? idx : 0);
    categoryCombo_->blockSignals(false);
}

void PasswordManagerPage::refreshGroups()
{
    const auto currentGroupId = selectedGroupId();

    groupModel_->setGroups(repo_->listGroups());
    groupView_->expandAll();

    auto idx = groupModel_->indexForGroupId(currentGroupId > 0 ? currentGroupId : 1);
    if (!idx.isValid())
        idx = groupModel_->indexForGroupId(1);

    if (idx.isValid())
        groupView_->setCurrentIndex(idx);

    static_cast<PasswordFilterProxyModel *>(proxy_)->setGroupIds(groupModel_->descendantGroupIds(selectedGroupId()));
}

void PasswordManagerPage::updateUiState()
{
    const auto initialized = vault_->isInitialized();
    const auto unlocked = vault_->isUnlocked();
    const auto hasSelection = selectedEntryId() > 0;
    const auto groupId = selectedGroupId();

    if (faviconService_)
        faviconService_->setNetworkEnabled(unlocked);

    if (!initialized) {
        statusLabel_->setText("状态：未初始化（请先设置主密码）");
    } else if (unlocked) {
        statusLabel_->setText("状态：已解锁");
    } else {
        statusLabel_->setText("状态：已锁定");
    }

    createBtn_->setEnabled(!initialized);
    unlockBtn_->setEnabled(initialized && !unlocked);
    lockBtn_->setEnabled(initialized && unlocked);
    changePwdBtn_->setEnabled(initialized && unlocked);

    importBtn_->setEnabled(initialized && unlocked);
    exportBtn_->setEnabled(initialized && unlocked);
    importCsvBtn_->setEnabled(initialized && unlocked);
    exportCsvBtn_->setEnabled(initialized && unlocked);
    healthBtn_->setEnabled(initialized && unlocked);
    graphBtn_->setEnabled(initialized && unlocked);

    addBtn_->setEnabled(initialized && unlocked);
    editBtn_->setEnabled(initialized && unlocked && hasSelection);
    deleteBtn_->setEnabled(initialized && unlocked && hasSelection);
    moveBtn_->setEnabled(initialized && unlocked && hasSelection);
    copyPwdBtn_->setEnabled(initialized && unlocked && hasSelection);
    copyUserBtn_->setEnabled(hasSelection);

    groupAddBtn_->setEnabled(initialized && unlocked);
    groupRenameBtn_->setEnabled(initialized && unlocked && groupId > 1);
    groupDeleteBtn_->setEnabled(initialized && unlocked && groupId > 1);

    if (unlocked)
        resetAutoLockTimer();
    else
        autoLockTimer_->stop();
}

void PasswordManagerPage::createVault()
{
    const auto pwd = promptPasswordWithConfirm(this, "设置主密码", "主密码：");
    if (pwd.isEmpty())
        return;

    if (!vault_->createVault(pwd)) {
        QMessageBox::critical(this, "失败", vault_->lastError());
        return;
    }

    QMessageBox::information(this, "完成", "已创建并解锁 Vault。");
    refreshAll();
}

void PasswordManagerPage::unlockVault()
{
    const auto pwd = promptPassword(this, "解锁", "主密码：");
    if (pwd.isEmpty())
        return;

    if (!vault_->unlock(pwd)) {
        QMessageBox::warning(this, "解锁失败", vault_->lastError());
        return;
    }

    refreshAll();
}

void PasswordManagerPage::lockVault()
{
    vault_->lock();
    refreshAll();
}

void PasswordManagerPage::changeMasterPassword()
{
    const auto pwd = promptPasswordWithConfirm(this, "修改主密码", "新主密码：");
    if (pwd.isEmpty())
        return;

    if (!vault_->changeMasterPassword(pwd)) {
        QMessageBox::warning(this, "失败", vault_->lastError());
        return;
    }

    QMessageBox::information(this, "完成", "主密码已更新，所有条目已重新加密。");
    refreshAll();
}

void PasswordManagerPage::showHealthReport()
{
    if (!vault_->isUnlocked()) {
        QMessageBox::information(this, "提示", "请先解锁");
        return;
    }

    const auto dbPath = QDir(AppPaths::appDataDir()).filePath("password.sqlite3");
    PasswordHealthDialog dlg(dbPath, vault_->masterKey(), this);
    connect(&dlg, &PasswordHealthDialog::entryActivated, this, [this](qint64 id) { editEntryById(id); });
    dlg.exec();
}

void PasswordManagerPage::addGroup()
{
    if (!vault_->isUnlocked()) {
        QMessageBox::information(this, "提示", "请先解锁");
        return;
    }

    bool ok = false;
    const auto name = QInputDialog::getText(this, "新建分组", "分组名：", QLineEdit::Normal, "", &ok);
    if (!ok)
        return;

    const auto createdId = repo_->createGroup(selectedGroupId(), name);
    if (!createdId.has_value()) {
        QMessageBox::warning(this, "失败", repo_->lastError());
        return;
    }

    refreshGroups();
    const auto idx = groupModel_->indexForGroupId(createdId.value());
    if (idx.isValid())
        groupView_->setCurrentIndex(idx);
}

void PasswordManagerPage::renameSelectedGroup()
{
    if (!vault_->isUnlocked()) {
        QMessageBox::information(this, "提示", "请先解锁");
        return;
    }

    const auto groupId = selectedGroupId();
    if (groupId <= 1)
        return;

    const auto currentName = groupView_->currentIndex().data().toString();
    bool ok = false;
    const auto name = QInputDialog::getText(this, "重命名分组", "分组名：", QLineEdit::Normal, currentName, &ok);
    if (!ok)
        return;

    if (!repo_->renameGroup(groupId, name)) {
        QMessageBox::warning(this, "失败", repo_->lastError());
        return;
    }

    refreshGroups();
}

void PasswordManagerPage::deleteSelectedGroup()
{
    if (!vault_->isUnlocked()) {
        QMessageBox::information(this, "提示", "请先解锁");
        return;
    }

    const auto groupId = selectedGroupId();
    if (groupId <= 1)
        return;

    if (QMessageBox::question(this, "确认删除", "确定要删除该分组吗？（必须为空）") != QMessageBox::Yes)
        return;

    const auto currentIndex = groupModel_->indexForGroupId(groupId);
    const auto parentIndex = groupModel_->parent(currentIndex);
    const auto parentGroupId = groupModel_->groupIdForIndex(parentIndex) > 0 ? groupModel_->groupIdForIndex(parentIndex) : 1;

    if (!repo_->deleteGroup(groupId)) {
        QMessageBox::warning(this, "失败", repo_->lastError());
        return;
    }

    refreshGroups();
    const auto idx = groupModel_->indexForGroupId(parentGroupId);
    if (idx.isValid())
        groupView_->setCurrentIndex(idx);
}

void PasswordManagerPage::addEntry()
{
    if (!vault_->isUnlocked()) {
        QMessageBox::information(this, "提示", "请先解锁");
        return;
    }

    PasswordEntryDialog dlg(repo_->listCategories(), repo_->listAllTags(), this);
    dlg.setWindowTitle("新增密码条目");
    if (dlg.exec() != QDialog::Accepted)
        return;

    auto secrets = dlg.entry();
    secrets.entry.groupId = selectedGroupId();
    if (secrets.entry.title.trimmed().isEmpty() || secrets.password.isEmpty())
        return;

    if (!repo_->addEntry(secrets)) {
        QMessageBox::warning(this, "失败", repo_->lastError());
        return;
    }

    refreshAll();
}

qint64 PasswordManagerPage::selectedEntryId() const
{
    const auto idx = tableView_->currentIndex();
    if (!idx.isValid())
        return 0;

    const auto sourceIndex = proxy_->mapToSource(idx);
    const auto item = model_->itemAt(sourceIndex.row());
    return item.id;
}

qint64 PasswordManagerPage::selectedGroupId() const
{
    if (!groupView_ || !groupModel_)
        return 1;

    const auto id = groupModel_->groupIdForIndex(groupView_->currentIndex());
    return id > 0 ? id : 1;
}

void PasswordManagerPage::editSelectedEntry()
{
    editEntryById(selectedEntryId());
}

void PasswordManagerPage::editEntryById(qint64 id)
{
    if (!vault_->isUnlocked()) {
        QMessageBox::information(this, "提示", "请先解锁");
        return;
    }

    if (id <= 0)
        return;

    const auto loaded = repo_->loadEntry(id);
    if (!loaded.has_value()) {
        QMessageBox::warning(this, "失败", repo_->lastError());
        return;
    }

    PasswordEntryDialog dlg(repo_->listCategories(), repo_->listAllTags(), this);
    dlg.setWindowTitle("编辑密码条目");
    dlg.setEntry(loaded.value());
    if (dlg.exec() != QDialog::Accepted)
        return;

    auto secrets = dlg.entry();
    secrets.entry.id = id;

    if (!repo_->updateEntry(secrets)) {
        QMessageBox::warning(this, "失败", repo_->lastError());
        return;
    }

    refreshAll();
}

void PasswordManagerPage::deleteSelectedEntry()
{
    if (!vault_->isUnlocked()) {
        QMessageBox::information(this, "提示", "请先解锁");
        return;
    }

    const auto id = selectedEntryId();
    if (id <= 0)
        return;

    if (QMessageBox::question(this, "确认删除", "确定要删除选中的条目吗？") != QMessageBox::Yes)
        return;

    if (!repo_->deleteEntry(id)) {
        QMessageBox::warning(this, "失败", repo_->lastError());
        return;
    }

    refreshAll();
}

void PasswordManagerPage::moveSelectedEntryToGroup()
{
    if (!vault_->isUnlocked()) {
        QMessageBox::information(this, "提示", "请先解锁");
        return;
    }

    const auto id = selectedEntryId();
    if (id <= 0)
        return;

    const auto groups = repo_->listGroups();
    if (groups.isEmpty()) {
        QMessageBox::warning(this, "失败", "没有可用分组");
        return;
    }

    QHash<qint64, PasswordGroup> groupsById;
    for (const auto &g : groups)
        groupsById.insert(g.id, g);

    auto makePath = [&](qint64 groupId) {
        QStringList parts;
        qint64 current = groupId;
        int guard = 0;
        while (current > 0 && groupsById.contains(current) && guard++ < 32) {
            const auto &g = groupsById[current];
            parts.prepend(g.name);
            current = g.parentId;
        }
        return parts.join("/");
    };

    struct Option final
    {
        qint64 id = 0;
        QString path;
    };

    QVector<Option> options;
    options.reserve(groups.size());
    for (const auto &g : groups) {
        if (g.id <= 0)
            continue;
        options.push_back(Option{g.id, makePath(g.id)});
    }

    std::sort(options.begin(), options.end(), [](const Option &a, const Option &b) {
        return a.path.toLower() < b.path.toLower();
    });

    QStringList items;
    QVector<qint64> ids;
    items.reserve(options.size());
    ids.reserve(options.size());
    for (const auto &opt : options) {
        ids.push_back(opt.id);
        items.push_back(opt.path);
    }

    qint64 currentGroupId = 1;
    const auto currentIndex = tableView_->currentIndex();
    if (currentIndex.isValid()) {
        const auto sourceIndex = proxy_->mapToSource(currentIndex);
        currentGroupId = model_->itemAt(sourceIndex.row()).groupId;
        if (currentGroupId <= 0)
            currentGroupId = 1;
    }

    int defaultIndex = 0;
    for (int i = 0; i < ids.size(); ++i) {
        if (ids.at(i) == currentGroupId) {
            defaultIndex = i;
            break;
        }
    }

    bool ok = false;
    const auto selected = QInputDialog::getItem(this, "移动到分组", "选择分组：", items, defaultIndex, false, &ok);
    if (!ok)
        return;

    const auto selectedIdx = items.indexOf(selected);
    if (selectedIdx < 0 || selectedIdx >= ids.size())
        return;

    const auto groupId = ids.at(selectedIdx);
    if (!repo_->moveEntryToGroup(id, groupId)) {
        QMessageBox::warning(this, "失败", repo_->lastError());
        return;
    }

    refreshAll();
}

void PasswordManagerPage::copySelectedUsername()
{
    const auto idx = tableView_->currentIndex();
    if (!idx.isValid())
        return;

    const auto username = proxy_->index(idx.row(), 1).data().toString();
    if (username.trimmed().isEmpty())
        return;

    QApplication::clipboard()->setText(username);
    hintLabel_->setText("账号已复制到剪贴板");
}

void PasswordManagerPage::copySelectedPassword()
{
    if (!vault_->isUnlocked()) {
        QMessageBox::information(this, "提示", "请先解锁");
        return;
    }

    const auto id = selectedEntryId();
    if (id <= 0)
        return;

    const auto loaded = repo_->loadEntry(id);
    if (!loaded.has_value()) {
        QMessageBox::warning(this, "失败", repo_->lastError());
        return;
    }

    if (QMessageBox::question(this, "复制密码", "复制密码到剪贴板？") != QMessageBox::Yes)
        return;

    const auto pwd = loaded->password;
    QApplication::clipboard()->setText(pwd);
    lastClipboardSecret_ = pwd;
    clipboardClearTimer_->start();
    hintLabel_->setText("密码已复制（15 秒后自动清空）");
}

void PasswordManagerPage::resetAutoLockTimer()
{
    autoLockTimer_->start();
}

void PasswordManagerPage::exportBackup()
{
    if (!vault_->isUnlocked()) {
        QMessageBox::information(this, "提示", "请先解锁");
        return;
    }

    const auto path = QFileDialog::getSaveFileName(this, "导出备份", "", "Toolbox 密码备份 (*.tbxpm)");
    if (path.isEmpty())
        return;

    const auto backupPassword = promptPasswordWithConfirm(this, "备份加密密码", "备份密码：");
    if (backupPassword.isEmpty())
        return;

    QJsonArray groups;
    for (const auto &g : repo_->listGroups()) {
        QJsonObject obj;
        obj["id"] = g.id;
        obj["parent_id"] = g.parentId;
        obj["name"] = g.name;
        groups.append(obj);
    }

    QJsonArray entries;
    for (const auto &summary : repo_->listEntries()) {
        const auto full = repo_->loadEntry(summary.id);
        if (!full.has_value()) {
            QMessageBox::warning(this, "失败", repo_->lastError());
            return;
        }

        QJsonObject obj;
        obj["title"] = full->entry.title;
        obj["username"] = full->entry.username;
        obj["password"] = full->password;
        obj["url"] = full->entry.url;
        obj["group_id"] = full->entry.groupId;
        obj["entry_type"] = static_cast<int>(full->entry.type);
        obj["category"] = full->entry.category;
        QJsonArray tags;
        for (const auto &tag : full->entry.tags)
            tags.append(tag);
        obj["tags"] = tags;
        obj["notes"] = full->notes;
        obj["created_at"] = full->entry.createdAt.toSecsSinceEpoch();
        obj["updated_at"] = full->entry.updatedAt.toSecsSinceEpoch();
        entries.append(obj);
    }

    QJsonObject plainRoot;
    plainRoot["version"] = 1;
    plainRoot["exported_at"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    plainRoot["groups"] = groups;
    plainRoot["entries"] = entries;

    const auto plainJson = QJsonDocument(plainRoot).toJson(QJsonDocument::Compact);

    const auto salt = Crypto::randomBytes(16);
    const int iterations = 120000;
    const auto key = Crypto::pbkdf2Sha256(backupPassword.toUtf8(), salt, iterations, 32);
    const auto sealed = Crypto::seal(key, plainJson);

    QJsonObject fileRoot;
    fileRoot["format"] = "ToolboxPasswordBackup";
    fileRoot["version"] = 1;

    QJsonObject kdf;
    kdf["salt"] = QString::fromLatin1(salt.toBase64());
    kdf["iterations"] = iterations;
    fileRoot["kdf"] = kdf;
    fileRoot["ciphertext"] = QString::fromLatin1(sealed.toBase64());
    fileRoot["exported_at"] = plainRoot["exported_at"];

    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "失败", "无法写入文件");
        return;
    }
    out.write(QJsonDocument(fileRoot).toJson(QJsonDocument::Indented));
    if (!out.commit()) {
        QMessageBox::warning(this, "失败", "写入文件失败");
        return;
    }

    QMessageBox::information(this, "完成", "备份已导出。");
}

void PasswordManagerPage::importBackup()
{
    if (!vault_->isUnlocked()) {
        QMessageBox::information(this, "提示", "请先解锁");
        return;
    }

    const auto path = QFileDialog::getOpenFileName(this, "导入备份", "", "Toolbox 密码备份 (*.tbxpm)");
    if (path.isEmpty())
        return;

    QFile in(path);
    if (!in.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "失败", "无法读取文件");
        return;
    }

    const auto doc = QJsonDocument::fromJson(in.readAll());
    if (!doc.isObject()) {
        QMessageBox::warning(this, "失败", "文件格式不正确");
        return;
    }

    const auto root = doc.object();
    if (root.value("format").toString() != "ToolboxPasswordBackup") {
        QMessageBox::warning(this, "失败", "不是 Toolbox 密码备份文件");
        return;
    }
    if (root.value("version").toInt() != 1) {
        QMessageBox::warning(this, "失败", "备份文件版本不支持");
        return;
    }

    const auto kdf = root.value("kdf").toObject();
    const auto salt = QByteArray::fromBase64(kdf.value("salt").toString().toLatin1());
    const auto iterations = kdf.value("iterations").toInt();
    const auto ciphertext = QByteArray::fromBase64(root.value("ciphertext").toString().toLatin1());

    if (salt.isEmpty() || iterations <= 0 || ciphertext.isEmpty()) {
        QMessageBox::warning(this, "失败", "备份文件缺少必要字段");
        return;
    }

    const auto backupPassword = promptPassword(this, "输入备份密码", "备份密码：");
    if (backupPassword.isEmpty())
        return;

    const auto key = Crypto::pbkdf2Sha256(backupPassword.toUtf8(), salt, iterations, 32);
    const auto plain = Crypto::open(key, ciphertext);
    if (!plain.has_value()) {
        QMessageBox::warning(this, "失败", "解密失败：密码错误或文件损坏");
        return;
    }

    const auto innerDoc = QJsonDocument::fromJson(plain.value());
    if (!innerDoc.isObject()) {
        QMessageBox::warning(this, "失败", "备份内容损坏");
        return;
    }

    const auto innerRoot = innerDoc.object();
    if (innerRoot.value("version").toInt() != 1) {
        QMessageBox::warning(this, "失败", "备份内容版本不支持");
        return;
    }

    QHash<qint64, qint64> groupIdMap;
    groupIdMap.insert(1, 1);
    bool hasGroupMap = false;
    if (innerRoot.value("groups").isArray()) {
        struct BackupGroup final
        {
            qint64 id = 0;
            qint64 parentId = 0;
            QString name;
        };

        QHash<QString, qint64> existingGroups;
        for (const auto &g : repo_->listGroups()) {
            existingGroups.insert(QString("%1\n%2").arg(g.parentId).arg(g.name.trimmed().toLower()), g.id);
        }

        const auto ensureGroup = [&](qint64 parentId, const QString &name) -> qint64 {
            const auto trimmed = name.trimmed();
            if (trimmed.isEmpty())
                return 0;

            if (parentId <= 0)
                parentId = 1;

            const auto key = QString("%1\n%2").arg(parentId).arg(trimmed.toLower());
            if (existingGroups.contains(key))
                return existingGroups.value(key);

            const auto created = repo_->createGroup(parentId, trimmed);
            if (!created.has_value())
                return 0;
            existingGroups.insert(key, created.value());
            return created.value();
        };

        QVector<BackupGroup> pending;
        for (const auto &v : innerRoot.value("groups").toArray()) {
            const auto obj = v.toObject();
            BackupGroup g;
            g.id = static_cast<qint64>(obj.value("id").toDouble());
            g.parentId = static_cast<qint64>(obj.value("parent_id").toDouble());
            g.name = obj.value("name").toString();
            if (g.id <= 1)
                continue;
            if (g.name.trimmed().isEmpty())
                continue;
            pending.push_back(g);
        }

        bool progress = true;
        while (!pending.isEmpty() && progress) {
            progress = false;
            for (int i = 0; i < pending.size();) {
                const auto g = pending.at(i);
                auto parentOldId = g.parentId;
                if (parentOldId <= 0)
                    parentOldId = 1;
                if (!groupIdMap.contains(parentOldId)) {
                    ++i;
                    continue;
                }

                const auto parentNewId = groupIdMap.value(parentOldId, 1);
                const auto newId = ensureGroup(parentNewId, g.name);
                if (newId <= 0) {
                    QMessageBox::warning(this, "失败", repo_->lastError().isEmpty() ? "创建分组失败" : repo_->lastError());
                    return;
                }
                groupIdMap.insert(g.id, newId);
                pending.removeAt(i);
                progress = true;
            }
        }

        for (const auto &g : pending) {
            const auto newId = ensureGroup(1, g.name);
            if (newId > 0)
                groupIdMap.insert(g.id, newId);
        }

        hasGroupMap = true;
    }

    QSet<qint64> groupIds;
    if (!hasGroupMap) {
        for (const auto &g : repo_->listGroups())
            groupIds.insert(g.id);
    }

    const auto entries = innerRoot.value("entries").toArray();
    int imported = 0;
    for (const auto &v : entries) {
        const auto obj = v.toObject();
        PasswordEntrySecrets secrets;
        secrets.entry.title = obj.value("title").toString();
        secrets.entry.username = obj.value("username").toString();
        secrets.password = obj.value("password").toString();
        secrets.entry.url = obj.value("url").toString();
        const auto groupId = static_cast<qint64>(obj.value("group_id").toDouble());
        if (obj.contains("entry_type"))
            secrets.entry.type = passwordEntryTypeFromInt(obj.value("entry_type").toInt());
        if (hasGroupMap)
            secrets.entry.groupId = groupIdMap.value(groupId, 1);
        else
            secrets.entry.groupId = groupIds.contains(groupId) ? groupId : 1;
        secrets.entry.category = obj.value("category").toString();
        const auto tags = obj.value("tags");
        if (tags.isArray()) {
            for (const auto &tv : tags.toArray()) {
                const auto t = tv.toString().trimmed();
                if (!t.isEmpty())
                    secrets.entry.tags.push_back(t);
            }
        }
        secrets.notes = obj.value("notes").toString();
        const auto createdAt = static_cast<qint64>(obj.value("created_at").toDouble());
        const auto updatedAt = static_cast<qint64>(obj.value("updated_at").toDouble());

        if (secrets.entry.title.trimmed().isEmpty() || secrets.password.isEmpty())
            continue;

        if (!repo_->addEntryWithTimestamps(secrets, createdAt, updatedAt)) {
            QMessageBox::warning(this, "失败", repo_->lastError());
            return;
        }
        imported++;
    }

    refreshAll();
    QMessageBox::information(this, "完成", QString("导入完成：%1 条。").arg(imported));
}

void PasswordManagerPage::exportCsv()
{
    if (!vault_->isUnlocked()) {
        QMessageBox::information(this, "提示", "请先解锁");
        return;
    }

    const auto warning =
        "CSV 导出包含明文密码，存在安全风险。\n"
        "建议：导出后仅用于临时迁移，使用完立即删除，并避免通过网盘/聊天软件传输。\n\n"
        "确定继续导出吗？";

    if (QMessageBox::warning(this, "风险提示", warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    const auto path = QFileDialog::getSaveFileName(this, "导出 CSV", "", "CSV 文件 (*.csv)");
    if (path.isEmpty())
        return;

    QVector<PasswordEntrySecrets> entries;
    const auto summaries = repo_->listEntries();
    entries.reserve(summaries.size());
    for (const auto &summary : summaries) {
        const auto full = repo_->loadEntry(summary.id);
        if (!full.has_value()) {
            QMessageBox::warning(this, "失败", repo_->lastError());
            return;
        }
        entries.push_back(full.value());
    }

    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "失败", "无法写入文件");
        return;
    }
    out.write(exportPasswordCsv(entries));
    if (!out.commit()) {
        QMessageBox::warning(this, "失败", "写入文件失败");
        return;
    }

    QMessageBox::information(this, "完成", QString("已导出 %1 条。").arg(entries.size()));
}

void PasswordManagerPage::importCsv()
{
    if (!vault_->isUnlocked()) {
        QMessageBox::information(this, "提示", "请先解锁");
        return;
    }

    QString groupName = "全部";
    if (groupModel_) {
        const auto idx = groupModel_->indexForGroupId(selectedGroupId());
        if (idx.isValid())
            groupName = idx.data(Qt::DisplayRole).toString();
    }

    PasswordCsvImportDialog dlg(selectedGroupId(), groupName, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const auto path = dlg.csvPath();
    if (path.isEmpty())
        return;
    const auto options = dlg.options();

    const auto dbPath = QDir(AppPaths::appDataDir()).filePath("password.sqlite3");
    auto *thread = new QThread(this);
    auto *worker = new PasswordCsvImportWorker(path, dbPath, vault_->masterKey(), selectedGroupId(), nullptr);
    worker->setOptions(options);
    worker->moveToThread(thread);

    auto *progress = new QProgressDialog("正在导入 CSV…", "取消", 0, 0, this);
    progress->setWindowModality(Qt::WindowModal);
    progress->setAutoClose(false);
    progress->setAutoReset(false);
    progress->show();

    connect(progress, &QProgressDialog::canceled, this, [worker]() { worker->requestCancel(); });
    connect(thread, &QThread::started, worker, &PasswordCsvImportWorker::run);
    connect(worker, &PasswordCsvImportWorker::progressRangeChanged, progress, &QProgressDialog::setRange);
    connect(worker, &PasswordCsvImportWorker::progressValueChanged, progress, &QProgressDialog::setValue);

    connect(worker, &PasswordCsvImportWorker::failed, this, [this, progress](const QString &error) {
        progress->close();
        if (error.contains("取消")) {
            QMessageBox::information(this, "已取消", error);
        } else {
            QMessageBox::warning(this, "导入失败", error);
        }
    });

    connect(worker,
            &PasswordCsvImportWorker::finished,
            this,
            [this, progress](int inserted, int updated, int skippedDuplicates, int skippedInvalid, const QStringList &warnings) {
                progress->close();
                refreshAll();

                QString msg = QString("导入完成：\n新增：%1 条。\n更新：%2 条。\n跳过重复：%3 条。\n跳过无效：%4 条。")
                                  .arg(inserted)
                                  .arg(updated)
                                  .arg(skippedDuplicates)
                                  .arg(skippedInvalid);
                if (!warnings.isEmpty())
                    msg.append(QString("\n\n提示：\n- %1").arg(warnings.join("\n- ")));
                QMessageBox::information(this, "完成", msg);
            });

    connect(worker, &PasswordCsvImportWorker::finished, thread, &QThread::quit);
    connect(worker, &PasswordCsvImportWorker::failed, thread, &QThread::quit);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(thread, &QThread::finished, progress, &QObject::deleteLater);

    thread->start();
}
