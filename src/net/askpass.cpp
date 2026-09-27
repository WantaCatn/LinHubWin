#include "net/askpass.h"

#include <QByteArray>
#include <QFile>
#include <QFileDevice>
#include <QIODevice>
#include <QStandardPaths>
#include <QUuid>
#include <QtGlobal>
#include <cstdio>

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

void AskPass::store(const QString &password, QString *outFile)
{
    if (!outFile)
        return;
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString path = dir + QLatin1Char('/') + QStringLiteral("linhub-ask-")
        + QUuid::createUuid().toString().remove(QLatin1Char('{')).remove(QLatin1Char('}')).remove(QLatin1Char('-'));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
#ifdef Q_OS_UNIX
    chmod(path.toLocal8Bit().constData(), 0600);
#endif
    f.write(password.toUtf8());
    f.close();
    *outFile = path;
}

void AskPass::clear(const QString &file)
{
    if (!file.isEmpty())
        QFile::remove(file);
}

int AskPass::run()
{
    const QByteArray path = qgetenv("LINHUB_ASKPASS_FILE");
    if (path.isEmpty())
        return 1;
    QFile f(QString::fromLocal8Bit(path));
    if (!f.open(QIODevice::ReadOnly))
        return 1;
    const QByteArray pw = f.readAll();
    f.close();
    fwrite(pw.constData(), 1, static_cast<size_t>(pw.size()), stdout);
    fputc('\n', stdout);
    return 0;
}
