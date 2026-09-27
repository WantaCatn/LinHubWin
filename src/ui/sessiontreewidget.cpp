#include "ui/sessiontreewidget.h"
#include "storage/sessionrepository.h"
#include "ui/icons.h"
#include "ui/sessionpane.h"

#include <QAbstractItemView>
#include <QAction>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHash>
#include <QKeyEvent>
#include <QKeySequence>
#include <QHeaderView>
#include <QMenu>
#include <QMimeData>
#include <QPoint>
#include <QSet>
#include <QSize>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTreeWidgetItem>
#include <QVariant>
#include <functional>

namespace {
constexpr int kRoleId = Qt::UserRole;
constexpr int kRoleKind = Qt::UserRole + 1;
constexpr int kKindFolder = 1;
constexpr int kKindSession = 2;

class NoFocusDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        option->state &= ~QStyle::State_HasFocus;
        if (option->state & QStyle::State_Selected) {
            const QColor hi = option->palette.color(QPalette::HighlightedText);
            option->palette.setColor(QPalette::Text, hi);
            option->palette.setBrush(QPalette::Text, hi);
        }
    }
};
}

SessionTreeWidget::SessionTreeWidget(QWidget *parent)
    : QTreeWidget(parent)
{
    setHeaderHidden(true);
    setAnimated(true);
    setUniformRowHeights(true);
    setIconSize(QSize(18, 18));
    setIndentation(16);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setAllColumnsShowFocus(true);
    setItemDelegate(new NoFocusDelegate(this));
    setContextMenuPolicy(Qt::CustomContextMenu);
    setDragEnabled(true);
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::MoveAction);
    setDropIndicatorShown(true);
    connect(this, &QTreeWidget::customContextMenuRequested, this, &SessionTreeWidget::contextMenu);
    connect(this, &QTreeWidget::itemDoubleClicked, this, &SessionTreeWidget::onDoubleClicked);
}

QStringList SessionTreeWidget::mimeTypes() const
{
    return { QString::fromLatin1(linhubSessionMime()) };
}

QMimeData *SessionTreeWidget::mimeData(const QList<QTreeWidgetItem *> items) const
{
    auto *mime = new QMimeData;
    QByteArray ids;
    QString title;
    for (auto *it : items) {
        if (!it || it->data(0, kRoleKind).toInt() != kKindSession)
            continue;
        const qint64 id = it->data(0, kRoleId).toLongLong();
        if (!id)
            continue;
        if (!ids.isEmpty())
            ids += '\n';
        ids += QByteArray::number(id);
        if (title.isEmpty())
            title = it->text(0);
    }
    mime->setData(QString::fromLatin1(linhubSessionMime()), ids);
    mime->setText(title);
    return mime;
}

void SessionTreeWidget::setFilter(const QString &keyword)
{
    m_filter = keyword.trimmed();
    reload();
}

QTreeWidgetItem *SessionTreeWidget::folderItem(qint64 id) const
{
    const auto found = findItems(QString(), Qt::MatchContains | Qt::MatchRecursive);
    for (auto *it : found) {
        if (it->data(0, kRoleKind).toInt() == kKindFolder && it->data(0, kRoleId).toLongLong() == id)
            return it;
    }
    return nullptr;
}

void SessionTreeWidget::reload()
{
    SessionRepository repo;
    const auto folders = repo.allFolders();
    const auto sessions = m_filter.isEmpty() ? repo.allSessions() : repo.search(m_filter);

    clear();
    QHash<qint64, QTreeWidgetItem *> folderMap;

    auto *fav = new QTreeWidgetItem(this);
    fav->setIcon(0, AppIcons::get(QStringLiteral("star")));
    fav->setText(0, QStringLiteral("收藏"));
    fav->setData(0, kRoleKind, kKindFolder);
    fav->setData(0, kRoleId, qint64(-1));

    auto *rootUngrouped = new QTreeWidgetItem(this);
    rootUngrouped->setIcon(0, AppIcons::get(QStringLiteral("folder")));
    rootUngrouped->setText(0, QStringLiteral("未分组"));
    rootUngrouped->setData(0, kRoleKind, kKindFolder);
    rootUngrouped->setData(0, kRoleId, qint64(0));

    std::function<QTreeWidgetItem *(const Folder &)> ensureFolder;
    ensureFolder = [&](const Folder &f) -> QTreeWidgetItem * {
        if (folderMap.contains(f.id))
            return folderMap.value(f.id);
        QTreeWidgetItem *parent = this->invisibleRootItem();
        if (f.parentId != 0) {
            for (const auto &p : folders) {
                if (p.id == f.parentId) {
                    parent = ensureFolder(p);
                    break;
                }
            }
        }
        auto *item = new QTreeWidgetItem(parent);
        item->setIcon(0, AppIcons::get(QStringLiteral("folder")));
        item->setText(0, f.name);
        item->setData(0, kRoleKind, kKindFolder);
        item->setData(0, kRoleId, f.id);
        folderMap.insert(f.id, item);
        return item;
    };

    for (const auto &f : folders)
        ensureFolder(f);

    for (const auto &s : sessions) {
        QTreeWidgetItem *parent = folderMap.value(s.folderId);
        if (!parent)
            parent = rootUngrouped;
        auto addUnder = [&](QTreeWidgetItem *p) {
            auto *item = new QTreeWidgetItem(p);
            item->setIcon(0, AppIcons::protocol(s.protocol));
            item->setText(0, s.displayTitle());
            item->setToolTip(0, QStringLiteral("%1  %2@%3:%4")
                                    .arg(protocolDisplayName(s.protocol),
                                         s.username, s.host, QString::number(s.port)));
            item->setData(0, kRoleKind, kKindSession);
            item->setData(0, kRoleId, s.id);
        };
        addUnder(parent);
        if (s.favorite)
            addUnder(fav);
    }
    expandAll();
}

