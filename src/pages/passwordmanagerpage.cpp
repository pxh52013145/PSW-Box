#include "passwordmanagerpage.h"

#include "core/crypto.h"
#include "pages/passwordentrydialog.h"
#include "password/passwordentrymodel.h"
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
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

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

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        const auto model = sourceModel();
        if (!model)
            return true;

        const auto title = model->index(sourceRow, 0, sourceParent).data().toString();
        const auto username = model->index(sourceRow, 1, sourceParent).data().toString();
        const auto url = model->index(sourceRow, 2, sourceParent).data().toString();
        const auto category = model->index(sourceRow, 3, sourceParent).data().toString();

        if (!category_.isEmpty() && category_ != "全部" && category_ != category)
            return false;

        if (searchText_.isEmpty())
            return true;

        const auto haystack = QString("%1 %2 %3 %4").arg(title, username, url, category);
        return haystack.contains(searchText_, Qt::CaseInsensitive);
    }

private:
    QString searchText_;
    QString category_ = "全部";
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
    model_ = new PasswordEntryModel(this);
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

    topRow->addWidget(createBtn_);
    topRow->addWidget(unlockBtn_);
    topRow->addWidget(lockBtn_);
    topRow->addWidget(changePwdBtn_);
    topRow->addSpacing(12);
    topRow->addWidget(importBtn_);
    topRow->addWidget(exportBtn_);
    root->addLayout(topRow);

    auto *filterRow = new QHBoxLayout();
    searchEdit_ = new QLineEdit(this);
    searchEdit_->setPlaceholderText("搜索标题/账号/网址/分类…");
    categoryCombo_ = new QComboBox(this);
    categoryCombo_->addItem("全部");
    categoryCombo_->addItem("未分类");

    filterRow->addWidget(new QLabel("搜索：", this));
    filterRow->addWidget(searchEdit_, 1);
    filterRow->addSpacing(12);
    filterRow->addWidget(new QLabel("分类：", this));
    filterRow->addWidget(categoryCombo_);
    root->addLayout(filterRow);

    tableView_ = new QTableView(this);
    tableView_->setModel(proxy_);
    tableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView_->setSortingEnabled(true);
    tableView_->horizontalHeader()->setStretchLastSection(true);
    tableView_->verticalHeader()->setVisible(false);
    tableView_->setWordWrap(false);
    root->addWidget(tableView_, 1);

    auto *actionRow = new QHBoxLayout();
    addBtn_ = new QPushButton("新增", this);
    editBtn_ = new QPushButton("编辑", this);
    deleteBtn_ = new QPushButton("删除", this);
    copyUserBtn_ = new QPushButton("复制账号", this);
    copyPwdBtn_ = new QPushButton("复制密码", this);

    actionRow->addWidget(addBtn_);
    actionRow->addWidget(editBtn_);
    actionRow->addWidget(deleteBtn_);
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

    connect(addBtn_, &QPushButton::clicked, this, &PasswordManagerPage::addEntry);
    connect(editBtn_, &QPushButton::clicked, this, &PasswordManagerPage::editSelectedEntry);
    connect(deleteBtn_, &QPushButton::clicked, this, &PasswordManagerPage::deleteSelectedEntry);
    connect(copyUserBtn_, &QPushButton::clicked, this, &PasswordManagerPage::copySelectedUsername);
    connect(copyPwdBtn_, &QPushButton::clicked, this, &PasswordManagerPage::copySelectedPassword);

    connect(tableView_, &QTableView::doubleClicked, this, [this]() { editSelectedEntry(); });
    connect(tableView_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() { updateUiState(); });

    connect(searchEdit_, &QLineEdit::textChanged, this, [this](const QString &text) {
        static_cast<PasswordFilterProxyModel *>(proxy_)->setSearchText(text);
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

void PasswordManagerPage::updateUiState()
{
    const auto initialized = vault_->isInitialized();
    const auto unlocked = vault_->isUnlocked();
    const auto hasSelection = selectedEntryId() > 0;

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

    addBtn_->setEnabled(initialized && unlocked);
    editBtn_->setEnabled(initialized && unlocked && hasSelection);
    deleteBtn_->setEnabled(initialized && unlocked && hasSelection);
    copyPwdBtn_->setEnabled(initialized && unlocked && hasSelection);
    copyUserBtn_->setEnabled(hasSelection);

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

void PasswordManagerPage::addEntry()
{
    if (!vault_->isUnlocked()) {
        QMessageBox::information(this, "提示", "请先解锁");
        return;
    }

    PasswordEntryDialog dlg(repo_->listCategories(), this);
    dlg.setWindowTitle("新增密码条目");
    if (dlg.exec() != QDialog::Accepted)
        return;

    auto secrets = dlg.entry();
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

void PasswordManagerPage::editSelectedEntry()
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

    PasswordEntryDialog dlg(repo_->listCategories(), this);
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
        obj["category"] = full->entry.category;
        obj["notes"] = full->notes;
        obj["created_at"] = full->entry.createdAt.toSecsSinceEpoch();
        obj["updated_at"] = full->entry.updatedAt.toSecsSinceEpoch();
        entries.append(obj);
    }

    QJsonObject plainRoot;
    plainRoot["version"] = 1;
    plainRoot["exported_at"] = QDateTime::currentDateTime().toSecsSinceEpoch();
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

    const auto entries = innerRoot.value("entries").toArray();
    int imported = 0;
    for (const auto &v : entries) {
        const auto obj = v.toObject();
        PasswordEntrySecrets secrets;
        secrets.entry.title = obj.value("title").toString();
        secrets.entry.username = obj.value("username").toString();
        secrets.password = obj.value("password").toString();
        secrets.entry.url = obj.value("url").toString();
        secrets.entry.category = obj.value("category").toString();
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
