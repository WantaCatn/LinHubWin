#include "ui/mainwindow.h"

#include "app/application.h"
#include "app/appsettings.h"
#include "storage/historyrepository.h"
#include "storage/sessionrepository.h"
#include "ui/authpromptdialog.h"
#include "ui/composebar.h"
#include "ui/findbar.h"
#include "ui/historypanel.h"
#include "ui/icons.h"
#include "ui/quickconnectbar.h"
#include "ui/sessioneditdialog.h"
#include "ui/sessiontreewidget.h"
#include "ui/settingsdialog.h"
#include "ui/sessionpane.h"
#include "ui/sftpwidget.h"
#include "ui/startpage.h"
#include "ui/tabterminal.h"
#include "util/crypto.h"
#include "util/sshconfigimporter.h"
#include "version.h"

#include <QAction>
#include <QApplication>
#include <QSplitter>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QDockWidget>
#include <QFileDialog>
#include <QIcon>
#include <QInputDialog>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QPaintEvent>
#include <QPainter>
#include <QStyleOptionTab>
#include <QVector>
#include <QLayoutItem>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QColor>
#include <QEvent>
#include <QFont>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>

namespace {

class ToolbarButtonFilter : public QObject
{
public:
    explicit ToolbarButtonFilter(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        auto *btn = qobject_cast<QToolButton *>(watched);
        if (!btn || event->type() != QEvent::Paint)
            return QObject::eventFilter(watched, event);

        QPainter p(btn);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QSize is = btn->iconSize().isEmpty() ? QSize(22, 22) : btn->iconSize();
        const int ix = (btn->width() - is.width()) / 2;
        const int iy = 2;
        const QRect iconR(ix, iy, is.width(), is.height());
        const bool checked = btn->isChecked();
        const bool hover = btn->underMouse() && btn->isEnabled();

        if (checked) {
            const QRect bg = iconR.adjusted(-3, -2, 3, 2);
            p.setPen(QPen(QColor(147, 197, 253, 150), 1));
            p.setBrush(QColor(96, 165, 250, 48));
            p.drawRoundedRect(bg, 5, 5);
        } else if (hover) {
            const QRect bg = iconR.adjusted(-3, -2, 3, 2);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(26, 109, 255, 36));
            p.drawRoundedRect(bg, 5, 5);
        }

        btn->icon().paint(&p, iconR, Qt::AlignCenter,
                          btn->isEnabled() ? QIcon::Normal : QIcon::Disabled);

        QFont f = btn->font();
        f.setPixelSize(12);
        f.setWeight(QFont::Normal);
        f.setHintingPreference(QFont::PreferFullHinting);
        f.setStyleStrategy(QFont::PreferQuality);
        p.setFont(f);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        p.setPen(btn->palette().color(btn->isEnabled() ? QPalette::Active : QPalette::Disabled,
                                      QPalette::ButtonText));
        const QRect textR(0, iy + is.height(), btn->width(),
                          qMax(16, btn->height() - iy - is.height()));
        p.drawText(textR, Qt::AlignHCenter | Qt::AlignTop | Qt::TextSingleLine, btn->text());
        return true;
    }
};

