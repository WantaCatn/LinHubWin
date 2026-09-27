#include "util/sshconfigimporter.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

QVector<Session> SshConfigImporter::fromFile(const QString &path)
{
    QVector<Session> out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return out;

    Session current;
    bool hasHost = false;
    auto flush = [&] {
        if (hasHost && current.host != QLatin1String("*")) {
            if (current.name.isEmpty())
                current.name = current.host;
            current.protocol = Protocol::Ssh;
            out.push_back(current);
        }
        current = Session{};
        hasHost = false;
    };

    QTextStream ts(&f);
    while (!ts.atEnd()) {
        QString line = ts.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const int sp = line.indexOf(QRegularExpression(QStringLiteral("\\s+")));
        if (sp < 0)
            continue;
        const QString key = line.left(sp);
        const QString value = line.mid(sp).trimmed();
        if (key.compare(QLatin1String("Host"), Qt::CaseInsensitive) == 0) {
            flush();
            current.host = value.split(QLatin1Char(' ')).first();
            current.name = current.host;
            hasHost = true;
        } else if (key.compare(QLatin1String("HostName"), Qt::CaseInsensitive) == 0) {
            current.host = value;
        } else if (key.compare(QLatin1String("User"), Qt::CaseInsensitive) == 0) {
            current.username = value;
        } else if (key.compare(QLatin1String("Port"), Qt::CaseInsensitive) == 0) {
            current.port = value.toInt();
        } else if (key.compare(QLatin1String("IdentityFile"), Qt::CaseInsensitive) == 0) {
            current.keyPath = value;
            current.authType = AuthType::Key;
        } else if (key.compare(QLatin1String("ProxyJump"), Qt::CaseInsensitive) == 0) {
            current.jumpHost = value;
        } else if (key.compare(QLatin1String("ForwardX11"), Qt::CaseInsensitive) == 0) {
            current.x11Forward = value.compare(QLatin1String("yes"), Qt::CaseInsensitive) == 0;
        }
    }
    flush();
    return out;
}

QVector<Session> SshConfigImporter::fromDefault()
{
    return fromFile(QDir::homePath() + QStringLiteral("/.ssh/config"));
}
