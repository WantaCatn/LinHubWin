#include "ui/sessioneditdialog.h"
#include "storage/sessionrepository.h"
#include "terminal/cell.h"
#include "ui/icons.h"
#include "util/crypto.h"
#include "util/qtcompat.h"

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QEvent>
#include <QWheelEvent>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QHeaderView>
#include <QGuiApplication>
#include <QScreen>
#include <QScrollArea>
#include <QSize>
#include <QSpinBox>
#include <QAbstractItemView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QtGlobal>

namespace {

QWidget *wrapForm(QFormLayout *form)
{
    auto *inner = new QWidget;
    inner->setLayout(form);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(inner);
    return scroll;
}

} // namespace

SessionEditDialog::SessionEditDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("会话属性"));
    setModal(true);
    setMinimumSize(460, 360);
    resize(520, 420);
    setSizeGripEnabled(true);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 8);
    root->setSpacing(8);

    auto *tabs = new QTabWidget;

    m_name = new QLineEdit;
    m_protocol = new QComboBox;
    m_protocol->addItem(AppIcons::protocol(Protocol::Ssh), QStringLiteral("SSH"), static_cast<int>(Protocol::Ssh));
    m_protocol->addItem(AppIcons::protocol(Protocol::Sftp), QStringLiteral("SFTP"), static_cast<int>(Protocol::Sftp));
    m_protocol->addItem(AppIcons::protocol(Protocol::Telnet), QStringLiteral("Telnet"), static_cast<int>(Protocol::Telnet));
    m_protocol->addItem(AppIcons::protocol(Protocol::Local), QStringLiteral("本地 Shell"), static_cast<int>(Protocol::Local));
    m_protocol->addItem(AppIcons::protocol(Protocol::Serial), QStringLiteral("串口"), static_cast<int>(Protocol::Serial));
    m_protocol->setIconSize(QSize(16, 16));
    m_folder = new QComboBox;
    m_folder->addItem(QStringLiteral("未分组"), qint64(0));
    for (const auto &f : SessionRepository().allFolders())
        m_folder->addItem(f.name, f.id);
    m_host = new QLineEdit;
    m_port = new QSpinBox;
    m_port->setRange(1, 65535);
    m_port->setValue(22);
    m_user = new QLineEdit;
    m_user->setPlaceholderText(QStringLiteral("留空则连接时询问"));
    m_auth = new QComboBox;
    m_auth->addItem(QStringLiteral("密码"), static_cast<int>(AuthType::Password));
    m_auth->addItem(QStringLiteral("私钥"), static_cast<int>(AuthType::Key));
    m_auth->addItem(QStringLiteral("ssh-agent"), static_cast<int>(AuthType::Agent));
    m_auth->addItem(QStringLiteral("交互式"), static_cast<int>(AuthType::Interactive));
    m_password = new QLineEdit;
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText(QStringLiteral("留空则连接时询问"));
    m_savePassword = new QCheckBox(QStringLiteral("记住密码（加密保存在本机会话库）"));
    m_savePassword->setChecked(true);
    m_keyPath = new QLineEdit;
    auto *keyRow = new QWidget;
    auto *keyLay = new QHBoxLayout(keyRow);
    keyLay->setContentsMargins(0, 0, 0, 0);
    keyLay->addWidget(m_keyPath);
    auto *browse = new QPushButton(QStringLiteral("浏览"));
    browse->setProperty("secondary", true);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString p = QFileDialog::getOpenFileName(this, QStringLiteral("选择私钥"),
                                                       QDir::homePath() + QStringLiteral("/.ssh"));
        if (!p.isEmpty())
            m_keyPath->setText(p);
    });
    keyLay->addWidget(browse);
    m_tags = new QLineEdit;
    m_tags->setPlaceholderText(QStringLiteral("生产,核心  逗号分隔"));
    m_favorite = new QCheckBox(QStringLiteral("收藏"));

    auto *basic = new QFormLayout;
    basic->setSpacing(6);
    basic->addRow(QStringLiteral("显示名称"), m_name);
    basic->addRow(QStringLiteral("协议"), m_protocol);
    basic->addRow(QStringLiteral("分组"), m_folder);
    basic->addRow(QStringLiteral("主机"), m_host);
    basic->addRow(QStringLiteral("端口"), m_port);
    basic->addRow(QStringLiteral("用户名"), m_user);
    basic->addRow(QStringLiteral("认证"), m_auth);
    basic->addRow(QStringLiteral("密码"), m_password);
    basic->addRow(QString(), m_savePassword);
    basic->addRow(QStringLiteral("私钥"), keyRow);
    basic->addRow(QStringLiteral("标签"), m_tags);
    basic->addRow(QString(), m_favorite);
    tabs->addTab(wrapForm(basic), QStringLiteral("常规"));

    m_jumps = new QTableWidget;
    m_jumps->setColumnCount(4);
    m_jumps->setHorizontalHeaderLabels({QStringLiteral("主机"), QStringLiteral("端口"),
                                        QStringLiteral("用户名"), QStringLiteral("密码")});
    m_jumps->horizontalHeader()->setStretchLastSection(true);
    m_jumps->setColumnWidth(0, 140);
    m_jumps->setColumnWidth(1, 56);
    m_jumps->setColumnWidth(2, 90);
    m_jumps->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_jumps->setMaximumHeight(140);
    auto *jumpBtns = new QWidget;
    auto *jumpLay = new QHBoxLayout(jumpBtns);
    jumpLay->setContentsMargins(0, 0, 0, 0);
    auto *addJump = new QPushButton(QStringLiteral("添加跳板"));
    addJump->setProperty("secondary", true);
    auto *delJump = new QPushButton(QStringLiteral("删除跳板"));
    delJump->setProperty("secondary", true);
    jumpLay->addWidget(addJump);
    jumpLay->addWidget(delJump);
    jumpLay->addStretch();
    connect(addJump, &QPushButton::clicked, this, [this] { addJumpRow(); });
    connect(delJump, &QPushButton::clicked, this, [this] {
        const int r = m_jumps->currentRow();
        if (r >= 0)
            m_jumps->removeRow(r);
    });
    m_extra = new QLineEdit;
    m_startup = new QLineEdit;
    m_keepAlive = new QSpinBox;
    m_keepAlive->setRange(0, 3600);
    m_keepAlive->setValue(60);
    m_keepAlive->setSuffix(QStringLiteral(" 秒"));
    m_x11 = new QCheckBox(QStringLiteral("X11 转发"));
    m_log = new QCheckBox(QStringLiteral("记录会话日志"));
    m_log->setChecked(true);
    m_sshCompat = new QComboBox;
    m_sshCompat->addItem(QStringLiteral("自动（系统 OpenSSH，推荐）"), QStringLiteral("auto"));
    m_sshCompat->addItem(QStringLiteral("兼容旧设备（老 sshd / 交换机）"), QStringLiteral("legacy"));
    m_sshCompat->addItem(QStringLiteral("仅现代算法"), QStringLiteral("modern"));
    m_serial = new QLineEdit;
