#include "ui/sftpwidget.h"
#include "app/appsettings.h"
#include "storage/transferrepository.h"
#include "ui/authpromptdialog.h"
#include "ui/icons.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QColor>
#include <QPalette>
#include <QDate>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
#include <QPoint>
#include <QContextMenuEvent>
#include <QTabWidget>
#include <QAbstractItemView>
#include <QComboBox>
#include <QFileIconProvider>
#include <QFileSystemModel>
#include <QFileSystemWatcher>
#include <QListView>
#include <QTreeView>
#include <QMap>
#include <QShowEvent>
#include <algorithm>
#include <functional>

#ifdef LINHUB_HAVE_X11
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

static QString formatSize(qint64 n)
{
    if (n < 0)
        return QStringLiteral("-");
    if (n < 1024)
        return QString::number(n) + QStringLiteral(" B");
    if (n < 1024 * 1024)
        return QString::number(n / 1024.0, 'f', 1) + QStringLiteral(" KB");
    if (n < 1024LL * 1024 * 1024)
        return QString::number(n / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
    return QString::number(n / (1024.0 * 1024.0 * 1024.0), 'f', 2) + QStringLiteral(" GB");
}

QString joinRemote(const QString &dir, const QString &name)
{
    if (name.isEmpty())
        return dir;
    if (dir.isEmpty() || dir == QLatin1String("."))
        return name;
    if (dir == QLatin1String("/"))
        return QLatin1Char('/') + name;
    return dir + QLatin1Char('/') + name;
}

int modeFromLs(const QString &mode)
{
    int m = 0;
    auto bit = [&](int i, int val) {
        if (i < mode.size()) {
            const QChar c = mode.at(i);
            if (c == QLatin1Char('r') || c == QLatin1Char('w') || c == QLatin1Char('x')
                || c == QLatin1Char('s') || c == QLatin1Char('t') || c == QLatin1Char('S')
                || c == QLatin1Char('T'))
                m |= val;
        }
    };
    bit(1, 0400);
    bit(2, 0200);
    bit(3, 0100);
    bit(4, 0040);
    bit(5, 0020);
    bit(6, 0010);
    bit(7, 0004);
    bit(8, 0002);
    bit(9, 0001);
    return m;
}

QString jobStateText(const SftpJob &j)
{
    switch (j.state) {
    case SftpJob::Queued: return QStringLiteral("排队中");
    case SftpJob::Running: return QStringLiteral("传输中");
    case SftpJob::Paused: return QStringLiteral("已暂停");
    case SftpJob::Done: return QStringLiteral("完成");
    case SftpJob::Failed: return j.message.isEmpty() ? QStringLiteral("失败") : j.message;
    }
    return {};
}

QString desktopDir()
{
    const QString home = QDir::homePath();
    const QStringList cands = {
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
        home + QStringLiteral("/Desktop"),
        home + QStringLiteral("/桌面")
    };
    for (const QString &c : cands) {
        if (!c.isEmpty() && QDir(c).exists())
            return c;
    }
    return QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
}

void openLocalPath(const QString &path, bool asDir)
{
    if (path.isEmpty())
        return;
    QFileInfo fi(path);
    QString target;
    if (asDir || fi.isDir())
        target = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();
    else
        target = fi.absoluteFilePath();
    if (target.isEmpty() || !QFileInfo::exists(target))
        return;
    if (QDesktopServices::openUrl(QUrl::fromLocalFile(target)))
        return;
    if (QProcess::startDetached(QStringLiteral("xdg-open"), QStringList() << target))
        return;
    QProcess::startDetached(QStringLiteral("peony"), QStringList() << target);
}

void openContainingFolder(const QString &path)
{
    QFileInfo fi(path);
    const QString dir = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();
    if (dir.isEmpty())
        return;
    if (QDesktopServices::openUrl(QUrl::fromLocalFile(dir)))
        return;
    if (QProcess::startDetached(QStringLiteral("xdg-open"), QStringList() << dir))
        return;
    QProcess::startDetached(QStringLiteral("peony"), QStringList() << dir);
}

enum {
    KindRole = Qt::UserRole,
    IdRole = Qt::UserRole + 1,
    PathRole = Qt::UserRole + 2,
    DirRole = Qt::UserRole + 3,
    JobIdRole = Qt::UserRole + 6
};
enum ItemKind { KindLive = 1, KindRecord = 2, KindFs = 3, KindDay = 4 };

class NoFocusDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        option->state &= ~QStyle::State_HasFocus;
        if (index.column() == 1) {
            option->state &= ~QStyle::State_Selected;
            option->backgroundBrush = Qt::NoBrush;
            return;
        }
        if (option->state & QStyle::State_Selected) {
            const QColor hi = option->palette.color(QPalette::HighlightedText);
            option->palette.setColor(QPalette::Text, hi);
            option->palette.setBrush(QPalette::Text, hi);
        }
    }
};

class RecordTree : public QTreeWidget
{
public:
    std::function<void(const QPoint &)> onMenu;

    explicit RecordTree(QWidget *parent = nullptr)
        : QTreeWidget(parent)
    {
        setContextMenuPolicy(Qt::DefaultContextMenu);
        viewport()->setContextMenuPolicy(Qt::DefaultContextMenu);
        setSelectionMode(QAbstractItemView::ExtendedSelection);
        setUniformRowHeights(true);
        setExpandsOnDoubleClick(false);
        setAllColumnsShowFocus(false);
        setSelectionBehavior(QAbstractItemView::SelectRows);
        setFocusPolicy(Qt::ClickFocus);
        header()->setStretchLastSection(true);
        setItemDelegate(new NoFocusDelegate(this));
    }

protected:
    void contextMenuEvent(QContextMenuEvent *event) override
    {
        const QPoint vp = viewport()->mapFromGlobal(event->globalPos());
        QTreeWidgetItem *it = itemAt(vp);
        if (it && !it->isSelected()) {
            clearSelection();
            it->setSelected(true);
            setCurrentItem(it);
        }
        if (onMenu)
            onMenu(event->globalPos());
        event->accept();
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::RightButton) {
            const QPoint vp = viewport()->mapFrom(this, event->pos());
            QTreeWidgetItem *it = itemAt(vp);
            if (it && !it->isSelected()) {
                if (!(event->modifiers() & Qt::ControlModifier))
                    clearSelection();
                it->setSelected(true);
                setCurrentItem(it);
            }
        }
        QTreeWidget::mousePressEvent(event);
        if (event->button() == Qt::LeftButton && !itemAt(event->pos())) {
            clearSelection();
            setCurrentItem(nullptr);
        }
    }
};

class SftpTransferDialog : public QDialog
{
public:
    std::function<void(int, const QString &)> onLiveAction;

    explicit SftpTransferDialog(QWidget *parent)
        : QDialog(parent)
    {
        setWindowTitle(QStringLiteral("传输详情"));
        resize(760, 560);
        auto *lay = new QVBoxLayout(this);
        auto *hint = new QLabel(QStringLiteral("下载与上传分开保存，记录最多 7 天。本次任务可暂停、继续、重试失败项或删除。文件夹重试只重试失败的文件。"));
        hint->setWordWrap(true);
        hint->setObjectName(QStringLiteral("hintLabel"));
        lay->addWidget(hint);

        auto *tabs = new QTabWidget;
        tabs->addTab(makePane(false), QStringLiteral("下载"));
        tabs->addTab(makePane(true), QStringLiteral("上传"));
        lay->addWidget(tabs, 1);

        auto *box = new QDialogButtonBox(QDialogButtonBox::Close);
        box->button(QDialogButtonBox::Close)->setText(QStringLiteral("关闭"));
        connect(box, &QDialogButtonBox::rejected, this, &QDialog::hide);
        connect(box, &QDialogButtonBox::accepted, this, &QDialog::hide);
        lay->addWidget(box);
    }

    void refresh(const QVector<SftpJob> &jobs)
    {
        fillLive(m_dlLive, jobs, false);
        fillLive(m_ulLive, jobs, true);
        if (m_needHistory)
            reloadHistory();
    }

    void reloadHistory()
    {
        m_needHistory = false;
        TransferRepository repo;
        repo.purgeOlderThanDays(7);
        fillHistory(m_dlHist, repo.recent(), false);
        fillHistory(m_ulHist, repo.recent(), true);
    }

    void markHistoryDirty() { m_needHistory = true; }

protected:
    void showEvent(QShowEvent *event) override
    {
        QDialog::showEvent(event);
        reloadHistory();
    }

private:
    QWidget *makePane(bool upload)
    {
        auto *page = new QWidget;
        auto *lay = new QVBoxLayout(page);
        lay->setContentsMargins(0, 8, 0, 0);
        lay->addWidget(new QLabel(upload ? QStringLiteral("本次上传") : QStringLiteral("本次下载")));
        RecordTree *live = new RecordTree;
        live->setHeaderLabels({QStringLiteral("文件"), QStringLiteral("进度"), QStringLiteral("状态"), QStringLiteral("位置")});
        live->setRootIsDecorated(false);
        live->setMaximumHeight(150);
        live->header()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        live->header()->setStretchLastSection(true);
        live->header()->setStyleSheet(QStringLiteral(
            "QHeaderView::section { padding-left: 6px; padding-right: 4px; }"));
        live->header()->setSectionResizeMode(0, QHeaderView::Interactive);
        live->header()->setSectionResizeMode(1, QHeaderView::Fixed);
        live->header()->setSectionResizeMode(2, QHeaderView::Fixed);
        live->header()->setSectionResizeMode(3, QHeaderView::Stretch);
        live->setColumnWidth(0, 170);
        live->setColumnWidth(1, 176);
        live->setColumnWidth(2, 58);
        live->onMenu = [this, live](const QPoint &gp) { showLiveMenu(live, gp); };
        connect(live, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *it, int) {
            if (!it)
                return;
            openLocalPath(itemPath(it), it->data(0, DirRole).toBool());
        });
        lay->addWidget(live);

