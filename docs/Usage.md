# Toolbox 使用说明（Launcher / 密码管理器 / 翻译工具）

本仓库包含 3 个可运行程序：
- `ToolboxLauncher`：启动器（建议从这里进入）。
- `ToolboxPassword`：密码管理器（SQLite + 加密 + 导入导出 + 线程 + 网络）。
- `ToolboxTranslate`：翻译工具（HTTP + 历史记录）。

> 提示：三个程序均为“单实例”，重复启动会唤起已运行窗口。

## 1. 构建与运行（Qt Creator）

1) 用 Qt Creator 打开根目录 `Toolbox.pro`（qmake 工程，`subdirs`）。  
2) 选择 Kit：Qt 6.x + MinGW（建议与 Qt 安装目录一致的 MinGW 版本）。  
3) 直接运行以下任意目标：
   - `ToolboxLauncher`（推荐）
   - `ToolboxPassword`
   - `ToolboxTranslate`

常见编译错误排查：
- 如果出现 `bits/wordsize.h` 等头文件缺失/ABI 不匹配，通常是 **qmake 对应的 MinGW 与实际编译用的 MinGW 版本不一致**；请在 Qt Creator 中确保 Qt Kit 与 MinGW 工具链匹配。
- 避免把工程放在包含中文/特殊字符的路径下（你已迁移为英文路径则无需关注）。

## 2. 启动器（ToolboxLauncher）

- 运行后会显示两个入口：`密码管理器` 与 `翻译工具`。
- 点击按钮会启动对应独立进程；关闭启动器 **不影响** 已启动的工具继续运行。

## 3. 密码管理器（ToolboxPassword）

### 3.1 第一次使用：创建 Vault（主密码）

1) 首次启动会提示设置“主密码”（请妥善保存，遗忘后无法解密既有数据）。  
2) 创建成功后 Vault 自动解锁，可开始新增条目。

Vault 行为：
- **自动锁定**：解锁状态下无操作约 5 分钟会自动锁定（安全考虑）。
- **复制密码保护**：复制密码前会二次确认；复制后约 15 秒会自动清空剪贴板（若剪贴板内容未被你手动更改）。

### 3.2 主界面与基本操作

主界面为左右分栏：
- 左侧：分组树（支持新增/重命名/删除空分组）。
- 右侧：条目表格（标题/用户名/URL/分类/标签），支持搜索与筛选。

条目操作（常用）：
- 新增/编辑/删除条目。
- 复制用户名、复制密码。
- 选择分组后可将条目移动到分组内。

筛选与组织：
- 搜索框：按标题/类型/用户名/URL/分类/标签模糊搜索。
- 类型筛选：下拉选择类型（或“全部”）。
- 分类筛选：下拉选择分类（或“全部”）。
- 标签筛选：可多选标签，作为“必须包含”的过滤条件。
- 分组：点击左侧分组后，仅显示该分组（含子分组）下的条目。

### 3.3 密码生成器与强度提示

- 在新增/编辑条目弹窗中可打开密码生成器。
- 支持配置长度、字符集、排除易混淆字符、每类至少 1 个等规则。
- 弹窗中会显示密码强度条（用于演示与提示，不代表绝对安全）。

### 3.4 安全报告（后台扫描）

点击“安全报告”后会在后台线程扫描并给出问题列表（不阻塞 UI），包括：
- 弱密码、重复密码、长期未更新等。
- 双击问题项可定位到对应条目进行编辑。

可选：在线泄露检查（需要联网）
- 使用 Pwned Passwords 的 **k-anonymity** 查询方式（仅发送 SHA-1 前缀，不上传明文密码），并带本地缓存与过期策略。
- 仅用于风险提示，是否启用由你选择。

### 3.5 网站图标（favicon）

- 条目填写 URL 后会尝试抓取网站 `favicon.ico` 并在列表中显示。
- 带本地缓存与过期策略；Vault 锁定时不会发起网络请求。

### 3.6 备份/恢复（.tbxpm，加密）

导出备份：
1) 点击“导出备份”，选择保存路径（扩展名 `.tbxpm`）。
2) 设置“备份加密密码”（与主密码无关，可单独设置）。

