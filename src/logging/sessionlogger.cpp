#include "logging/sessionlogger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>

QString SessionLogger::createLogPath(const Session &session)
{
    if (!session.logPath.trimmed().isEmpty()) {
        QDir().mkpath(QFileInfo(session.logPath).absolutePath());
        return session.logPath;
    }
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/logs");
    QDir().mkpath(dir);
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString safe = session.displayTitle()
                             .replace(QLatin1Char('/'), QLatin1Char('_'))
                             .replace(QLatin1Char('\\'), QLatin1Char('_'))
                             .replace(QLatin1Char(' '), QLatin1Char('_'));
    return dir + QLatin1Char('/') + safe + QLatin1Char('-') + stamp + QStringLiteral(".log");
}

void SessionLogger::writeHeader(const QString &path, const Session &session)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    QTextStream ts(&f);
    ts << QStringLiteral("==== LinHub 会话日志 ====\n");
    ts << QStringLiteral("会话: ") << session.displayTitle() << QLatin1Char('\n');
    ts << QStringLiteral("协议: ") << protocolDisplayName(session.protocol) << QLatin1Char('\n');
    ts << QStringLiteral("主机: ") << session.host << QLatin1Char('\n');
    ts << QStringLiteral("开始: ") << QDateTime::currentDateTime().toString(Qt::ISODate) << QLatin1Char('\n');
    ts << QStringLiteral("========================\n");
}

void SessionLogger::append(const QString &path, const QByteArray &data)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append))
        return;
    f.write(data);
}
