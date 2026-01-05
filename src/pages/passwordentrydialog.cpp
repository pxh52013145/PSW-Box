#include "passwordentrydialog.h"

#include "passwordgeneratordialog.h"

#include "password/passwordstrength.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

PasswordEntryDialog::PasswordEntryDialog(const QStringList &categories, const QStringList &availableTags, QWidget *parent)
    : QDialog(parent)
{
    setupUi(categories, availableTags);
    updateOkButtonState();
    updateStrengthIndicator();
}

void PasswordEntryDialog::setupUi(const QStringList &categories, const QStringList &availableTags)
{
    setWindowTitle("密码条目");
    resize(560, 420);

    auto *root = new QVBoxLayout(this);

    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);

    titleEdit_ = new QLineEdit(this);
    titleEdit_->setPlaceholderText("例如：GitHub / QQ / 学校教务系统");
    form->addRow("标题：", titleEdit_);

    usernameEdit_ = new QLineEdit(this);
    usernameEdit_->setPlaceholderText("邮箱/手机号/用户名");
    form->addRow("账号：", usernameEdit_);

    auto *pwdRow = new QHBoxLayout();
    passwordEdit_ = new QLineEdit(this);
    passwordEdit_->setEchoMode(QLineEdit::Password);
    passwordEdit_->setPlaceholderText("输入密码");
    pwdRow->addWidget(passwordEdit_, 1);

    passwordToggleBtn_ = new QToolButton(this);
    passwordToggleBtn_->setText("显示");
    pwdRow->addWidget(passwordToggleBtn_);

    passwordGenBtn_ = new QToolButton(this);
    passwordGenBtn_->setText("生成");
    pwdRow->addWidget(passwordGenBtn_);

    auto *pwdContainer = new QWidget(this);
    pwdContainer->setLayout(pwdRow);
    form->addRow("密码：", pwdContainer);

    auto *strengthRow = new QHBoxLayout();
    strengthBar_ = new QProgressBar(this);
    strengthBar_->setRange(0, 100);
    strengthBar_->setTextVisible(false);
    strengthRow->addWidget(strengthBar_, 1);

    strengthLabel_ = new QLabel(this);
    strengthRow->addWidget(strengthLabel_);

    auto *strengthContainer = new QWidget(this);
    strengthContainer->setLayout(strengthRow);
    form->addRow("强度：", strengthContainer);

    urlEdit_ = new QLineEdit(this);
    urlEdit_->setPlaceholderText("https://example.com");
    form->addRow("网址：", urlEdit_);

    categoryCombo_ = new QComboBox(this);
    categoryCombo_->setEditable(true);
    categoryCombo_->addItem("");
    categoryCombo_->addItems(categories);
    form->addRow("分类：", categoryCombo_);

    auto *tagsContainer = new QWidget(this);
    auto *tagsLayout = new QVBoxLayout(tagsContainer);
    tagsLayout->setContentsMargins(0, 0, 0, 0);

    tagsList_ = new QListWidget(tagsContainer);
    tagsList_->setSelectionMode(QAbstractItemView::NoSelection);
    tagsList_->setMaximumHeight(96);
    for (const auto &tag : availableTags) {
        const auto trimmed = tag.trimmed();
        if (trimmed.isEmpty())
            continue;
        auto *item = new QListWidgetItem(trimmed, tagsList_);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
    tagsLayout->addWidget(tagsList_);

    auto *tagAddRow = new QHBoxLayout();
    newTagEdit_ = new QLineEdit(tagsContainer);
    newTagEdit_->setPlaceholderText("添加标签（回车）");
    tagAddRow->addWidget(newTagEdit_, 1);
    tagAddBtn_ = new QToolButton(tagsContainer);
    tagAddBtn_->setText("添加");
    tagAddRow->addWidget(tagAddBtn_);
    tagsLayout->addLayout(tagAddRow);
    form->addRow("标签：", tagsContainer);

    notesEdit_ = new QPlainTextEdit(this);
    notesEdit_->setPlaceholderText("备注（可选）");
    notesEdit_->setMinimumHeight(90);
    form->addRow("备注：", notesEdit_);

    root->addLayout(form);

    buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox_->button(QDialogButtonBox::Ok)->setText("确定");
    buttonBox_->button(QDialogButtonBox::Cancel)->setText("取消");
    root->addWidget(buttonBox_);

    connect(buttonBox_, &QDialogButtonBox::accepted, this, [this]() {
        data_.entry.title = titleEdit_->text().trimmed();
        data_.entry.username = usernameEdit_->text().trimmed();
        data_.password = passwordEdit_->text();
        data_.entry.url = urlEdit_->text().trimmed();
        data_.entry.category = categoryCombo_->currentText().trimmed();
        data_.entry.tags = selectedTags();
        data_.notes = notesEdit_->toPlainText();
        accept();
    });
    connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(titleEdit_, &QLineEdit::textChanged, this, &PasswordEntryDialog::updateOkButtonState);
    connect(passwordEdit_, &QLineEdit::textChanged, this, &PasswordEntryDialog::updateOkButtonState);
    connect(passwordEdit_, &QLineEdit::textChanged, this, &PasswordEntryDialog::updateStrengthIndicator);

    connect(passwordToggleBtn_, &QToolButton::clicked, this, &PasswordEntryDialog::togglePasswordVisibility);
    connect(passwordGenBtn_, &QToolButton::clicked, this, &PasswordEntryDialog::generatePassword);

    connect(tagAddBtn_, &QToolButton::clicked, this, &PasswordEntryDialog::addTagFromEdit);
    connect(newTagEdit_, &QLineEdit::returnPressed, this, &PasswordEntryDialog::addTagFromEdit);
}

