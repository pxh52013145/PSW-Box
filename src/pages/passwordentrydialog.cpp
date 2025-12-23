#include "passwordentrydialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRandomGenerator>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QString makeRandomPassword(int length)
{
    static const QString chars = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789!@#$%^&*()-_=+[]{};:,.?";
    QString out;
    out.reserve(length);
    auto *rng = QRandomGenerator::system();
    for (int i = 0; i < length; ++i)
        out.append(chars.at(static_cast<int>(rng->generate() % static_cast<quint32>(chars.size()))));
    return out;
}

} // namespace

PasswordEntryDialog::PasswordEntryDialog(const QStringList &categories, QWidget *parent) : QDialog(parent)
{
    setupUi(categories);
    updateOkButtonState();
}

void PasswordEntryDialog::setupUi(const QStringList &categories)
{
    setWindowTitle("密码条目");
    resize(520, 360);

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

    urlEdit_ = new QLineEdit(this);
    urlEdit_->setPlaceholderText("https://example.com");
    form->addRow("网址：", urlEdit_);

    categoryCombo_ = new QComboBox(this);
    categoryCombo_->setEditable(true);
    categoryCombo_->addItem("");
    categoryCombo_->addItems(categories);
    form->addRow("分类：", categoryCombo_);

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
        data_.notes = notesEdit_->toPlainText();
        accept();
    });
    connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(titleEdit_, &QLineEdit::textChanged, this, &PasswordEntryDialog::updateOkButtonState);
    connect(passwordEdit_, &QLineEdit::textChanged, this, &PasswordEntryDialog::updateOkButtonState);

    connect(passwordToggleBtn_, &QToolButton::clicked, this, &PasswordEntryDialog::togglePasswordVisibility);
    connect(passwordGenBtn_, &QToolButton::clicked, this, &PasswordEntryDialog::generatePassword);
}

void PasswordEntryDialog::updateOkButtonState()
{
    const auto titleOk = !titleEdit_->text().trimmed().isEmpty();
    const auto passwordOk = !passwordEdit_->text().isEmpty();
    buttonBox_->button(QDialogButtonBox::Ok)->setEnabled(titleOk && passwordOk);
}

void PasswordEntryDialog::togglePasswordVisibility()
{
    passwordVisible_ = !passwordVisible_;
    passwordEdit_->setEchoMode(passwordVisible_ ? QLineEdit::Normal : QLineEdit::Password);
    passwordToggleBtn_->setText(passwordVisible_ ? "隐藏" : "显示");
}

void PasswordEntryDialog::generatePassword()
{
    passwordEdit_->setText(makeRandomPassword(16));
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

    notesEdit_->setPlainText(secrets.notes);
    updateOkButtonState();
}

PasswordEntrySecrets PasswordEntryDialog::entry() const
{
    return data_;
}