void showAboutDialog(QWidget *parent)
{
    auto *dlg = new QDialog(parent);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(QStringLiteral("关于 LinHub"));
    dlg->resize(460, 420);
    auto *lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(12, 12, 12, 12);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *body = new QLabel;
    body->setWordWrap(true);
    body->setTextFormat(Qt::RichText);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    body->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    body->setText(QStringLiteral(
        "<h3 style='margin:0 0 8px 0;'>LinHub %1</h3>"
        "<p>面向麒麟系统的终端与文件传输管理器，用法如下。</p>"
        "<p><b>会话库（左侧）</b><br>"
        "双击会话打开标签。按住 Ctrl 点选多个后，再双击、回车或右键「打开所选会话」可一次全部打开。"
        "右键可新建、编辑、删除、收藏。顶部搜索框可过滤名称。</p>"
        "<p><b>快速连接</b><br>"
        "上方快速连接栏输入 <code>用户@地址</code> 或地址后回车即可连接。视图菜单可隐藏该栏。</p>"
        "<p><b>标签与分屏</b><br>"
        "标签显示会话名称。双击已连接标签会复制一个相同会话。"
        "标签可拖到其他分屏。工具栏可横分、竖分、四分或取消分屏；点选哪个屏，右侧 SFTP 就跟随哪个屏。</p>"
        "<p><b>终端</b><br>"
        "连接后可在设置里记住密码。断开后标签回到本地提示符，点「重新连接」再连。"
        "查找用 Ctrl+F。补全请用 Ctrl+Tab，避免和系统切换窗口冲突。</p>"
        "<p><b>SFTP（右侧）</b><br>"
        "随当前标签切换主机和目录。可打开「跟随终端」让列表跟着命令行当前目录走。"
        "拖入本地文件/文件夹上传，拖出远程文件下载。右键可打开、编辑、删除、新建文件或文件夹。"
        "下方进度条点开传输详情：可暂停、继续、重试失败项、打开文件或所在文件夹。记录保留 7 天。</p>"
        "<p><b>发送栏与历史</b><br>"
        "底部发送栏可把命令发到当前标签或全部标签。左侧历史记录以往连接，双击可再连。</p>"
        "<p><b>设置</b><br>"
        "工具 → 设置：主题、字体、断开确认、编辑保存后是否自动上传、图片打开方式等。</p>"
        "<p>使用系统 OpenSSH，连接新发行版服务器一般不会出现旧版 Xshell 的算法不匹配。</p>"
    ).arg(QStringLiteral(LINHUB_VERSION)));
    scroll->setWidget(body);
    lay->addWidget(scroll, 1);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok);
    box->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    QObject::connect(box, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    lay->addWidget(box);
    dlg->exec();
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral(LINHUB_APP_DISPLAY_NAME_ZH));
    resize(1400, 860);
    QIcon appIcon(QStringLiteral(":/icons/linhub.png"));
    if (appIcon.isNull())
        appIcon = QIcon(QStringLiteral(":/icons/linhub.svg"));
    setWindowIcon(appIcon);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_quick = new QuickConnectBar(this);
    m_splitHolder = new QWidget(this);
    m_splitHolder->setObjectName(QStringLiteral("splitHolder"));
    auto *splitLay = new QVBoxLayout(m_splitHolder);
    splitLay->setContentsMargins(0, 0, 0, 0);
    splitLay->setSpacing(0);
    m_find = new FindBar(this);
    m_compose = new ComposeBar(this);

    layout->addWidget(m_quick);
    layout->addWidget(m_splitHolder, 1);
    layout->addWidget(m_find);
    layout->addWidget(m_compose);
    setCentralWidget(central);

    auto *first = createPane();
    m_panes.append(first);
    splitLay->addWidget(first);
    setActivePane(first);

    auto *leftDock = new QDockWidget(QStringLiteral("会话库"), this);
    leftDock->setObjectName(QStringLiteral("sessionDock"));
    auto *left = new QWidget;
    auto *leftLay = new QVBoxLayout(left);
    leftLay->setContentsMargins(6, 6, 6, 6);
    m_search = new QLineEdit;
    m_search->setPlaceholderText(QStringLiteral("搜索名称 / 主机 / 标签 / 备注…"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(AppIcons::get(QStringLiteral("find")), QLineEdit::LeadingPosition);
    m_tree = new SessionTreeWidget;
    leftLay->addWidget(m_search);
    leftLay->addWidget(m_tree, 1);
    leftDock->setWidget(left);
    addDockWidget(Qt::LeftDockWidgetArea, leftDock);

    auto *histDock = new QDockWidget(QStringLiteral("连接历史"), this);
    histDock->setObjectName(QStringLiteral("historyDock"));
    m_history = new HistoryPanel;
    histDock->setWidget(m_history);
    addDockWidget(Qt::LeftDockWidgetArea, histDock);
    tabifyDockWidget(leftDock, histDock);
    leftDock->raise();

    m_sftpDock = new QDockWidget(QStringLiteral("SFTP 文件"), this);
    m_sftpDock->setObjectName(QStringLiteral("sftpDock"));
    m_sftp = new SftpWidget;
    m_sftp->setMinimumWidth(150);
    m_sftpDock->setMinimumWidth(150);
    m_sftpDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable
                            | QDockWidget::DockWidgetFloatable);
    m_sftpDock->setWidget(m_sftp);
    addDockWidget(Qt::RightDockWidgetArea, m_sftpDock);

    auto addMenuAct = [](QMenu *menu, const QIcon &icon, const QString &text,
                         const QKeySequence &ks = QKeySequence()) {
        QAction *a = menu->addAction(icon, text);
        if (!ks.isEmpty())
            a->setShortcut(ks);
        return a;
    };

    auto *fileMenu = menuBar()->addMenu(QStringLiteral("文件"));
    connect(addMenuAct(fileMenu, AppIcons::get(QStringLiteral("new")), QStringLiteral("新建会话"),
                       QKeySequence::New),
            &QAction::triggered, this, [this] { createSession(0); });
    connect(addMenuAct(fileMenu, AppIcons::get(QStringLiteral("folder")), QStringLiteral("新建分组")),
            &QAction::triggered, this, [this] { createFolder(0); });
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("导入 ~/.ssh/config"), this, [this] {
        SessionRepository repo;
        int n = 0;
        for (auto s : SshConfigImporter::fromDefault()) {
            s.notes = QStringLiteral("从 SSH config 导入");
            if (repo.insert(s))
                ++n;
        }
        m_tree->reload();
        reloadStartPages();
        QMessageBox::information(this, QStringLiteral("导入"),
                                 QStringLiteral("已导入 %1 条 SSH 配置。").arg(n));
    });
    fileMenu->addAction(QStringLiteral("导入指定 config"), this, [this] {
        const QString p = QFileDialog::getOpenFileName(this, QStringLiteral("选择 SSH config"));
        if (p.isEmpty())
            return;
        SessionRepository repo;
        int n = 0;
        for (auto s : SshConfigImporter::fromFile(p)) {
            if (repo.insert(s))
                ++n;
        }
        m_tree->reload();
        reloadStartPages();
        QMessageBox::information(this, QStringLiteral("导入"),
                                 QStringLiteral("已导入 %1 条。").arg(n));
    });
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("导出会话库…"), this, [this] { exportSessions(); });
    fileMenu->addAction(QStringLiteral("导入会话库…"), this, [this] { importSessions(); });
    fileMenu->addSeparator();
    connect(addMenuAct(fileMenu, AppIcons::get(QStringLiteral("close")), QStringLiteral("退出"),
                       QKeySequence::Quit),
            &QAction::triggered, this, &QWidget::close);

    auto *sessMenu = menuBar()->addMenu(QStringLiteral("会话"));
    connect(addMenuAct(sessMenu, AppIcons::get(QStringLiteral("local")), QStringLiteral("打开本地 Shell"),
                       QKeySequence(QStringLiteral("Ctrl+Shift+T"))),
            &QAction::triggered, this, [this] { openLocalShell(); });
    connect(addMenuAct(sessMenu, AppIcons::get(QStringLiteral("reconnect")), QStringLiteral("重新连接"),
                       QKeySequence(QStringLiteral("Ctrl+R"))),
            &QAction::triggered, this, [this] { reconnectCurrent(); });
    connect(addMenuAct(sessMenu, AppIcons::get(QStringLiteral("disconnect")), QStringLiteral("断开连接")),
            &QAction::triggered, this, [this] { disconnectCurrent(); });
    connect(addMenuAct(sessMenu, AppIcons::get(QStringLiteral("duplicate")), QStringLiteral("复制当前标签")),
            &QAction::triggered, this, [this] { duplicateCurrent(); });
    sessMenu->addAction(QStringLiteral("关闭其他标签"), this, [this] { closeOtherTabs(); });
    sessMenu->addSeparator();
    connect(addMenuAct(sessMenu, AppIcons::get(QStringLiteral("copy")), QStringLiteral("复制"),
                       QKeySequence(QStringLiteral("Ctrl+Shift+C"))),
            &QAction::triggered, this, [this] { copyCurrent(); });
    connect(addMenuAct(sessMenu, AppIcons::get(QStringLiteral("paste")), QStringLiteral("粘贴"),
                       QKeySequence(QStringLiteral("Ctrl+Shift+V"))),
            &QAction::triggered, this, [this] { pasteCurrent(); });
    sessMenu->addAction(QStringLiteral("复制全部"), this, [this] {
        if (auto *t = currentTab())
            t->terminal()->copyAll();
    });
    sessMenu->addAction(QStringLiteral("清屏"), this, [this] {
        if (auto *t = currentTab())
            t->terminal()->clearScreen();
    });
    connect(addMenuAct(sessMenu, AppIcons::get(QStringLiteral("find")), QStringLiteral("查找"),
                       QKeySequence::Find),
            &QAction::triggered, this, [this] { m_find->focusInput(); });
    sessMenu->addSeparator();
    connect(addMenuAct(sessMenu, AppIcons::get(QStringLiteral("zoom-in")), QStringLiteral("终端放大"),
                       QKeySequence(QStringLiteral("Ctrl+="))),
            &QAction::triggered, this, [this] {
                if (auto *t = currentTab()) t->terminal()->zoom(1);
            });
    connect(addMenuAct(sessMenu, AppIcons::get(QStringLiteral("zoom-out")), QStringLiteral("终端缩小"),
                       QKeySequence(QStringLiteral("Ctrl+-"))),
            &QAction::triggered, this, [this] {
                if (auto *t = currentTab()) t->terminal()->zoom(-1);
            });

    auto *viewMenu = menuBar()->addMenu(QStringLiteral("查看"));
    connect(addMenuAct(viewMenu, AppIcons::get(QStringLiteral("home")), QStringLiteral("起始页")),
            &QAction::triggered, this, [this] { showStartPage(); });
    m_actQuick = addMenuAct(viewMenu, AppIcons::get(QStringLiteral("quick")), QStringLiteral("快速连接栏"),
                            QKeySequence(QStringLiteral("Ctrl+Shift+Q")));
    m_actQuick->setCheckable(true);
    m_actCompose = addMenuAct(viewMenu, AppIcons::get(QStringLiteral("compose")), QStringLiteral("发送 / 快速命令栏"),
                              QKeySequence(QStringLiteral("Ctrl+Shift+B")));
    m_actCompose->setCheckable(true);
    connect(addMenuAct(viewMenu, AppIcons::get(QStringLiteral("fullscreen")), QStringLiteral("全屏"),
                       QKeySequence(QStringLiteral("F11"))),
            &QAction::triggered, this, [this] { toggleFullscreen(); });
    viewMenu->addSeparator();
    m_actSplitH = addMenuAct(viewMenu, AppIcons::get(QStringLiteral("split-h")), QStringLiteral("横向分屏"));
    m_actSplitV = addMenuAct(viewMenu, AppIcons::get(QStringLiteral("split-v")), QStringLiteral("纵向分屏"));
    m_actSplitQ = addMenuAct(viewMenu, AppIcons::get(QStringLiteral("split-quad")), QStringLiteral("四分屏"));
    m_actSplitNone = addMenuAct(viewMenu, AppIcons::get(QStringLiteral("split-join")), QStringLiteral("取消分屏"));
    m_actSplitH->setCheckable(true);
    m_actSplitV->setCheckable(true);
    m_actSplitQ->setCheckable(true);
    m_actSplitNone->setCheckable(true);
    connect(m_actSplitH, &QAction::triggered, this, [this] { applySplit(SplitHorizontal); });
    connect(m_actSplitV, &QAction::triggered, this, [this] { applySplit(SplitVertical); });
    connect(m_actSplitQ, &QAction::triggered, this, [this] { applySplit(SplitQuad); });
    connect(m_actSplitNone, &QAction::triggered, this, [this] { applySplit(SplitSingle); });
    viewMenu->addSeparator();
    viewMenu->addAction(leftDock->toggleViewAction());
    viewMenu->addAction(histDock->toggleViewAction());
    viewMenu->addAction(m_sftpDock->toggleViewAction());

    auto *toolMenu = menuBar()->addMenu(QStringLiteral("工具"));
    connect(addMenuAct(toolMenu, AppIcons::get(QStringLiteral("settings")), QStringLiteral("设置"),
                       QKeySequence::Preferences),
            &QAction::triggered, this, [this] { openSettings(); });

    auto *helpMenu = menuBar()->addMenu(QStringLiteral("帮助"));
    helpMenu->addAction(QStringLiteral("关于 LinHub"), this, [this] { showAboutDialog(this); });

    m_toolbar = addToolBar(QStringLiteral("主工具栏"));
    m_toolbar->setObjectName(QStringLiteral("mainToolbar"));
    m_toolbar->setMovable(false);
    m_toolbar->setIconSize(AppIcons::toolbarSize());
    m_toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    auto *actNew = tbAction(QStringLiteral("new"), QStringLiteral("新建会话"), QStringLiteral("新建"));
    connect(actNew, &QAction::triggered, this, [this] { createSession(0); });
    auto *actLocal = tbAction(QStringLiteral("local"), QStringLiteral("本地 Shell"), QStringLiteral("本地"));
    connect(actLocal, &QAction::triggered, this, [this] { openLocalShell(); });
    auto *actHome = tbAction(QStringLiteral("home"), QStringLiteral("起始页"), QStringLiteral("起始"));
    connect(actHome, &QAction::triggered, this, [this] { showStartPage(); });
    m_toolbar->addSeparator();
    m_actReconnect = tbAction(QStringLiteral("reconnect"), QStringLiteral("重新连接"), QStringLiteral("重连"));
    connect(m_actReconnect, &QAction::triggered, this, [this] { reconnectCurrent(); });
    m_actDisconnect = tbAction(QStringLiteral("disconnect"), QStringLiteral("断开连接"), QStringLiteral("断开"));
    connect(m_actDisconnect, &QAction::triggered, this, [this] { disconnectCurrent(); });
    auto *actDup = tbAction(QStringLiteral("duplicate"), QStringLiteral("复制标签"), QStringLiteral("克隆"));
    connect(actDup, &QAction::triggered, this, [this] { duplicateCurrent(); });
    m_toolbar->addSeparator();
    auto *actCopy = tbAction(QStringLiteral("copy"), QStringLiteral("复制"), QStringLiteral("复制"));
    connect(actCopy, &QAction::triggered, this, [this] { copyCurrent(); });
    auto *actPaste = tbAction(QStringLiteral("paste"), QStringLiteral("粘贴"), QStringLiteral("粘贴"));
    connect(actPaste, &QAction::triggered, this, [this] { pasteCurrent(); });
    auto *actFind = tbAction(QStringLiteral("find"), QStringLiteral("查找 (Ctrl+F)"), QStringLiteral("查找"));
    connect(actFind, &QAction::triggered, this, [this] { m_find->focusInput(); });
    m_toolbar->addSeparator();
    auto *actZoomOut = tbAction(QStringLiteral("zoom-out"), QStringLiteral("缩小"), QStringLiteral("缩小"));
    connect(actZoomOut, &QAction::triggered, this, [this] {
        if (auto *t = currentTab()) t->terminal()->zoom(-1);
    });
    auto *actZoomIn = tbAction(QStringLiteral("zoom-in"), QStringLiteral("放大"), QStringLiteral("放大"));
    connect(actZoomIn, &QAction::triggered, this, [this] {
        if (auto *t = currentTab()) t->terminal()->zoom(1);
    });
    auto *actFs = tbAction(QStringLiteral("fullscreen"), QStringLiteral("全屏 (F11)"), QStringLiteral("全屏"));
    connect(actFs, &QAction::triggered, this, [this] { toggleFullscreen(); });
    m_toolbar->addSeparator();
    m_actQuick->setIcon(AppIcons::get(QStringLiteral("quick")));
    m_actQuick->setText(QStringLiteral("快连"));
    m_actQuick->setToolTip(QStringLiteral("显示 / 隐藏快速连接栏"));
    m_toolbar->addAction(m_actQuick);
    m_actCompose->setIcon(AppIcons::get(QStringLiteral("compose")));
    m_actCompose->setText(QStringLiteral("发送"));
    m_actCompose->setToolTip(QStringLiteral("显示 / 隐藏发送栏"));
    m_toolbar->addAction(m_actCompose);
    m_actSftp = m_sftpDock->toggleViewAction();
    m_actSftp->setIcon(AppIcons::get(QStringLiteral("sftp")));
    m_actSftp->setText(QStringLiteral("SFTP"));
    m_actSftp->setToolTip(QStringLiteral("显示 / 隐藏 SFTP"));
    m_toolbar->addAction(m_actSftp);
    m_toolbar->addSeparator();
    auto *actSplitH = tbAction(QStringLiteral("split-h"), QStringLiteral("横向分屏"), QStringLiteral("横分"));
    connect(actSplitH, &QAction::triggered, this, [this] { applySplit(SplitHorizontal); });
    auto *actSplitV = tbAction(QStringLiteral("split-v"), QStringLiteral("纵向分屏"), QStringLiteral("竖分"));
    connect(actSplitV, &QAction::triggered, this, [this] { applySplit(SplitVertical); });
    auto *actSplitQ = tbAction(QStringLiteral("split-quad"), QStringLiteral("四分屏"), QStringLiteral("四分"));
    connect(actSplitQ, &QAction::triggered, this, [this] { applySplit(SplitQuad); });
    auto *actSplitN = tbAction(QStringLiteral("split-join"), QStringLiteral("取消分屏"), QStringLiteral("合屏"));
    connect(actSplitN, &QAction::triggered, this, [this] { applySplit(SplitSingle); });
    m_toolbar->addSeparator();
    auto *actSettings = tbAction(QStringLiteral("settings"), QStringLiteral("设置"), QStringLiteral("设置"));
    connect(actSettings, &QAction::triggered, this, [this] { openSettings(); });

    auto *tbFilter = new ToolbarButtonFilter(m_toolbar);
    const auto buttons = m_toolbar->findChildren<QToolButton *>();
    for (QToolButton *btn : buttons) {
        btn->setAutoRaise(true);
        btn->installEventFilter(tbFilter);
    }

    connect(m_actQuick, &QAction::toggled, this, [this](bool on) {
        m_quick->setVisible(on);
        AppSettings::instance().setShowQuickConnect(on);
    });
    connect(m_actCompose, &QAction::toggled, this, [this](bool on) {
        m_compose->setVisible(on);
        AppSettings::instance().setShowComposeBar(on);
    });

    connect(m_tree, &SessionTreeWidget::openSessionRequested, this, [this](const Session &s) {
        openSession(s);
    });
    connect(m_tree, &SessionTreeWidget::editSessionRequested, this, &MainWindow::editSession);
    connect(m_tree, &SessionTreeWidget::newSessionRequested, this, &MainWindow::createSession);
    connect(m_tree, &SessionTreeWidget::newFolderRequested, this, &MainWindow::createFolder);
    connect(m_tree, &SessionTreeWidget::sessionsMutated, this, [this] { reloadStartPages(); });
    connect(m_tree, &SessionTreeWidget::renameFolderRequested, this, &MainWindow::renameFolder);
    connect(m_tree, &SessionTreeWidget::deleteSessionRequested, this, [this](qint64 id) {
        if (QMessageBox::question(this, QStringLiteral("删除"), QStringLiteral("确定删除该会话？"))
            != QMessageBox::Yes)
            return;
        SessionRepository().remove(id);
        m_tree->reload();
        reloadStartPages();
    });
    connect(m_tree, &SessionTreeWidget::deleteFolderRequested, this, [this](qint64 id) {
        if (QMessageBox::question(this, QStringLiteral("删除"), QStringLiteral("确定删除该分组？会话将移至未分组。"))
            != QMessageBox::Yes)
            return;
        SessionRepository().removeFolder(id);
        m_tree->reload();
        reloadStartPages();
    });
    connect(m_tree, &SessionTreeWidget::favoriteToggled, this, [this](qint64 id, bool fav) {
        SessionRepository().setFavorite(id, fav);
        m_tree->reload();
        reloadStartPages();
    });
    connect(m_search, &QLineEdit::textChanged, m_tree, &SessionTreeWidget::setFilter);
    connect(m_quick, &QuickConnectBar::connectRequested, this, [this](const Session &s, const QString &pw) {
        openSession(s, pw);
    });
    connect(m_sftp, &SftpWidget::followRequested, this, &MainWindow::syncSftpFollow);
    connect(m_compose, &ComposeBar::sendRequested, this, &MainWindow::sendToCurrentOrAll);
    connect(m_compose, &ComposeBar::manageRequested, this, [this] { openSettings(); });
    connect(m_history, &HistoryPanel::reconnectRequested, this, [this](qint64 sid) {
        const Session s = SessionRepository().byId(sid);
        if (s.id)
            openSession(s);
    });
    connect(m_find, &FindBar::findNext, this, &MainWindow::findNext);
    connect(m_find, &FindBar::findPrev, this, &MainWindow::findPrev);
    connect(m_find, &FindBar::closed, this, [this] {
        if (auto *t = currentTab())
            t->terminal()->setFocus();
    });
    auto *findShortcut = new QShortcut(QKeySequence::Find, this);
    connect(findShortcut, &QShortcut::activated, this, [this] { m_find->focusInput(); });
    auto *f3 = new QShortcut(QKeySequence(Qt::Key_F3), this);
    connect(f3, &QShortcut::activated, this, &MainWindow::findNext);
    auto *sf3 = new QShortcut(QKeySequence(Qt::SHIFT + Qt::Key_F3), this);
    connect(sf3, &QShortcut::activated, this, &MainWindow::findPrev);
    auto *f11 = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(f11, &QShortcut::activated, this, &MainWindow::toggleFullscreen);

    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        if (auto *pane = paneOf(now))
            setActivePane(pane);
    });
    updateSplitActions();

    m_tree->reload();
    m_history->reload();
    applyAppearance();

    const QByteArray geo = AppSettings::instance().windowGeometry();
    if (!geo.isEmpty())
        restoreGeometry(geo);
    const QByteArray st = AppSettings::instance().windowState();
    if (!st.isEmpty())
        restoreState(st);

    statusBar()->showMessage(QStringLiteral("就绪 — 系统 OpenSSH · 会话无上限"));
    updateConnectionActions();
}

