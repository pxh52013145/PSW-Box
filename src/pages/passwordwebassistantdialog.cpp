#include "passwordwebassistantdialog.h"

#ifdef TBX_HAS_WEBENGINE

#include "pages/passwordentrydialog.h"
#include "password/passwordrepository.h"
#include "password/passwordurl.h"
#include "password/passwordwebloginmatcher.h"
#include "password/passwordvault.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineView>

namespace {

class PasswordWebAssistantBridge final : public QObject
{
    Q_OBJECT

public:
    PasswordWebAssistantBridge(PasswordRepository *repo, PasswordVault *vault, QWidget *uiParent)
        : repo_(repo), vault_(vault), uiParent_(uiParent)
    {
    }

    Q_INVOKABLE void requestCredentials(const QString &pageUrl)
    {
        if (!repo_ || !vault_) {
            QMessageBox::warning(uiParent_, "失败", "Web 助手未初始化");
            return;
        }

        if (!vault_->isUnlocked()) {
            QMessageBox::information(uiParent_, "提示", "请先解锁 Vault 再使用 Web 助手。");
            return;
        }

        const auto host = PasswordUrl::hostFromUrl(pageUrl);
        if (host.isEmpty()) {
            QMessageBox::warning(uiParent_, "失败", "无法解析当前网页域名。");
            return;
        }

        QVector<PasswordEntry> candidates;
        for (const auto &e : repo_->listEntries()) {
            if (e.type != PasswordEntryType::WebLogin)
                continue;
            if (!PasswordUrl::urlMatchesHost(e.url, host))
                continue;
            candidates.push_back(e);
        }

        if (candidates.isEmpty()) {
            QMessageBox::information(uiParent_, "未找到", QString("未找到 %1 的 Web 登录条目。").arg(host));
            return;
        }

        QStringList items;
        items.reserve(candidates.size());
        for (const auto &e : candidates) {
            const auto user = e.username.trimmed().isEmpty() ? QString("<无账号>") : e.username.trimmed();
            items.push_back(QString("%1 | %2 | #%3").arg(e.title, user).arg(e.id));
        }

        bool ok = false;
        const auto picked = QInputDialog::getItem(uiParent_, "选择账号", QString("为 %1 选择要填充的账号：").arg(host), items, 0, false, &ok);
        if (!ok || picked.isEmpty())
            return;

        qint64 entryId = 0;
        {
            const auto parts = picked.split('#');
            if (parts.size() >= 2)
                entryId = parts.last().toLongLong();
        }
        if (entryId <= 0) {
            QMessageBox::warning(uiParent_, "失败", "选择的条目无效。");
            return;
        }

        const auto loaded = repo_->loadEntry(entryId);
        if (!loaded.has_value()) {
            QMessageBox::warning(uiParent_, "失败", repo_->lastError());
            return;
        }

        emit fillCredentials(loaded->entry.username, loaded->password);
    }

