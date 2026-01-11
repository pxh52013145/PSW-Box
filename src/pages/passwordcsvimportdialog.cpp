#include "passwordcsvimportdialog.h"

#include "password/passwordcsv.h"
#include "password/passwordentry.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>

namespace {

QString formatDisplayText(PasswordCsvFormat format)
{
    switch (format) {
    case PasswordCsvFormat::Toolbox:
        return "Toolbox CSV";
    case PasswordCsvFormat::Chrome:
        return "Chrome/Edge CSV";
    case PasswordCsvFormat::KeePassXC:
        return "KeePassXC CSV";
    default:
        return "Unknown";
    }
}

QString dupPolicyLabel(PasswordCsvDuplicatePolicy p)
{
    switch (p) {
    case PasswordCsvDuplicatePolicy::Skip:
        return "跳过重复（推荐）";
    case PasswordCsvDuplicatePolicy::Update:
        return "更新重复（用 CSV 覆盖库中条目）";
    case PasswordCsvDuplicatePolicy::ImportAnyway:
        return "全部导入（允许重复）";
    default:
        return "跳过重复（推荐）";
    }
}

} // namespace

PasswordCsvImportDialog::PasswordCsvImportDialog(qint64 baseGroupId, const QString &baseGroupName, QWidget *parent)
    : QDialog(parent),
      baseGroupId_(baseGroupId > 0 ? baseGroupId : 1),
      baseGroupName_(baseGroupName.trimmed().isEmpty() ? QString("全部") : baseGroupName.trimmed())
{
    setupUi();
    wireSignals();
    reloadPreview();
}

PasswordCsvImportDialog::~PasswordCsvImportDialog() = default;

QString PasswordCsvImportDialog::csvPath() const
{
    return pathEdit_ ? pathEdit_->text().trimmed() : QString();
}

PasswordCsvImportOptions PasswordCsvImportDialog::options() const
{
    PasswordCsvImportOptions opt;

    if (dupPolicyCombo_)
        opt.duplicatePolicy = static_cast<PasswordCsvDuplicatePolicy>(dupPolicyCombo_->currentData().toInt());
    if (createGroupsCheck_)
        opt.createGroupsFromCategoryPath = createGroupsCheck_->isChecked();
    if (typeCombo_)
        opt.defaultEntryType = passwordEntryTypeFromInt(typeCombo_->currentData().toInt());

    return opt;
}

