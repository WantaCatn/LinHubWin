#pragma once

#include "core/session.h"

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

class QTimer;

class SshMaster : public QObject
{
    Q_OBJECT
public:
    explicit SshMaster(QObject *parent = nullptr);
    ~SshMaster() override { stop(); }
    SshMaster(const SshMaster &) = delete;
    SshMaster &operator=(const SshMaster &) = delete;

    void start(const Session &session, const QString &password);
    void stop();
    QString controlPath() const { return m_path; }
    bool isReady() const { return !m_path.isEmpty() && m_proc.state() == QProcess::Running; }
    QString lastError() const { return m_error; }

signals:
    void ready();
    void failed(const QString &reason);

private:
    void tick();

    QProcess m_proc;
    QTimer *m_timer = nullptr;
    QElapsedTimer m_deadline;
    QString m_path;
    QString m_error;
};

class SessionLauncher
{
public:
    static bool build(const Session &session,
                      const QString &password,
                      QString *program,
                      QStringList *arguments,
                      QString *workingDir,
                      const QString &controlPath = QString());
    static QString askpassPath();
    static void preparePassword(const QString &password);
    static void clearPassword();
    static QStringList sshOptions(const Session &session, bool includeForwards = false);
    static QStringList muxArgs(const QString &controlPath, bool asMaster);
    static QString sshTarget(const Session &session);
    static int sshPort(const Session &session);
    static QString sshProgram();
};