        lay->addWidget(new QLabel(upload ? QStringLiteral("上传记录") : QStringLiteral("下载记录")));
        RecordTree *hist = new RecordTree;
        hist->setHeaderLabels({QStringLiteral("名称"), QStringLiteral("位置"), QStringLiteral("大小"), QStringLiteral("状态")});
        hist->setColumnWidth(0, 180);
        hist->setColumnWidth(1, 280);
        hist->setColumnWidth(2, 80);
        hist->onMenu = [this, hist](const QPoint &gp) { showMenu(hist, gp, true); };
        connect(hist, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem *it) { expandFolder(it); });
        connect(hist, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *it, int) {
            if (!it || it->data(0, KindRole).toInt() == KindDay)
                return;
            openLocalPath(itemPath(it), it->data(0, DirRole).toBool());
        });
        lay->addWidget(hist, 1);

        auto *btns = new QHBoxLayout;
        auto *openFile = new QPushButton(QStringLiteral("打开文件"));
        auto *openDir = new QPushButton(QStringLiteral("打开所在文件夹"));
        auto *delSel = new QPushButton(QStringLiteral("删除所选记录"));
        openFile->setProperty("secondary", true);
        openDir->setProperty("secondary", true);
        delSel->setProperty("secondary", true);
        connect(openFile, &QPushButton::clicked, this, [this, live, hist] {
            QTreeWidgetItem *it = currentRecord(live, hist);
            openLocalPath(itemPath(it), it && it->data(0, DirRole).toBool());
        });
        connect(openDir, &QPushButton::clicked, this, [this, live, hist] {
            openContainingFolder(itemPath(currentRecord(live, hist)));
        });
        connect(delSel, &QPushButton::clicked, this, [this, hist] { deleteSelected(hist); });
        btns->addWidget(openFile);
        btns->addWidget(openDir);
        btns->addWidget(delSel);
        btns->addStretch();
        lay->addLayout(btns);

        if (upload) {
            m_ulLive = live;
            m_ulHist = hist;
        } else {
            m_dlLive = live;
            m_dlHist = hist;
        }
        return page;
    }

    static QString itemPath(QTreeWidgetItem *it)
    {
        return it ? it->data(0, PathRole).toString() : QString();
    }

    static QTreeWidgetItem *currentRecord(RecordTree *live, RecordTree *hist)
    {
        if (hist) {
            QList<QTreeWidgetItem *> sel = hist->selectedItems();
            if (!sel.isEmpty())
                return sel.first();
            if (hist->currentItem())
                return hist->currentItem();
        }
        if (live) {
            QList<QTreeWidgetItem *> sel = live->selectedItems();
            if (!sel.isEmpty())
                return sel.first();
            if (live->currentItem())
                return live->currentItem();
        }
        return nullptr;
    }

    void fillLive(RecordTree *tree, const QVector<SftpJob> &jobs, bool upload)
    {
        if (!tree)
            return;
        tree->clear();
        for (int i = jobs.size() - 1; i >= 0; --i) {
            const SftpJob &j = jobs.at(i);
            if (j.upload != upload)
                continue;
            auto *it = new QTreeWidgetItem(tree);
            it->setText(0, j.name);
            it->setText(2, jobStateText(j));
            it->setTextAlignment(2, Qt::AlignLeft | Qt::AlignVCenter);
            it->setText(3, j.local);
            it->setToolTip(3, j.local);
            it->setData(0, KindRole, KindLive);
            it->setData(0, PathRole, j.local);
            it->setData(0, DirRole, j.isDir);
            it->setData(0, JobIdRole, j.id);
            auto *bar = new QProgressBar;
            bar->setObjectName(QStringLiteral("sftpProgress"));
            bar->setFixedHeight(10);
            bar->setRange(0, 1000);
            int pct = 0;
            if (j.size > 0)
                pct = static_cast<int>(qMin(j.done, j.size) * 1000 / j.size);
            else if (j.state == SftpJob::Done)
                pct = 1000;
            else if (j.state == SftpJob::Running)
                pct = 50;
            bar->setValue(pct);
            bar->setFormat(j.size > 0
                               ? formatSize(j.done) + QLatin1Char('/') + formatSize(j.size)
                               : (j.state == SftpJob::Running ? QStringLiteral("...") : QString()));
            bar->setTextVisible(false);
            bar->setAttribute(Qt::WA_TranslucentBackground, true);
            auto *wrap = new QWidget;
            wrap->setAutoFillBackground(false);
            wrap->setAttribute(Qt::WA_TranslucentBackground, true);
            wrap->setStyleSheet(QStringLiteral("background: transparent;"));
            auto *hl = new QHBoxLayout(wrap);
            hl->setContentsMargins(4, 5, 4, 5);
            hl->setSpacing(0);
            hl->addWidget(bar);
            tree->setItemWidget(it, 1, wrap);
            if (j.state == SftpJob::Failed)
                it->setForeground(2, QColor(QStringLiteral("#dc2626")));
            else if (j.state == SftpJob::Done)
                it->setForeground(2, QColor(QStringLiteral("#16a34a")));
            else if (j.state == SftpJob::Paused)
                it->setForeground(2, QColor(QStringLiteral("#d97706")));
            else if (j.state == SftpJob::Running)
                it->setForeground(2, QColor(QStringLiteral("#2563eb")));
        }
    }

    void fillHistory(RecordTree *tree, const QVector<TransferRecord> &recs, bool upload)
    {
        if (!tree)
            return;
        tree->clear();
        QMap<QDate, QVector<TransferRecord>> groups;
        for (const TransferRecord &r : recs) {
            if (r.upload != upload)
                continue;
            groups[r.createdAt.isValid() ? r.createdAt.date() : QDate::currentDate()].append(r);
        }
        QList<QDate> days = groups.keys();
        std::sort(days.begin(), days.end(), [](const QDate &a, const QDate &b) { return a > b; });
        const QDate today = QDate::currentDate();
        for (const QDate &day : days) {
            auto *group = new QTreeWidgetItem(tree);
            QString label = day.toString(QStringLiteral("yyyy-MM-dd"));
            if (day == today)
                label = QStringLiteral("今天 (%1)").arg(label);
            else if (day == today.addDays(-1))
                label = QStringLiteral("昨天 (%1)").arg(label);
            group->setText(0, label);
            group->setFirstColumnSpanned(true);
            group->setData(0, KindRole, KindDay);
            group->setExpanded(day == today);
            for (const TransferRecord &r : groups.value(day)) {
                auto *it = new QTreeWidgetItem(group);
                it->setText(0, r.name);
                it->setText(1, r.localPath.isEmpty() ? r.remotePath : r.localPath);
                it->setText(2, r.isDir ? QStringLiteral("文件夹") : formatSize(r.size));
                it->setText(3, r.status);
                it->setData(0, KindRole, KindRecord);
                it->setData(0, IdRole, r.id);
                it->setData(0, PathRole, r.localPath);
                it->setData(0, DirRole, r.isDir);
                it->setIcon(0, r.isDir ? m_icons.icon(QFileIconProvider::Folder)
                                       : m_icons.icon(QFileInfo(r.localPath)));
                if (r.status.contains(QStringLiteral("失败")))
                    it->setForeground(3, QColor(QStringLiteral("#f85149")));
                else
                    it->setForeground(3, QColor(QStringLiteral("#3fb950")));
                if (r.isDir || QFileInfo(r.localPath).isDir()) {
                    it->setData(0, DirRole, true);
                    auto *dummy = new QTreeWidgetItem(it);
                    dummy->setText(0, QStringLiteral("..."));
                }
            }
        }
    }

    void expandFolder(QTreeWidgetItem *parent)
    {
        if (!parent)
            return;
        const int kind = parent->data(0, KindRole).toInt();
        if (kind != KindRecord && kind != KindFs)
            return;
        if (!parent->data(0, DirRole).toBool())
            return;
        const QString path = itemPath(parent);
        if (path.isEmpty() || !QFileInfo(path).isDir())
            return;
        if (parent->childCount() == 1 && parent->child(0)->text(0) == QLatin1String("..."))
            parent->takeChildren();
        else if (parent->childCount() > 0)
            return;
        QDir dir(path);
        const QFileInfoList infos = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                                                      QDir::DirsFirst | QDir::Name);
        for (const QFileInfo &fi : infos) {
            auto *ch = new QTreeWidgetItem(parent);
            ch->setText(0, fi.fileName());
            ch->setText(1, fi.absoluteFilePath());
            ch->setText(2, fi.isDir() ? QStringLiteral("文件夹") : formatSize(fi.size()));
            ch->setData(0, KindRole, KindFs);
            ch->setData(0, PathRole, fi.absoluteFilePath());
            ch->setData(0, DirRole, fi.isDir());
            ch->setIcon(0, m_icons.icon(fi));
            if (fi.isDir()) {
                auto *dummy = new QTreeWidgetItem(ch);
                dummy->setText(0, QStringLiteral("..."));
            }
        }
    }

    QVector<qint64> selectedRecordIds(RecordTree *hist) const
    {
        QVector<qint64> ids;
        if (!hist)
            return ids;
        auto add = [&ids](QTreeWidgetItem *it) {
            if (!it)
                return;
            if (it->data(0, KindRole).toInt() == KindRecord) {
                const qint64 id = it->data(0, IdRole).toLongLong();
                if (id && !ids.contains(id))
                    ids.append(id);
            } else if (it->data(0, KindRole).toInt() == KindDay) {
                for (int i = 0; i < it->childCount(); ++i) {
                    QTreeWidgetItem *ch = it->child(i);
                    if (ch && ch->data(0, KindRole).toInt() == KindRecord) {
                        const qint64 id = ch->data(0, IdRole).toLongLong();
                        if (id && !ids.contains(id))
                            ids.append(id);
                    }
                }
            }
        };
        const QList<QTreeWidgetItem *> sel = hist->selectedItems();
        for (QTreeWidgetItem *it : sel)
            add(it);
        return ids;
    }

    void deleteSelected(RecordTree *hist)
    {
        const QVector<qint64> ids = selectedRecordIds(hist);
        if (ids.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("删除记录"), QStringLiteral("请先选中要删除的记录"));
            return;
        }
        if (QMessageBox::question(this, QStringLiteral("删除记录"),
                                  QStringLiteral("删除选中的 %1 条记录？不会删除本地文件。").arg(ids.size()))
            != QMessageBox::Yes)
            return;
        TransferRepository().removeMany(ids);
        reloadHistory();
    }

    void showLiveMenu(RecordTree *tree, const QPoint &globalPos)
    {
        if (!tree)
            return;
        QTreeWidgetItem *it = tree->itemAt(tree->viewport()->mapFromGlobal(globalPos));
        if (!it)
            it = tree->currentItem();
        if (!it)
            return;
        const int jobId = it->data(0, JobIdRole).toInt();
        if (!jobId || !onLiveAction)
            return;
        const QString st = it->text(2);
        QMenu menu(this);
        const QString path = itemPath(it);
        const bool isDir = it->data(0, DirRole).toBool();
        QAction *actOpen = menu.addAction(isDir ? QStringLiteral("打开文件夹")
                                                : QStringLiteral("打开文件"));
        QAction *actFolder = menu.addAction(QStringLiteral("打开所在文件夹"));
        menu.addSeparator();
        QAction *actPause = nullptr;
        QAction *actResume = nullptr;
        QAction *actRetry = nullptr;
        if (st.contains(QStringLiteral("传输中")) || st.contains(QStringLiteral("排队中")))
            actPause = menu.addAction(QStringLiteral("暂停"));
        if (st.contains(QStringLiteral("已暂停")))
            actResume = menu.addAction(QStringLiteral("继续"));
        if (st.contains(QStringLiteral("失败")))
            actRetry = menu.addAction(QStringLiteral("重试"));
        QAction *actDel = menu.addAction(QStringLiteral("删除任务"));
        QAction *a = menu.exec(globalPos);
        if (!a)
            return;
        if (a == actOpen)
            openLocalPath(path, isDir);
        else if (a == actFolder)
            openContainingFolder(path);
        else if (a == actPause)
            onLiveAction(jobId, QStringLiteral("pause"));
        else if (a == actResume)
            onLiveAction(jobId, QStringLiteral("resume"));
        else if (a == actRetry)
            onLiveAction(jobId, QStringLiteral("retry"));
        else if (a == actDel)
            onLiveAction(jobId, QStringLiteral("delete"));
    }

    void showMenu(RecordTree *tree, const QPoint &globalPos, bool history)
    {
        if (!tree)
            return;
        QTreeWidgetItem *it = tree->itemAt(tree->viewport()->mapFromGlobal(globalPos));
        if (!it)
            it = tree->currentItem();
        QMenu menu(this);
        QAction *actOpen = nullptr;
        QAction *actFolder = nullptr;
        QAction *actDel = nullptr;
        QAction *actDelBatch = nullptr;
        const QString path = itemPath(it);
        const int kind = it ? it->data(0, KindRole).toInt() : 0;
        if (it && kind != KindDay) {
            actOpen = menu.addAction(it->data(0, DirRole).toBool()
                                         ? QStringLiteral("打开文件夹")
                                         : QStringLiteral("打开文件"));
            actFolder = menu.addAction(QStringLiteral("打开所在文件夹"));
        }
        if (history) {
            if (!menu.isEmpty())
                menu.addSeparator();
            actDel = menu.addAction(QStringLiteral("删除记录"));
            actDelBatch = menu.addAction(QStringLiteral("删除所选记录"));
        }
        if (menu.isEmpty())
            return;
        QAction *a = menu.exec(globalPos);
        if (!a)
            return;
        if (a == actOpen)
            openLocalPath(path, it && it->data(0, DirRole).toBool());
        else if (a == actFolder)
            openContainingFolder(path);
        else if (a == actDel) {
            if (kind == KindRecord) {
                TransferRepository().remove(it->data(0, IdRole).toLongLong());
                reloadHistory();
            } else {
                deleteSelected(tree);
            }
        } else if (a == actDelBatch) {
            deleteSelected(tree);
        }
    }

    RecordTree *m_dlLive = nullptr;
    RecordTree *m_dlHist = nullptr;
    RecordTree *m_ulLive = nullptr;
    RecordTree *m_ulHist = nullptr;
    QFileIconProvider m_icons;
    bool m_needHistory = true;
};