#ifdef Q_OS_WIN
    m_serial->setPlaceholderText(QStringLiteral("COM3"));
#else
    m_serial->setPlaceholderText(QStringLiteral("/dev/ttyUSB0"));
#endif
    m_baud = new QSpinBox;
    m_baud->setRange(1200, 4000000);
    m_baud->setValue(115200);

    auto *hint = new QLabel(QStringLiteral(
        "LinHub 调用系统 OpenSSH，不是内置老 SSH 库。连接新发行版服务器一般不会出现 Xshell 那种 KEX 算法不匹配。\n"
        "只有连老设备谈不拢时，才需要改成「兼容旧设备」。"));
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#8b9cb3; font-size:13px;"));

    auto *ssh = new QFormLayout;
    ssh->setSpacing(6);
    ssh->addRow(QStringLiteral("算法兼容"), m_sshCompat);
    ssh->addRow(QString(), hint);
    auto *jumpHint = new QLabel(QStringLiteral(
        "从上到下依次跳转。连接时先登录第一台跳板，再在终端自动输入后续 ssh 及密码。"));
    jumpHint->setWordWrap(true);
    jumpHint->setStyleSheet(QStringLiteral("color:#8b9cb3; font-size:13px;"));
    ssh->addRow(QStringLiteral("SSH 跳转"), m_jumps);
    ssh->addRow(QString(), jumpBtns);
    ssh->addRow(QString(), jumpHint);
    m_forwards = new QTableWidget;
    m_forwards->setColumnCount(5);
    m_forwards->setHorizontalHeaderLabels({QStringLiteral("类型"), QStringLiteral("绑定地址"),
                                           QStringLiteral("端口"), QStringLiteral("目标主机"),
                                           QStringLiteral("目标端口")});
    m_forwards->horizontalHeader()->setStretchLastSection(true);
    m_forwards->setColumnWidth(0, 88);
    m_forwards->setColumnWidth(1, 100);
    m_forwards->setColumnWidth(2, 56);
    m_forwards->setColumnWidth(3, 110);
    m_forwards->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_forwards->setMaximumHeight(130);
    auto *fwBtns = new QWidget;
    auto *fwLay = new QHBoxLayout(fwBtns);
    fwLay->setContentsMargins(0, 0, 0, 0);
    auto *addFw = new QPushButton(QStringLiteral("添加转发"));
    addFw->setProperty("secondary", true);
    auto *delFw = new QPushButton(QStringLiteral("删除转发"));
    delFw->setProperty("secondary", true);
    fwLay->addWidget(addFw);
    fwLay->addWidget(delFw);
    fwLay->addStretch();
    connect(addFw, &QPushButton::clicked, this, [this] { addForwardRow(); });
    connect(delFw, &QPushButton::clicked, this, [this] {
        const int r = m_forwards->currentRow();
        if (r >= 0)
            m_forwards->removeRow(r);
    });
    auto *fwHint = new QLabel(QStringLiteral(
        "本地转发(-L)：本机端口转到远程主机。远程转发(-R)：远程端口转到本机。动态(-D)：SOCKS 代理。"));
    fwHint->setWordWrap(true);
    fwHint->setStyleSheet(QStringLiteral("color:#8b9cb3; font-size:13px;"));
    ssh->addRow(QStringLiteral("端口转发"), m_forwards);
    ssh->addRow(QString(), fwBtns);
    ssh->addRow(QString(), fwHint);
    ssh->addRow(QStringLiteral("额外参数"), m_extra);
    ssh->addRow(QStringLiteral("登录后命令"), m_startup);
    ssh->addRow(QStringLiteral("保活"), m_keepAlive);
    ssh->addRow(QString(), m_x11);
    ssh->addRow(QString(), m_log);
    ssh->addRow(QStringLiteral("串口设备"), m_serial);
    ssh->addRow(QStringLiteral("波特率"), m_baud);
    tabs->addTab(wrapForm(ssh), QStringLiteral("SSH / 串口"));

    m_scheme = new QComboBox;
    m_scheme->addItems(ColorScheme::names());
    m_notes = new QPlainTextEdit;
    m_notes->setPlaceholderText(QStringLiteral("备注、资产编号、责任人"));
    m_notes->setMinimumHeight(80);

    auto *more = new QFormLayout;
    more->addRow(QStringLiteral("终端配色"), m_scheme);
    more->addRow(QStringLiteral("备注"), m_notes);
    tabs->addTab(wrapForm(more), QStringLiteral("外观 / 备注"));

    root->addWidget(tabs, 1);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    box->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    auto *connectBtn = box->addButton(QStringLiteral("连接"), QDialogButtonBox::ActionRole);
    connectBtn->setDefault(true);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(connectBtn, &QPushButton::clicked, this, [this] { done(ConnectResult); });
    root->addWidget(box);

    connect(m_protocol, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this](int) { syncProtocolUi(); });
    syncProtocolUi();

    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect ag = screen->availableGeometry();
        if (height() > ag.height() - 80)
            resize(width(), qMax(360, ag.height() - 80));
    }

    const auto combos = findChildren<QComboBox *>();
    for (QComboBox *cb : combos)
        guardWheel(cb);
    const auto spins = findChildren<QAbstractSpinBox *>();
    for (QAbstractSpinBox *sp : spins)
        guardWheel(sp);
}