    Q_INVOKABLE void reportSubmitted(const QString &pageUrl,
                                     const QString &pageTitle,
                                     const QString &username,
                                     const QString &password,
                                     const QString &kind)
    {
        if (!repo_ || !vault_) {
            QMessageBox::warning(uiParent_, "失败", "Web 助手未初始化");
            return;
        }

        if (!vault_->isUnlocked()) {
            QMessageBox::information(uiParent_, "提示", "请先解锁 Vault 再使用 Web 助手。");
            return;
        }

        if (password.isEmpty())
            return;

        const auto match = PasswordWebLoginMatcher::match(repo_->listEntries(), pageUrl, username);
        const auto host = match.host;
        if (host.isEmpty())
            return;

        const auto isChange = kind.trimmed().compare("change", Qt::CaseInsensitive) == 0;
        const auto displayUser = username.trimmed().isEmpty() ? QString("<无账号>") : username.trimmed();

        auto pickEntryId = [&](const QVector<PasswordEntry> &candidates, const QString &title, const QString &label) -> qint64 {
            if (candidates.isEmpty())
                return 0;

            QStringList items;
            items.reserve(candidates.size());
            for (const auto &e : candidates) {
                const auto user = e.username.trimmed().isEmpty() ? QString("<无账号>") : e.username.trimmed();
                items.push_back(QString("%1 | %2 | #%3").arg(e.title, user).arg(e.id));
            }

            bool ok = false;
            const auto picked = QInputDialog::getItem(uiParent_, title, label, items, 0, false, &ok);
            if (!ok || picked.isEmpty())
                return 0;

            const auto parts = picked.split('#');
            if (parts.size() < 2)
                return 0;
            return parts.last().toLongLong();
        };

        auto editEntryAndSave = [&](PasswordEntrySecrets &secrets, bool isUpdate) -> bool {
            PasswordEntryDialog dlg(repo_->listCategories(), repo_->listAllTags(), uiParent_);
            dlg.setEntry(secrets);
            if (dlg.exec() != QDialog::Accepted)
                return false;

            secrets = dlg.entry();
            if (isUpdate) {
                if (!repo_->updateEntry(secrets)) {
                    QMessageBox::warning(uiParent_, "失败", repo_->lastError());
                    return false;
                }
                QMessageBox::information(uiParent_, "完成", "已更新条目。");
                return true;
            }

            if (!repo_->addEntry(secrets)) {
                QMessageBox::warning(uiParent_, "失败", repo_->lastError());
                return false;
            }
            QMessageBox::information(uiParent_, "完成", "已保存为新条目。");
            return true;
        };

        auto quickSaveNew = [&]() {
            PasswordEntrySecrets secrets;
            secrets.entry.groupId = 1;
            secrets.entry.type = PasswordEntryType::WebLogin;
            secrets.entry.title = host;
            secrets.entry.username = username.trimmed();
            secrets.entry.url = pageUrl;
            secrets.password = password;

            if (!repo_->addEntry(secrets)) {
                QMessageBox::warning(uiParent_, "失败", repo_->lastError());
                return;
            }
            QMessageBox::information(uiParent_, "完成", "已保存为新条目。");
        };

        auto quickUpdate = [&](qint64 entryId) {
            const auto loaded = repo_->loadEntry(entryId);
            if (!loaded.has_value()) {
                QMessageBox::warning(uiParent_, "失败", repo_->lastError());
                return;
            }

            auto secrets = loaded.value();
            if (!isChange && secrets.password == password)
                return;

            secrets.password = password;
            if (secrets.entry.url.trimmed().isEmpty())
                secrets.entry.url = pageUrl;

            if (!repo_->updateEntry(secrets)) {
                QMessageBox::warning(uiParent_, "失败", repo_->lastError());
                return;
            }
            QMessageBox::information(uiParent_, "完成", "已更新条目。");
        };

        auto confirmSaveNew = [&]() {
            QMessageBox box(uiParent_);
            box.setWindowTitle("保存密码？");
            box.setIcon(QMessageBox::Question);
            box.setText(QString("检测到提交了账号密码：\n站点：%1\n账号：%2\n\n是否保存为新条目？").arg(host, displayUser));
            auto *saveBtn = box.addButton("保存", QMessageBox::AcceptRole);
            auto *editBtn = box.addButton("编辑后保存", QMessageBox::ActionRole);
            box.addButton("忽略", QMessageBox::RejectRole);
            box.exec();

            if (box.clickedButton() == saveBtn) {
                quickSaveNew();
                return;
            }
            if (box.clickedButton() == editBtn) {
                PasswordEntrySecrets secrets;
                secrets.entry.groupId = 1;
                secrets.entry.type = PasswordEntryType::WebLogin;
                secrets.entry.title = host;
                secrets.entry.username = username.trimmed();
                secrets.entry.url = pageUrl;
                secrets.password = password;
                (void)editEntryAndSave(secrets, false);
                return;
            }
        };

        auto confirmUpdate = [&](qint64 entryId) {
            const auto loaded = repo_->loadEntry(entryId);
            if (!loaded.has_value()) {
                QMessageBox::warning(uiParent_, "失败", repo_->lastError());
                return;
            }

            auto secrets = loaded.value();
            if (!isChange && secrets.password == password)
                return;

            const auto entryUser =
                secrets.entry.username.trimmed().isEmpty() ? QString("<无账号>") : secrets.entry.username.trimmed();

            QMessageBox box(uiParent_);
            box.setWindowTitle("更新密码？");
            box.setIcon(QMessageBox::Question);
            box.setText(QString("检测到提交了密码：\n站点：%1\n账号：%2\n\n是否更新已存在的条目？").arg(host, entryUser));
            auto *updateBtn = box.addButton("更新", QMessageBox::AcceptRole);
            auto *editBtn = box.addButton("编辑后更新", QMessageBox::ActionRole);
            box.addButton("忽略", QMessageBox::RejectRole);
            box.exec();

            if (box.clickedButton() == updateBtn) {
                quickUpdate(entryId);
                return;
            }
            if (box.clickedButton() == editBtn) {
                secrets.password = password;
                if (secrets.entry.url.trimmed().isEmpty())
                    secrets.entry.url = pageUrl;
                (void)editEntryAndSave(secrets, true);
                return;
            }
        };

        if (!match.userEntries.isEmpty()) {
            const auto entryId = match.userEntries.size() == 1
                                     ? match.userEntries.at(0).id
                                     : pickEntryId(match.userEntries, "选择条目", QString("在 %1 选择要更新的账号：").arg(host));
            if (entryId > 0)
                confirmUpdate(entryId);
            return;
        }

        if (isChange && !match.hostEntries.isEmpty()) {
            QMessageBox box(uiParent_);
            box.setWindowTitle("保存/更新？");
            box.setIcon(QMessageBox::Question);
            box.setText(QString("检测到可能是改密提交：\n站点：%1\n账号：%2\n\n未找到精确匹配条目，要更新现有条目还是保存为新条目？")
                            .arg(host, displayUser));
            auto *updateExistingBtn = box.addButton("更新现有", QMessageBox::AcceptRole);
            auto *saveNewBtn = box.addButton("保存为新条目", QMessageBox::ActionRole);
            box.addButton("忽略", QMessageBox::RejectRole);
            box.exec();

            if (box.clickedButton() == updateExistingBtn) {
                const auto entryId =
                    pickEntryId(match.hostEntries, "选择条目", QString("在 %1 选择要更新的账号：").arg(host));
                if (entryId > 0)
                    confirmUpdate(entryId);
                return;
            }

            if (box.clickedButton() == saveNewBtn) {
                confirmSaveNew();
                return;
            }

            return;
        }

        if (username.trimmed().isEmpty() && !match.hostEntries.isEmpty()) {
            QMessageBox box(uiParent_);
            box.setWindowTitle("保存/更新？");
            box.setIcon(QMessageBox::Question);
            box.setText(QString("检测到提交了密码，但未识别账号：\n站点：%1\n\n要更新现有条目还是保存为新条目？").arg(host));
            auto *updateExistingBtn = box.addButton("更新现有", QMessageBox::AcceptRole);
            auto *saveNewBtn = box.addButton("保存为新条目", QMessageBox::ActionRole);
            box.addButton("忽略", QMessageBox::RejectRole);
            box.exec();

            if (box.clickedButton() == updateExistingBtn) {
                const auto entryId =
                    pickEntryId(match.hostEntries, "选择条目", QString("在 %1 选择要更新的账号：").arg(host));
                if (entryId > 0)
                    confirmUpdate(entryId);
                return;
            }

            if (box.clickedButton() == saveNewBtn) {
                confirmSaveNew();
                return;
            }

            return;
        }

        Q_UNUSED(pageTitle);
        confirmSaveNew();
    }

signals:
    void fillCredentials(const QString &username, const QString &password);

private:
    PasswordRepository *repo_ = nullptr;
    PasswordVault *vault_ = nullptr;
    QWidget *uiParent_ = nullptr;
};

} // namespace