QAction *MainWindow::tbAction(const QString &icon, const QString &tip, const QString &label)
{
    QAction *a = m_toolbar->addAction(AppIcons::get(icon), label.isEmpty() ? tip : label);
    a->setToolTip(tip);
    a->setStatusTip(tip);
    return a;
}

void MainWindow::setTabLed(TabTerminal *tab)
{
    if (!tab)
        return;
    int led = 0;
    if (tab->ledState() == TabTerminal::LedGreen)
        led = 1;
    else if (tab->ledState() == TabTerminal::LedRed)
        led = 2;
    if (auto *pane = paneOf(tab))
        pane->setTabLed(tab, led);
}

void MainWindow::bindTab(TabTerminal *tab)
{
    connect(tab, &TabTerminal::activity, this, [this, tab] {
        if (tab == currentTab()) {
            tab->markSeen();
            return;
        }
        setTabLed(tab);
    });
    connect(tab, &TabTerminal::connectionChanged, this, [this, tab] {
        if (auto *pane = paneOf(tab))
            pane->setTabTitle(tab, tab->tabTitle());
        setTabLed(tab);
        updateConnectionActions();
    });
    connect(tab, &TabTerminal::muxReady, this, [this, tab] {
        if (currentTab() != tab)
            return;
        if (tab->session().protocol == Protocol::Ssh || tab->session().protocol == Protocol::Sftp)
            m_sftp->attachSession(sessionEntryTarget(tab->session()), tab->entryPassword(),
                                  tab->controlPath());
    });
}

