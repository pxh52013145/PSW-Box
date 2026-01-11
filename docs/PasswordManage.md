# Password Manager 模块（已落地）

本模块对应任务书题目「个人密码管理器（数据库 + 加密 + 文件操作）」的核心功能：账号密码安全存储、加密保护、按类别过滤、加密备份导入导出。

> 当前实现已拆分为独立程序 `ToolboxPassword`，可由 `ToolboxLauncher` 启动；退出启动器不影响密码管理器运行。

## 1. 已实现功能
- Vault：首次设置主密码；解锁/锁定；5 分钟无操作自动锁定；修改主密码会对所有条目重新加密。
- 条目管理：新增/编辑/删除；搜索；按分类/标签过滤；移动到分组；复制账号/复制密码（复制密码二次确认，15 秒后自动清空剪贴板）。
- 条目类型：支持 6 类（Web 登录/桌面客户端/API Key/Token/数据库凭据/服务器 SSH/设备 Wi-Fi），支持按类型筛选。
- 信息组织：分组树（新建/重命名/删除空分组）；标签多选。
- 密码生成器：可配置长度/字符集/排除易混淆字符/每类至少 1 个；编辑弹窗实时强度评分。
- 安全报告：后台扫描弱密码/重复密码/久未更新条目，双击可定位编辑。
- 在线泄露检查：安全报告可选启用（k-anonymity，查询 SHA-1 前缀，不上传明文密码；带本地缓存）。
- 网站图标：根据条目 URL 自动拉取 favicon，并在列表中显示（带本地缓存与过期策略）。
- 互操作：CSV 导入/导出（导入异步+进度、去重；导出/导入均有明文风险提示）。
- Web 助手（可选）：内置 WebView 模拟 KeePassXC 的“输入框小图标填充 + 保存/更新提示”，识别密码框并注入“钥”按钮，表单提交时提示保存/更新（需 Qt WebEngine：`webenginewidgets` + `webchannel`）。
- 关系图谱：按“类型→服务→账号”可视化管理；点击节点可联动列表筛选（用于演示多账号管理）。
- 数据持久化：SQLite 存储；密码与备注字段加密后写入数据库。
- 备份/恢复：导出/导入加密备份文件（`.tbxpm`，包含分组结构与标签信息）。
- 备份兼容：备份内容包含 `entry_type`（对旧备份保持兼容，缺失时默认 Web 登录）。

## 2. 关键 Qt 模块覆盖
- 数据库：`QtSql` + SQLite（增删改查）。
- Model/View：`QTreeView` + 自定义 `QAbstractItemModel`（分组树）+ `QTableView` + 自定义 `QAbstractTableModel` + `QSortFilterProxyModel`。
- 多线程：`QThread`（安全报告扫描不阻塞 UI）。
- 网络通信：`QtNetwork`（`QNetworkAccessManager` 拉取网站 favicon）。
- 文件操作：备份/CSV 文件读写（`QFileDialog` / `QSaveFile` / JSON/CSV）。

## 3. 数据库设计
- `vault_meta`：保存 KDF 参数与主密码校验值（`id=1` 单行）。
- `password_entries`：保存条目基本信息（明文）与机密字段（密文）：
  - 明文：`group_id/entry_type/title/username/url/category`、时间戳
  - 密文：`password_enc/notes_enc`
- `groups`：分组表（树结构，`parent_id` 指向父分组）。
- `tags`：标签表（去重）。
- `entry_tags`：条目-标签关联表（多对多）。
- `favicon_cache`：网站图标缓存（按 host 缓存 favicon 二进制与时间戳）。
- `pwned_prefix_cache`：泄露检查缓存（按 SHA-1 前缀缓存查询响应与时间戳）。

## 4. 加密设计（课程项目落地版）
- KDF：PBKDF2-SHA256（默认 `iterations=120000`，`salt=16 bytes`，`key=32 bytes`）。
- 加密/校验：基于 HMAC-SHA256 生成密钥流（异或加密）+ HMAC 校验（tag 截断为 16 bytes）。
- 备注：这是课程设计的实现方案，目的是满足“加密存储 + 可演示”的要求，并非专业密码学库的替代品。

## 5. 代码位置
- 程序入口：`apps/password_app/*`
- 数据库：`src/password/passworddatabase.*`（独立 SQLite 文件：`password.sqlite3`）
- Vault/加密：`src/password/passwordvault.*`、`src/core/crypto.*`
- 数据访问：`src/password/passwordrepository.*`
- 列表模型：`src/password/passwordentrymodel.*`
- 分组树模型：`src/password/passwordgroupmodel.*`
- 密码生成与强度评估：`src/password/passwordgenerator.*`、`src/password/passwordstrength.*`
- 安全报告：`src/password/passwordhealth*`、`src/pages/passwordhealthdialog.*`
- UI：`src/pages/passwordmanagerpage.*`、`src/pages/passwordentrydialog.*`、`src/pages/passwordgeneratordialog.*`
