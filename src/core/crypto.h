#pragma once

#include <QByteArray>

#include <optional>

namespace Crypto {

QByteArray randomBytes(int size);
QByteArray sha256(const QByteArray &data);
QByteArray pbkdf2Sha256(const QByteArray &passwordUtf8, const QByteArray &salt, int iterations, int dkLen);

QByteArray seal(const QByteArray &key, const QByteArray &plaintext);
std::optional<QByteArray> open(const QByteArray &key, const QByteArray &blob);

void secureZero(QByteArray &data);

} // namespace Crypto