void MainWindow::updateConnectionActions()
{
    auto *tab = currentTab();
    const bool remoteOn = tab && tab->remoteConnected();
    if (m_actReconnect)
        m_actReconnect->setEnabled(!remoteOn && tab);
    if (m_actDisconnect)
        m_actDisconnect->setEnabled(remoteOn);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    AppSettings::instance().setWindowGeometry(saveGeometry());
    AppSettings::instance().setWindowState(saveState());
    QMainWindow::closeEvent(event);
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && isMinimized())
        Application::releaseSpareResources();
}

void MainWindow::applyAppearance()
{
    const auto &st = AppSettings::instance();
    QSignalBlocker b1(m_actQuick);
    QSignalBlocker b2(m_actCompose);
    m_actQuick->setChecked(st.showQuickConnect());
    m_actCompose->setChecked(st.showComposeBar());
    m_quick->setVisible(st.showQuickConnect());
    m_compose->setVisible(st.showComposeBar());
    if (m_toolbar)
        m_toolbar->setVisible(st.showToolbar());
    applyTermFontToAll();
    if (m_sftp)
        m_sftp->refreshFileColors();
}

void MainWindow::applyTermFontToAll()
{
    const QFont f = AppSettings::instance().termFont();
    for (SessionPane *pane : m_panes) {
        for (TabTerminal *tab : pane->terminals()) {
            if (tab && tab->terminal()) {
                tab->terminal()->setTerminalFont(f);
                tab->terminal()->applyDisplaySettings();
            }
        }
    }
}

