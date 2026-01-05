#include "passwordhealthdialog.h"

#include "password/passwordhealthmodel.h"
#include "password/passwordhealthworker.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QThread>
#include <QVBoxLayout>

namespace {

class HealthFilterProxyModel final : public QSortFilterProxyModel
{
public:
    explicit HealthFilterProxyModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {}

    void setOnlyIssues(bool onlyIssues)
    {
        onlyIssues_ = onlyIssues;
        invalidateFilter();
    }

    void setSearchText(const QString &text)
    {
        searchText_ = text.trimmed();
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        const auto model = sourceModel();
        if (!model)
            return true;

        const auto issues = model->index(sourceRow, PasswordHealthModel::IssuesColumn, sourceParent).data().toString();
        if (onlyIssues_ && issues.trimmed().isEmpty())
            return false;

        if (searchText_.isEmpty())
            return true;

        const auto title = model->index(sourceRow, PasswordHealthModel::TitleColumn, sourceParent).data().toString();
        const auto username = model->index(sourceRow, PasswordHealthModel::UsernameColumn, sourceParent).data().toString();
        const auto url = model->index(sourceRow, PasswordHealthModel::UrlColumn, sourceParent).data().toString();
        const auto score = model->index(sourceRow, PasswordHealthModel::ScoreColumn, sourceParent).data().toString();
        const auto updated = model->index(sourceRow, PasswordHealthModel::UpdatedColumn, sourceParent).data().toString();

        const auto haystack = QString("%1 %2 %3 %4 %5 %6").arg(title, username, url, issues, score, updated);
        return haystack.contains(searchText_, Qt::CaseInsensitive);
    }

private:
    bool onlyIssues_ = true;
    QString searchText_;
};

} // namespace

PasswordHealthDialog::PasswordHealthDialog(const QString &dbPath, const QByteArray &masterKey, QWidget *parent)
    : QDialog(parent), dbPath_(dbPath), masterKey_(masterKey)
{
    qRegisterMetaType<QVector<PasswordHealthItem>>();

    setupUi();
    wireSignals();
    startScan();
}

PasswordHealthDialog::~PasswordHealthDialog()
{
    cancelScan();

    if (thread_) {
        thread_->quit();
        thread_->wait();
    }

    masterKey_.fill('\0');
}

void PasswordHealthDialog::setupUi()
{
    setWindowTitle("安全报告");
    resize(980, 620);

    auto *root = new QVBoxLayout(this);

    statusLabel_ = new QLabel(this);
    statusLabel_->setText("准备扫描…");
    root->addWidget(statusLabel_);

    auto *toolsRow = new QHBoxLayout();
    searchEdit_ = new QLineEdit(this);
    searchEdit_->setPlaceholderText("搜索标题/账号/网址/问题…");
    toolsRow->addWidget(new QLabel("搜索：", this));
    toolsRow->addWidget(searchEdit_, 1);

    onlyIssuesCheck_ = new QCheckBox("仅显示有问题", this);
    onlyIssuesCheck_->setChecked(true);
    toolsRow->addWidget(onlyIssuesCheck_);

    pwnedCheck_ = new QCheckBox("在线泄露检查（需要联网）", this);
    pwnedCheck_->setToolTip("使用匿名哈希前缀查询（不上传明文密码），需要联网。");
    pwnedCheck_->setChecked(false);
    toolsRow->addWidget(pwnedCheck_);

    scanBtn_ = new QPushButton("重新扫描", this);
    toolsRow->addWidget(scanBtn_);
    cancelBtn_ = new QPushButton("取消", this);
    toolsRow->addWidget(cancelBtn_);
    root->addLayout(toolsRow);

    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 0);
    progressBar_->setTextVisible(false);
    root->addWidget(progressBar_);

    model_ = new PasswordHealthModel(this);
    proxy_ = new HealthFilterProxyModel(this);
    proxy_->setSourceModel(model_);

    tableView_ = new QTableView(this);
    tableView_->setModel(proxy_);
    tableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView_->setSortingEnabled(true);
    tableView_->horizontalHeader()->setStretchLastSection(true);
    tableView_->verticalHeader()->setVisible(false);
    root->addWidget(tableView_, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText("关闭");
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    root->addWidget(buttons);
}

void PasswordHealthDialog::wireSignals()
{
    connect(onlyIssuesCheck_, &QCheckBox::toggled, this, [this](bool checked) {
        static_cast<HealthFilterProxyModel *>(proxy_)->setOnlyIssues(checked);
    });
    connect(searchEdit_, &QLineEdit::textChanged, this, [this](const QString &text) {
        static_cast<HealthFilterProxyModel *>(proxy_)->setSearchText(text);
    });
    connect(scanBtn_, &QPushButton::clicked, this, &PasswordHealthDialog::startScan);
    connect(cancelBtn_, &QPushButton::clicked, this, &PasswordHealthDialog::cancelScan);

    connect(tableView_, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
        if (!index.isValid())
            return;
        const auto sourceIndex = proxy_->mapToSource(index);
        const auto item = model_->itemAt(sourceIndex.row());
        if (item.entryId > 0)
            emit entryActivated(item.entryId);
    });
}

