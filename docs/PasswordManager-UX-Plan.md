# 密码管理器（体验向）开发计划：Phase 划分（方案 B：内置 WebView 注入）

> 目标：以“演示体验”为主，对标 KeePassXC 的“浏览器输入框识别 + 一键填充 + 保存/更新提示”的**最小闭环**；安全模型（KDBX/浏览器扩展/硬件钥匙等）不在 MVP 范围。
>
> 方案选择：**方案 B**（Qt Widgets 内置 `QWebEngineView` + JS 注入 + `QWebChannel`）。优点是作业答辩演示效果强、工程量可控；缺点是只能覆盖内置 WebView，不覆盖系统浏览器。

---

## 0. MVP 范围与验收点（先定清楚）

### 0.1 MVP 必做能力（可现场演示）
- **条目类型体系（6 类）**：Web 登录 / 桌面(客户端)账号 / API Key(Token) / 数据库凭据 / 服务器(SSH) / 设备(Wi‑Fi)。
- **多账号**：同一网站/服务下可保存多个账号；一键填充时可选择账号。
- **内置 Web 助手闭环**：
  - 打开登录页 → 自动识别密码输入框 → 显示“小图标按钮”
  - 点击图标 → 选择账号 → 自动填充（用户名/密码）
  - 在页面提交（登录/注册/改密）→ 触发“保存/更新/忽略”对话框 → 写回数据库
- **CSV 互操作（基础）**：至少支持导入常见浏览器 CSV 与 KeePassXC CSV；导出支持 Toolbox 自有格式。
- **可视化管理（简洁图谱）**：以“服务/账号/类型”为节点的关系图；点击节点可联动筛选右侧列表。

### 0.2 非 MVP（后续扩展）
- “常用密码模板/密码库注入”（可作为 Phase 6 扩展）。
- 系统浏览器级别注入（方案 A：扩展 + native messaging）。
- KDBX 格式兼容、复合密钥、硬件钥匙等安全模型升级。

---

## 1. 数据结构与类型体系（Phase 1）

### 1.1 数据库 schema 调整
- `password_entries` 增加 `entry_type`（枚举/整型）与必要的“类型特有字段”：
  - Web：`url`（host 作为匹配主键），可选 `form_url`
  - 桌面/客户端：`app_name` / `app_id`（可先用 `url` 或 `category` 兜底）
  - API Key/Token：`service` / `key_id` / `token_type`（MVP 可先只用 `notes` + `title`）
  - 数据库：`db_type` / `host` / `port` / `db_name`
  - 服务器/SSH：`host` / `port` / `user` / `auth_type`
  - 设备/Wi‑Fi：`ssid` / `security` / `device_model`
- 注意：MVP 可以先只落地 **`entry_type`** + 少量字段，其余字段用 `notes` 承载，先保证 UI 结构与筛选逻辑跑通。

### 1.2 UI 结构（Widgets + Layout）
- 新增“类型”筛选与编辑：列表页可按类型过滤；编辑弹窗里类型必选。
- 编辑弹窗随类型切换显示不同字段（可用 `QStackedWidget`）。
- 多账号：Web 类型中“同 host 多 username 多条目”的展示与选择逻辑先跑通。

### 1.3 Phase 1 验收演示
- 能新增 6 类条目、列表可按类型过滤、编辑弹窗字段切换正常。
- 同一网站保存两个账号，列表可区分并可快速复制用户名/密码。

---

## 2. 内置 Web 助手：识别 + 注入入口（Phase 2）

### 2.1 技术路线
- 引入 Qt 模块：`QtWebEngineWidgets` + `QtWebChannel`。
- 页面加载完成后注入 JS：
  - 扫描 `input[type=password]` 与关联的用户名输入（常见为 `type=text/email`，可用启发式：`name/id/autocomplete`）。
  - 在密码框右侧插入一个小按钮（模拟 KeePassXC‑Browser 的图标体验）。
  - 点击按钮通过 `QWebChannel` 调用 C++：传 `origin/host` + 页面标题。

### 2.2 C++ 侧职责（MVP）
- 根据 `host` 查库返回候选账号列表（多账号）。
- 弹出一个轻量选择 UI（`QMenu` / `QDialog`）：
  - 选择条目后，把 `username/password` 回传给 JS 进行 `input.value = ...` 填充。