namespace {

#ifdef LINHUB_HAVE_X11
Display *x11Display()
{
    static Display *dpy = XOpenDisplay(nullptr);
    return dpy;
}

QByteArray xdsRead(WId wid)
{
    Display *dpy = x11Display();
    if (!dpy || !wid)
        return {};
    Atom atom = XInternAtom(dpy, "XdndDirectSave0", True);
    if (atom == None)
        return {};
    Atom actual = None;
    int format = 0;
    unsigned long nitems = 0;
    unsigned long after = 0;
    unsigned char *data = nullptr;
    if (XGetWindowProperty(dpy, static_cast<Window>(wid), atom, 0, 65535, False, AnyPropertyType,
                           &actual, &format, &nitems, &after, &data)
            != Success
        || !data)
        return {};
    QByteArray out(reinterpret_cast<char *>(data), int(nitems));
    XFree(data);
    return out;
}

void xdsWrite(WId wid, const QByteArray &value)
{
    Display *dpy = x11Display();
    if (!dpy || !wid)
        return;
    Atom atom = XInternAtom(dpy, "XdndDirectSave0", False);
    Atom type = XInternAtom(dpy, "text/plain", False);
    XChangeProperty(dpy, static_cast<Window>(wid), atom, type, 8, PropModeReplace,
                    reinterpret_cast<const unsigned char *>(value.constData()), value.size());
    XFlush(dpy);
}

Window toplevelOf(Display *dpy, Window w)
{
    Window root = DefaultRootWindow(dpy);
    Window parent = w;
    Window cur = w;
    Window *children = nullptr;
    unsigned int n = 0;
    int guard = 0;
    while (parent && parent != root && guard++ < 32) {
        cur = parent;
        if (!XQueryTree(dpy, cur, &root, &parent, &children, &n))
            break;
        if (children)
            XFree(children);
    }
    return cur;
}

QString windowUtf8Name(Display *dpy, Window w)
{
    Atom netName = XInternAtom(dpy, "_NET_WM_NAME", True);
    Atom utf8 = XInternAtom(dpy, "UTF8_STRING", True);
    if (netName != None) {
        Atom actual = None;
        int format = 0;
        unsigned long nitems = 0, after = 0;
        unsigned char *data = nullptr;
        if (XGetWindowProperty(dpy, w, netName, 0, 1024, False, utf8 != None ? utf8 : AnyPropertyType,
                               &actual, &format, &nitems, &after, &data) == Success
            && data) {
            QString s = QString::fromUtf8(reinterpret_cast<char *>(data), int(nitems));
            XFree(data);
            if (!s.trimmed().isEmpty())
                return s.trimmed();
        }
    }
    Atom actual = None;
    int format = 0;
    unsigned long nitems = 0, after = 0;
    unsigned char *data = nullptr;
    if (XGetWindowProperty(dpy, w, XA_WM_NAME, 0, 1024, False, AnyPropertyType,
                           &actual, &format, &nitems, &after, &data) == Success
        && data) {
        QString s = QString::fromLocal8Bit(reinterpret_cast<char *>(data), int(nitems));
        XFree(data);
        return s.trimmed();
    }
    return {};
}

QString windowClassName(Display *dpy, Window w)
{
    XClassHint hint;
    hint.res_name = nullptr;
    hint.res_class = nullptr;
    if (!XGetClassHint(dpy, w, &hint))
        return {};
    QString s = QString::fromLocal8Bit(hint.res_class) + QLatin1Char(' ')
        + QString::fromLocal8Bit(hint.res_name);
    if (hint.res_class)
        XFree(hint.res_class);
    if (hint.res_name)
        XFree(hint.res_name);
    return s;
}

bool isDesktopWindow(Display *dpy, Window w)
{
    const QString cls = windowClassName(dpy, w).toLower();
    if (cls.contains(QLatin1String("peony-desktop"))
        || cls.contains(QLatin1String("desktop_window"))
        || cls.contains(QLatin1String("plasmashell"))
        || cls.contains(QLatin1String("xfdesktop")))
        return true;
    Atom typeAtom = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", True);
    Atom desktop = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", True);
    if (typeAtom == None || desktop == None)
        return false;
    Atom actual = None;
    int format = 0;
    unsigned long nitems = 0, after = 0;
    unsigned char *data = nullptr;
    if (XGetWindowProperty(dpy, w, typeAtom, 0, 8, False, XA_ATOM,
                           &actual, &format, &nitems, &after, &data) == Success
        && data) {
        const Atom *atoms = reinterpret_cast<const Atom *>(data);
        for (unsigned long i = 0; i < nitems; ++i) {
            if (atoms[i] == desktop) {
                XFree(data);
                return true;
            }
        }
        XFree(data);
    }
    return false;
}

bool isFileManagerWindow(const QString &cls)
{
    const QString s = cls.toLower();
    const char *keys[] = {
        "peony", "caja", "nautilus", "nemo", "thunar", "dolphin",
        "pcmanfm", "dde-file-manager", "ukui", "fm", "files"
    };
    for (const char *k : keys) {
        if (s.contains(QLatin1String(k)))
            return true;
    }
    return false;
}

QString resolveFolderTitle(const QString &raw)
{
    QString title = raw.trimmed();
    const QStringList apps = {
        QStringLiteral("Peony"), QStringLiteral("peony"), QStringLiteral("文件管理器"),
        QStringLiteral("File Manager"), QStringLiteral("Dolphin"), QStringLiteral("Nautilus"),
        QStringLiteral("Caja"), QStringLiteral("Thunar"), QStringLiteral("PCManFM"),
        QStringLiteral("UKUI")
    };
    for (const QString &sep : {QStringLiteral(" — "), QStringLiteral(" - "), QStringLiteral(" – ")}) {
        for (const QString &app : apps) {
            const QString tail = sep + app;
            if (title.endsWith(tail))
                title.chop(tail.size());
        }
    }
    title = title.trimmed();
    if (title.startsWith(QLatin1Char('/')) && QDir(title).exists())
        return title;

    const QString home = QDir::homePath();
    struct Map { const char *name; QStandardPaths::StandardLocation loc; const char *extra; };
    const Map maps[] = {
        {"桌面", QStandardPaths::DesktopLocation, "Desktop"},
        {"Desktop", QStandardPaths::DesktopLocation, "桌面"},
        {"下载", QStandardPaths::DownloadLocation, "Downloads"},
        {"Downloads", QStandardPaths::DownloadLocation, "下载"},
        {"文档", QStandardPaths::DocumentsLocation, "Documents"},
        {"Documents", QStandardPaths::DocumentsLocation, "文档"},
        {"图片", QStandardPaths::PicturesLocation, "Pictures"},
        {"Pictures", QStandardPaths::PicturesLocation, "图片"},
        {"音乐", QStandardPaths::MusicLocation, "Music"},
        {"Music", QStandardPaths::MusicLocation, "音乐"},
        {"视频", QStandardPaths::MoviesLocation, "Videos"},
        {"Videos", QStandardPaths::MoviesLocation, "视频"},
        {"主目录", QStandardPaths::HomeLocation, ""},
        {"Home", QStandardPaths::HomeLocation, ""},
        {"家", QStandardPaths::HomeLocation, ""}
    };
    for (const Map &m : maps) {
        if (title.compare(QString::fromUtf8(m.name), Qt::CaseInsensitive) == 0) {
            QString p = QStandardPaths::writableLocation(m.loc);
            if (!p.isEmpty() && QDir(p).exists())
                return p;
            if (m.extra && m.extra[0]) {
                const QString e = home + QLatin1Char('/') + QString::fromUtf8(m.extra);
                if (QDir(e).exists())
                    return e;
            }
        }
    }
    QDir homeDir(home);
    const QFileInfoList infos = homeDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : infos) {
        if (fi.fileName().compare(title, Qt::CaseInsensitive) == 0)
            return fi.absoluteFilePath();
    }
    return {};
}

QString detectDropDirectory(QWidget *self, const QVector<WId> &ours)
{
    Display *dpy = x11Display();
    if (!dpy)
        return {};
    Window root = None, child = None;
    int rx = 0, ry = 0, wx = 0, wy = 0;
    unsigned int mask = 0;
    if (!XQueryPointer(dpy, DefaultRootWindow(dpy), &root, &child, &rx, &ry, &wx, &wy, &mask))
        return {};
    if (!child)
        return desktopDir();

    Window top = toplevelOf(dpy, child);
    for (WId id : ours) {
        if (id && (static_cast<Window>(id) == top || static_cast<Window>(id) == child))
            return {};
    }
    if (self) {
        const WId topId = self->window()->effectiveWinId();
        if (topId && static_cast<Window>(topId) == top)
            return {};
    }
    if (isDesktopWindow(dpy, top) || isDesktopWindow(dpy, child))
        return desktopDir();

    const QString cls = windowClassName(dpy, top);
    if (cls.contains(QLatin1String("linhub"), Qt::CaseInsensitive))
        return {};
    if (isFileManagerWindow(cls)) {
        const QString folder = resolveFolderTitle(windowUtf8Name(dpy, top));
        if (!folder.isEmpty())
            return folder;
    }
    return {};
}
#endif

QString xdsLocalPath(const QByteArray &raw)
{
    QByteArray t = raw.trimmed();
    if (t.endsWith('\0'))
        t.chop(1);
    const QString s = QString::fromUtf8(t);
    if (s.startsWith(QLatin1String("file:")))
        return QUrl::fromEncoded(t).toLocalFile();
    return {};
}

class SftpDragMime : public QMimeData
{
public:
    struct Item {
        QString remote;
        QString name;
        qint64 size = 0;
        bool isDir = false;
    };

    SftpDragMime(SftpWidget *owner, const QVector<Item> &items, QWidget *source, const QVector<WId> &wids)
        : m_owner(owner)
        , m_items(items)
        , m_source(source)
        , m_wids(wids)
    {
#ifdef LINHUB_HAVE_X11
        if (!m_items.isEmpty()) {
            const QByteArray name = m_items.first().name.toUtf8();
            for (WId w : m_wids)
                xdsWrite(w, name);
        }
#endif
    }

    QStringList formats() const override
    {
        QStringList f = QMimeData::formats();
        const QStringList extra = {QStringLiteral("XdndDirectSave0"),
                                   QStringLiteral("text/uri-list"),
                                   QStringLiteral("text/plain"),
                                   QStringLiteral("x-special/gnome-copied-files")};
        for (int i = extra.size() - 1; i >= 0; --i) {
            if (!f.contains(extra.at(i)))
                f.prepend(extra.at(i));
        }
        return f;
    }

    bool hasFormat(const QString &m) const override
    {
        return m == QLatin1String("XdndDirectSave0") || m == QLatin1String("text/uri-list")
            || m == QLatin1String("text/plain") || m == QLatin1String("x-special/gnome-copied-files")
            || QMimeData::hasFormat(m);
    }

    bool saved() const { return m_saved; }

    QString captureDest() const
    {
#ifdef LINHUB_HAVE_X11
        for (WId w : m_wids) {
            const QString d = xdsLocalPath(xdsRead(w));
            if (!d.isEmpty())
                return d;
        }
        return detectDropDirectory(m_source, m_wids);
#else
        return {};
#endif
    }

    bool saveToDestination(const QString &destHint)
    {
        if (m_saved || !m_owner)
            return m_ok;
        QString dest = destHint;
        if (dest.isEmpty())
            dest = captureDest();
        if (dest.isEmpty())
            return false;
        m_saved = true;
        QFileInfo fi(dest);
        QString dir;
        QString firstName;
        const bool oneFile = m_items.size() == 1 && !m_items.first().isDir;
        if (oneFile && !fi.isDir() && !dest.endsWith(QLatin1Char('/'))) {
            dir = fi.absolutePath();
            firstName = fi.fileName();
        } else {
            dir = (fi.isDir() || dest.endsWith(QLatin1Char('/'))) ? dest : fi.absolutePath();
            if (dir.endsWith(QLatin1Char('/')))
                dir.chop(1);
        }
        QDir().mkpath(dir);
        QList<QUrl> urls;
        m_ok = true;
        for (int i = 0; i < m_items.size(); ++i) {
            const Item &it = m_items.at(i);
            const QString localName = (i == 0 && !firstName.isEmpty()) ? firstName : it.name;
            const QString local = dir + QLatin1Char('/') + localName;
            if (!m_owner->saveRemoteTo(it.remote, local, it.name, it.size, it.isDir)) {
                m_ok = false;
                continue;
            }
            urls.append(QUrl::fromLocalFile(local));
        }
        applyUrls(urls);
#ifdef LINHUB_HAVE_X11
        const QByteArray reply = m_ok ? QByteArray("S") : QByteArray("F");
        for (WId w : m_wids)
            xdsWrite(w, reply);
#endif
        return m_ok;
    }

protected:
    QVariant retrieveData(const QString &mimeType, QVariant::Type type) const override
    {
        if (!m_saved) {
            const QString dest = captureDest();
            if (!dest.isEmpty())
                const_cast<SftpDragMime *>(this)->saveToDestination(dest);
        }
        if (mimeType == QLatin1String("XdndDirectSave0")) {
            if (m_saved)
                return QByteArray(m_ok ? "S" : "F");
            if (!m_items.isEmpty())
                return QByteArray(m_items.first().name.toUtf8());
            return QByteArray();
        }
        return QMimeData::retrieveData(mimeType, type);
    }

private:
    void applyUrls(const QList<QUrl> &urls)
    {
        if (urls.isEmpty())
            return;
        QByteArray uriList;
        QByteArray gnome("copy\n");
        for (int i = 0; i < urls.size(); ++i) {
            const QByteArray enc = urls.at(i).toEncoded();
            uriList += enc;
            uriList += "\r\n";
            if (i)
                gnome += '\n';
            gnome += enc;
        }
        setUrls(urls);
        setData(QStringLiteral("text/uri-list"), uriList);
        setData(QStringLiteral("x-special/gnome-copied-files"), gnome);
        setData(QStringLiteral("text/plain"), urls.first().toLocalFile().toUtf8());
    }

