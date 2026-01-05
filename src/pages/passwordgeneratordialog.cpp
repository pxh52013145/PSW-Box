#include "passwordgeneratordialog.h"

#include "password/passwordstrength.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

PasswordGeneratorDialog::PasswordGeneratorDialog(QWidget *parent) : QDialog(parent)
{
    setupUi();
    wireSignals();
    regenerate();
}

QString PasswordGeneratorDialog::password() const
{
    return password_;
}

void PasswordGeneratorDialog::setupUi()
{
    setWindowTitle("生成密码");
    resize(560, 260);

    auto *root = new QVBoxLayout(this);

    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);

    lengthSpin_ = new QSpinBox(this);
    lengthSpin_->setRange(4, 64);
    lengthSpin_->setValue(16);
    form->addRow("长度：", lengthSpin_);

    auto *typesRow = new QHBoxLayout();
    upperCheck_ = new QCheckBox("大写", this);
    lowerCheck_ = new QCheckBox("小写", this);
    digitCheck_ = new QCheckBox("数字", this);
    symbolCheck_ = new QCheckBox("符号", this);
    upperCheck_->setChecked(true);
    lowerCheck_->setChecked(true);
    digitCheck_->setChecked(true);
    symbolCheck_->setChecked(true);
    typesRow->addWidget(upperCheck_);
    typesRow->addWidget(lowerCheck_);
    typesRow->addWidget(digitCheck_);
    typesRow->addWidget(symbolCheck_);
    typesRow->addStretch(1);

    auto *typesContainer = new QWidget(this);
    typesContainer->setLayout(typesRow);
    form->addRow("字符集：", typesContainer);

    auto *rulesRow = new QHBoxLayout();
    excludeAmbiguousCheck_ = new QCheckBox("排除易混淆字符（O/0/I/l/1）", this);
    excludeAmbiguousCheck_->setChecked(true);
    requireEachCheck_ = new QCheckBox("每类至少 1 个", this);
    requireEachCheck_->setChecked(true);
    rulesRow->addWidget(excludeAmbiguousCheck_);
    rulesRow->addWidget(requireEachCheck_);
    rulesRow->addStretch(1);

    auto *rulesContainer = new QWidget(this);
    rulesContainer->setLayout(rulesRow);
    form->addRow("规则：", rulesContainer);

    root->addLayout(form);

    auto *previewRow = new QHBoxLayout();
    previewEdit_ = new QLineEdit(this);
    previewEdit_->setReadOnly(true);
    previewRow->addWidget(previewEdit_, 1);

    regenBtn_ = new QPushButton("重新生成", this);
    previewRow->addWidget(regenBtn_);

    copyBtn_ = new QPushButton("复制", this);
    previewRow->addWidget(copyBtn_);

    root->addLayout(previewRow);

    auto *strengthRow = new QHBoxLayout();
    strengthBar_ = new QProgressBar(this);
    strengthBar_->setRange(0, 100);
    strengthBar_->setTextVisible(false);
    strengthRow->addWidget(strengthBar_, 1);

    strengthLabel_ = new QLabel(this);
    strengthRow->addWidget(strengthLabel_);
    root->addLayout(strengthRow);

    errorLabel_ = new QLabel(this);
    errorLabel_->setStyleSheet("color:#c00000");
    root->addWidget(errorLabel_);

    buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox_->button(QDialogButtonBox::Ok)->setText("使用该密码");
    buttonBox_->button(QDialogButtonBox::Cancel)->setText("取消");
    root->addWidget(buttonBox_);
}

void PasswordGeneratorDialog::wireSignals()
{
    connect(buttonBox_, &QDialogButtonBox::accepted, this, [this]() {
        if (password_.isEmpty())
            regenerate();
        if (password_.isEmpty())
            return;
        accept();
    });
    connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(regenBtn_, &QPushButton::clicked, this, &PasswordGeneratorDialog::regenerate);
    connect(copyBtn_, &QPushButton::clicked, this, [this]() {
        if (password_.isEmpty())
            return;
        QApplication::clipboard()->setText(password_);
    });

    const auto regenOnChange = [this]() { regenerate(); };
    connect(lengthSpin_, &QSpinBox::valueChanged, this, regenOnChange);
    connect(upperCheck_, &QCheckBox::toggled, this, regenOnChange);
    connect(lowerCheck_, &QCheckBox::toggled, this, regenOnChange);
    connect(digitCheck_, &QCheckBox::toggled, this, regenOnChange);
    connect(symbolCheck_, &QCheckBox::toggled, this, regenOnChange);
    connect(excludeAmbiguousCheck_, &QCheckBox::toggled, this, regenOnChange);
    connect(requireEachCheck_, &QCheckBox::toggled, this, regenOnChange);
}

PasswordGeneratorOptions PasswordGeneratorDialog::optionsFromUi() const
{
    PasswordGeneratorOptions opt;
    opt.length = lengthSpin_->value();
    opt.useUpper = upperCheck_->isChecked();
    opt.useLower = lowerCheck_->isChecked();
    opt.useDigits = digitCheck_->isChecked();
    opt.useSymbols = symbolCheck_->isChecked();
    opt.excludeAmbiguous = excludeAmbiguousCheck_->isChecked();
    opt.requireEachSelectedType = requireEachCheck_->isChecked();
    return opt;
}

void PasswordGeneratorDialog::regenerate()
{
    errorLabel_->clear();

    QString error;
    const auto pwd = generatePassword(optionsFromUi(), &error);
    if (pwd.isEmpty()) {
        password_.clear();
        previewEdit_->clear();
        strengthBar_->setValue(0);
        strengthLabel_->setText("极弱(0)");
        errorLabel_->setText(error.isEmpty() ? "生成失败" : error);
        return;
    }

    password_ = pwd;
    previewEdit_->setText(pwd);

    const auto strength = evaluatePasswordStrength(pwd);
    strengthBar_->setValue(strength.score);
    strengthLabel_->setText(QString("%1(%2)").arg(strength.label).arg(strength.score));
}