SessionEditDialog::~SessionEditDialog() = default;

void SessionEditDialog::guardWheel(QWidget *w)
{
    if (!w)
        return;
    w->installEventFilter(this);
    const auto kids = w->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *k : kids)
        k->installEventFilter(this);
}

bool SessionEditDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::Wheel)
        return QDialog::eventFilter(watched, event);

    auto *w = qobject_cast<QWidget *>(watched);
    if (!w || (w != this && !isAncestorOf(w)))
        return QDialog::eventFilter(watched, event);

    QComboBox *combo = nullptr;
    QAbstractSpinBox *spin = nullptr;
    for (QWidget *p = w; p && p != this; p = p->parentWidget()) {
        combo = qobject_cast<QComboBox *>(p);
        spin = qobject_cast<QAbstractSpinBox *>(p);
        if (combo || spin)
            break;
    }
    if (combo && combo->view() && combo->view()->isVisible())
        return QDialog::eventFilter(watched, event);
    if (!combo && !spin)
        return QDialog::eventFilter(watched, event);

    QWidget *viewport = nullptr;
    for (QWidget *p = w; p; p = p->parentWidget()) {
        if (auto *sa = qobject_cast<QScrollArea *>(p)) {
            viewport = sa->viewport();
            break;
        }
    }
    if (viewport) {
        auto *we = static_cast<QWheelEvent *>(event);
        const QPoint gpos = we->globalPosition().toPoint();
        QWheelEvent copy(viewport->mapFromGlobal(gpos),
                         gpos,
                         we->pixelDelta(),
                         we->angleDelta(),
                         we->buttons(),
                         we->modifiers(),
                         we->phase(),
                         we->inverted());
        QCoreApplication::sendEvent(viewport, &copy);
    }
    return true;
}

