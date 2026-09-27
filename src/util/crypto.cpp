#include "util/crypto.h"

#include <QCryptographicHash>
#include <QSysInfo>
#include <QtGlobal>

namespace {

QByteArray machineKey()
{
    const QByteArray seed = QSysInfo::machineUniqueId()
        + QByteArrayLiteral("LinHub-session-v1")
        + qgetenv("USER")
        + qgetenv("USERNAME");
    return QCryptographicHash::hash(seed, QCryptographicHash::Sha256);
}

QByteArray xorWithKey(const QByteArray &data, const QByteArray &key)
{
    QByteArray out(data.size(), Qt::Uninitialized);
    for (int i = 0; i < data.size(); ++i)
        out[i] = data[i] ^ key[i % key.size()];
    return out;
}

} // namespace

QString Crypto::encrypt(const QString &plain)
{
    if (plain.isEmpty())
        return {};
    const QByteArray raw = xorWithKey(plain.toUtf8(), machineKey());
    return QString::fromLatin1(raw.toBase64());
}

QString Crypto::decrypt(const QString &encoded)
{
    if (encoded.isEmpty())
        return {};
    const QByteArray raw = QByteArray::fromBase64(encoded.toLatin1());
    return QString::fromUtf8(xorWithKey(raw, machineKey()));
}

bool Crypto::isBlank(const QString &encoded)
{
    return encoded.trimmed().isEmpty();
}