void MainWindow::openSettings()
{
    SettingsDialog dlg(this);
    connect(&dlg, &SettingsDialog::appearanceChanged, this, &MainWindow::applyAppearance);
    dlg.exec();
    m_compose->reloadCommands();
    m_history->reload();
    reloadStartPages();
}

void MainWindow::syncSftpFollow()
{
    auto *tab = currentTab();
    if (!tab || !tab->terminal())
        return;
    tab->terminal()->enableCwdReporting();
    const QString cwd = tab->terminal()->remoteCwd();
    if (!cwd.isEmpty())
        m_sftp->applyTerminalCwd(cwd);
}

void MainWindow::openLocalShell()
{
    Session s;
    s.protocol = Protocol::Local;
    s.name = QStringLiteral("本地 Shell");
    openSession(s);
}

void MainWindow::showStartPage()
{
    if (m_activePane)
        m_activePane->showStartPage();
}

void MainWindow::findNext()
{
    if (auto *t = currentTab())
        t->terminal()->findText(m_find->text(), true);
}

void MainWindow::findPrev()
{
    if (auto *t = currentTab())
        t->terminal()->findText(m_find->text(), false);
}

void MainWindow::toggleFullscreen()
{
    if (isFullScreen())
        showNormal();
    else
        showFullScreen();
}

void MainWindow::disconnectCurrent()
{
    auto *t = currentTab();
    if (!t)
        return;
    if (AppSettings::instance().confirmDisconnect()) {
        const QString host = t->session().host.isEmpty() ? t->session().displayTitle()
                                                         : t->session().host;
        bool dontAsk = false;
        if (!AuthPromptDialog::confirm(
                this, QStringLiteral("断开连接"),
                QStringLiteral("要断开 %1 吗?").arg(host),
                QStringLiteral("不再提示"), &dontAsk))
            return;
        if (dontAsk)
            AppSettings::instance().setConfirmDisconnect(false);
    }
    t->disconnectRemote();
}

void MainWindow::copyCurrent()
{
    if (auto *t = currentTab())
        t->terminal()->copySelection();
}

void MainWindow::pasteCurrent()
{
    if (auto *t = currentTab())
        t->terminal()->pasteClipboard();
}