void PasswordCsvImportDialog::setupUi()
{
    setWindowTitle("CSV 导入向导");
    resize(900, 640);

    auto *root = new QVBoxLayout(this);

    auto *tip = new QLabel(
        "说明：此功能会读取 CSV 中的明文密码并写入 Vault（加密存储）。\n"
        "建议仅导入可信来源的 CSV，导入完成后及时删除原文件。",
        this);
    tip->setWordWrap(true);
    root->addWidget(tip);

    auto *fileRow = new QHBoxLayout();
    pathEdit_ = new QLineEdit(this);
    pathEdit_->setPlaceholderText("选择 CSV 文件…");
    fileRow->addWidget(new QLabel("CSV 文件：", this));
    fileRow->addWidget(pathEdit_, 1);
    browseBtn_ = new QPushButton("浏览…", this);
    fileRow->addWidget(browseBtn_);
    root->addLayout(fileRow);

    formatLabel_ = new QLabel(this);
    formatLabel_->setText("识别：-");
    root->addWidget(formatLabel_);

    summaryLabel_ = new QLabel(this);
    summaryLabel_->setWordWrap(true);
    summaryLabel_->setText("预览：未选择文件");
    root->addWidget(summaryLabel_);

    auto *optsBox = new QGroupBox("导入选项", this);
    auto *optsForm = new QFormLayout(optsBox);

    optsForm->addRow("导入到分组：", new QLabel(QString("%1（#%2）").arg(baseGroupName_).arg(baseGroupId_), optsBox));

    createGroupsCheck_ = new QCheckBox("从 CSV 的 Group/Category 字段自动创建分组（按 / 分隔）", optsBox);
    createGroupsCheck_->setChecked(false);
    optsForm->addRow("分组映射：", createGroupsCheck_);

    dupPolicyCombo_ = new QComboBox(optsBox);
    dupPolicyCombo_->addItem(dupPolicyLabel(PasswordCsvDuplicatePolicy::Skip), static_cast<int>(PasswordCsvDuplicatePolicy::Skip));
    dupPolicyCombo_->addItem(dupPolicyLabel(PasswordCsvDuplicatePolicy::Update), static_cast<int>(PasswordCsvDuplicatePolicy::Update));
    dupPolicyCombo_->addItem(dupPolicyLabel(PasswordCsvDuplicatePolicy::ImportAnyway),
                             static_cast<int>(PasswordCsvDuplicatePolicy::ImportAnyway));
    optsForm->addRow("重复处理：", dupPolicyCombo_);

    typeCombo_ = new QComboBox(optsBox);
    typeCombo_->addItem(passwordEntryTypeLabel(PasswordEntryType::WebLogin), static_cast<int>(PasswordEntryType::WebLogin));
    typeCombo_->addItem(passwordEntryTypeLabel(PasswordEntryType::DesktopClient), static_cast<int>(PasswordEntryType::DesktopClient));
    typeCombo_->addItem(passwordEntryTypeLabel(PasswordEntryType::ApiKeyToken), static_cast<int>(PasswordEntryType::ApiKeyToken));
    typeCombo_->addItem(passwordEntryTypeLabel(PasswordEntryType::DatabaseCredential),
                        static_cast<int>(PasswordEntryType::DatabaseCredential));
    typeCombo_->addItem(passwordEntryTypeLabel(PasswordEntryType::ServerSsh), static_cast<int>(PasswordEntryType::ServerSsh));
    typeCombo_->addItem(passwordEntryTypeLabel(PasswordEntryType::DeviceWifi), static_cast<int>(PasswordEntryType::DeviceWifi));
    optsForm->addRow("默认类型：", typeCombo_);

    root->addWidget(optsBox);

    previewModel_ = new QStandardItemModel(this);
    previewModel_->setColumnCount(5);
    previewModel_->setHorizontalHeaderLabels({"标题", "账号", "网址", "分类/Group", "标签"});

    previewView_ = new QTableView(this);
    previewView_->setModel(previewModel_);
    previewView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    previewView_->setSelectionMode(QAbstractItemView::SingleSelection);
    previewView_->horizontalHeader()->setStretchLastSection(true);
    previewView_->verticalHeader()->setVisible(false);
    root->addWidget(previewView_, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("开始导入");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        if (csvPath().isEmpty()) {
            QMessageBox::warning(this, "导入失败", "请先选择 CSV 文件");
            return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void PasswordCsvImportDialog::wireSignals()
{
    connect(browseBtn_, &QPushButton::clicked, this, &PasswordCsvImportDialog::browseCsv);
    connect(pathEdit_, &QLineEdit::textChanged, this, [this]() { reloadPreview(); });
}

void PasswordCsvImportDialog::browseCsv()
{
    const auto path = QFileDialog::getOpenFileName(this, "选择 CSV", "", "CSV 文件 (*.csv)");
    if (path.isEmpty())
        return;
    if (pathEdit_)
        pathEdit_->setText(path);
}

void PasswordCsvImportDialog::reloadPreview()
{
    if (!previewModel_)
        return;

    previewModel_->removeRows(0, previewModel_->rowCount());

    const auto path = csvPath();
    if (path.isEmpty()) {
        if (formatLabel_)
            formatLabel_->setText("识别：-");
        if (summaryLabel_)
            summaryLabel_->setText("预览：未选择文件");
        setOkEnabled(false);
        return;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        setPreviewError("无法读取 CSV 文件");
        return;
    }

    const auto bytes = f.readAll();

    QString detectError;
    const auto info = detectPasswordCsv(bytes, &detectError);
    if (!detectError.isEmpty()) {
        setPreviewError(detectError);
        return;
    }

    if (formatLabel_)
        formatLabel_->setText(QString("识别：%1（分隔符：%2）").arg(formatDisplayText(info.format)).arg(info.delimiter));

    const auto parsed = parsePasswordCsv(bytes, baseGroupId_);
    if (!parsed.ok()) {
        setPreviewError(parsed.error);
        return;
    }

    createGroupsCheck_->setEnabled(parsed.totalRows > 0 && info.hasCategoryLikeColumn);

    const int previewCount = std::min(20, static_cast<int>(parsed.entries.size()));
    previewModel_->setRowCount(previewCount);
    for (int i = 0; i < previewCount; ++i) {
        const auto &e = parsed.entries.at(i).entry;
        previewModel_->setItem(i, 0, new QStandardItem(e.title));
        previewModel_->setItem(i, 1, new QStandardItem(e.username));
        previewModel_->setItem(i, 2, new QStandardItem(e.url));
        previewModel_->setItem(i, 3, new QStandardItem(e.category));
        previewModel_->setItem(i, 4, new QStandardItem(e.tags.join(",")));
    }

    if (summaryLabel_) {
        QString msg = QString("共 %1 行，解析到 %2 条可导入记录。\n空密码跳过：%3，非法跳过：%4。")
                          .arg(parsed.totalRows)
                          .arg(parsed.entries.size())
                          .arg(parsed.skippedEmpty)
                          .arg(parsed.skippedInvalid);
        if (!parsed.warnings.isEmpty())
            msg.append(QString("\n\n提示：\n- %1").arg(parsed.warnings.join("\n- ")));
        summaryLabel_->setText(msg);
    }

    setOkEnabled(true);
}

void PasswordCsvImportDialog::setPreviewError(const QString &error)
{
    if (formatLabel_)
        formatLabel_->setText("识别：-");
    if (summaryLabel_)
        summaryLabel_->setText(QString("预览失败：%1").arg(error));
    setOkEnabled(false);
}

void PasswordCsvImportDialog::setOkEnabled(bool enabled)
{
    if (auto *box = findChild<QDialogButtonBox *>()) {
        if (auto *ok = box->button(QDialogButtonBox::Ok))
            ok->setEnabled(enabled);
    }
}