    SftpWidget *m_owner = nullptr;
    QVector<Item> m_items;
    QWidget *m_source = nullptr;
    QVector<WId> m_wids;
    bool m_saved = false;
    bool m_ok = false;
};

class SftpTreeWidget : public QTreeWidget
{
public:
    explicit SftpTreeWidget(SftpWidget *owner)
        : QTreeWidget(owner)
        , m_owner(owner)
    {
        setAcceptDrops(true);
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragDrop);
        setDefaultDropAction(Qt::CopyAction);
        setDropIndicatorShown(false);
        setSelectionMode(QAbstractItemView::ExtendedSelection);
        setContextMenuPolicy(Qt::CustomContextMenu);
        header()->setMinimumSectionSize(36);
        header()->setStretchLastSection(true);
        setAllColumnsShowFocus(true);
        setSelectionBehavior(QAbstractItemView::SelectRows);
        setItemDelegate(new NoFocusDelegate(this));
        setColumnWidth(0, 200);
        viewport()->setAcceptDrops(true);
        viewport()->winId();
        winId();
        window()->winId();
    }

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
            if (m_owner)
                m_owner->deleteSelected();
            return;
        }
        QTreeWidget::keyPressEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        m_pressItem = itemAt(event->pos());
        m_pressPos = event->pos();
        QTreeWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if ((event->buttons() & Qt::LeftButton) && m_pressItem
            && (event->pos() - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
            startDrag(Qt::CopyAction);
            m_pressItem = nullptr;
            return;
        }
        QTreeWidget::mouseMoveEvent(event);
    }

    void startDrag(Qt::DropActions) override
    {
        QList<QTreeWidgetItem *> items = selectedItems();
        if (items.isEmpty() && m_pressItem)
            items << m_pressItem;
        QVector<SftpDragMime::Item> pack;
        QPixmap pix;
        for (QTreeWidgetItem *it : items) {
            if (!it)
                continue;
            const QString name = it->text(0);
            if (name.isEmpty() || name == QLatin1String(".."))
                continue;
            SftpDragMime::Item one;
            one.remote = it->data(0, Qt::UserRole).toString();
            one.name = name;
            one.size = it->data(0, Qt::UserRole + 2).toLongLong();
            one.isDir = it->data(0, Qt::UserRole + 1).toBool();
            pack.append(one);
            if (pix.isNull())
                pix = it->icon(0).pixmap(16, 16);
        }
        if (pack.isEmpty())
            return;
        QVector<WId> wids;
        wids << winId() << viewport()->winId() << window()->effectiveWinId();
        auto *mime = new SftpDragMime(m_owner, pack, this, wids);
        QDrag drag(this);
        drag.setMimeData(mime);
        if (!pix.isNull())
            drag.setPixmap(pix);
        drag.exec(Qt::CopyAction, Qt::CopyAction);
        if (!mime->saved()) {
            QString dest = mime->captureDest();
            if (dest.isEmpty())
                dest = QFileDialog::getExistingDirectory(
                    m_owner, QStringLiteral("选择保存位置"), desktopDir());
            if (!dest.isEmpty())
                mime->saveToDestination(dest);
        }
    }

    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->source() == this) {
            event->ignore();
            return;
        }
        if (event->mimeData()->hasUrls() || event->mimeData()->hasFormat(QStringLiteral("text/uri-list")))
            event->acceptProposedAction();
        else
            event->ignore();
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (event->source() == this) {
            event->ignore();
            return;
        }
        if (event->mimeData()->hasUrls() || event->mimeData()->hasFormat(QStringLiteral("text/uri-list")))
            event->acceptProposedAction();
        else
            event->ignore();
    }

    void dropEvent(QDropEvent *event) override
    {
        if (!m_owner || event->source() == this) {
            event->ignore();
            return;
        }
        QStringList files;
        for (const QUrl &url : event->mimeData()->urls()) {
            if (url.isLocalFile())
                files << url.toLocalFile();
        }
        if (!files.isEmpty()) {
            m_owner->uploadLocalFiles(files);
            event->acceptProposedAction();
        } else {
            event->ignore();
        }
    }

private:
    SftpWidget *m_owner = nullptr;
    QTreeWidgetItem *m_pressItem = nullptr;
    QPoint m_pressPos;
};

bool sftpLightTheme(QWidget *w)
{
    if (AppSettings::instance().uiTheme() == QLatin1String("light"))
        return true;
    if (w) {
        const QColor bg = w->palette().color(QPalette::Base);
        if (bg.isValid() && bg.lightness() > 160)
            return true;
    }
    return false;
}

QColor sftpEntryColor(const SftpEntry &e, QWidget *w = nullptr)
{
    const bool light = sftpLightTheme(w);
    if (e.name == QLatin1String(".."))
        return QColor(light ? QStringLiteral("#334155") : QStringLiteral("#8b9cb3"));
    if (e.isDir)
        return QColor(light ? QStringLiteral("#1e3a8a") : QStringLiteral("#58a6ff"));
    if (e.isLink)
        return QColor(light ? QStringLiteral("#155e75") : QStringLiteral("#39d7e5"));
    if (e.isExec)
        return QColor(light ? QStringLiteral("#14532d") : QStringLiteral("#3fb950"));
    const QString n = e.name.toLower();
    auto match = [&](const QStringList &exts) {
        for (const QString &x : exts) {
            if (n.endsWith(x))
                return true;
        }
        return false;
    };
    if (match({QStringLiteral(".tar"), QStringLiteral(".gz"), QStringLiteral(".tgz"),
               QStringLiteral(".zip"), QStringLiteral(".bz2"), QStringLiteral(".xz"),
               QStringLiteral(".7z"), QStringLiteral(".rar"), QStringLiteral(".zst")}))
        return QColor(light ? QStringLiteral("#991b1b") : QStringLiteral("#f85149"));
    if (match({QStringLiteral(".jpg"), QStringLiteral(".jpeg"), QStringLiteral(".png"),
               QStringLiteral(".gif"), QStringLiteral(".bmp"), QStringLiteral(".svg"),
               QStringLiteral(".webp"), QStringLiteral(".ico")}))
        return QColor(light ? QStringLiteral("#5b21b6") : QStringLiteral("#d2a8ff"));
    if (match({QStringLiteral(".mp3"), QStringLiteral(".wav"), QStringLiteral(".flac"),
               QStringLiteral(".mp4"), QStringLiteral(".mkv"), QStringLiteral(".avi"),
               QStringLiteral(".mov")}))
        return QColor(light ? QStringLiteral("#075985") : QStringLiteral("#79c0ff"));
    if (match({QStringLiteral(".pdf"), QStringLiteral(".doc"), QStringLiteral(".docx"),
               QStringLiteral(".xls"), QStringLiteral(".xlsx"), QStringLiteral(".ppt"),
               QStringLiteral(".txt"), QStringLiteral(".md")}))
        return QColor(light ? QStringLiteral("#854d0e") : QStringLiteral("#e3b341"));
    if (match({QStringLiteral(".c"), QStringLiteral(".h"), QStringLiteral(".cpp"),
               QStringLiteral(".hpp"), QStringLiteral(".py"), QStringLiteral(".js"),
               QStringLiteral(".ts"), QStringLiteral(".go"), QStringLiteral(".rs"),
               QStringLiteral(".java"), QStringLiteral(".sh"), QStringLiteral(".bash")}))
        return QColor(light ? QStringLiteral("#14532d") : QStringLiteral("#7ee787"));
    if (match({QStringLiteral(".so"), QStringLiteral(".a"), QStringLiteral(".o"),
               QStringLiteral(".exe"), QStringLiteral(".dll"), QStringLiteral(".ko")}))
        return QColor(light ? QStringLiteral("#9a3412") : QStringLiteral("#ffa657"));
    return QColor(light ? QStringLiteral("#0f172a") : QStringLiteral("#c9d1d9"));
}

} // namespace

#ifdef None
#undef None
#endif

SftpWidget::SftpWidget(QWidget *parent)
    : QWidget(parent)
    , m_watch(new QFileSystemWatcher(this))
{
    purgeDownloadCache();
    connect(m_watch, &QFileSystemWatcher::fileChanged, this, &SftpWidget::onEditedFileChanged);
    setAcceptDrops(true);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);
    m_pathEdit = new QLineEdit;
    m_pathEdit->setPlaceholderText(QStringLiteral("远程路径，回车进入该目录"));
    m_pathEdit->setText(QStringLiteral("未连接"));
    m_pathEdit->setClearButtonEnabled(false);
    auto *btns = new QHBoxLayout;
    auto *up = new QPushButton;
    up->setIcon(AppIcons::get(QStringLiteral("up")));
    up->setToolTip(QStringLiteral("上级目录"));
    up->setProperty("secondary", true);
    up->setFixedSize(30, 26);
    auto *refresh = new QPushButton;
    refresh->setIcon(AppIcons::get(QStringLiteral("refresh")));
    refresh->setToolTip(QStringLiteral("刷新"));
    refresh->setProperty("secondary", true);
    refresh->setFixedSize(30, 26);
    m_followBtn = new QToolButton;
    m_followBtn->setIcon(AppIcons::get(QStringLiteral("follow")));
    m_followBtn->setIconSize(QSize(16, 16));
    m_followBtn->setCheckable(true);
    m_followBtn->setAutoRaise(true);
    m_followBtn->setToolTip(QStringLiteral("跟随终端当前目录"));
    m_followBtn->setFixedSize(30, 26);
    auto *download = new QPushButton;
    download->setIcon(AppIcons::get(QStringLiteral("download")));
    download->setToolTip(QStringLiteral("下载"));
    download->setFixedSize(30, 26);
    auto *upload = new QPushButton;
    upload->setIcon(AppIcons::get(QStringLiteral("upload")));
    upload->setToolTip(QStringLiteral("上传"));
    upload->setFixedSize(30, 26);
    auto *mkdir = new QPushButton;
    mkdir->setIcon(AppIcons::get(QStringLiteral("folder")));
    mkdir->setToolTip(QStringLiteral("新建文件夹"));
    mkdir->setProperty("secondary", true);
    mkdir->setFixedSize(30, 26);
    btns->addWidget(up);
    btns->addWidget(refresh);
    btns->addWidget(mkdir);
    btns->addWidget(m_followBtn);
    btns->addStretch();
    btns->addWidget(download);
    btns->addWidget(upload);

    m_tree = new SftpTreeWidget(this);
    m_tree->setIconSize(QSize(16, 16));
    m_tree->setHeaderLabels({QStringLiteral("名称"), QStringLiteral("大小"), QStringLiteral("修改时间")});
    m_tree->header()->setStretchLastSection(true);
    m_tree->setColumnWidth(0, 200);
    m_tree->setColumnWidth(1, 72);
    m_tree->setToolTip(QStringLiteral("拖入上传，拖出下载；右键打开菜单"));

    auto *prog = new QVBoxLayout;
    prog->setSpacing(2);
    auto *progRow = new QHBoxLayout;
    m_barLabel = new QLabel(QStringLiteral("就绪"));
    m_barLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_detailBtn = new QPushButton(QStringLiteral("详情"));
    m_detailBtn->setProperty("secondary", true);
    m_detailBtn->setFixedHeight(22);
    progRow->addWidget(m_barLabel, 1);
    progRow->addWidget(m_detailBtn);
    m_bar = new QProgressBar;
    m_bar->setObjectName(QStringLiteral("sftpProgress"));
    m_bar->setRange(0, 1000);
    m_bar->setValue(0);
    m_bar->setTextVisible(false);
    m_bar->setFixedHeight(8);
    prog->addLayout(progRow);
    prog->addWidget(m_bar);

    root->addWidget(m_pathEdit);
    root->addLayout(btns);
    root->addWidget(m_tree, 1);
    root->addLayout(prog);

    connect(&m_client, &SftpClient::listed, this, [this](const QString &path, const QVector<SftpEntry> &entries) {
        m_currentPath = path;
        m_pathEdit->setText(path);
        m_userNav = false;
        m_tree->clear();
        for (const auto &e : entries) {
            auto *it = new QTreeWidgetItem(m_tree);
            it->setIcon(0, AppIcons::get(e.isDir ? QStringLiteral("folder")
                                                 : (e.isLink ? QStringLiteral("connect")
                                                             : QStringLiteral("file"))));
            it->setText(0, e.name);
            it->setForeground(0, sftpEntryColor(e, m_tree));
            it->setText(1, e.isDir ? QString() : formatSize(e.size));
            it->setText(2, e.mtime);
            it->setData(0, Qt::UserRole, e.path);
            it->setData(0, Qt::UserRole + 1, e.isDir);
            it->setData(0, Qt::UserRole + 2, e.size);
            it->setData(0, Qt::UserRole + 3, e.mode);
            it->setData(0, Qt::UserRole + 4, e.mtime);
            Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
            if (e.name != QLatin1String(".."))
                flags |= Qt::ItemIsDragEnabled;
            it->setFlags(flags);
        }
    });
    connect(&m_client, &SftpClient::errorOccurred, this, [this](const QString &m) {
        const QString tried = m_pathEdit->text().trimmed();
        if (m_userNav) {
            m_userNav = false;
            QMessageBox::warning(this, QStringLiteral("SFTP"),
                                 QStringLiteral("目录不存在或无法进入：\n%1\n%2").arg(tried, m));
            m_pathEdit->setText(m_currentPath);
        }
        m_barLabel->setText(m);
    });
    connect(&m_client, &SftpClient::bytesTransferred, this, [this](qint64 done, qint64) {
        if (auto *j = jobById(m_activeJobId)) {
            j->done = done;
            updateProgressUi();
        }
    });
    connect(&m_client, &SftpClient::transferFinished, this, [this](bool ok, const QString &m) {
        if (auto *j = jobById(m_activeJobId)) {
            if (j->state != SftpJob::Paused) {
                j->state = ok ? SftpJob::Done : SftpJob::Failed;
                if (ok && j->size > 0)
                    j->done = j->size;
                if (!ok)
                    j->message = m;
                persistJob(*j);
                updateParentJob(j->parentId);
            }
        }
        m_activeJobId = 0;
        updateProgressUi();
        pumpQueue();
        maybeReloadAfterTransfer();
    });
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *it, int) {
        const SftpEntry e = entryFromItem(it);
        if (e.name == QLatin1String("..") || e.isDir) {
            m_userNav = true;
            reload(e.path);
        } else {
            openEntry(e);
        }
    });
    connect(m_tree, &QWidget::customContextMenuRequested, this, &SftpWidget::showContextMenu);
    connect(up, &QPushButton::clicked, this, &SftpWidget::navigateUp);
    connect(refresh, &QPushButton::clicked, this, [this] { reload(m_currentPath); });
    connect(download, &QPushButton::clicked, this, &SftpWidget::downloadCurrent);
    connect(upload, &QPushButton::clicked, this, [this] { uploadPicker(); });
    connect(mkdir, &QPushButton::clicked, this, &SftpWidget::mkdirHere);
    connect(m_followBtn, &QToolButton::toggled, this, &SftpWidget::setFollow);
    connect(m_detailBtn, &QPushButton::clicked, this, &SftpWidget::showTransferDetails);
    connect(m_pathEdit, &QLineEdit::returnPressed, this, &SftpWidget::goToEnteredPath);
}

