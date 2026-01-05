# AI 输出错误/不适用复盘（至少 2 例）

本文记录开发过程中 AI 输出的错误或不适用建议、验证方式与最终修正，满足课程“AI 使用记录”要求。

## 1) Qt6 编译错误：误用 `QNetworkRequest::KnownHeaders::AcceptHeader`

**场景**
- 在实现在线泄露检查（Pwned Passwords Range API）时，需要设置 `Accept: text/plain`。

**AI 建议（不适用）**
- 使用 `req.setHeader(QNetworkRequest::KnownHeaders::AcceptHeader, "text/plain");`

**为什么不适用**
- Qt6 的 `QNetworkRequest::KnownHeaders` 不包含 `AcceptHeader`，会直接编译失败。

**如何验证**
- 本地编译报错：`'AcceptHeader' is not a member of 'QNetworkRequest::KnownHeaders'`。

**最终修正**
- 改为使用原始 Header：`req.setRawHeader("Accept", "text/plain");`。

---

## 2) 测试入口选择不当：`QTEST_GUILESS_MAIN` 无法创建 `QPixmap`

**场景**
- 为 favicon 缓存补集成测试，需要在解码图标时创建 `QIcon/QPixmap`。

**AI 建议（不完整）**
- 将测试入口从 `QTEST_APPLESS_MAIN` 改为 `QTEST_GUILESS_MAIN`，以便创建 `QCoreApplication` 供 `QtSql` 使用。

**为什么不适用**
- `QTEST_GUILESS_MAIN` 只创建 `QCoreApplication`，但 `QPixmap` 需要 `QGuiApplication`（或 `QApplication`）。

**如何验证**
- 运行测试出现告警：`QPixmap::fromImage: QPixmap cannot be created without a QGuiApplication`，并导致 favicon 用例失败。

**最终修正**
- 改用 `QTEST_MAIN`（创建 `QGuiApplication`），同时保留数据库相关测试能力。