QList<qint64> SessionTreeWidget::selectedSessionIds() const
{
    QList<qint64> ids;
    for (QTreeWidgetItem *it : selectedItems()) {
        if (!it || it->data(0, kRoleKind).toInt() != kKindSession)
            continue;
        const qint64 id = it->data(0, kRoleId).toLongLong();
        if (id && !ids.contains(id))
            ids.append(id);
    }
    return ids;
}

qint64 SessionTreeWidget::folderIdOf(QTreeWidgetItem *item) const
{
    if (!item)
        return 0;
    if (item->data(0, kRoleKind).toInt() == kKindFolder) {
        const qint64 id = item->data(0, kRoleId).toLongLong();
        return id < 0 ? 0 : id;
    }
    SessionRepository repo;
    return repo.byId(item->data(0, kRoleId).toLongLong()).folderId;
}

static QString uniqueSessionName(const QString &base, qint64 folderId)
{
    QSet<QString> used;
    for (const Session &s : SessionRepository().allSessions()) {
        if (s.folderId == folderId)
            used.insert(s.displayTitle());
    }
    if (!used.contains(base))
        return base;
    const QString copy = base + QStringLiteral(" 副本");
    if (!used.contains(copy))
        return copy;
    for (int i = 2; i < 1000; ++i) {
        const QString cand = copy + QString::number(i);
        if (!used.contains(cand))
            return cand;
    }
    return copy;
}

void SessionTreeWidget::openSelectedSessions()
{
    SessionRepository repo;
    for (qint64 id : selectedSessionIds()) {
        const Session s = repo.byId(id);
        if (s.id)
            emit openSessionRequested(s);
    }
}

void SessionTreeWidget::copySelectedSessions()
{
    m_copiedIds = selectedSessionIds();
}

void SessionTreeWidget::pasteSessionsTo(qint64 folderId)
{
    if (folderId < 0)
        folderId = 0;
    SessionRepository repo;
    bool changed = false;
    for (qint64 id : m_copiedIds) {
        Session s = repo.byId(id);
        if (!s.id)
            continue;
        const QString title = s.displayTitle();
        s.id = 0;
        s.folderId = folderId;
        s.favorite = false;
        s.name = uniqueSessionName(title, folderId);
        if (repo.insert(s))
            changed = true;
    }
    if (!changed)
        return;
    reload();
    emit sessionsMutated();
}

void SessionTreeWidget::moveSessionsTo(const QList<qint64> &ids, qint64 folderId)
{
    if (folderId < 0)
        folderId = 0;
    SessionRepository repo;
    bool changed = false;
    for (qint64 id : ids) {
        Session s = repo.byId(id);
        if (!s.id || s.folderId == folderId)
            continue;
        s.folderId = folderId;
        if (repo.update(s))
            changed = true;
    }
    if (!changed)
        return;
    reload();
    emit sessionsMutated();
}

void SessionTreeWidget::onDoubleClicked(QTreeWidgetItem *item, int)
{
    if (!item || item->data(0, kRoleKind).toInt() != kKindSession)
        return;
    item->setSelected(true);
    openSelectedSessions();
}

void SessionTreeWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Copy)) {
        copySelectedSessions();
        return;
    }
    if (event->matches(QKeySequence::Paste)) {
        pasteSessionsTo(folderIdOf(currentItem()));
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        openSelectedSessions();
        return;
    }
    QTreeWidget::keyPressEvent(event);
}

void SessionTreeWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData() && event->mimeData()->hasFormat(QString::fromLatin1(linhubSessionMime()))) {
        event->acceptProposedAction();
        return;
    }
    QTreeWidget::dragEnterEvent(event);
}

void SessionTreeWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData() && event->mimeData()->hasFormat(QString::fromLatin1(linhubSessionMime()))) {
        event->acceptProposedAction();
        return;
    }
    QTreeWidget::dragMoveEvent(event);
}