void SessionEditDialog::syncProtocolUi()
{
    const auto p = static_cast<Protocol>(m_protocol->currentData().toInt());
    const bool net = (p == Protocol::Ssh || p == Protocol::Telnet || p == Protocol::Sftp);
    const bool ssh = (p == Protocol::Ssh || p == Protocol::Sftp);
    const bool serial = (p == Protocol::Serial);
    m_host->setEnabled(net);
    m_port->setEnabled(net);
    m_user->setEnabled(ssh || p == Protocol::Telnet);
    m_auth->setEnabled(ssh);
    m_password->setEnabled(ssh);
    m_savePassword->setEnabled(ssh);
    m_keyPath->setEnabled(ssh);
    m_jumps->setEnabled(ssh);
    if (m_forwards)
        m_forwards->setEnabled(ssh);
    m_x11->setEnabled(p == Protocol::Ssh);
    m_sshCompat->setEnabled(ssh);
    m_serial->setEnabled(serial);
    m_baud->setEnabled(serial);
    if (p == Protocol::Ssh && m_port->value() == 23)
        m_port->setValue(22);
    if (p == Protocol::Telnet && m_port->value() == 22)
        m_port->setValue(23);
}

void SessionEditDialog::setSession(const Session &session)
{
    m_original = session;
    m_name->setText(session.name);
    const int pi = m_protocol->findData(static_cast<int>(session.protocol));
    if (pi >= 0)
        m_protocol->setCurrentIndex(pi);
    const int fi = m_folder->findData(session.folderId);
    if (fi >= 0)
        m_folder->setCurrentIndex(fi);
    m_host->setText(session.host);
    m_port->setValue(session.port > 0 ? session.port : 22);
    m_user->setText(session.username);
    const int ai = m_auth->findData(static_cast<int>(session.authType));
    if (ai >= 0)
        m_auth->setCurrentIndex(ai);
    m_password->setText(Crypto::decrypt(session.passwordEnc));
    m_savePassword->setChecked(true);
    m_keyPath->setText(session.keyPath);
    m_jumps->setRowCount(0);
    QVector<JumpHop> hops = session.jumps;
    if (hops.isEmpty() && !session.jumpHost.trimmed().isEmpty()) {
        JumpHop h;
        const QString jh = session.jumpHost.trimmed();
        const int at = jh.indexOf(QLatin1Char('@'));
        if (at > 0) {
            h.username = jh.left(at);
            h.host = jh.mid(at + 1);
        } else {
            h.host = jh;
        }
        hops.append(h);
    }
    for (const JumpHop &h : hops)
        addJumpRow(h);
    m_forwards->setRowCount(0);
    for (const PortForward &f : session.forwards)
        addForwardRow(f);
    m_extra->setText(session.extraArgs);
    m_startup->setText(session.startupCommand);
    m_tags->setText(session.tags.join(QLatin1Char(',')));
    const int si = m_scheme->findText(session.colorScheme);
    if (si >= 0)
        m_scheme->setCurrentIndex(si);
    const int ci = m_sshCompat->findData(session.sshCompat.isEmpty() ? QStringLiteral("auto") : session.sshCompat);
    if (ci >= 0)
        m_sshCompat->setCurrentIndex(ci);
    m_x11->setChecked(session.x11Forward);
    m_log->setChecked(session.logEnabled);
    m_favorite->setChecked(session.favorite);
    m_keepAlive->setValue(session.keepAlive);
    m_serial->setText(session.serialDevice);
    m_baud->setValue(session.serialBaud);
    m_notes->setPlainText(session.notes);
    syncProtocolUi();
}