PasswordWebAssistantDialog::PasswordWebAssistantDialog(PasswordRepository *repo, PasswordVault *vault, QWidget *parent)
    : QDialog(parent), repo_(repo), vault_(vault)
{
    setupUi();
    setupWebChannel();
    injectScripts();

    navigateToUrlText("https://");
}

PasswordWebAssistantDialog::~PasswordWebAssistantDialog() = default;

void PasswordWebAssistantDialog::setupUi()
{
    setWindowTitle("Web 助手（演示）");
    resize(980, 720);

    auto *root = new QVBoxLayout(this);

    auto *tip = new QLabel(
        "用法：打开登录页 → 密码框右侧会出现“钥”按钮 → 点击后选择账号 → 自动填充。\n"
        "保存/更新：表单提交（登录/注册/改密）时会提示保存或更新。\n"
        "说明：这是内置 WebView 的演示实现，不覆盖系统浏览器。",
        this);
    tip->setWordWrap(true);
    root->addWidget(tip);

    auto *navRow = new QHBoxLayout();
    urlEdit_ = new QLineEdit(this);
    urlEdit_->setPlaceholderText("输入网址，例如：https://github.com/login");
    navRow->addWidget(urlEdit_, 1);

    goBtn_ = new QPushButton("打开", this);
    navRow->addWidget(goBtn_);

    demoBtn_ = new QPushButton("演示页", this);
    navRow->addWidget(demoBtn_);
    root->addLayout(navRow);

    view_ = new QWebEngineView(this);
    root->addWidget(view_, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText("关闭");
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(goBtn_, &QPushButton::clicked, this, [this]() { navigateToUrlText(urlEdit_->text()); });
    connect(urlEdit_, &QLineEdit::returnPressed, this, [this]() { navigateToUrlText(urlEdit_->text()); });
    connect(demoBtn_, &QPushButton::clicked, this, &PasswordWebAssistantDialog::loadDemoPage);

    connect(view_, &QWebEngineView::urlChanged, this, [this](const QUrl &url) {
        if (urlEdit_)
            urlEdit_->setText(url.toString());
    });
}

void PasswordWebAssistantDialog::setupWebChannel()
{
    channel_ = new QWebChannel(this);
    auto *bridge = new PasswordWebAssistantBridge(repo_, vault_, this);
    channel_->registerObject("tbxBridge", bridge);
    view_->page()->setWebChannel(channel_);
}

void PasswordWebAssistantDialog::injectScripts()
{
    QWebEngineScript script;
    script.setName("tbx_password_assistant");
    script.setInjectionPoint(QWebEngineScript::DocumentReady);
    script.setRunsOnSubFrames(true);

    script.setSourceCode(R"JS(
(function () {
  if (window.__tbxpmInjected) return;
  window.__tbxpmInjected = true;

  function loadWebChannel(callback) {
    if (window.QWebChannel) {
      callback();
      return;
    }
    var s = document.createElement('script');
    s.src = 'qrc:///qtwebchannel/qwebchannel.js';
    s.onload = callback;
    (document.head || document.documentElement).appendChild(s);
  }

  function setValue(el, value) {
    if (!el) return;
    el.focus();
    el.value = value;
    el.dispatchEvent(new Event('input', { bubbles: true }));
    el.dispatchEvent(new Event('change', { bubbles: true }));
  }

  function findUsernameInput(pwdInput) {
    var form = pwdInput && (pwdInput.form || pwdInput.closest('form'));
    if (!form) return null;

    var inputs = form.querySelectorAll('input');
    var best = null;
    for (var i = 0; i < inputs.length; i++) {
      var el = inputs[i];
      if (!el || el === pwdInput) continue;
      var type = (el.getAttribute('type') || '').toLowerCase();
      if (type === 'password') continue;
      if (el.disabled || el.readOnly) continue;

      var ac = (el.getAttribute('autocomplete') || '').toLowerCase();
      var id = (el.getAttribute('id') || '').toLowerCase();
      var name = (el.getAttribute('name') || '').toLowerCase();

      if (ac === 'username') return el;
      if (type === 'email') best = el;
      if (name.includes('user') || name.includes('email') || name.includes('login') || id.includes('user') || id.includes('email') || id.includes('login')) {
        best = el;
      }
    }
    return best;
  }

  function attachButton(pwdInput) {
    if (!pwdInput || pwdInput.dataset.tbxpmAdded) return;
    pwdInput.dataset.tbxpmAdded = '1';

    var wrapper = document.createElement('span');
    wrapper.style.position = 'relative';
    wrapper.style.display = 'inline-block';
    wrapper.style.width = (pwdInput.offsetWidth ? pwdInput.offsetWidth + 'px' : 'auto');

    pwdInput.parentNode.insertBefore(wrapper, pwdInput);
    wrapper.appendChild(pwdInput);

    var btn = document.createElement('button');
    btn.type = 'button';
    btn.textContent = '钥';
    btn.title = 'Toolbox：填充账号/密码';
    btn.style.position = 'absolute';
    btn.style.right = '4px';
    btn.style.top = '50%';
    btn.style.transform = 'translateY(-50%)';
    btn.style.zIndex = '2147483647';
    btn.style.padding = '0 6px';
    btn.style.border = '1px solid #bbb';
    btn.style.borderRadius = '6px';
    btn.style.background = '#fff';
    btn.style.cursor = 'pointer';
    btn.style.opacity = '0.85';
    btn.style.fontSize = '12px';

    btn.addEventListener('click', function (ev) {
      ev.preventDefault();
      ev.stopPropagation();
      window.__tbxpmLastPasswordInput = pwdInput;
      if (window.tbxBridge && window.tbxBridge.requestCredentials) {
        window.tbxBridge.requestCredentials(location.href);
      }
    });

    wrapper.appendChild(btn);
    try {
      var pr = parseFloat(getComputedStyle(pwdInput).paddingRight) || 0;
      pwdInput.style.paddingRight = (pr + 32) + 'px';
    } catch (e) {}
  }

  function passwordInputsIn(form) {
    if (!form) return [];
    var list = form.querySelectorAll('input[type="password"]');
    var out = [];
    for (var i = 0; i < list.length; i++) {
      var el = list[i];
      if (!el) continue;
      if (el.disabled || el.readOnly) continue;
      out.push(el);
    }
    return out;
  }

  function pickPasswordToSave(pwds) {
    if (!pwds || pwds.length === 0) return '';
    if (pwds.length === 1) return pwds[0].value || '';

    var values = [];
    for (var i = 0; i < pwds.length; i++) values.push(pwds[i].value || '');

    var last = values[values.length - 1];
    if (last) {
      for (var j = 0; j < values.length - 1; j++) {
        if (values[j] === last) return last;
      }
    }

    for (var i = 0; i < pwds.length; i++) {
      var ac = (pwds[i].getAttribute('autocomplete') || '').toLowerCase();
      if (ac === 'new-password' && pwds[i].value) return pwds[i].value;
    }

    if (values.length >= 2 && values[1]) return values[1];
    for (var i = 0; i < values.length; i++) if (values[i]) return values[i];
    return '';
  }

  function classifyKind(pwds) {
    if (!pwds || pwds.length === 0) return 'unknown';
    if (pwds.length >= 2) return 'change';
    var ac = (pwds[0].getAttribute('autocomplete') || '').toLowerCase();
    if (ac === 'new-password') return 'change';
    return 'login';
  }

  function attachSubmit(form) {
    if (!form || form.dataset.tbxpmSubmitAdded) return;
    form.dataset.tbxpmSubmitAdded = '1';

    form.addEventListener('submit', function () {
      try {
        var pwds = passwordInputsIn(form);
        if (!pwds.length) return;

        var kind = classifyKind(pwds);
        var pwd = pickPasswordToSave(pwds);
        if (!pwd) return;

        var refPwd = pwds[0];
        for (var i = 0; i < pwds.length; i++) {
          var ac = (pwds[i].getAttribute('autocomplete') || '').toLowerCase();
          if (ac === 'new-password') {
            refPwd = pwds[i];
            break;
          }
        }

        var userInput = findUsernameInput(refPwd);
        var user = userInput ? (userInput.value || '') : '';

        var now = Date.now();
        if (window.__tbxpmLastSubmitAt && now - window.__tbxpmLastSubmitAt < 800) return;
        window.__tbxpmLastSubmitAt = now;

        if (window.tbxBridge && window.tbxBridge.reportSubmitted) {
          window.tbxBridge.reportSubmitted(location.href, document.title || '', user, pwd, kind);
        }
      } catch (e) {}
    }, true);
  }

  function scan() {
    var pwds = document.querySelectorAll('input[type="password"]');
    for (var i = 0; i < pwds.length; i++) {
      attachButton(pwds[i]);
      var form = pwds[i].form || pwds[i].closest('form');
      attachSubmit(form);
    }
  }

  loadWebChannel(function () {
    new QWebChannel(qt.webChannelTransport, function (channel) {
      window.tbxBridge = channel.objects.tbxBridge;
      if (window.tbxBridge && window.tbxBridge.fillCredentials) {
        window.tbxBridge.fillCredentials.connect(function (username, password) {
          var pwdInput = window.__tbxpmLastPasswordInput || document.querySelector('input[type="password"]');
          if (pwdInput) {
            setValue(pwdInput, password || '');
            var userInput = findUsernameInput(pwdInput);
            if (userInput && username) setValue(userInput, username);
          }
        });
      }

      scan();
      var obs = new MutationObserver(function () { scan(); });
      obs.observe(document.documentElement, { childList: true, subtree: true });
    });
  });
})();
    )JS");

    view_->page()->scripts().insert(script);
}

void PasswordWebAssistantDialog::loadDemoPage()
{
    if (!view_)
        return;

    static const char kHtml[] = R"HTML(
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <title>Toolbox Web 助手演示页</title>
  <style>
    body { font-family: -apple-system, Segoe UI, Roboto, Arial, sans-serif; margin: 20px; }
    .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; }
    .card { border: 1px solid #ddd; border-radius: 10px; padding: 14px; }
    .card h2 { margin: 0 0 10px; font-size: 16px; }
    label { display: block; margin: 8px 0 4px; color: #333; font-size: 13px; }
    input { width: 100%; padding: 10px 10px; border: 1px solid #ccc; border-radius: 8px; }
    button { margin-top: 10px; padding: 8px 12px; border: 1px solid #888; border-radius: 8px; background: #f8f8f8; cursor: pointer; }
    code { background: #f3f3f3; padding: 2px 6px; border-radius: 6px; }
  </style>
</head>
<body>
  <h1>Web 助手演示页</h1>
  <p>用于离线演示：密码框右侧会出现“钥”按钮；提交后会提示保存/更新。</p>
  <p>建议先在密码库里创建一个 <code>demo.tbx</code> 的 Web 登录条目用于填充演示。</p>

  <div class="grid">
    <div class="card">
      <h2>登录（1 个密码框）</h2>
      <form>
        <label>账号</label>
        <input type="text" name="username" autocomplete="username" placeholder="alice"/>
        <label>密码</label>
        <input type="password" name="password" autocomplete="current-password" placeholder="password"/>
        <button type="submit">提交登录</button>
      </form>
    </div>

    <div class="card">
      <h2>注册（2 个密码框）</h2>
      <form>
        <label>邮箱</label>
        <input type="email" name="email" autocomplete="username" placeholder="alice@example.com"/>
        <label>密码</label>
        <input type="password" name="new_password" autocomplete="new-password" placeholder="new password"/>
        <label>确认密码</label>
        <input type="password" name="confirm_password" autocomplete="new-password" placeholder="confirm"/>
        <button type="submit">提交注册</button>
      </form>
    </div>

    <div class="card">
      <h2>改密（3 个密码框）</h2>
      <form>
        <label>旧密码</label>
        <input type="password" name="old_password" autocomplete="current-password" placeholder="old password"/>
        <label>新密码</label>
        <input type="password" name="new_password" autocomplete="new-password" placeholder="new password"/>
        <label>确认新密码</label>
        <input type="password" name="confirm_password" autocomplete="new-password" placeholder="confirm"/>
        <button type="submit">提交改密</button>
      </form>
    </div>
  </div>

  <script>
    document.addEventListener('submit', function (ev) {
      ev.preventDefault();
      alert('已提交（演示页不跳转）');
    }, true);
  </script>
</body>
</html>
    )HTML";

    const auto base = QUrl("https://demo.tbx/login");
    view_->setHtml(QString::fromUtf8(kHtml), base);
    if (urlEdit_)
        urlEdit_->setText(base.toString());
}

void PasswordWebAssistantDialog::navigateToUrlText(const QString &urlText)
{
    if (!view_)
        return;

    const auto url = QUrl::fromUserInput(urlText.trimmed());
    if (!url.isValid()) {
        QMessageBox::warning(this, "失败", "网址无效。");
        return;
    }

    view_->load(url);
}

#include "passwordwebassistantdialog.moc"

#endif // TBX_HAS_WEBENGINE