void MainWindow::openSession(Session session, const QString &password)
{
    if (session.id) {
        const Session fresh = SessionRepository().byId(session.id);
        if (fresh.id)
            session = fresh;
    }

    const bool remote = session.protocol == Protocol::Ssh
        || session.protocol == Protocol::Sftp
        || session.protocol == Protocol::Telnet;

    if (remote && session.username.trimmed().isEmpty()) {
        QString user;
        bool rememberUser = true;
        if (!AuthPromptDialog::prompt(
                this,
                QStringLiteral("用户名"),
                QStringLiteral("连接到 %1").arg(session.displayTitle()),
                QLineEdit::Normal,
                QStringLiteral("记住用户名"),
                &user,
                &rememberUser))
            return;
        if (user.isEmpty())
            return;
        session.username = user;
        if (rememberUser && session.id) {
            SessionRepository().update(session);
            m_tree->reload();
        }
    }

    QString pw = password;
    if (pw.isEmpty())
        pw = Crypto::decrypt(session.passwordEnc);

    bool hopsChanged = false;
    for (int i = 0; i < session.jumps.size(); ++i) {
        JumpHop &hop = session.jumps[i];
        if (hop.username.trimmed().isEmpty()) {
            QString user;
            bool rememberUser = true;
            if (!AuthPromptDialog::prompt(
                    this,
                    QStringLiteral("跳板用户名"),
                    QStringLiteral("跳板 %1 (%2)").arg(i + 1).arg(hop.host),
                    QLineEdit::Normal,
                    QStringLiteral("记住用户名"),
                    &user,
                    &rememberUser))
                return;
            if (user.isEmpty())
                return;
            hop.username = user;
            hopsChanged = hopsChanged || rememberUser;
        }
        if (Crypto::decrypt(hop.passwordEnc).isEmpty()) {
            QString hopPw;
            bool rememberPw = true;
            if (!AuthPromptDialog::prompt(
                    this,
                    QStringLiteral("跳板密码"),
                    QStringLiteral("跳板 %1 (%2@%3)").arg(i + 1).arg(hop.username).arg(hop.host),
                    QLineEdit::Password,
                    QStringLiteral("记住密码"),
                    &hopPw,
                    &rememberPw))
                return;
            if (hopPw.isEmpty())
                return;
            hop.passwordEnc = Crypto::encrypt(hopPw);
            hopsChanged = hopsChanged || rememberPw;
        }
    }

    const bool needPassword = pw.isEmpty()
        && (session.protocol == Protocol::Ssh || session.protocol == Protocol::Sftp)
        && session.authType == AuthType::Password;
    if (needPassword) {
        bool rememberPw = true;
        if (!AuthPromptDialog::prompt(
                this,
                QStringLiteral("密码"),
                QStringLiteral("连接到 %1").arg(session.displayTitle()),
                QLineEdit::Password,
                QStringLiteral("记住密码"),
                &pw,
                &rememberPw))
            return;
        if (pw.isEmpty())
            return;
        if (rememberPw && session.id) {
            session.passwordEnc = Crypto::encrypt(pw);
            SessionRepository().update(session);
            m_tree->reload();
        }
    }

    if (hopsChanged && session.id)
        SessionRepository().update(session);

    if (!m_activePane)
        return;
    auto *tab = new TabTerminal(session, pw, m_activePane->tabs());
    tab->terminal()->setTerminalFont(AppSettings::instance().termFont());
    m_activePane->addSessionTab(tab, AppIcons::protocol(session.protocol), sessionTabTitle(session));
    tab->startIfNeeded();
    bindTab(tab);
    setTabLed(tab);

    ConnectionRecord rec;
    rec.sessionId = session.id;
    rec.sessionName = session.displayTitle();
    rec.protocol = session.protocol;
    rec.host = session.host;
    rec.username = session.username;
    const qint64 hid = HistoryRepository().insertStart(rec);
    tab->terminal()->setHistoryId(hid);
    if (session.id)
        SessionRepository().touchConnected(session.id);
    reloadStartPages();

    connect(tab->terminal(), &TerminalWidget::cwdChanged, this, [this, tab](const QString &cwd) {
        if (currentTab() == tab)
            m_sftp->applyTerminalCwd(cwd);
    });
    connect(tab, &TabTerminal::finished, this, [this, tab](int code) {
        HistoryRepository().finish(tab->terminal()->historyId(), code == 0,
                                   code == 0 ? QString() : QStringLiteral("exit %1").arg(code),
                                   tab->terminal()->logFile());
        m_history->reload();
        m_tree->reload();
        reloadStartPages();
        if (auto *pane = paneOf(tab))
            pane->setTabTitle(tab, tab->tabTitle());
        setTabLed(tab);
        updateConnectionActions();
    });

    if (session.protocol == Protocol::Ssh || session.protocol == Protocol::Sftp) {
        QTimer::singleShot(1200, this, [this, tab] {
            if (currentTab() != tab)
                return;
            m_sftp->attachSession(sessionEntryTarget(tab->session()), tab->entryPassword(),
                                  tab->controlPath());
        });
    }

    updateStatus();
    updateConnectionActions();
}

void MainWindow::createSession(qint64 folderId)
{
    Session s;
    s.folderId = folderId;
    s.protocol = Protocol::Ssh;
    s.port = 22;
    int result = QDialog::Rejected;
    {
        SessionEditDialog dlg(this);
        dlg.setSession(s);
        result = dlg.exec();
        if (result != QDialog::Accepted && result != SessionEditDialog::ConnectResult)
            return;
        s = dlg.session();
    }
    s.id = SessionRepository().insert(s);
    m_tree->reload();
    reloadStartPages();
    if (s.id)
        openSession(s);
}

void MainWindow::editSession(const Session &session)
{
    Session s;
    int result = QDialog::Rejected;
    {
        SessionEditDialog dlg(this);
        dlg.setSession(session);
        result = dlg.exec();
        if (result != QDialog::Accepted && result != SessionEditDialog::ConnectResult)
            return;
        s = dlg.session();
    }
    SessionRepository().update(s);
    m_tree->reload();
    reloadStartPages();
    if (result == SessionEditDialog::ConnectResult)
        openSession(s);
}

void MainWindow::createFolder(qint64 parentId)
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("新建分组"),
                                               QStringLiteral("分组名称"), QLineEdit::Normal,
                                               QStringLiteral("新分组"), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    Folder f;
    f.parentId = parentId;
    f.name = name.trimmed();
    SessionRepository().insertFolder(f);
    m_tree->reload();
}

void MainWindow::cloneSession(const Session &session)
{
    Session s = session;
    s.id = 0;
    s.name = session.displayTitle() + QStringLiteral(" 副本");
    s.id = SessionRepository().insert(s);
    m_tree->reload();
    reloadStartPages();
}

