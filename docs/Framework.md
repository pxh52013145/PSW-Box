# ToolboxPassword（Qt）总体方案（Windows 优先）

## 1. 目标与范围

### 1.1 产品定位
- 桌面端“个人密码管理器”，以本地加密存储为核心，强调可演示与可验收。
- 核心能力：条目 CRUD、分组/标签筛选、CSV 导入导出、加密备份/恢复、（可选）Web 助手填充演示闭环。

### 1.2 明确非目标（MVP 阶段）
- 不做“无授权读取/解密浏览器本地密码库”的方案（风险高、兼容差、易触发安全软件）。
- 不追求对所有第三方应用 100% 自动识别/填充（以浏览器网页与主流控件体系为主，逐步补齐）。

## 2. MVP 里程碑（建议）
- M0：基础设施：日志、数据目录、单实例、主题与基础 UI。
- M1：最小闭环：本地加密 Vault + 条目 CRUD + 搜索/筛选。
- M2：互操作：CSV 导入/导出 + 加密备份/恢复（`.tbxpm`）。
- M3：安全与体验：密码生成器 + 强度提示 + 安全报告（后台线程）+ favicon（缓存）。
- M4（可选加分）：Web 助手（Qt WebEngine + WebChannel）演示“填充 + 保存/更新”闭环。

## 3. 总体架构

### 3.1 分层
- `App(UI)`：Qt 主窗口与对话框（密码管理器界面）。
- `Core`：配置、日志、加密、单实例、通用工具。
- `Password`：Vault、导入导出、匹配策略（URL/站点）、业务模型与数据存储。

### 3.2 进程与通信
当前实现为单程序：
- `ToolboxPassword`：密码管理器程序（SQLite + 加密 + 导入导出）。
- 单实例：使用 `QLocalServer/QLocalSocket` 实现“单实例 + 唤起已运行窗口”。
- 预留扩展：后续浏览器扩展/对接桥接可继续基于 `QLocalServer` 或本地 WebSocket/HTTP（仅 `127.0.0.1`）。

## 4. 密码管理器（Password）

### 4.1 为什么“扩展 + 桌面端”是主路径
网页中的账号/密码输入框在浏览器渲染进程内（DOM），系统级 UIA 很难稳定拿到字段语义，更难可靠填充/监听提交。  
因此对“网页”建议：
- 浏览器扩展负责：检测表单、定位字段、执行填充、捕获提交事件。
- 桌面端负责：Vault、匹配与策略、UI 弹窗、审计日志与同步扩展状态。

### 4.2 数据模型（最小）
- `Vault`：加密容器；包含多个 `LoginItem`
- `LoginItem`：
  - `id`
  - `origin`（协议+域名/或匹配规则集合）
  - `username`
  - `password`（仅在解锁状态可解密）
  - `notes`、`tags`
  - `updatedAt`、`createdAt`
  - 可选：`totpSecret`、`customFields`、`passwordHistory`

### 4.3 站点匹配策略（建议从简单到复杂）
- Level 1：按 `eTLD+1`（示例：`accounts.google.com` 与 `mail.google.com` 归为 `google.com`）匹配。
- Level 2：支持 `origin` 精确匹配与允许列表（`https://a.example.com`）。
- Level 3：支持规则（wildcard/regex）与 Android App package、Windows AppId（后续）。

### 4.4 Vault 与安全（建议）
核心原则：即使应用被拷贝文件，也无法离线解密。
- Master Password + KDF（`Argon2id`）派生主密钥。
- 内容加密：推荐 `XChaCha20-Poly1305`（随机 nonce）或 `AES-256-GCM`（如依赖 OpenSSL/QCA）。
- 本机绑定（可选增强）：Windows DPAPI 再包一层“设备密钥”，用于“免密解锁”或提升离线攻击成本。
- 自动锁定：超时、睡眠、切换用户、屏幕锁定事件触发。
- 剪贴板安全：复制密码后定时清空；可选“仅一次粘贴”。

### 4.5 与浏览器对接（推荐方案）
- 导入：优先支持浏览器“官方导出 CSV”导入（Chrome/Edge/Firefox 都支持），作为无扩展情况下的基础路径。
- 扩展对接：
  - 扩展检测到登录表单 → 发送 `{origin, pageTitle, formMeta}` 到桌面端
  - 桌面端返回候选账号列表（不返回明文密码，直到用户确认/策略允许）
  - 用户选择账号 → 桌面端签发一次性填充令牌（短时有效）→ 扩展用令牌请求解密并填充
  - 表单提交时扩展回传 `{origin, username, passwordCandidate, isUpdate}` → 桌面端弹窗确认保存/更新

### 4.6 原生应用填充（可选，后置）
- 优先覆盖：标准 Win32/WinUI/WPF 输入控件（UIA `ValuePattern` 可写）。
- 降级策略：仅提供“账号一键复制/密码一键复制”而不做自动写入控件。

## 6. 代码组织建议（qmake 起步，可平滑迁移 CMake）
建议目录（不强制，先在 docs 对齐）：
- `apps/password_app`：密码管理器入口
- `src/core`：配置、日志、加密、单实例等基础设施
- `src/password`：密码模块核心（Vault/DB/Repo/Model）
- `src/pages`：密码管理器 UI 页面组件
- `src/resources`：QSS / 图标等资源（.qrc）
- `tests/password_integration`：密码模块集成测试
- `extensions/`：浏览器扩展（后续加入）

## 7. 风险与对策（必须提前认清）
- 安全合规：任何“读取浏览器本地加密库”的做法都要非常谨慎；建议以“用户导入/扩展授权”作为标准路径。
- 兼容性：不同 Qt Kit/编译器与组件差异较大（尤其是 Qt WebEngine），建议核心功能不依赖可选组件并提供禁用/降级。
- 体验：明文导出/剪贴板等高风险操作要明确提示；锁定、二次确认与超时策略要可演示。

## 8. 下一步（建议）
1) 补齐/优化核心体验：Vault 解锁、条目 CRUD、搜索与分组/标签。
2) 完善互操作：CSV 导入导出与加密备份/恢复。
3) 可选加分：Web 助手与（后续）扩展对接演示。

细化方案见：`docs/PasswordManage.md`。

## 9. 参考与对标（建议先看这些实现思路）
- 密码管理器：KeePassXC（Qt）、Bitwarden（桌面端+浏览器扩展）、Browserpass（扩展桥接本地服务）

## 10. Backlog（可直接按此拆分迭代）
- M0：基础设施：日志、数据目录、单实例、主题
- M1：Vault：解锁/锁定状态机、加密读写、条目 CRUD、搜索与标签
- M2：互操作：导入 CSV、导出 CSV、加密备份导入导出（`.tbxpm`）
- M3：安全与体验：密码生成器、强度提示、安全报告（线程）、favicon 缓存、（可选）泄露检查
- M4：可选：Web 助手与（后续）扩展对接