导入备份：
1) 点击“导入备份”，选择 `.tbxpm` 文件。
2) 输入备份密码后导入；会恢复条目、分组结构与标签信息（必要时自动创建缺失分组）。

### 3.7 CSV 导入/导出（明文风险）

CSV 导出：
- 会导出 **明文密码**，仅建议用于临时迁移；完成后请及时删除 CSV，避免通过网盘/聊天软件传输。

CSV 导入：
- 支持自动识别分隔符（`,`/`;`/`Tab`）与引号转义。
- 需要表头包含 `password`；推荐表头：
  - `title,username,password,url,category,tags,notes`
- 对浏览器导出也做了兼容（例如 `origin` / `formActionOrigin` 作为 URL）。
- 导入会打开“向导”预览并自动识别常见格式（Toolbox / Chrome/Edge / KeePassXC）。
- 可选策略：跳过重复 / 更新重复 / 全部导入；可选“从 Group/Category 自动创建分组”；可选设置导入条目的默认类型。
- 导入过程异步执行并显示进度，默认去重策略为 `host + username`（若无 URL 则用 `title + username`）。

### 3.8 Web 助手（内置 WebView，演示用）

- 入口：主界面右上角 `Web 助手`。
- 功能：打开内置登录页后，会在密码输入框右侧注入一个“小钥匙”按钮；点击后选择匹配账号即可自动填充用户名/密码。
- 保存/更新：在登录/注册/改密等表单提交时，会弹窗提示“保存为新条目 / 更新已有条目 / 忽略”。
- 演示页：对话框内置“演示页”（`demo.tbx`），可离线验证填充与保存/更新流程。
- 依赖：需要 Qt Kit 安装 `Qt WebEngine`（`webenginewidgets` + `webchannel` + `positioning`）。Windows 下通常建议用 `MSVC` Kit（例如 `Qt 6.9.2 MSVC2022 64bit`）；若当前 Kit 未安装，按钮会被禁用（工程仍可编译运行）。
- 安装：运行 Qt 安装目录下的 `MaintenanceTool.exe` → `Add or remove components` → 勾选当前 Kit 对应的 `Qt WebEngine` / `Qt WebChannel` / `Qt Positioning` → 完成后在 Qt Creator 里切换到已安装这些模块的 Kit 并重新构建。

### 3.9 关系图谱（可视化管理）

- 入口：主界面右上角 `图谱`。
- 展示：按 “类型 → 服务(host/标题) → 账号(username)” 组织的关系图，便于演示“一个站点多个账号”的情况。
- 交互：点击节点会自动应用筛选并返回列表。

### 3.10 数据与日志位置（Windows 默认）

程序数据目录使用 `QStandardPaths::AppDataLocation`（随程序名不同而不同），例如：
- 密码库：`%APPDATA%\\CourseDesign\\ToolboxPassword\\password.sqlite3`
- 日志：`%APPDATA%\\CourseDesign\\ToolboxPassword\\logs\\toolbox.log`

## 4. 翻译工具（ToolboxTranslate）

当前版本为“输入文本翻译 + 历史记录”界面（不含全局热键取词/浮窗模式）：

基本使用：
1) 选择翻译引擎：
   - `Mock`：离线调试用，会返回 `【Mock】...`。
   - `MyMemory`：在线翻译（需要联网）。
2) 选择源语言（支持 `auto/zh-CN/en`）与目标语言（`zh-CN/en`）。
3) 输入文本 → 点击“翻译”。
4) 点击“复制译文”可将结果写入剪贴板；双击历史记录可回填输入/输出。

数据与日志：
- 历史记录库：`%APPDATA%\\CourseDesign\\ToolboxTranslate\\translate.sqlite3`
- 日志：`%APPDATA%\\CourseDesign\\ToolboxTranslate\\logs\\toolbox.log`

## 5. 集成测试（密码模块）

本仓库包含密码模块的集成测试工程：
- 工程文件：`tests/password_integration/ToolboxPasswordIntegrationTests.pro`

建议做法：
- 在 Qt Creator 中打开并运行该测试工程，确保所有用例通过。
- 若使用命令行构建，请确保 qmake 与 MinGW 工具链版本匹配后再运行测试可执行文件。


