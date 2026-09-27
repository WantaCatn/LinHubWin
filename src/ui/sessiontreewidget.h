#pragma once

#include "core/session.h"

#include <QTreeWidget>

class QMimeData;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;

class SessionTreeWidget : public QTreeWidget
{
    Q_OBJECT
public:
    explicit SessionTreeWidget(QWidget *parent = nullptr);
    void reload();
    void setFilter(const QString &keyword);

signals:
    void openSessionRequested(const Session &session);
    void editSessionRequested(const Session &session);
    void newSessionRequested(qint64 folderId);
    void newFolderRequested(qint64 parentId);
    void deleteSessionRequested(qint64 id);
    void deleteFolderRequested(qint64 id);
    void renameFolderRequested(qint64 id);
    void favoriteToggled(qint64 id, bool favorite);
    void sessionsMutated();

protected:
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QList<QTreeWidgetItem *> items) const override;
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void contextMenu(const QPoint &pos);
    void onDoubleClicked(QTreeWidgetItem *item, int column);
    void openSelectedSessions();
    void copySelectedSessions();
    void pasteSessionsTo(qint64 folderId);
    void moveSessionsTo(const QList<qint64> &ids, qint64 folderId);
    QList<qint64> selectedSessionIds() const;
    qint64 folderIdOf(QTreeWidgetItem *item) const;
    QTreeWidgetItem *folderItem(qint64 id) const;

    QString m_filter;
    QList<qint64> m_copiedIds;
};