### 2.3 Phase 2 验收演示
- 现场输入 URL 打开登录页 → 自动出现图标 → 选择条目 → 页面输入框被填充。

---

## 3. 保存/更新：监听提交与改密识别（Phase 3）

### 3.1 监听策略（MVP 可接受的启发式）
- JS 监听 `form.submit`/`submit` 事件：
  - 读取当前 form 内的用户名字段 + 密码字段（可能有多个密码字段）
  - 判断场景：
    - 1 个密码框：按“登录/保存密码”处理
    - 2 个密码框：按“注册/改密（新密码+确认）”处理
    - 3 个密码框：按“改密（旧/新/确认）”处理
- 将采集到的数据（`host`, `username`, `password`, `newPassword` 等）送到 C++。

### 3.2 写回策略（多账号优先）
- 通过 `host + username` 查找是否已有条目：
  - 有：弹窗确认“更新密码 / 新增一条 / 忽略”
  - 无：弹窗确认“保存为新条目 / 忽略”
- 更新时只改 `password` 与 `updated_at`（MVP）。

### 3.3 Phase 3 验收演示
- 在内置 WebView 登录一次：提交后弹窗提示保存；确认后库中出现对应条目。
- 改密页面提交：弹窗提示更新；确认后条目密码更新。

---

## 4. CSV 互操作：导入导出增强（Phase 4）

### 4.1 导入（从“解析”升级为“导入向导”）
- 新增导入向导：识别来源格式 + 字段映射 + 去重/合并策略。
- 最低支持：
  - KeePassXC CSV（含 Group Path → 可选自动创建分组树）
  - Chrome/Edge CSV（常见字段：`name,url,username,password` 及其变体）
- 合并策略：
  - `host + username` 命中：默认提示“更新/新增/跳过”（可提供“全局默认策略”）

### 4.2 导出
- 保留现有 Toolbox CSV 格式；
- 可追加 KeePassXC CSV 导出（可选项，若时间不足可后置）。

### 4.3 Phase 4 验收演示
- 导入 Chrome 导出 CSV：条目正确落地，且可在内置 WebView 一键填充。
- 导入 KeePassXC CSV：标题/账号/URL/备注正确；分组（至少能映射到 category 或自动建组二选一）。

---

## 5. 简洁关系图谱（Phase 5）

### 5.1 图谱模型（MVP）
- 节点：
  - `Service(host/app/service)` 节点
  - `Account(username)` 节点
  - `Type(entry_type)` 节点（用于聚类/过滤）
- 边：
  - Service ↔ Account（一条 entry 一条边或聚合计数）
  - Type ↔ Service（可选）

### 5.2 实现建议（Widgets）
- `QGraphicsView/QGraphicsScene` 绘制；
- 简单布局：按 Type 分列/分圈，Service 在中间，Account 围绕；先保证“可读 + 可点击”。
- 联动：点击节点 → 右侧表格应用筛选（按 host/username/type）。

### 5.3 Phase 5 验收演示
- 图谱可显示“一个站点多个账号”的关系；
- 点击节点可筛选列表并定位到条目编辑。

---

## 6. 可选扩展（Phase 6+）

### 6.1 常用密码模板（非 MVP）
- 新增“模板库”（无账号，仅名称 + 密码 + 备注/标签）；
- 内置 Web 助手图标弹出菜单中增加“注入常用密码”：
  - 登录页：填 password
  - 注册/改密页：同时填 password + confirm password

### 6.2 体验优化
- 站点匹配规则增强（兼容子域名、`www.`、端口、form action）。
- 更友好的保存对话框（显示站点图标 + 标题 + 账号预览）。

---

## 7. 建议的 Git 提交节奏（配合课程评分）
- 文档与规划：`文档(密码)：补充 WebView 注入 Phase 计划`
- Phase 1：`新增(密码)：条目类型 entry_type 与 UI 切换`
- Phase 2：`新增(密码)：内置 WebView 与输入框图标注入`
- Phase 3：`新增(密码)：表单提交监听与保存/更新对话框`
- Phase 4：`新增(密码)：CSV 导入向导与来源格式适配`
- Phase 5：`新增(密码)：关系图谱视图与列表联动`