SftpWidget::~SftpWidget() = default;

QSize SftpWidget::sizeHint() const
{
    return QSize(210, 420);
}

QSize SftpWidget::minimumSizeHint() const
{
    return QSize(150, 180);
}

void SftpWidget::deleteSelected()
{
    deleteEntries(selectedEntries());
}

bool SftpWidget::canTransfer() const
{
    return m_client.isConfigured();
}

SftpEntry SftpWidget::entryFromItem(QTreeWidgetItem *it) const
{
    SftpEntry e;
    if (!it)
        return e;
    e.name = it->text(0);
    e.path = it->data(0, Qt::UserRole).toString();
    e.isDir = it->data(0, Qt::UserRole + 1).toBool();
    e.size = it->data(0, Qt::UserRole + 2).toLongLong();
    e.mode = it->data(0, Qt::UserRole + 3).toString();
    e.mtime = it->data(0, Qt::UserRole + 4).toString();
    return e;
}

QVector<SftpEntry> SftpWidget::selectedEntries() const
{
    QVector<SftpEntry> out;
    for (QTreeWidgetItem *it : m_tree->selectedItems()) {
        const SftpEntry e = entryFromItem(it);
        if (e.name != QLatin1String(".."))
            out.append(e);
    }
    return out;
}

SftpJob *SftpWidget::jobById(int id)
{
    for (SftpJob &j : m_jobs) {
        if (j.id == id)
            return &j;
    }
    return nullptr;
}

void SftpWidget::persistJob(SftpJob &job)
{
    if (job.persisted)
        return;
    if (job.state != SftpJob::Done && job.state != SftpJob::Failed)
        return;
    job.persisted = true;
    TransferRecord r;
    r.upload = job.upload;
    r.isDir = job.isDir;
    r.name = job.name;
    r.localPath = job.local;
    r.remotePath = job.remote;
    r.size = job.size > 0 ? job.size : QFileInfo(job.local).size();
    r.status = job.state == SftpJob::Done ? QStringLiteral("完成") : QStringLiteral("失败");
    r.message = job.message;
    r.sessionId = m_session.id;
    TransferRepository().insert(r);
    if (m_details) {
        m_details->markHistoryDirty();
        if (m_details->isVisible())
            m_details->reloadHistory();
    }
}

void SftpWidget::maybeReloadAfterTransfer()
{
    if (m_activeJobId != 0)
        return;
    for (const SftpJob &j : m_jobs) {
        if (j.state == SftpJob::Queued || j.state == SftpJob::Running || j.state == SftpJob::Paused)
            return;
    }
    QTimer::singleShot(400, this, [this] {
        if (m_activeJobId != 0)
            return;
        for (const SftpJob &j : m_jobs) {
            if (j.state == SftpJob::Queued || j.state == SftpJob::Running || j.state == SftpJob::Paused)
                return;
        }
        if (canTransfer() && !m_currentPath.isEmpty())
            reload(m_currentPath);
    });
}

bool SftpWidget::saveRemoteTo(const QString &remotePath, const QString &localPath, const QString &name,
                             qint64 size, bool isDir)
{
    if (!canTransfer())
        return false;
    const QString safeName = name.isEmpty() ? QFileInfo(localPath).fileName() : name;
    SftpJob j;
    j.id = m_nextJobId++;
    j.upload = false;
    j.remote = remotePath;
    j.local = localPath;
    j.name = safeName;
    j.size = size;
    j.isDir = isDir;
    j.state = SftpJob::Running;
    m_jobs.append(j);
    const int id = j.id;
    updateProgressUi();

    bool ok = false;
    if (isDir) {
        QDir().mkpath(localPath);
        const QStringList dirs = m_client.listRemoteDirs(remotePath);
        QString prefix = remotePath;
        if (!prefix.endsWith(QLatin1Char('/')))
            prefix += QLatin1Char('/');
        for (const QString &d : dirs) {
            QString rel = d;
            if (rel.startsWith(prefix))
                rel = rel.mid(prefix.size());
            else
                rel = QFileInfo(d).fileName();
            if (!rel.isEmpty())
                QDir().mkpath(localPath + QLatin1Char('/') + rel);
        }
        const QStringList files = m_client.listRemoteFiles(remotePath);
        ok = true;
        qint64 total = 0;
        for (const QString &remote : files) {
            QString rel = remote;
            if (rel.startsWith(prefix))
                rel = rel.mid(prefix.size());
            else
                rel = QFileInfo(remote).fileName();
            const QString local = localPath + QLatin1Char('/') + rel;
            if (!m_client.downloadBlocking(remote, local, 180000, 0))
                ok = false;
            else
                total += QFileInfo(local).size();
        }
        if (auto *job = jobById(id))
            job->size = total;
    } else {
        ok = m_client.downloadBlocking(remotePath, localPath, 180000, size);
    }

    if (auto *job = jobById(id)) {
        job->state = ok ? SftpJob::Done : SftpJob::Failed;
        if (ok)
            job->done = job->size > 0 ? job->size : QFileInfo(localPath).size();
        else if (job->message.isEmpty())
            job->message = QStringLiteral("下载失败");
        persistJob(*job);
    }
    updateProgressUi();
    pumpQueue();
    return ok;
}

void SftpWidget::attachSession(const Session &session, const QString &password, const QString &controlPath)
{
    const bool sameSession = m_client.isConfigured()
        && m_session.id == session.id
        && m_session.host == session.host
        && m_session.port == session.port
        && m_session.username == session.username
        && m_controlPath == controlPath
        && m_password == password;
    m_session = session;
    m_password = password;
    m_controlPath = controlPath;
    m_client.configure(session, password, controlPath);
    if (sameSession)
        return;
    reload(QStringLiteral("."));
}

void SftpWidget::refreshFileColors()
{
    if (!m_tree)
        return;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *it = m_tree->topLevelItem(i);
        if (!it)
            continue;
        it->setForeground(0, sftpEntryColor(entryFromItem(it), m_tree));
    }
}

void SftpWidget::detachSession()
{
    m_session = Session();
    m_password.clear();
    m_controlPath.clear();
    m_client.clear();
    m_currentPath = QStringLiteral(".");
    if (m_pathEdit)
        m_pathEdit->setText(QStringLiteral("未连接"));
    if (m_tree)
        m_tree->clear();
}

void SftpWidget::setFollow(bool on)
{
    m_follow = on;
    if (on)
        emit followRequested();
}

void SftpWidget::applyTerminalCwd(const QString &cwd)
{
    if (!m_follow || cwd.isEmpty() || !canTransfer())
        return;
    if (cwd == m_currentPath)
        return;
    reload(cwd);
}

void SftpWidget::reload(const QString &path)
{
    if (!canTransfer())
        return;
    m_client.list(path);
}

void SftpWidget::goToEnteredPath()
{
    const QString p = m_pathEdit->text().trimmed();
    if (!canTransfer()) {
        QMessageBox::information(this, QStringLiteral("SFTP"), QStringLiteral("尚未连接会话"));
        return;
    }
    if (p.isEmpty() || p == QLatin1String("未连接"))
        return;
    m_userNav = true;
    QString err;
    const QString abs = m_client.resolvePath(p, &err);
    if (abs.isEmpty()) {
        m_userNav = false;
        QMessageBox::warning(this, QStringLiteral("SFTP"),
                             QStringLiteral("目录不存在或无法进入：\n%1\n%2").arg(p, err));
        m_pathEdit->setText(m_currentPath == QLatin1String(".") ? QString() : m_currentPath);
        return;
    }
    reload(abs);
}

void SftpWidget::navigateUp()
{
    if (m_currentPath == QLatin1String("/") || m_currentPath.isEmpty())
        return;
    m_userNav = true;
    reload(m_currentPath + QStringLiteral("/.."));
}

void SftpWidget::enqueueDownload(const QString &remote, const QString &local, const QString &name, qint64 size,
                                int parentId, bool cached)
{
    SftpJob j;
    j.id = m_nextJobId++;
    j.upload = false;
    j.remote = remote;
    j.local = local;
    j.name = name;
    j.size = size;
    j.state = SftpJob::Queued;
    j.parentId = parentId;
    j.cached = cached;
    m_jobs.append(j);
    updateProgressUi();
    pumpQueue();
}

void SftpWidget::enqueueUpload(const QString &local, const QString &remote, const QString &name, qint64 size,
                              int parentId)
{
    SftpJob j;
    j.id = m_nextJobId++;
    j.upload = true;
    j.remote = remote;
    j.local = local;
    j.name = name;
    j.size = size;
    j.state = SftpJob::Queued;
    j.parentId = parentId;
    m_jobs.append(j);
    updateProgressUi();
    pumpQueue();
}

void SftpWidget::enqueueDirUpload(const QString &localDir, const QString &remoteDir)
{
    QDir root(localDir);
    if (!root.exists()) {
        QMessageBox::warning(this, QStringLiteral("上传"),
                             QStringLiteral("本地文件夹不存在：\n%1").arg(localDir));
        return;
    }
    QStringList remoteDirs;
    remoteDirs.append(remoteDir);
    struct FileItem {
        QString local;
        QString remote;
        QString name;
        qint64 size = 0;
    };
    QVector<FileItem> files;

    QDirIterator it(localDir, QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo fi = it.fileInfo();
        QString rel = root.relativeFilePath(fi.absoluteFilePath());
        rel.replace(QLatin1Char('\\'), QLatin1Char('/'));
        if (rel.isEmpty() || rel == QLatin1String("."))
            continue;
        const QString rem = joinRemote(remoteDir, rel);
        if (fi.isDir()) {
            if (!remoteDirs.contains(rem))
                remoteDirs.append(rem);
        } else if (fi.isFile() && !fi.isSymLink()) {
            FileItem f;
            f.local = fi.absoluteFilePath();
            f.remote = rem;
            f.name = rel;
            f.size = fi.size();
            files.append(f);
        }
    }

    m_barLabel->setText(QStringLiteral("正在创建远程目录…"));
    QString err;
    if (!m_client.mkdirPaths(remoteDirs, &err)) {
        bool allOk = true;
        QString lastErr = err;
        for (const QString &d : remoteDirs) {
            QString oneErr;
            if (!m_client.mkdirPath(d, &oneErr)) {
                allOk = false;
                lastErr = oneErr;
                break;
            }
        }
        if (!allOk) {
            QMessageBox::warning(this, QStringLiteral("上传"),
                                 lastErr.trimmed().isEmpty()
                                     ? QStringLiteral("创建远程目录失败：\n%1").arg(remoteDir)
                                     : lastErr);
            return;
        }
    }

    SftpJob dirJob;
    dirJob.id = m_nextJobId++;
    dirJob.upload = true;
    dirJob.isDir = true;
    dirJob.remote = remoteDir;
    dirJob.local = QFileInfo(localDir).absoluteFilePath();
    dirJob.name = QFileInfo(localDir).fileName();
    dirJob.state = files.isEmpty() ? SftpJob::Done : SftpJob::Running;
    m_jobs.append(dirJob);
    const int parentId = dirJob.id;
    if (files.isEmpty())
        persistJob(m_jobs.last());

    for (const FileItem &f : files)
        enqueueUpload(f.local, f.remote, f.name, f.size, parentId);
    if (files.isEmpty())
        updateProgressUi();
    maybeReloadAfterTransfer();
}