void MainWindow::renameFolder(qint64 id)
{
    SessionRepository repo;
    Folder found;
    for (const Folder &f : repo.allFolders()) {
        if (f.id == id) {
            found = f;
            break;
        }
    }
    if (!found.id)
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("重命名分组"),
                                               QStringLiteral("分组名称"), QLineEdit::Normal,
                                               found.name, &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    found.name = name.trimmed();
    repo.updateFolder(found);
    m_tree->reload();
}

void MainWindow::exportSessions()
{
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出会话库"),
                                                      QStringLiteral("linhub-sessions.json"),
                                                      QStringLiteral("JSON (*.json)"));
    if (path.isEmpty())
        return;
    const auto ans = QMessageBox::question(
        this, QStringLiteral("导出会话库"),
        QStringLiteral("是否把已保存的密码一并导出？\n选“是”时密码会以明文写入文件，请妥善保管。"),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (ans == QMessageBox::Cancel)
        return;
    QString err;
    if (!SessionRepository().exportToFile(path, ans == QMessageBox::Yes, &err)) {
        QMessageBox::warning(this, QStringLiteral("导出"),
                             err.isEmpty() ? QStringLiteral("导出失败") : err);
        return;
    }
    QMessageBox::information(this, QStringLiteral("导出"), QStringLiteral("已导出到：\n%1").arg(path));
}

void MainWindow::importSessions()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("导入会话库"),
                                                      QString(), QStringLiteral("JSON (*.json)"));
    if (path.isEmpty())
        return;
    QString err;
    const int n = SessionRepository().importFromFile(path, &err);
    if (n < 0) {
        QMessageBox::warning(this, QStringLiteral("导入"),
                             err.isEmpty() ? QStringLiteral("导入失败") : err);
        return;
    }
    m_tree->reload();
    reloadStartPages();
    QMessageBox::information(this, QStringLiteral("导入"), QStringLiteral("已导入 %1 条会话。").arg(n));
}

TabTerminal *MainWindow::currentTab() const
{
    return m_activePane ? m_activePane->currentTerminal() : nullptr;
}

SessionPane *MainWindow::paneOf(QWidget *w) const
{
    if (!w)
        return nullptr;
    for (SessionPane *pane : m_panes) {
        if (pane && pane->contains(w))
            return pane;
    }
    return nullptr;
}

void MainWindow::reconnectCurrent()
{
    auto *tab = currentTab();
    if (!tab || tab->remoteConnected())
        return;
    tab->reconnect();
    if (tab->session().protocol == Protocol::Ssh || tab->session().protocol == Protocol::Sftp)
        m_sftp->attachSession(sessionEntryTarget(tab->session()), tab->entryPassword(), tab->controlPath());
    if (auto *pane = paneOf(tab))
        pane->setTabTitle(tab, tab->tabTitle());
    setTabLed(tab);
    updateConnectionActions();
}

void MainWindow::duplicateCurrent()
{
    auto *tab = currentTab();
    if (!tab)
        return;
    openSession(tab->session());
}

void MainWindow::closeOtherTabs()
{
    if (m_activePane)
        m_activePane->closeOtherTabs();
    updateStatus();
}

void MainWindow::sendToCurrentOrAll(const QString &command, bool toAll)
{
    QString cmd = command;
    if (cmd.isEmpty())
        return;
    auto sendOne = [&](TabTerminal *tab) {
        if (tab && tab->terminal())
            tab->terminal()->sendText(cmd, true);
    };
    if (toAll) {
        for (SessionPane *pane : m_panes) {
            for (TabTerminal *tab : pane->terminals())
                sendOne(tab);
        }
    } else {
        sendOne(currentTab());
    }
}

void MainWindow::updateStatus()
{
    int sessions = 0;
    for (SessionPane *pane : m_panes)
        sessions += pane->terminals().size();
    QString extra;
    if (auto *tab = currentTab()) {
        const Session s = tab->session();
        extra = QStringLiteral("    %1    %2×%3")
                    .arg(s.displayTitle())
                    .arg(tab->terminal()->cols())
                    .arg(tab->terminal()->rows());
    } else if (m_activePane && m_activePane->isShowingStartPage()) {
        extra = QStringLiteral("    起始页");
    }
    statusBar()->showMessage(QStringLiteral("会话 %1    会话库 %2    历史 %3%4")
                                 .arg(sessions)
                                 .arg(SessionRepository().allSessions().size())
                                 .arg(HistoryRepository().count())
                                 .arg(extra));
}

void MainWindow::reloadStartPages()
{
    for (SessionPane *pane : m_panes) {
        if (pane && pane->startPage())
            pane->startPage()->reload();
    }
}

SessionPane *MainWindow::createPane()
{
    auto *pane = new SessionPane(m_splitHolder ? m_splitHolder : this);
    wirePane(pane);
    return pane;
}

