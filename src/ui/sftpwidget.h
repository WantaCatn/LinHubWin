#pragma once

#include "core/session.h"
#include "net/sftpclient.h"

#include <QSize>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QLabel;
class QToolButton;
class QFileSystemWatcher;
class QDragEnterEvent;
class QDropEvent;
class QProgressBar;
class QPushButton;
class QMenu;

struct SftpJob {
    int id = 0;
    bool upload = false;
    QString name;
    QString local;
    QString remote;
    qint64 size = 0;
    qint64 done = 0;
    bool isDir = false;
    enum State { Queued, Running, Paused, Done, Failed } state = Queued;
    QString message;
    bool persisted = false;
    int parentId = 0;
    bool cached = false;
};

class SftpTransferDialog;

class SftpWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SftpWidget(QWidget *parent = nullptr);
    ~SftpWidget() override;
    void attachSession(const Session &session, const QString &password, const QString &controlPath = QString());
    void detachSession();
    void refreshFileColors();
    void applyTerminalCwd(const QString &cwd);
    bool followTerminal() const { return m_follow; }
    void uploadLocalFiles(const QStringList &localPaths, const QString &remoteDir = QString());
    void deleteSelected();
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    QString downloadToTemp(const QString &remotePath, const QString &name, qint64 size = 0);
    bool saveRemoteTo(const QString &remotePath, const QString &localPath, const QString &name,
                      qint64 size, bool isDir);

signals:
    void followRequested();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void reload(const QString &path);
    void navigateUp();
    void downloadCurrent();
    void uploadPicker(const QString &remoteDir = QString());
    void goToEnteredPath();
    QString resolvedRemoteDir(const QString &remoteDir);
    void setFollow(bool on);
    void pumpQueue();
    bool canTransfer() const;
    SftpEntry entryFromItem(QTreeWidgetItem *it) const;
    void showContextMenu(const QPoint &pos);
    void openEntry(const SftpEntry &e, const QString &app = QString());
    void editEntry(const SftpEntry &e);
    void openWithEntry(const SftpEntry &e);
    void deleteEntries(const QVector<SftpEntry> &entries);
    void mkdirHere();
    void createFileHere();
    void renameEntry(const SftpEntry &e);
    void showProperties(const SftpEntry &e);
    void enqueueDownload(const QString &remote, const QString &local, const QString &name, qint64 size,
                         int parentId = 0, bool cached = false);
    void enqueueUpload(const QString &local, const QString &remote, const QString &name, qint64 size,
                       int parentId = 0);
    void enqueueDirUpload(const QString &localDir, const QString &remoteDir);
    void enqueueDirDownload(const QString &remoteDir, const QString &localDir);
    void updateProgressUi();
    void showTransferDetails();
    void persistJob(SftpJob &job);
    void maybeReloadAfterTransfer();
    SftpJob *jobById(int id);
    QVector<SftpEntry> selectedEntries() const;
    void pauseJob(int id);
    void resumeJob(int id);
    void retryJob(int id);
    void removeJob(int id);
    void handleLiveAction(int id, const QString &op);
    void updateParentJob(int parentId);
    void watchEditedFile(const QString &local, const QString &remote, const QString &name);
    void onEditedFileChanged(const QString &path);
    void purgeDownloadCache();
    QString cacheDir() const;

    SftpClient m_client;
    QTreeWidget *m_tree = nullptr;
    QLineEdit *m_pathEdit = nullptr;
    QToolButton *m_followBtn = nullptr;
    QProgressBar *m_bar = nullptr;
    QLabel *m_barLabel = nullptr;
    QPushButton *m_detailBtn = nullptr;
    QString m_currentPath = QStringLiteral(".");
    Session m_session;
    QString m_password;
    QString m_controlPath;
    bool m_follow = false;
    bool m_userNav = false;
    QFileSystemWatcher *m_watch = nullptr;
    struct EditWatch {
        QString local;
        QString remote;
        QString name;
        qint64 mtime = 0;
        qint64 size = 0;
    };
    QVector<EditWatch> m_edits;
    QVector<SftpJob> m_jobs;
    int m_nextJobId = 1;
    int m_activeJobId = 0;
    SftpTransferDialog *m_details = nullptr;
};