void SftpWidget::enqueueDirDownload(const QString &remoteDir, const QString &localDir)
{
    const QStringList dirs = m_client.listRemoteDirs(remoteDir);
    const QStringList files = m_client.listRemoteFiles(remoteDir);
    QDir().mkpath(localDir);
    QString prefix = remoteDir;
    if (!prefix.endsWith(QLatin1Char('/')))
        prefix += QLatin1Char('/');
    for (const QString &d : dirs) {
        QString rel = d;
        if (rel.startsWith(prefix))
            rel = rel.mid(prefix.size());
        else
            rel = QFileInfo(d).fileName();
        if (!rel.isEmpty())
            QDir().mkpath(localDir + QLatin1Char('/') + rel);
    }
    SftpJob dirJob;
    dirJob.id = m_nextJobId++;
    dirJob.upload = false;
    dirJob.isDir = true;
    dirJob.remote = remoteDir;
    dirJob.local = localDir;
    dirJob.name = QFileInfo(localDir).fileName();
    dirJob.state = files.isEmpty() ? SftpJob::Done : SftpJob::Running;
    m_jobs.append(dirJob);
    const int parentId = dirJob.id;
    if (files.isEmpty())
        persistJob(m_jobs.last());
    if (files.isEmpty() && dirs.isEmpty())
        return;
    for (const QString &remote : files) {
        QString rel = remote;
        if (rel.startsWith(prefix))
            rel = rel.mid(prefix.size());
        else
            rel = QFileInfo(remote).fileName();
        const QString local = localDir + QLatin1Char('/') + rel;
        enqueueDownload(remote, local, rel, 0, parentId, false);
    }
}

void SftpWidget::pumpQueue()
{
    if (m_activeJobId != 0 || m_client.isTransferring())
        return;
    SftpJob *next = nullptr;
    for (SftpJob &j : m_jobs) {
        if (j.state == SftpJob::Queued) {
            next = &j;
            break;
        }
    }
    if (!next)
        return;
    next->state = SftpJob::Running;
    m_activeJobId = next->id;
    updateProgressUi();
    if (next->upload)
        m_client.startUpload(next->local, next->remote, next->size);
    else
        m_client.startDownload(next->remote, next->local, next->size);
}

void SftpWidget::updateProgressUi()
{
    int queued = 0, running = 0, done = 0, failed = 0;
    qint64 all = 0, got = 0;
    const SftpJob *cur = nullptr;
    for (const SftpJob &j : m_jobs) {
        switch (j.state) {
        case SftpJob::Queued: ++queued; all += qMax(j.size, qint64(1)); break;
        case SftpJob::Running:
            ++running;
            cur = &j;
            all += qMax(j.size, qint64(1));
            got += j.done;
            break;
        case SftpJob::Paused: all += qMax(j.size, qint64(1)); got += j.done; break;
        case SftpJob::Done: ++done; all += qMax(j.size, qint64(1)); got += qMax(j.size, qint64(1)); break;
        case SftpJob::Failed: ++failed; break;
        }
    }
    const int total = queued + running + done + failed;
    int pct = 0;
    if (all > 0)
        pct = int(qMin(got, all) * 1000 / all);
    m_bar->setValue(pct);
    if (cur) {
        m_barLabel->setText((cur->upload ? QStringLiteral("↑ ") : QStringLiteral("↓ ")) + cur->name);
        m_bar->setToolTip(formatSize(cur->done) + QLatin1Char('/')
                          + (cur->size > 0 ? formatSize(cur->size) : QStringLiteral("?")));
    } else if (total == 0) {
        m_barLabel->setText(QStringLiteral("就绪"));
        m_bar->setValue(0);
    } else {
        QString t = QStringLiteral("完成 %1 / %2").arg(done).arg(total);
        if (failed > 0)
            t += QStringLiteral("，失败 %1").arg(failed);
        m_barLabel->setText(t);
    }
    if (m_details && m_details->isVisible())
        m_details->refresh(m_jobs);
}

void SftpWidget::showTransferDetails()
{
    if (!m_details) {
        m_details = new SftpTransferDialog(this);
        m_details->onLiveAction = [this](int id, const QString &op) { handleLiveAction(id, op); };
    }
    m_details->refresh(m_jobs);
    m_details->show();
    m_details->raise();
}

void SftpWidget::downloadCurrent()
{
    const QVector<SftpEntry> sel = selectedEntries();
    if (sel.isEmpty() || !canTransfer())
        return;
    if (sel.size() == 1 && !sel.first().isDir) {
        const QString dest = QFileDialog::getSaveFileName(this, QStringLiteral("保存到本地"), sel.first().name);
        if (dest.isEmpty())
            return;
        enqueueDownload(sel.first().path, dest, sel.first().name, sel.first().size);
        return;
    }
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("下载到文件夹"));
    if (dir.isEmpty())
        return;
    for (const SftpEntry &e : sel) {
        if (e.isDir)
            enqueueDirDownload(e.path, dir + QLatin1Char('/') + e.name);
        else
            enqueueDownload(e.path, dir + QLatin1Char('/') + e.name, e.name, e.size);
    }
}

namespace {

struct UploadPickResult {
    QStringList files;
    QStringList dirs;
    QString currentDir;
};

void addLocalPath(UploadPickResult *out, const QString &path)
{
    if (!out || path.trimmed().isEmpty())
        return;
    const QFileInfo fi(path);
    if (!fi.exists())
        return;
    const QString abs = QDir::cleanPath(fi.absoluteFilePath());
    if (abs.isEmpty() || abs == QLatin1String(".") || abs == QLatin1String(".."))
        return;
    const QString name = fi.fileName();
    if (name == QLatin1String(".") || name == QLatin1String(".."))
        return;
    if (fi.isDir()) {
        if (!out->dirs.contains(abs))
            out->dirs.append(abs);
    } else if (fi.isFile()) {
        if (!out->files.contains(abs))
            out->files.append(abs);
    }
}

UploadPickResult collectUploadPick(QFileDialog *dlg)
{
    UploadPickResult out;
    if (!dlg)
        return out;

    out.currentDir = QDir::cleanPath(dlg->directory().absolutePath());
    if (auto *model = dlg->findChild<QFileSystemModel *>()) {
        const QString root = model->rootPath();
        if (!root.isEmpty() && QFileInfo(root).isDir())
            out.currentDir = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
    }
    if (auto *combo = dlg->findChild<QComboBox *>(QStringLiteral("lookInCombo"))) {
        const QVariant data = combo->currentData();
        QString fromCombo;
        if (data.canConvert<QUrl>()) {
            const QUrl u = data.toUrl();
            if (u.isLocalFile())
                fromCombo = u.toLocalFile();
        }
        if (fromCombo.isEmpty())
            fromCombo = combo->itemData(combo->currentIndex(), Qt::EditRole).toString();
        if (fromCombo.isEmpty())
            fromCombo = combo->currentText();
        if (!fromCombo.isEmpty() && QFileInfo(fromCombo).isDir())
            out.currentDir = QDir::cleanPath(QFileInfo(fromCombo).absoluteFilePath());
    }

    for (const QString &s : dlg->selectedFiles())
        addLocalPath(&out, s);
    for (const QUrl &u : dlg->selectedUrls()) {
        if (u.isLocalFile())
            addLocalPath(&out, u.toLocalFile());
    }
    if (auto *edit = dlg->findChild<QLineEdit *>(QStringLiteral("fileNameEdit"))) {
        const QString typed = edit->text().trimmed();
        if (!typed.isEmpty() && !typed.contains(QLatin1Char('"'))) {
            QFileInfo typedFi(typed);
            if (!typedFi.isAbsolute())
                typedFi = QFileInfo(QDir(out.currentDir), typed);
            addLocalPath(&out, typedFi.absoluteFilePath());
        }
    }

    const QList<QAbstractItemView *> views = dlg->findChildren<QAbstractItemView *>();
    for (QAbstractItemView *v : views) {
        if (!v || !v->isVisible() || !v->selectionModel())
            continue;
        const QString obj = v->objectName();
        if (obj != QLatin1String("listView") && obj != QLatin1String("treeView"))
            continue;
        const QModelIndexList rows = v->selectionModel()->selectedRows();
        const QModelIndexList idxs = rows.isEmpty() ? v->selectionModel()->selectedIndexes() : rows;
        for (const QModelIndex &idx : idxs) {
            if (idx.column() != 0)
                continue;
            QString path = idx.data(QFileSystemModel::FilePathRole).toString();
            if (path.isEmpty())
                path = idx.data(Qt::UserRole + 1).toString();
            addLocalPath(&out, path);
        }
    }
    return out;
}

} // namespace

void SftpWidget::uploadPicker(const QString &remoteDir)
{
    if (!canTransfer()) {
        QMessageBox::information(this, QStringLiteral("上传"), QStringLiteral("尚未连接会话"));
        return;
    }
    const QString dest = resolvedRemoteDir(remoteDir);

    class UploadPickDialog : public QFileDialog
    {
    public:
        explicit UploadPickDialog(QWidget *parent)
            : QFileDialog(parent, QStringLiteral("选择要上传的内容"))
        {
        }
        void finish(int r) { QDialog::done(r); }
    };

    UploadPickDialog dlg(this);
    dlg.setOption(QFileDialog::DontUseNativeDialog, true);
    dlg.setOption(QFileDialog::ReadOnly, true);
    dlg.setFileMode(QFileDialog::ExistingFiles);
    dlg.setAcceptMode(QFileDialog::AcceptOpen);
    dlg.setDirectory(QDir::homePath());
    dlg.resize(740, 500);

    if (auto *box = dlg.findChild<QDialogButtonBox *>())
        box->hide();
    for (QAbstractItemView *v : dlg.findChildren<QAbstractItemView *>()) {
        if (!v)
            continue;
        const QString obj = v->objectName();
        if (obj == QLatin1String("listView") || obj == QLatin1String("treeView"))
            v->setSelectionMode(QAbstractItemView::ExtendedSelection);
    }

    auto *bar = new QWidget(&dlg);
    auto *vl = new QVBoxLayout(bar);
    vl->setContentsMargins(10, 2, 10, 8);
    vl->setSpacing(6);
    auto *hint = new QLabel(QStringLiteral("上传到 %1\n在列表里选中文件夹后点「上传文件夹」；也可以先进入该文件夹再上传。")
                                .arg(dest));
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#8b9cb3; font-size:13px;"));
    vl->addWidget(hint);
    auto *hl = new QHBoxLayout;
    hl->setSpacing(8);
    auto *filesBtn = new QPushButton(QStringLiteral("上传文件"));
    auto *folderBtn = new QPushButton(QStringLiteral("上传文件夹"));
    auto *cancelBtn = new QPushButton(QStringLiteral("取消"));
    cancelBtn->setProperty("secondary", true);
    filesBtn->setMinimumHeight(28);
    folderBtn->setMinimumHeight(28);
    cancelBtn->setMinimumHeight(28);
    hl->addWidget(filesBtn, 1);
    hl->addWidget(folderBtn, 1);
    hl->addWidget(cancelBtn, 1);
    vl->addLayout(hl);

    if (auto *grid = qobject_cast<QGridLayout *>(dlg.layout()))
        grid->addWidget(bar, grid->rowCount(), 0, 1, qMax(1, grid->columnCount()));
    else if (auto *boxLay = qobject_cast<QBoxLayout *>(dlg.layout()))
        boxLay->addWidget(bar);
    else if (dlg.layout())
        dlg.layout()->addWidget(bar);

    int choice = 0;
    QStringList picked;
    connect(filesBtn, &QPushButton::clicked, &dlg, [&] {
        const UploadPickResult c = collectUploadPick(&dlg);
        picked = c.files;
        for (const QString &d : c.dirs)
            picked << d;
        if (picked.isEmpty()) {
            QMessageBox::information(&dlg, QStringLiteral("上传"),
                                     QStringLiteral("请先在列表里选中要上传的文件或文件夹"));
            return;
        }
        choice = 1;
        dlg.finish(QDialog::Accepted);
    });
    connect(folderBtn, &QPushButton::clicked, &dlg, [&] {
        const UploadPickResult c = collectUploadPick(&dlg);
        picked = c.dirs;
        if (picked.isEmpty() && !c.currentDir.isEmpty() && QFileInfo(c.currentDir).isDir())
            picked << c.currentDir;
        if (picked.isEmpty()) {
            QMessageBox::information(&dlg, QStringLiteral("上传"),
                                     QStringLiteral("请先选中一个文件夹，或进入要上传的文件夹后再点「上传文件夹」"));
            return;
        }
        choice = 2;
        dlg.finish(QDialog::Accepted);
    });
    connect(cancelBtn, &QPushButton::clicked, &dlg, [&] { dlg.finish(QDialog::Rejected); });

    if (dlg.exec() != QDialog::Accepted || choice == 0 || picked.isEmpty())
        return;
    uploadLocalFiles(picked, dest);
}

