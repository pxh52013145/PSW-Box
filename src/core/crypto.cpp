#include "crypto.h"

#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QPasswordDigestor>
#include <QRandomGenerator>

namespace {

constexpr int kNonceSize = 16;
constexpr int kTagSize = 16;
constexpr int kKeystreamBlockSize = 32;
constexpr char kMagic[] = "TBX1";
constexpr quint8 kVersion = 1;

QByteArray hmacSha256(const QByteArray &key, const QByteArray &message)
{
    return QMessageAuthenticationCode::hash(message, key, QCryptographicHash::Sha256);
}

QByteArray deriveSubkey(const QByteArray &key, const QByteArray &context)
{
    return hmacSha256(key, context);
}

bool constantTimeEquals(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size())
        return false;
    quint8 diff = 0;
    for (qsizetype i = 0; i < a.size(); ++i) {
        diff |= static_cast<quint8>(a[i]) ^ static_cast<quint8>(b[i]);
    }
    return diff == 0;
}

QByteArray xorStream(const QByteArray &encKey, const QByteArray &nonce, const QByteArray &input)
{
    QByteArray out;
    out.resize(input.size());

    const auto blocks = (input.size() + kKeystreamBlockSize - 1) / kKeystreamBlockSize;
    for (qsizetype blockIndex = 0; blockIndex < blocks; ++blockIndex) {
        const auto counter = static_cast<quint32>(blockIndex);
        QByteArray counterBytes;
        counterBytes.resize(4);
        counterBytes[0] = static_cast<char>((counter >> 24) & 0xFF);
        counterBytes[1] = static_cast<char>((counter >> 16) & 0xFF);
        counterBytes[2] = static_cast<char>((counter >> 8) & 0xFF);
        counterBytes[3] = static_cast<char>(counter & 0xFF);

        const auto stream = hmacSha256(encKey, nonce + counterBytes);
        const auto offset = blockIndex * kKeystreamBlockSize;
        const auto chunk = qMin<qsizetype>(kKeystreamBlockSize, input.size() - offset);
        for (qsizetype i = 0; i < chunk; ++i)
            out[offset + i] = static_cast<char>(static_cast<quint8>(input[offset + i]) ^ static_cast<quint8>(stream[i]));
    }

    return out;
}

} // namespace

namespace Crypto {

QByteArray randomBytes(int size)
{
    QByteArray out;
    out.resize(size);
    auto *generator = QRandomGenerator::system();
    for (int i = 0; i < size; ++i)
        out[i] = static_cast<char>(generator->generate() & 0xFF);
    return out;
}

QByteArray sha256(const QByteArray &data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}

QByteArray pbkdf2Sha256(const QByteArray &passwordUtf8, const QByteArray &salt, int iterations, int dkLen)
{
    return QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256, passwordUtf8, salt, iterations, dkLen);
}

QByteArray seal(const QByteArray &key, const QByteArray &plaintext)
{
    const auto encKey = deriveSubkey(key, "ToolboxPM/enc");
    const auto macKey = deriveSubkey(key, "ToolboxPM/mac");

    const auto nonce = randomBytes(kNonceSize);
    const auto ciphertext = xorStream(encKey, nonce, plaintext);
    const auto tagFull = hmacSha256(macKey, nonce + ciphertext);
    const auto tag = tagFull.left(kTagSize);

    QByteArray out;
    out.reserve(sizeof(kMagic) - 1 + 1 + kNonceSize + kTagSize + ciphertext.size());
    out.append(kMagic, sizeof(kMagic) - 1);
    out.append(static_cast<char>(kVersion));
    out.append(nonce);
    out.append(tag);
    out.append(ciphertext);
    return out;
}

std::optional<QByteArray> open(const QByteArray &key, const QByteArray &blob)
{
    const auto headerSize = static_cast<int>(sizeof(kMagic) - 1 + 1 + kNonceSize + kTagSize);
    if (blob.size() < headerSize)
        return std::nullopt;

    if (blob.left(sizeof(kMagic) - 1) != QByteArray(kMagic, sizeof(kMagic) - 1))
        return std::nullopt;

    const auto version = static_cast<quint8>(blob[sizeof(kMagic) - 1]);
    if (version != kVersion)
        return std::nullopt;

    const auto nonceOffset = static_cast<int>(sizeof(kMagic) - 1 + 1);
    const auto tagOffset = nonceOffset + kNonceSize;
    const auto ciphertextOffset = tagOffset + kTagSize;

    const auto nonce = blob.mid(nonceOffset, kNonceSize);
    const auto tag = blob.mid(tagOffset, kTagSize);
    const auto ciphertext = blob.mid(ciphertextOffset);

    const auto encKey = deriveSubkey(key, "ToolboxPM/enc");
    const auto macKey = deriveSubkey(key, "ToolboxPM/mac");

    const auto expectedTag = hmacSha256(macKey, nonce + ciphertext).left(kTagSize);
    if (!constantTimeEquals(tag, expectedTag))
        return std::nullopt;

    return xorStream(encKey, nonce, ciphertext);
}

void secureZero(QByteArray &data)
{
    if (data.isEmpty())
        return;
    volatile char *p = data.data();
    for (qsizetype i = 0; i < data.size(); ++i)
        p[i] = 0;
    data.clear();
}

} // namespace Crypto