Session SessionEditDialog::session() const
{
    Session s = m_original;
    s.name = m_name->text().trimmed();
    s.protocol = static_cast<Protocol>(m_protocol->currentData().toInt());
    s.folderId = m_folder->currentData().toLongLong();
    s.host = m_host->text().trimmed();
    s.port = m_port->value();
    s.username = m_user->text().trimmed();
    s.authType = static_cast<AuthType>(m_auth->currentData().toInt());
    const QString pw = m_password->text();
    if (m_savePassword->isChecked()) {
        if (!pw.isEmpty())
            s.passwordEnc = Crypto::encrypt(pw);
    } else {
        s.passwordEnc.clear();
    }
    s.keyPath = m_keyPath->text().trimmed();
    s.jumps = hopsFromTable();
    if (!s.jumps.isEmpty()) {
        const JumpHop &h = s.jumps.first();
        s.jumpHost = h.username.isEmpty() ? h.host : (h.username + QLatin1Char('@') + h.host);
    } else {
        s.jumpHost.clear();
    }
    s.forwards = forwardsFromTable();
    s.extraArgs = m_extra->text().trimmed();
    s.startupCommand = m_startup->text().trimmed();
    s.tags = splitSkipEmpty(m_tags->text(), QLatin1Char(','));
    for (QString &t : s.tags)
        t = t.trimmed();
    s.colorScheme = m_scheme->currentText();
    s.sshCompat = m_sshCompat->currentData().toString();
    s.x11Forward = m_x11->isChecked();
    s.logEnabled = m_log->isChecked();
    s.favorite = m_favorite->isChecked();
    s.keepAlive = m_keepAlive->value();
    s.serialDevice = m_serial->text().trimmed();
    s.serialBaud = m_baud->value();
    s.notes = m_notes->toPlainText();
    if (s.name.isEmpty())
        s.name = s.displayTitle();
    return s;
}