QString SftpWidget::resolvedRemoteDir(const QString &remoteDir)
{
    QString d = remoteDir.trimmed();
    if (d.isEmpty())
        d = m_currentPath;
    if (d.isEmpty())
        d = QStringLiteral(".");
    QString err;
    const QString abs = m_client.resolvePath(d, &err);
    if (!abs.isEmpty())
        return abs;
    if (d.startsWith(QLatin1Char('/')))
        return d;
    return d;
}

void SftpWidget::uploadLocalFiles(const QStringList &localPaths, const QString &remoteDir)
{
    if (!canTransfer())
        return;
    const QString dest = resolvedRemoteDir(remoteDir);
    int n = 0;
    for (const QString &p : localPaths) {
        QFileInfo fi(p);
        if (!fi.exists())
            continue;
        const QString remote = joinRemote(dest, fi.fileName());
        if (fi.isDir()) {
            enqueueDirUpload(fi.absoluteFilePath(), remote);
            ++n;
        } else if (fi.isFile()) {
            enqueueUpload(fi.absoluteFilePath(), remote, fi.fileName(), fi.size());
            ++n;
        }
    }
    if (n == 0)
        QMessageBox::warning(this, QStringLiteral("上传"), QStringLiteral("没有可上传的文件或文件夹"));
}

QString SftpWidget::downloadToTemp(const QString &remotePath, const QString &name, qint64 size)
{
    if (!canTransfer())
        return {};
    const QString safe = QFileInfo(name).fileName();
    if (safe.isEmpty() || safe == QLatin1String(".") || safe == QLatin1String(".."))
        return {};
    const QString dir = cacheDir();
    QString dest = dir + QLatin1Char('/') + safe;
    if (QFileInfo::exists(dest)) {
        const qint64 have = QFileInfo(dest).size();
        if (size > 0 && have == size)
            return dest;
        if (size > 0 && have > size)
            dest = dir + QLatin1Char('/') + QString::number(m_nextJobId) + QLatin1Char('-') + safe;
    }
    if (!saveRemoteTo(remotePath, dest, safe, size, false))
        return {};
    return dest;
}

void SftpWidget::showContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *it = m_tree->itemAt(pos);
    QMenu menu(this);
    if (!it) {
        QAction *mkdir = menu.addAction(AppIcons::get(QStringLiteral("folder")), QStringLiteral("新建文件夹"));
        QAction *newFile = menu.addAction(AppIcons::get(QStringLiteral("file")), QStringLiteral("新建文件"));
        QAction *up = menu.addAction(AppIcons::get(QStringLiteral("upload")), QStringLiteral("上传"));
        QAction *ref = menu.addAction(AppIcons::get(QStringLiteral("reconnect")), QStringLiteral("刷新"));
        QAction *copyPath = menu.addAction(AppIcons::get(QStringLiteral("copy")), QStringLiteral("复制当前路径"));
        QAction *a = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (a == mkdir)
            mkdirHere();
        else if (a == newFile)
            createFileHere();
        else if (a == up)
            uploadPicker();
        else if (a == ref)
            reload(m_currentPath);
        else if (a == copyPath)
            QApplication::clipboard()->setText(m_currentPath);
        return;
    }

    const SftpEntry e = entryFromItem(it);
    const QVector<SftpEntry> sel = selectedEntries();
    const bool multi = sel.size() > 1;
    const bool isDot = e.name == QLatin1String("..");

    QAction *actOpen = nullptr;
    QAction *actEdit = nullptr;
    QAction *actOpenDef = nullptr;
    QAction *actOpenPick = nullptr;
    QAction *actDown = nullptr;
    QAction *actUp = nullptr;
    QAction *actDel = nullptr;
    QAction *actRename = nullptr;
    QAction *actCopyPath = nullptr;
    QAction *actMkdir = nullptr;
    QAction *actNewFile = nullptr;
    QAction *actProp = nullptr;

    if (!isDot) {
        actOpen = menu.addAction(AppIcons::get(QStringLiteral("folder")), QStringLiteral("打开"));
        if (!e.isDir && !multi) {
            actEdit = menu.addAction(QStringLiteral("编辑"));
            auto *openWith = menu.addMenu(QStringLiteral("打开方式"));
            actOpenDef = openWith->addAction(QStringLiteral("默认程序"));
            actOpenPick = openWith->addAction(QStringLiteral("选择程序..."));
        }
        menu.addSeparator();
        actDown = menu.addAction(AppIcons::get(QStringLiteral("download")),
                                 multi ? QStringLiteral("下载所选") : QStringLiteral("下载"));
        if (e.isDir && !multi)
            actUp = menu.addAction(AppIcons::get(QStringLiteral("upload")), QStringLiteral("上传到此文件夹"));
        menu.addSeparator();
        if (!multi)
            actRename = menu.addAction(QStringLiteral("重命名"));
        actMkdir = menu.addAction(AppIcons::get(QStringLiteral("folder")), QStringLiteral("新建文件夹"));
        actNewFile = menu.addAction(AppIcons::get(QStringLiteral("file")), QStringLiteral("新建文件"));
        actCopyPath = menu.addAction(AppIcons::get(QStringLiteral("copy")), QStringLiteral("复制路径"));
        menu.addSeparator();
        actDel = menu.addAction(AppIcons::get(QStringLiteral("clear")), QStringLiteral("删除"));
        if (!multi)
            actProp = menu.addAction(AppIcons::get(QStringLiteral("settings")), QStringLiteral("属性"));
    } else {
        actOpen = menu.addAction(QStringLiteral("打开"));
        actMkdir = menu.addAction(AppIcons::get(QStringLiteral("folder")), QStringLiteral("新建文件夹"));
        actNewFile = menu.addAction(AppIcons::get(QStringLiteral("file")), QStringLiteral("新建文件"));
    }

    QAction *a = menu.exec(m_tree->viewport()->mapToGlobal(pos));
    if (!a)
        return;
    if (a == actOpen) {
        if (e.isDir || isDot)
            reload(e.path);
        else
            openEntry(e);
    } else if (a == actEdit) {
        editEntry(e);
    } else if (a == actOpenDef) {
        openEntry(e);
    } else if (a == actOpenPick) {
        openWithEntry(e);
    } else if (a == actDown) {
        downloadCurrent();
    } else if (a == actUp) {
        uploadPicker(e.path);
    } else if (a == actRename) {
        renameEntry(e);
    } else if (a == actMkdir) {
        mkdirHere();
    } else if (a == actNewFile) {
        createFileHere();
    } else if (a == actCopyPath) {
        QApplication::clipboard()->setText(e.path);
    } else if (a == actDel) {
        deleteEntries(multi ? sel : QVector<SftpEntry>{e});
    } else if (a == actProp) {
        showProperties(e);
    }
}

namespace {

bool isImageName(const QString &name)
{
    const QString s = name.toLower();
    const QStringList ext = {
        QStringLiteral(".png"), QStringLiteral(".jpg"), QStringLiteral(".jpeg"),
        QStringLiteral(".gif"), QStringLiteral(".bmp"), QStringLiteral(".webp"),
        QStringLiteral(".svg"), QStringLiteral(".ico"), QStringLiteral(".tif"),
        QStringLiteral(".tiff")
    };
    for (const QString &e : ext) {
        if (s.endsWith(e))
            return true;
    }
    return false;
}

QString findImageViewer()
{
    const QStringList apps = {
        QStringLiteral("eog"), QStringLiteral("eom"), QStringLiteral("gpicview"),
        QStringLiteral("gwenview"), QStringLiteral("ristretto"), QStringLiteral("viewnior"),
        QStringLiteral("deepin-image-viewer"), QStringLiteral("ukui-photos")
    };
    for (const QString &name : apps) {
        const QString p = QStandardPaths::findExecutable(name);
        if (!p.isEmpty())
            return p;
    }
    return {};
}

} // namespace

void SftpWidget::openEntry(const SftpEntry &e, const QString &app)
{
    if (e.isDir) {
        reload(e.path);
        return;
    }
    const qint64 tooLarge = 80LL * 1024 * 1024;
    const qint64 large = 15LL * 1024 * 1024;
    if (e.size > tooLarge) {
        QMessageBox::warning(this, QStringLiteral("打开"),
                             QStringLiteral("文件过大（%1），无法直接打开，请使用下载。")
                                 .arg(formatSize(e.size)));
        return;
    }
    if (e.size > large) {
        if (QMessageBox::question(this, QStringLiteral("打开"),
                                  QStringLiteral("文件较大（%1），将先下载到本地再打开。继续？")
                                      .arg(formatSize(e.size)))
            != QMessageBox::Yes)
            return;
    }

    QString useApp = app;
    if (useApp.isEmpty() && isImageName(e.name)) {
        auto &st = AppSettings::instance();
        bool useViewer = st.useImageViewer();
        if (st.askImageOpen()) {
            bool dontAsk = false;
            const bool ok = AuthPromptDialog::confirm(
                this, QStringLiteral("打开图片"),
                QStringLiteral("这是图片文件，使用看图工具打开？"),
                QStringLiteral("以后不再提示"), &dontAsk);
            if (dontAsk) {
                st.setAskImageOpen(false);
                st.setUseImageViewer(ok);
            }
            useViewer = ok;
        }
        if (useViewer) {
            useApp = findImageViewer();
            if (useApp.isEmpty())
                useApp = QStandardPaths::findExecutable(QStringLiteral("xdg-open"));
        }
    }

    const QString local = downloadToTemp(e.path, e.name, e.size);
    if (local.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("打开"), QStringLiteral("下载失败"));
        return;
    }
    if (useApp.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(local));
    else
        QProcess::startDetached(useApp, QStringList() << local);
}

void SftpWidget::editEntry(const SftpEntry &e)
{
    const qint64 tooLarge = 80LL * 1024 * 1024;
    if (e.size > tooLarge) {
        QMessageBox::warning(this, QStringLiteral("编辑"),
                             QStringLiteral("文件过大（%1），无法直接编辑，请先下载。")
                                 .arg(formatSize(e.size)));
        return;
    }
    const QStringList editors = {
        QStringLiteral("pluma"), QStringLiteral("gedit"), QStringLiteral("kate"),
        QStringLiteral("kwrite"), QStringLiteral("mousepad"), QStringLiteral("xed"),
        QStringLiteral("leafpad")
    };
    QString app;
    for (const QString &name : editors) {
        app = QStandardPaths::findExecutable(name);
        if (!app.isEmpty())
            break;
    }
    const QString local = downloadToTemp(e.path, e.name, e.size);
    if (local.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("编辑"), QStringLiteral("下载失败"));
        return;
    }
    watchEditedFile(local, e.path, e.name);
    if (app.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(local));
    else
        QProcess::startDetached(app, QStringList() << local);
}

void SftpWidget::openWithEntry(const SftpEntry &e)
{
#ifdef Q_OS_WIN
    const QString startDir = QDir::rootPath();
#else
    const QString startDir = QStringLiteral("/usr/bin");
#endif
    const QString app = QFileDialog::getOpenFileName(this, QStringLiteral("选择程序"), startDir);
    if (app.isEmpty())
        return;
    openEntry(e, app);
}

void SftpWidget::mkdirHere()
{
    if (!canTransfer())
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("新建文件夹"),
                                               QStringLiteral("文件夹名称"), QLineEdit::Normal,
                                               QString(), &ok);
    if (!ok)
        return;
    const QString n = name.trimmed();
    if (n.isEmpty() || n.contains(QLatin1Char('/'))) {
        QMessageBox::warning(this, QStringLiteral("新建文件夹"), QStringLiteral("名称无效"));
        return;
    }
    QString err;
    if (!m_client.mkdirPath(joinRemote(resolvedRemoteDir(m_currentPath), n), &err)) {
        QMessageBox::warning(this, QStringLiteral("新建文件夹"),
                             err.trimmed().isEmpty() ? QStringLiteral("创建失败") : err);
        return;
    }
    reload(m_currentPath);
}

