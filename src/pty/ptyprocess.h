#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

class QSocketNotifier;
class QThread;

class PtyProcess : public QObject
{
    Q_OBJECT
public:
    explicit PtyProcess(QObject *parent = nullptr);
    ~PtyProcess() override;

    bool start(const QString &program,
               const QStringList &arguments,
               const QString &workingDir = QString());
    void write(const QByteArray &data);
    void setWinsize(int cols, int rows);
    void terminate(bool notify = true);
    bool isRunning() const;
    int pid() const { return m_pid; }
    QString lastError() const { return m_lastError; }
#ifdef Q_OS_WIN
    static bool hasConPty();
#endif

signals:
    void readyRead(const QByteArray &data);
    void finished(int exitCode);

private slots:
    void onWinChunk(const QByteArray &data);
    void onWinDone();

private:
    void onMasterReadable();
    void closeMaster();
    bool spawn(const QString &program, const QStringList &arguments,
               const QString &workingDir);
#ifdef Q_OS_WIN
    bool spawnWindows(const QString &program, const QStringList &arguments,
                      const QString &workingDir);
    bool spawnWindowsPipes(const QString &program, const QStringList &arguments,
                           const QString &workingDir, const QString &cmdLine);
    void startWinReader();
    void closeWindows();
#endif

    int m_masterFd = -1;
    int m_pid = -1;
    int m_cols = 80;
    int m_rows = 24;
    QSocketNotifier *m_notifier = nullptr;
    QString m_lastError;
#ifdef Q_OS_WIN
    quintptr m_inWrite = 0;
    quintptr m_outRead = 0;
    quintptr m_pty = 0;
    quintptr m_process = 0;
    quintptr m_thread = 0;
    QThread *m_reader = nullptr;
    bool m_running = false;
#endif
};