void SessionTreeWidget::dropEvent(QDropEvent *event)
{
    if (!event->mimeData() || !event->mimeData()->hasFormat(QString::fromLatin1(linhubSessionMime()))) {
        event->ignore();
        return;
    }
    QList<qint64> ids;
    const QString raw = QString::fromUtf8(event->mimeData()->data(QString::fromLatin1(linhubSessionMime())));
    for (const QString &part : raw.split(QLatin1Char('\n'), QString::SkipEmptyParts)) {
        const qint64 id = part.trimmed().toLongLong();
        if (id && !ids.contains(id))
            ids.append(id);
    }
    QTreeWidgetItem *over = itemAt(event->pos());
    if (over && over->data(0, kRoleKind).toInt() == kKindFolder
        && over->data(0, kRoleId).toLongLong() == -1) {
        for (qint64 id : ids)
            emit favoriteToggled(id, true);
        event->acceptProposedAction();
        return;
    }
    moveSessionsTo(ids, folderIdOf(over));
    event->acceptProposedAction();
}

void SessionTreeWidget::contextMenu(const QPoint &pos)
{
    auto *item = itemAt(pos);
    QMenu menu(this);
    qint64 folderId = 0;
    Session session;
    bool isSession = false;
    if (item) {
        if (item->data(0, kRoleKind).toInt() == kKindSession) {
            isSession = true;
            SessionRepository repo;
            session = repo.byId(item->data(0, kRoleId).toLongLong());
            folderId = session.folderId;
        } else {
            folderId = item->data(0, kRoleId).toLongLong();
            if (folderId < 0)
                folderId = 0;
        }
    }

    QAction *openAct = menu.addAction(AppIcons::get(QStringLiteral("connect")),
                                      selectedItems().size() > 1 ? QStringLiteral("打开所选会话")
                                                                 : QStringLiteral("打开会话"));
    connect(openAct, &QAction::triggered, this, [this, session, isSession] {
        if (selectedItems().size() > 1) {
            openSelectedSessions();
            return;
        }
        if (isSession && session.id)
            emit openSessionRequested(session);
    });
    QAction *newSess = menu.addAction(AppIcons::get(QStringLiteral("new")), QStringLiteral("新建会话"));
    connect(newSess, &QAction::triggered, this, [this, folderId] {
        emit newSessionRequested(folderId);
    });
    QAction *newFold = menu.addAction(AppIcons::get(QStringLiteral("folder")), QStringLiteral("新建分组"));
    connect(newFold, &QAction::triggered, this, [this, folderId] {
        emit newFolderRequested(folderId);
    });
    if (!m_copiedIds.isEmpty()) {
        QAction *pasteAct = menu.addAction(QStringLiteral("粘贴会话"));
        connect(pasteAct, &QAction::triggered, this, [this, folderId] {
            pasteSessionsTo(folderId);
        });
    }
    if (isSession) {
        QAction *favAct = menu.addAction(AppIcons::get(QStringLiteral("star")),
                                         session.favorite ? QStringLiteral("取消收藏")
                                                          : QStringLiteral("加入收藏"));
        connect(favAct, &QAction::triggered, this, [this, session] {
            emit favoriteToggled(session.id, !session.favorite);
        });
        QAction *editAct = menu.addAction(AppIcons::get(QStringLiteral("settings")), QStringLiteral("编辑"));
        connect(editAct, &QAction::triggered, this, [this, session] {
            emit editSessionRequested(session);
        });
        QAction *copyAct = menu.addAction(AppIcons::get(QStringLiteral("duplicate")), QStringLiteral("复制"));
        connect(copyAct, &QAction::triggered, this, [this] { copySelectedSessions(); });
        QMenu *moveMenu = menu.addMenu(QStringLiteral("移动到分组"));
        QAction *toUngrouped = moveMenu->addAction(QStringLiteral("未分组"));
        connect(toUngrouped, &QAction::triggered, this, [this] {
            moveSessionsTo(selectedSessionIds(), 0);
        });
        for (const Folder &f : SessionRepository().allFolders()) {
            QAction *toFold = moveMenu->addAction(f.name);
            connect(toFold, &QAction::triggered, this, [this, f] {
                moveSessionsTo(selectedSessionIds(), f.id);
            });
        }
        menu.addSeparator();
        QAction *delSess = menu.addAction(AppIcons::get(QStringLiteral("close")), QStringLiteral("删除会话"));
        connect(delSess, &QAction::triggered, this, [this, session] {
            emit deleteSessionRequested(session.id);
        });
    } else if (item && folderId > 0) {
        QAction *renFold = menu.addAction(QStringLiteral("重命名分组"));
        connect(renFold, &QAction::triggered, this, [this, folderId] {
            emit renameFolderRequested(folderId);
        });
        menu.addSeparator();
        QAction *delFold = menu.addAction(AppIcons::get(QStringLiteral("close")), QStringLiteral("删除分组"));
        connect(delFold, &QAction::triggered, this, [this, folderId] {
            emit deleteFolderRequested(folderId);
        });
    }
    menu.exec(mapToGlobal(pos));
}
