# 调试记录（Debug Notes）

## 1) Qt Creator 编译报错：Cannot find file: ...ToolboxPassword.pro

**现象**
- Qt Creator 构建 `Toolbox.pro`（`TEMPLATE = subdirs`）时，`mingw32-make` 输出类似：
  - `Cannot find file: C:\Users\...\Desktop\QT???\Final\Toolbox\apps\password_app\ToolboxPassword.pro.`
  - 随后 `Error 2` 中断。

**复现条件**
- Windows + Qt 6（MinGW 套件）+ qmake 工程。
- 工程源码路径包含中文/非 ASCII 字符（如 `...\Desktop\QT课程\Final\Toolbox\...`）。

**原因定位**
- qmake 生成的顶层 Makefile 在调用子工程 qmake 时写入了**源码绝对路径**；
- `mingw32-make` 在解析/执行含中文路径的命令行时会出现编码问题，路径被替换为 `???`，导致找不到 `.pro` 文件。

**解决方案**
- 将**源码路径改为纯英文/ASCII**：
  - 推荐：把仓库移动到如 `C:\repo\Toolbox` / `D:\repo\Toolbox`；
  - 或使用目录联接（junction）创建 ASCII 路径入口，然后在 Qt Creator 用该入口打开工程。
- 在 Qt Creator 中使用 ASCII 路径打开 `Toolbox.pro` 后，**新建一个同样为 ASCII 的构建目录**，并执行一次 **Run qmake** 再编译。

---

## 2) 编译报错：SingleInstance 使用 QVariant 不完整类型

**现象**
- 编译 `src/core/singleinstance.cpp` 时出现：
  - `invalid use of incomplete type 'class QVariant'`
  - `no matching function for call to '...setProperty(..., bool)'`

**原因定位**
- `QObject::property/setProperty` 依赖 `QVariant` 完整定义，但实现文件未包含 `<QVariant>`，导致隐式转换不可用。

**修复**
- 在 `src/core/singleinstance.cpp` 添加 `#include <QVariant>`。
- 在 `src/core/singleinstance.h` 添加 `#include <QByteArray>`，避免对间接包含的依赖。

---

## 3) 集成测试构建/运行问题：缺少 QtNetwork 与 QCoreApplication

**现象**
- 构建 `tests/password_integration` 报错：
  - `fatal error: QPasswordDigestor: No such file or directory`
- 运行测试报 QWARN：
  - `QSqlDatabase requires a QCoreApplication`
  - `QPixmap::fromImage: QPixmap cannot be created without a QGuiApplication`

**原因定位**
- `src/core/crypto.cpp` 使用 `QPasswordDigestor`（位于 `QtNetwork`），测试工程 `.pro` 未加入 `network` 模块，导致头文件不可见。
- 测试使用了 `QTEST_APPLESS_MAIN`/`QTEST_GUILESS_MAIN`：
  - 前者不创建 `Q(Core)Application`，导致 `QtSql`/`QStandardPaths` 等组件报错；
  - 后者只创建 `QCoreApplication`，可以让数据库正常，但 `PasswordFaviconService` 需要 `QGuiApplication` 来创建 `QPixmap`。

**修复**
- `tests/password_integration/ToolboxPasswordIntegrationTests.pro` 增加 `QT += ... network`。
- `tests/password_integration/tst_password_integration.cpp` 改用 `QTEST_MAIN`（创建 `QGuiApplication`，满足 QPixmap 需求）。

**验证**
- 运行 `build/PasswordIntegrationTests-Debug/release/ToolboxPasswordIntegrationTests.exe`：7 passed，0 failed。

---

## 4) Qt6 编译报错：QNetworkRequest 没有 AcceptHeader

**现象**
- 编译 `src/password/passwordhealthworker.cpp` 时报错：
  - `'AcceptHeader' is not a member of 'QNetworkRequest::KnownHeaders'`

**原因定位**
- Qt6 的 `QNetworkRequest::KnownHeaders` 不包含 `AcceptHeader`（与印象中某些示例/旧资料不一致）。

**修复**
- 使用 `req.setRawHeader("Accept", "text/plain");` 代替 `setHeader(KnownHeaders::AcceptHeader, ...)`。