void PasswordHealthDialog::startScan()
{
    if (running_) {
        QMessageBox::information(this, "提示", "正在扫描中…");
        return;
    }

    if (thread_) {
        QMessageBox::information(this, "提示", "上一次扫描尚未结束，请稍等…");
        return;
    }

    running_ = true;
    pwnedRequested_ = false;
    updateUiState();

    statusLabel_->setText("扫描中…");
    progressBar_->setRange(0, 0);
    progressBar_->setValue(0);

    bool enablePwned = pwnedCheck_ && pwnedCheck_->isChecked();
    if (enablePwned) {
        const auto info =
            "将进行在线泄露检查（Pwned Passwords）。\n\n"
            "说明：不会上传明文密码，只会发送 SHA-1 哈希的前 5 位前缀进行查询（k-anonymity 思路）。\n"
            "建议：在可信网络环境下使用。\n\n"
            "确定继续吗？";

        if (QMessageBox::question(this, "风险提示", info, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            != QMessageBox::Yes) {
            enablePwned = false;
            pwnedCheck_->setChecked(false);
        }
    }

    pwnedRequested_ = enablePwned;

    thread_ = new QThread(this);
    worker_ = new PasswordHealthWorker(dbPath_, masterKey_, enablePwned, enablePwned, nullptr);
    worker_->moveToThread(thread_);

    connect(thread_, &QThread::started, worker_, &PasswordHealthWorker::run);
    connect(worker_, &PasswordHealthWorker::progressRangeChanged, this, [this](int min, int max) {
        progressBar_->setRange(min, max);
    });
    connect(worker_, &PasswordHealthWorker::progressValueChanged, this, [this](int value) {
        if (progressBar_->maximum() > 0)
            progressBar_->setValue(value);
    });

    connect(worker_, &PasswordHealthWorker::failed, this, [this](const QString &error) {
        running_ = false;
        updateUiState();
        progressBar_->setRange(0, 1);
        progressBar_->setValue(0);
        statusLabel_->setText("扫描失败");
        QMessageBox::warning(this, "扫描失败", error);
    });

    connect(worker_, &PasswordHealthWorker::finished, this, [this](const QVector<PasswordHealthItem> &items) {
        model_->setItems(items);

        int weak = 0;
        int reused = 0;
        int stale = 0;
        int corrupted = 0;
        int pwned = 0;
        int pwnedUnknown = 0;
        int issues = 0;
        for (const auto &it : items) {
            if (it.weak)
                weak++;
            if (it.reused)
                reused++;
            if (it.stale)
                stale++;
            if (it.corrupted)
                corrupted++;
            if (it.pwned)
                pwned++;
            if (pwnedRequested_ && !it.corrupted && !it.pwnedChecked)
                pwnedUnknown++;
            if (it.hasIssues())
                issues++;
        }

        QStringList parts;
        parts << QString("弱密码 %1").arg(weak);
        parts << QString("重复 %1").arg(reused);
        parts << QString("久未更新 %1").arg(stale);
        parts << QString("损坏 %1").arg(corrupted);
        if (pwnedRequested_) {
            parts << QString("已泄露 %1").arg(pwned);
            if (pwnedUnknown > 0)
                parts << QString("未检查 %1").arg(pwnedUnknown);
        }

        statusLabel_->setText(QString("共 %1 条；有问题 %2 条（%3）。").arg(items.size()).arg(issues).arg(parts.join("，")));

        running_ = false;
        updateUiState();

        if (progressBar_->maximum() > 0) {
            progressBar_->setValue(progressBar_->maximum());
        } else {
            progressBar_->setRange(0, 1);
            progressBar_->setValue(1);
        }
    });

    connect(worker_, &PasswordHealthWorker::finished, thread_, &QThread::quit);
    connect(worker_, &PasswordHealthWorker::failed, thread_, &QThread::quit);
    connect(thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(thread_, &QThread::finished, thread_, &QObject::deleteLater);
    connect(thread_, &QThread::finished, this, [this]() {
        thread_ = nullptr;
        worker_ = nullptr;
    });

    thread_->start();
}

void PasswordHealthDialog::cancelScan()
{
    if (!running_)
        return;

    if (worker_)
        worker_->requestCancel();
    statusLabel_->setText("正在取消…");
}

void PasswordHealthDialog::updateUiState()
{
    scanBtn_->setEnabled(!running_);
    cancelBtn_->setEnabled(running_);
    searchEdit_->setEnabled(!running_);
    onlyIssuesCheck_->setEnabled(!running_);
    pwnedCheck_->setEnabled(!running_);
}