void SftpWidget::createFileHere()
{
    if (!canTransfer())
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("新建文件"),
                                               QStringLiteral("文件名称"), QLineEdit::Normal,
                                               QString(), &ok);
    if (!ok)
        return;
    const QString n = name.trimmed();
    if (n.isEmpty() || n.contains(QLatin1Char('/'))) {
        QMessageBox::warning(this, QStringLiteral("新建文件"), QStringLiteral("名称无效"));
        return;
    }
    QString err;
    if (!m_client.createFile(joinRemote(resolvedRemoteDir(m_currentPath), n), &err)) {
        QMessageBox::warning(this, QStringLiteral("新建文件"),
                             err.trimmed().isEmpty() ? QStringLiteral("创建失败") : err);
        return;
    }
    reload(m_currentPath);
}

void SftpWidget::renameEntry(const SftpEntry &e)
{
    if (!canTransfer() || e.name == QLatin1String(".."))
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("重命名"),
                                               QStringLiteral("新名称"), QLineEdit::Normal,
                                               e.name, &ok);
    if (!ok)
        return;
    const QString n = name.trimmed();
    if (n.isEmpty() || n == e.name || n.contains(QLatin1Char('/')))
        return;
    const QString dest = joinRemote(resolvedRemoteDir(m_currentPath), n);
    QString err;
    if (!m_client.renamePath(e.path, dest, &err)) {
        QMessageBox::warning(this, QStringLiteral("重命名"),
                             err.trimmed().isEmpty() ? QStringLiteral("重命名失败") : err);
        return;
    }
    reload(m_currentPath);
}

void SftpWidget::deleteEntries(const QVector<SftpEntry> &entries)
{
    if (entries.isEmpty())
        return;
    const QString msg = entries.size() == 1
                            ? QStringLiteral("确定删除 “%1”？").arg(entries.first().name)
                            : QStringLiteral("确定删除选中的 %1 项？").arg(entries.size());
    if (QMessageBox::question(this, QStringLiteral("删除"), msg) != QMessageBox::Yes)
        return;
    QString err;
    for (const SftpEntry &e : entries) {
        if (!m_client.removePath(e.path, e.isDir, &err)) {
            QMessageBox::warning(this, QStringLiteral("删除"),
                                 err.trimmed().isEmpty() ? QStringLiteral("删除失败") : err);
            break;
        }
    }
    reload(m_currentPath);
}

void SftpWidget::showProperties(const SftpEntry &e)
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("属性 — %1").arg(e.name));
    dlg.setMinimumWidth(380);
    auto *form = new QFormLayout(&dlg);
    form->addRow(QStringLiteral("名称"), new QLabel(e.name));
    form->addRow(QStringLiteral("路径"), new QLabel(e.path));
    form->addRow(QStringLiteral("类型"), new QLabel(e.isDir ? QStringLiteral("目录") : QStringLiteral("文件")));
    if (!e.isDir)
        form->addRow(QStringLiteral("大小"), new QLabel(formatSize(e.size)));
    form->addRow(QStringLiteral("修改时间"), new QLabel(e.mtime.isEmpty() ? QStringLiteral("-") : e.mtime));
    form->addRow(QStringLiteral("当前权限"), new QLabel(e.mode.isEmpty() ? QStringLiteral("-") : e.mode));

    int mode = modeFromLs(e.mode);
    auto *box = new QGroupBox(QStringLiteral("权限"));
    auto *grid = new QGridLayout(box);
    QCheckBox *bits[9];
    const QString labs[9] = {
        QStringLiteral("属主读"), QStringLiteral("属主写"), QStringLiteral("属主执行"),
        QStringLiteral("组读"), QStringLiteral("组写"), QStringLiteral("组执行"),
        QStringLiteral("其他读"), QStringLiteral("其他写"), QStringLiteral("其他执行")
    };
    const int vals[9] = {0400, 0200, 0100, 0040, 0020, 0010, 0004, 0002, 0001};
    const QString heads[3] = {QStringLiteral("属主"), QStringLiteral("组"), QStringLiteral("其他")};
    for (int col = 0; col < 3; ++col)
        grid->addWidget(new QLabel(heads[col]), 0, col);
    for (int i = 0; i < 9; ++i) {
        bits[i] = new QCheckBox(labs[i]);
        bits[i]->setChecked(mode & vals[i]);
        grid->addWidget(bits[i], 1 + (i % 3), i / 3);
    }
    form->addRow(box);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Close);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("应用权限"));
    buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("关闭"));
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, [&] {
        int m = 0;
        for (int i = 0; i < 9; ++i) {
            if (bits[i]->isChecked())
                m |= vals[i];
        }
        const QString oct = QString::number(m, 8);
        QString err;
        if (!m_client.chmodPath(e.path, oct, &err)) {
            QMessageBox::warning(&dlg, QStringLiteral("权限"),
                                 err.trimmed().isEmpty() ? QStringLiteral("修改失败") : err);
            return;
        }
        dlg.accept();
        reload(m_currentPath);
    });
    dlg.exec();
}

void SftpWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void SftpWidget::dropEvent(QDropEvent *event)
{
    QStringList files;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile())
            files << url.toLocalFile();
    }
    uploadLocalFiles(files);
    event->acceptProposedAction();
}

void SftpWidget::handleLiveAction(int id, const QString &op)
{
    if (op == QLatin1String("pause"))
        pauseJob(id);
    else if (op == QLatin1String("resume"))
        resumeJob(id);
    else if (op == QLatin1String("retry"))
        retryJob(id);
    else if (op == QLatin1String("delete"))
        removeJob(id);
}

void SftpWidget::updateParentJob(int parentId)
{
    if (!parentId)
        return;
    SftpJob *p = jobById(parentId);
    if (!p)
        return;
    bool anyFail = false, anyRun = false, anyQ = false, anyPause = false, anyChild = false;
    for (const SftpJob &j : m_jobs) {
        if (j.parentId != parentId)
            continue;
        anyChild = true;
        if (j.state == SftpJob::Failed)
            anyFail = true;
        else if (j.state == SftpJob::Running)
            anyRun = true;
        else if (j.state == SftpJob::Queued)
            anyQ = true;
        else if (j.state == SftpJob::Paused)
            anyPause = true;
    }
    if (!anyChild)
        return;
    if (anyRun || anyQ)
        p->state = SftpJob::Running;
    else if (anyPause)
        p->state = SftpJob::Paused;
    else if (anyFail)
        p->state = SftpJob::Failed;
    else
        p->state = SftpJob::Done;
    if (p->state == SftpJob::Done || p->state == SftpJob::Failed)
        persistJob(*p);
}

void SftpWidget::pauseJob(int id)
{
    SftpJob *j = jobById(id);
    if (!j)
        return;
    if (j->isDir) {
        for (SftpJob &c : m_jobs) {
            if (c.parentId != id)
                continue;
            if (c.state == SftpJob::Queued)
                c.state = SftpJob::Paused;
            else if (c.state == SftpJob::Running)
                pauseJob(c.id);
        }
        j->state = SftpJob::Paused;
        updateProgressUi();
        return;
    }
    if (j->state == SftpJob::Queued) {
        j->state = SftpJob::Paused;
    } else if (j->state == SftpJob::Running && m_activeJobId == j->id) {
        j->state = SftpJob::Paused;
        m_client.abortTransfer();
        m_activeJobId = 0;
    }
    updateParentJob(j->parentId);
    updateProgressUi();
    pumpQueue();
}

void SftpWidget::resumeJob(int id)
{
    SftpJob *j = jobById(id);
    if (!j)
        return;
    if (j->isDir) {
        for (SftpJob &c : m_jobs) {
            if (c.parentId == id && c.state == SftpJob::Paused)
                c.state = SftpJob::Queued;
        }
        j->state = SftpJob::Running;
        updateProgressUi();
        pumpQueue();
        return;
    }
    if (j->state == SftpJob::Paused) {
        j->state = SftpJob::Queued;
        updateParentJob(j->parentId);
        updateProgressUi();
        pumpQueue();
    }
}

void SftpWidget::retryJob(int id)
{
    SftpJob *j = jobById(id);
    if (!j)
        return;
    if (j->isDir) {
        for (SftpJob &c : m_jobs) {
            if (c.parentId == id && c.state == SftpJob::Failed) {
                c.state = SftpJob::Queued;
                c.message.clear();
                c.persisted = false;
            }
        }
        j->state = SftpJob::Running;
        j->persisted = false;
        updateProgressUi();
        pumpQueue();
        return;
    }
    if (j->state == SftpJob::Failed || j->state == SftpJob::Paused) {
        j->state = SftpJob::Queued;
        j->message.clear();
        j->persisted = false;
        updateParentJob(j->parentId);
        updateProgressUi();
        pumpQueue();
    }
}

void SftpWidget::removeJob(int id)
{
    SftpJob *j = jobById(id);
    if (!j)
        return;
    if (j->isDir) {
        QVector<int> kids;
        for (const SftpJob &c : m_jobs) {
            if (c.parentId == id)
                kids.append(c.id);
        }
        for (int kid : kids)
            removeJob(kid);
    }
    if (m_activeJobId == j->id) {
        m_client.abortTransfer();
        m_activeJobId = 0;
    }
    const int parentId = j->parentId;
    for (int i = 0; i < m_jobs.size(); ++i) {
        if (m_jobs.at(i).id == id) {
            m_jobs.remove(i);
            break;
        }
    }
    updateParentJob(parentId);
    updateProgressUi();
    pumpQueue();
}

void SftpWidget::watchEditedFile(const QString &local, const QString &remote, const QString &name)
{
    if (local.isEmpty())
        return;
    EditWatch w;
    w.local = local;
    w.remote = remote;
    w.name = name;
    const QFileInfo fi(local);
    w.mtime = fi.lastModified().toMSecsSinceEpoch();
    w.size = fi.size();
    for (int i = 0; i < m_edits.size(); ++i) {
        if (m_edits.at(i).local == local) {
            m_edits[i] = w;
            if (!m_watch->files().contains(local))
                m_watch->addPath(local);
            return;
        }
    }
    m_edits.append(w);
    m_watch->addPath(local);
}

void SftpWidget::onEditedFileChanged(const QString &path)
{
    int idx = -1;
    for (int i = 0; i < m_edits.size(); ++i) {
        if (m_edits.at(i).local == path) {
            idx = i;
            break;
        }
    }
    if (idx < 0)
        return;
    QTimer::singleShot(400, this, [this, path] {
        int at = -1;
        for (int i = 0; i < m_edits.size(); ++i) {
            if (m_edits.at(i).local == path) {
                at = i;
                break;
            }
        }
        if (at < 0)
            return;
        EditWatch w = m_edits.at(at);
        const QFileInfo fi(path);
        if (!fi.exists())
            return;
        const qint64 mt = fi.lastModified().toMSecsSinceEpoch();
        if (mt == w.mtime && fi.size() == w.size)
            return;
        w.mtime = mt;
        w.size = fi.size();
        m_edits[at] = w;
        if (!m_watch->files().contains(path))
            m_watch->addPath(path);

        auto &st = AppSettings::instance();
        bool upload = st.autoUploadOnEdit();
        if (st.promptEditUpload()) {
            bool dontAsk = false;
            upload = AuthPromptDialog::confirm(
                this, QStringLiteral("保存到服务器"),
                QStringLiteral("文件 “%1” 已修改，是否上传到服务器？").arg(w.name),
                QStringLiteral("以后不再提示"), &dontAsk);
            if (dontAsk) {
                st.setPromptEditUpload(false);
                st.setAutoUploadOnEdit(upload);
            }
        }
        if (upload && canTransfer())
            enqueueUpload(w.local, w.remote, w.name, w.size);
    });
}

QString SftpWidget::cacheDir() const
{
    const QString d = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/sftp-cache");
    QDir().mkpath(d);
    return d;
}

void SftpWidget::purgeDownloadCache()
{
    const QString root = cacheDir();
    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-7);
    QDirIterator it(root, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    QStringList dirs;
    while (it.hasNext()) {
        it.next();
        const QFileInfo fi = it.fileInfo();
        if (fi.lastModified() >= cutoff)
            continue;
        if (fi.isFile())
            QFile::remove(fi.absoluteFilePath());
        else
            dirs.prepend(fi.absoluteFilePath());
    }
    for (const QString &d : dirs)
        QDir(d).rmdir(d);
}