void SessionEditDialog::addJumpRow(const JumpHop &hop)
{
    const int r = m_jumps->rowCount();
    m_jumps->insertRow(r);
    m_jumps->setItem(r, 0, new QTableWidgetItem(hop.host));
    m_jumps->setItem(r, 1, new QTableWidgetItem(QString::number(hop.port > 0 ? hop.port : 22)));
    m_jumps->setItem(r, 2, new QTableWidgetItem(hop.username));
    auto *pw = new QLineEdit;
    pw->setEchoMode(QLineEdit::Password);
    pw->setText(Crypto::decrypt(hop.passwordEnc));
    m_jumps->setCellWidget(r, 3, pw);
}

QVector<JumpHop> SessionEditDialog::hopsFromTable() const
{
    QVector<JumpHop> hops;
    for (int r = 0; r < m_jumps->rowCount(); ++r) {
        JumpHop h;
        h.host = m_jumps->item(r, 0) ? m_jumps->item(r, 0)->text().trimmed() : QString();
        h.port = m_jumps->item(r, 1) ? m_jumps->item(r, 1)->text().toInt() : 22;
        if (h.port <= 0)
            h.port = 22;
        h.username = m_jumps->item(r, 2) ? m_jumps->item(r, 2)->text().trimmed() : QString();
        if (auto *pw = qobject_cast<QLineEdit *>(m_jumps->cellWidget(r, 3))) {
            const QString plain = pw->text();
            if (!plain.isEmpty())
                h.passwordEnc = Crypto::encrypt(plain);
            else if (r < m_original.jumps.size())
                h.passwordEnc = m_original.jumps.at(r).passwordEnc;
        }
        if (!h.host.isEmpty())
            hops.append(h);
    }
    return hops;
}

void SessionEditDialog::addForwardRow(const PortForward &fwd)
{
    const int r = m_forwards->rowCount();
    m_forwards->insertRow(r);
    auto *type = new QComboBox;
    type->addItem(QStringLiteral("本地 -L"), QStringLiteral("L"));
    type->addItem(QStringLiteral("远程 -R"), QStringLiteral("R"));
    type->addItem(QStringLiteral("动态 -D"), QStringLiteral("D"));
    const int ti = type->findData(fwd.type.isEmpty() ? QStringLiteral("L") : fwd.type);
    type->setCurrentIndex(ti >= 0 ? ti : 0);
    guardWheel(type);
    m_forwards->setCellWidget(r, 0, type);
    m_forwards->setItem(r, 1, new QTableWidgetItem(fwd.bindHost.isEmpty()
                                                       ? QStringLiteral("127.0.0.1")
                                                       : fwd.bindHost));
    m_forwards->setItem(r, 2, new QTableWidgetItem(fwd.bindPort > 0
                                                       ? QString::number(fwd.bindPort)
                                                       : QString()));
    m_forwards->setItem(r, 3, new QTableWidgetItem(fwd.destHost));
    m_forwards->setItem(r, 4, new QTableWidgetItem(fwd.destPort > 0
                                                       ? QString::number(fwd.destPort)
                                                       : QString()));
}

QVector<PortForward> SessionEditDialog::forwardsFromTable() const
{
    QVector<PortForward> out;
    for (int r = 0; r < m_forwards->rowCount(); ++r) {
        PortForward f;
        if (auto *type = qobject_cast<QComboBox *>(m_forwards->cellWidget(r, 0)))
            f.type = type->currentData().toString();
        f.bindHost = m_forwards->item(r, 1) ? m_forwards->item(r, 1)->text().trimmed()
                                            : QStringLiteral("127.0.0.1");
        f.bindPort = m_forwards->item(r, 2) ? m_forwards->item(r, 2)->text().toInt() : 0;
        f.destHost = m_forwards->item(r, 3) ? m_forwards->item(r, 3)->text().trimmed() : QString();
        f.destPort = m_forwards->item(r, 4) ? m_forwards->item(r, 4)->text().toInt() : 0;
        if (f.bindPort > 0)
            out.append(f);
    }
    return out;
}
