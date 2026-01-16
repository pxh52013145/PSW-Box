# ToolboxPassword 功能测试与演示指南（可按步骤照做）

> 目标：给出一套“从零开始 → 造数据 → 覆盖功能点 → 录屏可复现”的操作清单。

## 1. 环境要求
- Windows 10/11 + Qt Creator（qmake 工程）
- 可联网（用于 favicon、可选的在线泄露检查）
- 可选：Qt WebEngine（用于 `Web 助手`；不影响核心验收）

## 2. 重置数据（每次录屏前建议做一次）
1) 关闭 `ToolboxPassword`（确保数据库未被占用）。
2) 删除（或改名备份）程序数据目录下的数据库文件：
   - `QStandardPaths::AppDataLocation\\password.sqlite3`
3) 重新启动程序，会进入“首次使用/设置主密码”流程。

提示：
- 日志文件位于 `QStandardPaths::AppDataLocation\\logs\\toolbox.log`，排查问题时可查看。

## 3. 演示用数据集（建议照抄，保证“弱/重复”必定出现）
建议统一使用：
- 主密码（Vault）：`DemoMaster123!`（仅演示）
- 备份密码（.tbxpm）：`DemoBackup123!`（仅演示）

建议创建分组：
- `工作/开发`
- `个人/邮箱`

建议创建条目（至少 3 条）：
1) GitHub
   - 类型：Web 登录
   - 账号：`demo_github`
   - 密码：`SamePass!123`（用于触发“重复密码”）
   - URL：`https://github.com`
   - 标签：`work,2FA`
2) Gmail
   - 类型：Web 登录
   - 账号：`demo_mail`
   - 密码：`SamePass!123`（与 GitHub 相同）
   - URL：`https://mail.google.com`
   - 标签：`personal,2FA`
3) DemoSite（用于弱密码 + Web 助手演示）
   - 类型：Web 登录
   - 账号：`demo_user`
   - 密码：`123456`（用于触发“弱密码”）
   - URL：`https://demo.tbx/login`

## 4. 建议录屏/验收展示顺序（8–10 分钟）
1) 创建 Vault：`设置主密码` → 进入解锁态。
2) CRUD：新增/编辑/删除 + 分组切换 + 搜索筛选。
3) 复制保护：`复制密码` → 二次确认 → 15 秒后自动清空。
4) 生成器：编辑条目 → `生成` → 使用并保存。
5) 安全报告（线程）：`安全报告` → 扫描不阻塞 → 展示弱/重复。
6) 网络：列表中显示 favicon（首次会拉取，后续命中缓存）。
7) 文件：`导出备份(.tbxpm)`（加密）+（可选）导入恢复。
8) 可选加分：`Web 助手` → `演示页` → 密码框右侧 `钥` 按钮填充 + 提交提示保存/更新。

## 5. 可复制测试网页（演示“真实网页登录”用；不稳定就用内置演示页兜底）
- `https://the-internet.herokuapp.com/login`（`tomsmith` / `SuperSecretPassword!`）
- `https://practicetestautomation.com/practice-test-login/`（`student` / `Password123`）
- `https://www.saucedemo.com/`（`standard_user` / `secret_sauce`）
- `https://opensource-demo.orangehrmlive.com/web/index.php/auth/login`（`Admin` / `admin123`）
- `https://admin-demo.nopcommerce.com/login?ReturnUrl=%2Fadmin%2F`（`admin@yourstore.com` / `admin`）

注意：
- 真实网页登录填充通常需要浏览器插件/扩展；本项目的 `Web 助手` 是“内置 WebView 演示闭环”，用于答辩展示原理与交互。

