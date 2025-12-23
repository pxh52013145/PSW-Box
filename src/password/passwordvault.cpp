#include "passwordvault.h"

#include "core/crypto.h"
#include "core/database.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>

namespace {

constexpr int kSaltSize = 16;
constexpr int kMasterKeySize = 32;
constexpr int kDefaultIterations = 120000;

} // namespace

PasswordVault::PasswordVault(QObject *parent) : QObject(parent)
{
    reloadMetadata();
}

bool PasswordVault::isInitialized() const
{
    return meta_.has_value();
}

bool PasswordVault::isUnlocked() const
{
    return !masterKey_.isEmpty();
}

QString PasswordVault::lastError() const
{
    return lastError_;
}

bool PasswordVault::reloadMetadata()
{
    meta_ = readMeta();
    emit stateChanged();
    return meta_.has_value();
}

PasswordVault::Meta PasswordVault::defaultMeta()
{
    Meta meta;
    meta.salt = Crypto::randomBytes(kSaltSize);
    meta.iterations = kDefaultIterations;
    return meta;
}

QByteArray PasswordVault::computeVerifier(const QByteArray &masterKey)
{
    return Crypto::sha256(masterKey);
}

void PasswordVault::setError(const QString &error)
{
    lastError_ = error;
}

std::optional<PasswordVault::Meta> PasswordVault::readMeta()
{
    auto database = Database::db();
    if (!database.isOpen()) {
        setError("数据库未打开");
        return std::nullopt;
    }

    QSqlQuery query(database);
    query.prepare(R"sql(
        SELECT kdf_salt, kdf_iterations, verifier
        FROM vault_meta
        WHERE id = 1
        LIMIT 1
    )sql");

    if (!query.exec()) {
        setError(QString("读取 vault_meta 失败：%1").arg(query.lastError().text()));
        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    Meta meta;
    meta.salt = query.value(0).toByteArray();
    meta.iterations = query.value(1).toInt();
    meta.verifier = query.value(2).toByteArray();
    return meta;
}

bool PasswordVault::writeMeta(const Meta &meta)
{
    auto database = Database::db();
    if (!database.isOpen()) {
        setError("数据库未打开");
        return false;
    }

    QSqlQuery query(database);
    query.prepare(R"sql(
        INSERT INTO vault_meta(id, kdf_salt, kdf_iterations, verifier, created_at, updated_at)
        VALUES(1, ?, ?, ?, ?, ?)
        ON CONFLICT(id) DO UPDATE SET
            kdf_salt = excluded.kdf_salt,
            kdf_iterations = excluded.kdf_iterations,
            verifier = excluded.verifier,
            updated_at = excluded.updated_at
    )sql");
    query.addBindValue(meta.salt);
    query.addBindValue(meta.iterations);
    query.addBindValue(meta.verifier);
    const auto now = QDateTime::currentDateTime().toSecsSinceEpoch();
    query.addBindValue(now);
    query.addBindValue(now);

    if (!query.exec()) {
        setError(QString("写入 vault_meta 失败：%1").arg(query.lastError().text()));
        return false;
    }

    meta_ = meta;
    return true;
}

bool PasswordVault::createVault(const QString &masterPassword)
{
    if (isInitialized()) {
        setError("Vault 已初始化");
        return false;
    }

    if (masterPassword.trimmed().isEmpty()) {
        setError("主密码不能为空");
        return false;
    }

    auto meta = defaultMeta();
    const auto key = Crypto::pbkdf2Sha256(masterPassword.toUtf8(), meta.salt, meta.iterations, kMasterKeySize);
    meta.verifier = computeVerifier(key);

    if (!writeMeta(meta))
        return false;

    masterKey_ = key;
    emit stateChanged();
    return true;
}

bool PasswordVault::unlock(const QString &masterPassword)
{
    if (!meta_.has_value())
        meta_ = readMeta();

    if (!meta_.has_value()) {
        setError("Vault 未初始化");
        return false;
    }

    const auto key = Crypto::pbkdf2Sha256(masterPassword.toUtf8(), meta_->salt, meta_->iterations, kMasterKeySize);
    const auto verifier = computeVerifier(key);
    if (verifier != meta_->verifier) {
        setError("主密码错误");
        return false;
    }

    masterKey_ = key;
    emit stateChanged();
    return true;
}

void PasswordVault::lock()
{
    Crypto::secureZero(masterKey_);
    emit stateChanged();
}

QByteArray PasswordVault::masterKey() const
{
    return masterKey_;
}

bool PasswordVault::changeMasterPassword(const QString &newMasterPassword)
{
    if (!isUnlocked()) {
        setError("请先解锁");
        return false;
    }

    if (newMasterPassword.trimmed().isEmpty()) {
        setError("新主密码不能为空");
        return false;
    }

    auto database = Database::db();
    if (!database.isOpen()) {
        setError("数据库未打开");
        return false;
    }

    const auto oldKey = masterKey_;

    auto newMeta = defaultMeta();
    const auto newKey = Crypto::pbkdf2Sha256(newMasterPassword.toUtf8(), newMeta.salt, newMeta.iterations, kMasterKeySize);
    newMeta.verifier = computeVerifier(newKey);

    if (!database.transaction()) {
        setError(QString("开启事务失败：%1").arg(database.lastError().text()));
        return false;
    }

    QSqlQuery query(database);
    query.prepare(R"sql(
        SELECT id, password_enc, notes_enc
        FROM password_entries
    )sql");

    if (!query.exec()) {
        database.rollback();
        setError(QString("读取条目失败：%1").arg(query.lastError().text()));
        return false;
    }

    while (query.next()) {
        const auto id = query.value(0).toLongLong();
        const auto passwordEnc = query.value(1).toByteArray();
        const auto notesEnc = query.value(2).toByteArray();

        const auto passwordPlain = Crypto::open(oldKey, passwordEnc);
        if (!passwordPlain.has_value()) {
            database.rollback();
            setError(QString("解密密码失败（id=%1）").arg(id));
            return false;
        }

        std::optional<QByteArray> notesPlain;
        if (!notesEnc.isEmpty())
            notesPlain = Crypto::open(oldKey, notesEnc);

        if (!notesEnc.isEmpty() && !notesPlain.has_value()) {
            database.rollback();
            setError(QString("解密备注失败（id=%1）").arg(id));
            return false;
        }

        QSqlQuery update(database);
        update.prepare(R"sql(
            UPDATE password_entries
            SET password_enc = ?, notes_enc = ?, updated_at = ?
            WHERE id = ?
        )sql");
        update.addBindValue(Crypto::seal(newKey, passwordPlain.value()));
        update.addBindValue(notesPlain.has_value() ? Crypto::seal(newKey, notesPlain.value()) : QByteArray());
        update.addBindValue(QDateTime::currentDateTime().toSecsSinceEpoch());
        update.addBindValue(id);

        if (!update.exec()) {
            database.rollback();
            setError(QString("更新条目失败（id=%1）：%2").arg(id).arg(update.lastError().text()));
            return false;
        }
    }

    if (!writeMeta(newMeta)) {
        database.rollback();
        return false;
    }

    if (!database.commit()) {
        database.rollback();
        setError(QString("提交事务失败：%1").arg(database.lastError().text()));
        return false;
    }

    Crypto::secureZero(masterKey_);
    masterKey_ = newKey;
    emit stateChanged();
    return true;
}

