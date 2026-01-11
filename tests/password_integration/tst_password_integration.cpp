#include "core/apppaths.h"
#include "password/passwordcsv.h"
#include "password/passwordcsvimportworker.h"
#include "password/passworddatabase.h"
#include "password/passwordfaviconservice.h"
#include "password/passwordgenerator.h"
#include "password/passwordgraph.h"
#include "password/passwordhealthworker.h"
#include "password/passwordrepository.h"
#include "password/passwordstrength.h"
#include "password/passwordurl.h"
#include "password/passwordwebloginmatcher.h"
#include "password/passwordvault.h"

#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

class PasswordIntegrationTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(PasswordDatabase::open());
        resetDatabase();
    }

    void cleanup()
    {
        resetDatabase();
    }

    void generator_rules()
    {
        PasswordGeneratorOptions opt;
        opt.length = 12;
        opt.useUpper = true;
        opt.useLower = false;
        opt.useDigits = true;
        opt.useSymbols = false;
        opt.excludeAmbiguous = true;
        opt.requireEachSelectedType = true;

        QString error;
        const auto pwd = generatePassword(opt, &error);
        QVERIFY2(!pwd.isEmpty(), qPrintable(error));
        QCOMPARE(pwd.size(), opt.length);

        bool hasUpper = false;
        bool hasDigit = false;
        for (const auto &ch : pwd) {
            if (ch.isUpper())
                hasUpper = true;
            if (ch.isDigit())
                hasDigit = true;
        }
        QVERIFY(hasUpper);
        QVERIFY(hasDigit);
    }

    void strength_basic()
    {
        const auto weak = evaluatePasswordStrength("123456");
        QVERIFY(weak.score <= 10);

        const auto strong = evaluatePasswordStrength("Aq9!xZ3@pL8#");
        QVERIFY(strong.score >= 60);
    }

    void url_host_match_basics()
    {
        QCOMPARE(PasswordUrl::hostFromUrl("https://example.com/login"), QString("example.com"));
        QCOMPARE(PasswordUrl::hostFromUrl("http://www.example.com/"), QString("example.com"));
        QCOMPARE(PasswordUrl::hostFromUrl("example.com"), QString("example.com"));
        QVERIFY(PasswordUrl::urlMatchesHost("https://www.example.com/a", "example.com"));
        QVERIFY(PasswordUrl::hostsEqual("www.EXAMPLE.com", "example.com"));
    }

    void web_login_matcher_basics()
    {
        PasswordEntry e1;
        e1.type = PasswordEntryType::WebLogin;
        e1.title = "Example";
        e1.username = "Alice";
        e1.url = "https://www.example.com/login";

        PasswordEntry e2 = e1;
        e2.username = "";
        e2.url = "https://example.com/";

        PasswordEntry e3 = e1;
        e3.url = "https://other.example.com/";

        PasswordEntry e4 = e1;
        e4.type = PasswordEntryType::ApiKeyToken;
        e4.url = "https://example.com/";

        const auto r1 = PasswordWebLoginMatcher::match({e1, e2, e3, e4}, "https://example.com/sso", "alice");
        QCOMPARE(r1.host, QString("example.com"));
        QCOMPARE(r1.hostEntries.size(), 2);
        QCOMPARE(r1.userEntries.size(), 1);
        QCOMPARE(r1.userEntries.at(0).username, QString("Alice"));

        const auto r2 = PasswordWebLoginMatcher::match({e1, e2, e3, e4}, "https://example.com/login", "");
        QCOMPARE(r2.host, QString("example.com"));
        QCOMPARE(r2.hostEntries.size(), 2);
        QCOMPARE(r2.userEntries.size(), 1);
        QVERIFY(r2.userEntries.at(0).username.trimmed().isEmpty());
    }

    void schema_password_entries_has_entry_type()
    {
        auto db = PasswordDatabase::db();
        QVERIFY(db.isOpen());

        QSqlQuery q(db);
        QVERIFY(q.exec("PRAGMA table_info(password_entries)"));

        bool found = false;
        while (q.next()) {
            if (q.value(1).toString() == "entry_type") {
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

    void entry_type_persisted()
    {
        PasswordVault vault;
        QVERIFY(vault.createVault("master"));

        PasswordRepository repo(&vault);
        PasswordEntrySecrets e;
        e.entry.type = PasswordEntryType::DatabaseCredential;
        e.entry.title = "MySQL Prod";
        e.entry.username = "root";
        e.password = "StrongPwd!123";
        e.entry.url = "mysql://prod.example.com:3306";
        QVERIFY(repo.addEntry(e));

        const auto list = repo.listEntries();
        QCOMPARE(list.size(), 1);
        QCOMPARE(static_cast<int>(list.at(0).type), static_cast<int>(PasswordEntryType::DatabaseCredential));

        const auto loaded = repo.loadEntry(list.at(0).id);
        QVERIFY(loaded.has_value());
        QCOMPARE(static_cast<int>(loaded->entry.type), static_cast<int>(PasswordEntryType::DatabaseCredential));
    }

    void csv_export_parse_roundtrip()
    {
        PasswordVault vault;
        QVERIFY(vault.createVault("master"));

        PasswordRepository repo(&vault);
        PasswordEntrySecrets e;
        e.entry.title = "GitHub";
        e.entry.username = "user@example.com";
        e.password = "Aq9!xZ3@pL8#";
        e.entry.url = "https://github.com/login";
        e.entry.category = "开发";
        e.entry.tags = {"dev", "git"};
        e.notes = "note, with comma";
        QVERIFY(repo.addEntry(e));

        QVector<PasswordEntrySecrets> full;
        for (const auto &summary : repo.listEntries()) {
            const auto loaded = repo.loadEntry(summary.id);
            QVERIFY(loaded.has_value());
            full.push_back(loaded.value());
        }

        const auto csv = exportPasswordCsv(full);
        QVERIFY(csv.contains("title,username,password,url,category,tags,notes"));

        const auto parsed = parsePasswordCsv(csv, 1);
        QVERIFY(parsed.ok());
        QCOMPARE(parsed.entries.size(), 1);
        QCOMPARE(parsed.entries.at(0).entry.title, QString("GitHub"));
        QCOMPARE(parsed.entries.at(0).entry.username, QString("user@example.com"));
        QCOMPARE(parsed.entries.at(0).password, QString("Aq9!xZ3@pL8#"));
        QCOMPARE(parsed.entries.at(0).entry.url, QString("https://github.com/login"));
        QCOMPARE(parsed.entries.at(0).entry.category, QString("开发"));
        QVERIFY(parsed.entries.at(0).entry.tags.contains("dev"));
        QVERIFY(parsed.entries.at(0).entry.tags.contains("git"));
        QCOMPARE(parsed.entries.at(0).notes, QString("note, with comma"));
    }

    void csv_parse_chrome_export_header()
    {
        const QByteArray csv("\xEF\xBB\xBFname,url,username,password\r\nExample,https://example.com,alice,SecretPwd!\r\n");
        const auto parsed = parsePasswordCsv(csv, 1);
        QVERIFY(parsed.ok());
        QCOMPARE(parsed.entries.size(), 1);
        QCOMPARE(parsed.entries.at(0).entry.title, QString("Example"));
        QCOMPARE(parsed.entries.at(0).entry.username, QString("alice"));
        QCOMPARE(parsed.entries.at(0).password, QString("SecretPwd!"));
        QCOMPARE(parsed.entries.at(0).entry.url, QString("https://example.com"));
    }

    void csv_parse_keepassxc_export_header()
    {
        const QByteArray csv("Group,Title,Username,Password,URL,Notes\r\nPersonal/Email,Gmail,me@gmail.com,SecretPwd!,https://mail.google.com,hello\r\n");
        const auto parsed = parsePasswordCsv(csv, 1);
        QVERIFY(parsed.ok());
        QCOMPARE(parsed.entries.size(), 1);
        QCOMPARE(parsed.entries.at(0).entry.category, QString("Personal/Email"));
        QCOMPARE(parsed.entries.at(0).entry.title, QString("Gmail"));
        QCOMPARE(parsed.entries.at(0).entry.username, QString("me@gmail.com"));
        QCOMPARE(parsed.entries.at(0).password, QString("SecretPwd!"));
        QCOMPARE(parsed.entries.at(0).entry.url, QString("https://mail.google.com"));
        QCOMPARE(parsed.entries.at(0).notes, QString("hello"));
    }

    void csv_import_keepassxc_header()
    {
        PasswordVault vault;
        QVERIFY(vault.createVault("master"));

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto csvPath = QDir(dir.path()).filePath("keepassxc.csv");
        {
            QFile f(csvPath);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("Group,Title,Username,Password,URL,Notes\r\nPersonal/Email,Gmail,me@gmail.com,SecretPwd!,https://mail.google.com,hello\r\n");
        }

        const auto dbPath = QDir(AppPaths::appDataDir()).filePath("password.sqlite3");

        PasswordCsvImportWorker importer(csvPath, dbPath, vault.masterKey(), 1, nullptr);
        QSignalSpy spyFinished(&importer, &PasswordCsvImportWorker::finished);
        QSignalSpy spyFailed(&importer, &PasswordCsvImportWorker::failed);
        importer.run();
        QCOMPARE(spyFailed.count(), 0);
        QCOMPARE(spyFinished.count(), 1);

        PasswordRepository repo(&vault);
        const auto list = repo.listEntries();
        QCOMPARE(list.size(), 1);
        QCOMPARE(list.at(0).title, QString("Gmail"));
        QCOMPARE(list.at(0).username, QString("me@gmail.com"));
        QCOMPARE(list.at(0).url, QString("https://mail.google.com"));
        QCOMPARE(list.at(0).category, QString("Personal/Email"));

        const auto loaded = repo.loadEntry(list.at(0).id);
        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->password, QString("SecretPwd!"));
        QCOMPARE(loaded->notes, QString("hello"));
    }

    void csv_import_update_duplicates()
    {
        PasswordVault vault;
        QVERIFY(vault.createVault("master"));

        PasswordRepository repo(&vault);

        PasswordEntrySecrets e;
        e.entry.title = "Example";
        e.entry.username = "alice";
        e.password = "OldPwd!";
        e.entry.url = "https://example.com/login";
        e.entry.tags = {"oldtag"};
        QVERIFY(repo.addEntry(e));

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto csvPath = QDir(dir.path()).filePath("update.csv");
        {
            QFile f(csvPath);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("title,username,password,url,category,tags,notes\r\n");
            f.write("Example,alice,NewPwd!,https://example.com/login,Personal,newtag,hello\r\n");
        }

        const auto dbPath = QDir(AppPaths::appDataDir()).filePath("password.sqlite3");
        PasswordCsvImportWorker importer(csvPath, dbPath, vault.masterKey(), 1, nullptr);
        PasswordCsvImportOptions opt;
        opt.duplicatePolicy = PasswordCsvDuplicatePolicy::Update;
        importer.setOptions(opt);

        QSignalSpy spyFinished(&importer, &PasswordCsvImportWorker::finished);
        QSignalSpy spyFailed(&importer, &PasswordCsvImportWorker::failed);
        importer.run();
        QCOMPARE(spyFailed.count(), 0);
        QCOMPARE(spyFinished.count(), 1);

        const auto args = spyFinished.takeFirst();
        const int inserted = args.at(0).toInt();
        const int updated = args.at(1).toInt();
        QCOMPARE(inserted, 0);
        QCOMPARE(updated, 1);

        const auto list = repo.listEntries();
        QCOMPARE(list.size(), 1);
        const auto loaded = repo.loadEntry(list.at(0).id);
        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->password, QString("NewPwd!"));
        QVERIFY(loaded->entry.tags.contains("newtag"));
        QVERIFY(!loaded->entry.tags.contains("oldtag"));
    }

    void csv_import_create_groups_from_category_path()
    {
        PasswordVault vault;
        QVERIFY(vault.createVault("master"));

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto csvPath = QDir(dir.path()).filePath("keepassxc.csv");
        {
            QFile f(csvPath);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("Group,Title,Username,Password,URL,Notes\r\n");
            f.write("Personal/Email,Gmail,me@gmail.com,SecretPwd!,https://mail.google.com,hello\r\n");
        }

        const auto dbPath = QDir(AppPaths::appDataDir()).filePath("password.sqlite3");
        PasswordCsvImportWorker importer(csvPath, dbPath, vault.masterKey(), 1, nullptr);
        PasswordCsvImportOptions opt;
        opt.createGroupsFromCategoryPath = true;
        importer.setOptions(opt);

        QSignalSpy spyFinished(&importer, &PasswordCsvImportWorker::finished);
        QSignalSpy spyFailed(&importer, &PasswordCsvImportWorker::failed);
        importer.run();
        QCOMPARE(spyFailed.count(), 0);
        QCOMPARE(spyFinished.count(), 1);

        PasswordRepository repo(&vault);
        const auto groups = repo.listGroups();

        qint64 personalId = 0;
        qint64 emailId = 0;
        for (const auto &g : groups) {
            if (g.parentId == 1 && g.name == "Personal")
                personalId = g.id;
        }
        QVERIFY(personalId > 0);
        for (const auto &g : groups) {
            if (g.parentId == personalId && g.name == "Email")
                emailId = g.id;
        }
        QVERIFY(emailId > 0);

        const auto list = repo.listEntries();
        QCOMPARE(list.size(), 1);
        QCOMPARE(list.at(0).groupId, emailId);
    }

    void csv_import_default_entry_type()
    {
        PasswordVault vault;
        QVERIFY(vault.createVault("master"));

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto csvPath = QDir(dir.path()).filePath("type.csv");
        {
            QFile f(csvPath);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("title,username,password,url,category,tags,notes\r\n");
            f.write("GitHub Token,,ghp_xxx,,dev,token,\r\n");
        }

        const auto dbPath = QDir(AppPaths::appDataDir()).filePath("password.sqlite3");
        PasswordCsvImportWorker importer(csvPath, dbPath, vault.masterKey(), 1, nullptr);
        PasswordCsvImportOptions opt;
        opt.defaultEntryType = PasswordEntryType::ApiKeyToken;
        importer.setOptions(opt);

        QSignalSpy spyFinished(&importer, &PasswordCsvImportWorker::finished);
        QSignalSpy spyFailed(&importer, &PasswordCsvImportWorker::failed);
        importer.run();
        QCOMPARE(spyFailed.count(), 0);
        QCOMPARE(spyFinished.count(), 1);

        PasswordRepository repo(&vault);
        const auto list = repo.listEntries();
        QCOMPARE(list.size(), 1);
        QCOMPARE(static_cast<int>(list.at(0).type), static_cast<int>(PasswordEntryType::ApiKeyToken));
    }

    void csv_import_dedup_and_health_scan()
    {
        PasswordVault vault;
        QVERIFY(vault.createVault("master"));

        PasswordRepository repo(&vault);

        PasswordEntrySecrets e1;
        e1.entry.title = "SiteA";
        e1.entry.username = "a";
        e1.password = "SamePassword!123";
        e1.entry.url = "https://a.example.com";
        QVERIFY(repo.addEntry(e1));

        PasswordEntrySecrets e2;
        e2.entry.title = "SiteB";
        e2.entry.username = "b";
        e2.password = "SamePassword!123";
        e2.entry.url = "https://b.example.com";
        QVERIFY(repo.addEntry(e2));

        QVector<PasswordEntrySecrets> full;
        for (const auto &summary : repo.listEntries()) {
            const auto loaded = repo.loadEntry(summary.id);
            QVERIFY(loaded.has_value());
            full.push_back(loaded.value());
        }

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto csvPath = QDir(dir.path()).filePath("export.csv");
        {
            QFile f(csvPath);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(exportPasswordCsv(full));
        }

        const auto dbPath = QDir(AppPaths::appDataDir()).filePath("password.sqlite3");

        PasswordCsvImportWorker importer(csvPath, dbPath, vault.masterKey(), 1, nullptr);
        QSignalSpy spyFinished(&importer, &PasswordCsvImportWorker::finished);
        QSignalSpy spyFailed(&importer, &PasswordCsvImportWorker::failed);
        importer.run();
        QCOMPARE(spyFailed.count(), 0);
        QCOMPARE(spyFinished.count(), 1);

        // importing the same CSV again should dedup everything
        PasswordCsvImportWorker importer2(csvPath, dbPath, vault.masterKey(), 1, nullptr);
        QSignalSpy spyFinished2(&importer2, &PasswordCsvImportWorker::finished);
        importer2.run();
        QCOMPARE(spyFinished2.count(), 1);
        const auto args2 = spyFinished2.takeFirst();
        const int inserted2 = args2.at(0).toInt();
        const int updated2 = args2.at(1).toInt();
        QVERIFY(inserted2 == 0);
        QVERIFY(updated2 == 0);

        // make entries stale
        {
            auto db = PasswordDatabase::db();
            QVERIFY(db.isOpen());
            QSqlQuery q(db);
            const auto old = QDateTime::currentDateTime().addDays(-120).toSecsSinceEpoch();
            QVERIFY(q.exec(QString("UPDATE password_entries SET updated_at = %1").arg(old)));
        }

        PasswordHealthWorker health(dbPath, vault.masterKey(), false, true, nullptr);
        QSignalSpy spyHealthFinished(&health, &PasswordHealthWorker::finished);
        QSignalSpy spyHealthFailed(&health, &PasswordHealthWorker::failed);
        health.run();
        QCOMPARE(spyHealthFailed.count(), 0);
        QCOMPARE(spyHealthFinished.count(), 1);
        const auto healthArgs = spyHealthFinished.takeFirst();
        const auto items = qvariant_cast<QVector<PasswordHealthItem>>(healthArgs.at(0));
        QVERIFY(items.size() >= 2);

        int reusedCount = 0;
        int staleCount = 0;
        for (const auto &it : items) {
            if (it.reused)
                reusedCount++;
            if (it.stale)
                staleCount++;
        }
        QVERIFY(reusedCount >= 2);
        QVERIFY(staleCount >= 2);
    }

    void graph_build_basics()
    {
        PasswordEntry a;
        a.type = PasswordEntryType::WebLogin;
        a.title = "Example";
        a.username = "alice";
        a.url = "https://example.com/login";

        PasswordEntry b = a;
        b.username = "bob";

        PasswordEntry c;
        c.type = PasswordEntryType::ApiKeyToken;
        c.title = "GitHub Token";
        c.username = "";
        c.url = "";

        const auto graph = PasswordGraph::build({a, b, c});

        int typeCount = 0;
        int serviceCount = 0;
        int accountCount = 0;
        for (const auto &n : graph.nodes) {
            switch (n.kind) {
            case PasswordGraph::NodeKind::Type:
                typeCount++;
                break;
            case PasswordGraph::NodeKind::Service:
                serviceCount++;
                break;
            case PasswordGraph::NodeKind::Account:
                accountCount++;
                break;
            default:
                break;
            }
        }

        QCOMPARE(typeCount, 2);
        QCOMPARE(serviceCount, 2);
        QCOMPARE(accountCount, 3);
        QCOMPARE(graph.edges.size(), 5);

        bool foundWebService = false;
        for (const auto &n : graph.nodes) {
            if (n.kind != PasswordGraph::NodeKind::Service)
                continue;
            if (n.label == "example.com")
                foundWebService = true;
        }
        QVERIFY(foundWebService);
    }

    void favicon_cache_roundtrip()
    {
        auto db = PasswordDatabase::db();
        QVERIFY(db.isOpen());

        QImage img(16, 16, QImage::Format_ARGB32_Premultiplied);
        img.fill(qRgba(200, 60, 60, 255));
        QByteArray bytes;
        {
            QBuffer buf(&bytes);
            QVERIFY(buf.open(QIODevice::WriteOnly));
            QVERIFY(img.save(&buf, "PNG"));
        }

        QSqlQuery q(db);
        q.prepare(R"sql(
            INSERT OR REPLACE INTO favicon_cache(host, icon, content_type, fetched_at)
            VALUES(?, ?, ?, ?)
        )sql");
        q.addBindValue("example.com");
        q.addBindValue(bytes);
        q.addBindValue("image/png");
        q.addBindValue(QDateTime::currentDateTime().toSecsSinceEpoch());
        QVERIFY(q.exec());

        PasswordFaviconService service;
        service.setNetworkEnabled(false);
        const auto icon = service.iconForUrl("https://example.com/login");
        QVERIFY(!icon.isNull());
    }

    void pwned_offline_cache()
    {
        PasswordVault vault;
        QVERIFY(vault.createVault("master"));

        PasswordRepository repo(&vault);
        PasswordEntrySecrets e;
        e.entry.title = "Example";
        e.entry.username = "user";
        e.entry.url = "https://example.com";
        e.password = "password";
        QVERIFY(repo.addEntry(e));

        auto db = PasswordDatabase::db();
        QVERIFY(db.isOpen());

        const QByteArray prefix = "5BAA6";
        const QByteArray body = "1E4C9B93F3F0682250B6CF8331B7EE68FD8:3303003\r\n";

        QSqlQuery q(db);
        q.prepare(R"sql(
            INSERT OR REPLACE INTO pwned_prefix_cache(prefix, body, fetched_at)
            VALUES(?, ?, ?)
        )sql");
        q.addBindValue(QString::fromLatin1(prefix));
        q.addBindValue(body);
        q.addBindValue(QDateTime::currentDateTime().toSecsSinceEpoch());
        QVERIFY(q.exec());

        const auto dbPath = QDir(AppPaths::appDataDir()).filePath("password.sqlite3");

        PasswordHealthWorker health(dbPath, vault.masterKey(), true, false, nullptr);
        QSignalSpy spyFinished(&health, &PasswordHealthWorker::finished);
        QSignalSpy spyFailed(&health, &PasswordHealthWorker::failed);
        health.run();
        QCOMPARE(spyFailed.count(), 0);
        QCOMPARE(spyFinished.count(), 1);
        const auto args = spyFinished.takeFirst();
        const auto items = qvariant_cast<QVector<PasswordHealthItem>>(args.at(0));
        QVERIFY(items.size() >= 1);

        bool found = false;
        for (const auto &it : items) {
            if (it.title != "Example")
                continue;
            found = true;
            QVERIFY(it.pwnedChecked);
            QVERIFY(it.pwned);
            QVERIFY(it.pwnedCount >= 1);
        }
        QVERIFY(found);
    }

private:
    static void resetDatabase()
    {
        auto db = PasswordDatabase::db();
        QVERIFY(db.isOpen());

        QSqlQuery q(db);
        QVERIFY(q.exec("DELETE FROM password_entries"));
        QVERIFY(q.exec("DELETE FROM tags"));
        QVERIFY(q.exec("DELETE FROM favicon_cache"));
        QVERIFY(q.exec("DELETE FROM pwned_prefix_cache"));
        QVERIFY(q.exec("DELETE FROM groups WHERE id <> 1"));
        QVERIFY(q.exec("DELETE FROM vault_meta"));
    }
};

QTEST_MAIN(PasswordIntegrationTests)

#include "tst_password_integration.moc"
