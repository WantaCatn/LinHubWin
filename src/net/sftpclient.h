#pragma once

#include "core/session.h"

#include <QObject>
#include <QVector>

struct SftpEntry {
    QString name;
    QString path;
    bool isDir = false;
    bool isLink = false;
    bool isExec = false;
    qint64 size = 0;
    QString mtime;
    QString mode;
};

class QFile;
class QProcess;

class SftpClient : public QObject
{
    Q_OBJECT
public:
    explicit SftpClient(QObject *parent = nullptr);
    ~SftpClient() override;
    void configure(const Session &session, const QString &password, const QString &controlPath = QString());
    void clear();
    void list(const QString &path);
    void startDownload(const QString &remote, const QString &local, qint64 expectedSize = 0,
                       bool resume = true);
    void startUpload(const QString &local, const QString &remote, qint64 expectedSize = 0,
                     bool resume = true);
    bool downloadBlocking(const QString &remote, const QString &local, int timeoutMs = 120000,
                          qint64 expectedSize = 0, bool resume = true);
    qint64 remoteFileSize(const QString &path);
    bool chmodPath(const QString &remote, const QString &modeOctal, QString *err = nullptr);
    bool removePath(const QString &remote, bool recursive, QString *err = nullptr);
    bool mkdirPath(const QString &remote, QString *err = nullptr);
    bool createFile(const QString &remote, QString *err = nullptr);
    bool mkdirPaths(const QStringList &remotes, QString *err = nullptr);
    bool renamePath(const QString &from, const QString &to, QString *err = nullptr);
    QString resolvePath(const QString &path, QString *err = nullptr);
    QStringList listRemoteFiles(const QString &dir);
    QStringList listRemoteDirs(const QString &dir);
    bool isConfigured() const;
    bool isTransferring() const;
    void abortTransfer();

signals:
    void listed(const QString &path, const QVector<SftpEntry> &entries);
    void bytesTransferred(qint64 done, qint64 total);
    void transferFinished(bool ok, const QString &message);
    void errorOccurred(const QString &message);

private:
    QStringList sshCmdArgs(bool nullStdin) const;
    void prepareAuth();
    bool runRemote(const QString &cmd, QString *out, QString *err, int timeoutMs = 30000);
    void abortActive();
    void pumpUpload();
    void onDownloadReadyRead();
    void onTransferFinished(int code);

    Session m_session;
    QString m_password;
    QString m_controlPath;
    QProcess *m_proc = nullptr;
    QFile *m_file = nullptr;
    qint64 m_xferTotal = 0;
    qint64 m_xferDone = 0;
    bool m_uploading = false;
};