void MainWindow::wirePane(SessionPane *pane)
{
    connect(pane, &SessionPane::activated, this, [this, pane] { setActivePane(pane); });
    connect(pane, &SessionPane::currentChanged, this, [this, pane] {
        setActivePane(pane);
        updateStatus();
        updateConnectionActions();
        if (auto *t = pane->currentTerminal()) {
            t->markSeen();
            setTabLed(t);
            t->terminal()->setFocus(Qt::OtherFocusReason);
            if (t->session().protocol == Protocol::Ssh || t->session().protocol == Protocol::Sftp) {
                QTimer::singleShot(1200, this, [this, pane] {
                    if (m_activePane != pane)
                        return;
                    if (auto *cur = pane->currentTerminal()) {
                        if (cur->session().protocol == Protocol::Ssh
                            || cur->session().protocol == Protocol::Sftp)
                            m_sftp->attachSession(sessionEntryTarget(cur->session()),
                                                  cur->entryPassword(), cur->controlPath());
                    }
                });
            }
        }
        if (m_sftp->followTerminal())
            syncSftpFollow();
    });
    connect(pane, &SessionPane::sessionDropRequested, this, [this, pane](qint64 id) {
        setActivePane(pane);
        const Session s = SessionRepository().byId(id);
        if (s.id)
            openSession(s);
    });
    connect(pane, &SessionPane::startOpenSessionId, this, [this, pane](qint64 id) {
        setActivePane(pane);
        const Session s = SessionRepository().byId(id);
        if (s.id)
            openSession(s);
    });
    connect(pane, &SessionPane::startNewSsh, this, [this, pane] {
        setActivePane(pane);
        createSession(0);
    });
    connect(pane, &SessionPane::startLocalShell, this, [this, pane] {
        setActivePane(pane);
        openLocalShell();
    });
    connect(pane, &SessionPane::tabClosed, this, [this] { updateStatus(); });
    connect(pane, &SessionPane::tabDuplicateRequested, this, [this, pane](TabTerminal *tab) {
        if (!tab)
            return;
        setActivePane(pane);
        openSession(tab->session(), tab->password());
    });
    connect(pane, &SessionPane::tabTransferred, this, [this, pane](TabTerminal *tab) {
        setActivePane(pane);
        for (SessionPane *p : m_panes) {
            for (TabTerminal *t : p->terminals())
                setTabLed(t);
        }
        if (tab)
            setTabLed(tab);
        updateStatus();
        updateConnectionActions();
    });
    connect(pane, &SessionPane::tabContextMenu, this, [this, pane](const QPoint &pos) {
        setActivePane(pane);
        const int index = pane->tabBar()->tabAt(pos);
        if (index < 0)
            return;
        pane->setCurrentIndex(index);
        QMenu menu(this);
        QAction *re = menu.addAction(AppIcons::get(QStringLiteral("reconnect")), QStringLiteral("重新连接"));
        connect(re, &QAction::triggered, this, [this] { reconnectCurrent(); });
        QAction *dis = menu.addAction(AppIcons::get(QStringLiteral("disconnect")), QStringLiteral("断开连接"));
        connect(dis, &QAction::triggered, this, [this] { disconnectCurrent(); });
        QAction *dup = menu.addAction(AppIcons::get(QStringLiteral("duplicate")), QStringLiteral("复制标签"));
        connect(dup, &QAction::triggered, this, [this] { duplicateCurrent(); });
        menu.addSeparator();
        QAction *cls = menu.addAction(AppIcons::get(QStringLiteral("close")), QStringLiteral("关闭"));
        connect(cls, &QAction::triggered, this, [this, pane, index] { pane->closeTabAt(index); });
        menu.addAction(QStringLiteral("关闭其他"), this, [this] { closeOtherTabs(); });
        menu.exec(pane->tabBar()->mapToGlobal(pos));
    });
}

void MainWindow::syncSftpToPane(SessionPane *pane)
{
    if (!m_sftp || !pane)
        return;
    auto *tab = pane->currentTerminal();
    if (tab && tab->remoteConnected()
        && (tab->session().protocol == Protocol::Ssh || tab->session().protocol == Protocol::Sftp)) {
        m_sftp->attachSession(sessionEntryTarget(tab->session()), tab->entryPassword(), tab->controlPath());
        if (m_sftp->followTerminal())
            syncSftpFollow();
        return;
    }
    m_sftp->detachSession();
}

void MainWindow::setActivePane(SessionPane *pane)
{
    if (!pane)
        return;
    const bool splitOn = m_panes.size() > 1;
    const bool changed = (pane != m_activePane);
    m_activePane = pane;
    m_startPage = pane->startPage();
    for (SessionPane *p : m_panes)
        p->setActive(p == pane, splitOn);
    if (changed) {
        syncSftpToPane(pane);
        updateStatus();
        updateConnectionActions();
    }
}

void MainWindow::applySplit(SplitMode mode)
{
    int need = 1;
    if (mode == SplitHorizontal || mode == SplitVertical)
        need = 2;
    else if (mode == SplitQuad)
        need = 4;

    while (m_panes.size() < need)
        m_panes.append(createPane());

    while (m_panes.size() > need) {
        SessionPane *extra = m_panes.takeLast();
        extra->moveSessionTabsTo(m_panes.first());
        if (m_activePane == extra)
            m_activePane = m_panes.first();
        extra->hide();
        extra->deleteLater();
    }

    m_splitMode = mode;
    rebuildSplitLayout();
    auto *focus = m_activePane ? m_activePane : m_panes.first();
    m_activePane = nullptr;
    setActivePane(focus);
    for (SessionPane *p : m_panes) {
        for (TabTerminal *t : p->terminals())
            setTabLed(t);
    }
    updateSplitActions();
    updateStatus();
}

void MainWindow::rebuildSplitLayout()
{
    if (!m_splitHolder)
        return;
    auto *lay = qobject_cast<QVBoxLayout *>(m_splitHolder->layout());
    if (!lay)
        return;

    for (SessionPane *p : m_panes)
        p->setParent(nullptr);
    if (m_splitRoot) {
        m_splitRoot->hide();
        m_splitRoot->deleteLater();
        m_splitRoot = nullptr;
    }
    QLayoutItem *item = nullptr;
    while ((item = lay->takeAt(0)) != nullptr)
        delete item;

    const int n = m_panes.size();
    if (n <= 1) {
        lay->addWidget(m_panes.first());
        m_panes.first()->show();
        return;
    }
    if (n == 2) {
        auto *sp = new QSplitter(m_splitMode == SplitHorizontal ? Qt::Vertical : Qt::Horizontal, m_splitHolder);
        sp->setChildrenCollapsible(false);
        sp->addWidget(m_panes.at(0));
        sp->addWidget(m_panes.at(1));
        sp->setStretchFactor(0, 1);
        sp->setStretchFactor(1, 1);
        lay->addWidget(sp);
        m_splitRoot = sp;
        return;
    }

    auto *outer = new QSplitter(Qt::Vertical, m_splitHolder);
    auto *top = new QSplitter(Qt::Horizontal, outer);
    auto *bot = new QSplitter(Qt::Horizontal, outer);
    outer->setChildrenCollapsible(false);
    top->setChildrenCollapsible(false);
    bot->setChildrenCollapsible(false);
    top->addWidget(m_panes.at(0));
    top->addWidget(m_panes.at(1));
    bot->addWidget(m_panes.at(2));
    bot->addWidget(m_panes.at(3));
    outer->addWidget(top);
    outer->addWidget(bot);
    outer->setStretchFactor(0, 1);
    outer->setStretchFactor(1, 1);
    lay->addWidget(outer);
    m_splitRoot = outer;
}

void MainWindow::updateSplitActions()
{
    if (m_actSplitH)
        m_actSplitH->setChecked(m_splitMode == SplitHorizontal);
    if (m_actSplitV)
        m_actSplitV->setChecked(m_splitMode == SplitVertical);
    if (m_actSplitQ)
        m_actSplitQ->setChecked(m_splitMode == SplitQuad);
    if (m_actSplitNone)
        m_actSplitNone->setChecked(m_splitMode == SplitSingle);
}