void PasswordEntryDialog::updateOkButtonState()
{
    const auto titleOk = !titleEdit_->text().trimmed().isEmpty();
    const auto passwordOk = !passwordEdit_->text().isEmpty();
    buttonBox_->button(QDialogButtonBox::Ok)->setEnabled(titleOk && passwordOk);
}

void PasswordEntryDialog::updateStrengthIndicator()
{
    if (!strengthBar_ || !strengthLabel_)
        return;

    const auto pwd = passwordEdit_ ? passwordEdit_->text() : QString();
    const auto strength = evaluatePasswordStrength(pwd);
    strengthBar_->setValue(strength.score);
    strengthLabel_->setText(QString("%1(%2)").arg(strength.label).arg(strength.score));
}

void PasswordEntryDialog::togglePasswordVisibility()
{
    passwordVisible_ = !passwordVisible_;
    passwordEdit_->setEchoMode(passwordVisible_ ? QLineEdit::Normal : QLineEdit::Password);
    passwordToggleBtn_->setText(passwordVisible_ ? "隐藏" : "显示");
}

void PasswordEntryDialog::generatePassword()
{
    PasswordGeneratorDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const auto pwd = dlg.password();
    if (!pwd.isEmpty())
        passwordEdit_->setText(pwd);
}

void PasswordEntryDialog::addTagFromEdit()
{
    if (!tagsList_ || !newTagEdit_)
        return;

    const auto tag = newTagEdit_->text().trimmed();
    if (tag.isEmpty())
        return;

    for (int i = 0; i < tagsList_->count(); ++i) {
        auto *item = tagsList_->item(i);
        if (item && item->text().compare(tag, Qt::CaseInsensitive) == 0) {
            item->setCheckState(Qt::Checked);
            newTagEdit_->clear();
            return;
        }
    }

    auto *item = new QListWidgetItem(tag, tagsList_);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Checked);
    newTagEdit_->clear();
}

QStringList PasswordEntryDialog::selectedTags() const
{
    QStringList tags;
    if (!tagsList_)
        return tags;

    for (int i = 0; i < tagsList_->count(); ++i) {
        const auto *item = tagsList_->item(i);
        if (item && item->checkState() == Qt::Checked)
            tags.push_back(item->text());
    }

    return tags;
}

void PasswordEntryDialog::setEntry(const PasswordEntrySecrets &secrets)
{
    data_ = secrets;

    titleEdit_->setText(secrets.entry.title);
    usernameEdit_->setText(secrets.entry.username);
    passwordEdit_->setText(secrets.password);
    urlEdit_->setText(secrets.entry.url);

    const auto category = secrets.entry.category.trimmed();
    const auto idx = categoryCombo_->findText(category);
    if (idx >= 0) {
        categoryCombo_->setCurrentIndex(idx);
    } else {
        categoryCombo_->setCurrentText(category);
    }

    if (tagsList_) {
        for (int i = 0; i < tagsList_->count(); ++i) {
            auto *item = tagsList_->item(i);
            if (!item)
                continue;
            const auto checked = secrets.entry.tags.contains(item->text(), Qt::CaseInsensitive);
            item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        }

        for (const auto &tag : secrets.entry.tags) {
            bool found = false;
            for (int i = 0; i < tagsList_->count(); ++i) {
                const auto *item = tagsList_->item(i);
                if (item && item->text().compare(tag, Qt::CaseInsensitive) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                auto *item = new QListWidgetItem(tag, tagsList_);
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(Qt::Checked);
            }
        }
    }

    notesEdit_->setPlainText(secrets.notes);
    updateOkButtonState();
    updateStrengthIndicator();
}

PasswordEntrySecrets PasswordEntryDialog::entry() const
{
    return data_;
}
