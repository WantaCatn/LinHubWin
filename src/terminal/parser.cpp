#include "terminal/parser.h"
#include "terminal/screen.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVector>

VtParser::VtParser(Screen *screen)
    : m_screen(screen)
{
}

void VtParser::reset()
{
    m_state = State::Ground;
    m_csi.clear();
    m_osc.clear();
    m_utf8.clear();
    m_utfNeed = 0;
    m_csiPriv = false;
}

void VtParser::feed(const QByteArray &data)
{
    for (int i = 0; i < data.size(); ++i)
        handleByte(static_cast<unsigned char>(data.at(i)));
}

void VtParser::handleByte(unsigned char b)
{
    if (m_utfNeed > 0 && m_state == State::Ground) {
        if ((b & 0xC0) == 0x80) {
            m_utf8.push_back(static_cast<char>(b));
            --m_utfNeed;
            if (m_utfNeed == 0)
                flushUtf8();
            return;
        }
        m_utf8.clear();
        m_utfNeed = 0;
    }

    switch (m_state) {
    case State::Ground:  handleGround(b); break;
    case State::Escape:  handleEscape(b); break;
    case State::Csi:     handleCsi(b); break;
    case State::Osc:     handleOsc(b); break;
    case State::OscSt:
        finishOsc();
        m_state = State::Ground;
        if (b != '\\')
            handleByte(b);
        break;
    case State::Charset: m_state = State::Ground; break;
    case State::Dcs:
        if (b == 0x1b)
            m_state = State::Escape;
        else if (b == 0x07 || b == 0x9c)
            m_state = State::Ground;
        break;
    }
}

void VtParser::flushUtf8()
{
    const QString s = QString::fromUtf8(m_utf8);
    m_utf8.clear();
    for (const QChar &ch : s)
        m_screen->putCodepoint(ch.unicode());
}

void VtParser::handleGround(unsigned char b)
{
    switch (b) {
    case 0x07: m_screen->bel(); return;
    case 0x08: m_screen->backspace(); return;
    case 0x09: m_screen->tab(); return;
    case 0x0a:
    case 0x0b:
    case 0x0c: m_screen->lineFeed(); return;
    case 0x0d: m_screen->carriageReturn(); return;
    case 0x1b: m_state = State::Escape; return;
    case 0x9b: m_state = State::Csi; m_csi.clear(); m_csiPriv = false; return;
    default:
        break;
    }

    if (b < 0x20)
        return;

    if (b < 0x80) {
        m_screen->putCodepoint(b);
        return;
    }

    m_utf8.clear();
    m_utf8.push_back(static_cast<char>(b));
    if ((b & 0xE0) == 0xC0) m_utfNeed = 1;
    else if ((b & 0xF0) == 0xE0) m_utfNeed = 2;
    else if ((b & 0xF8) == 0xF0) m_utfNeed = 3;
    else m_utfNeed = 0;
}

void VtParser::handleEscape(unsigned char b)
{
    if (b == '[') {
        m_state = State::Csi;
        m_csi.clear();
        m_csiPriv = false;
        return;
    }
    if (b == ']') {
        m_state = State::Osc;
        m_osc.clear();
        return;
    }
    if (b == 'P') {
        m_state = State::Dcs;
        return;
    }
    if (b == '(' || b == ')' || b == '*' || b == '+') {
        m_state = State::Charset;
        return;
    }

    m_state = State::Ground;
    switch (b) {
    case '7': m_screen->saveCursor(); break;
    case '8': m_screen->restoreCursor(); break;
    case 'c': m_screen->reset(m_screen->scheme()); break;
    case 'D': m_screen->index(); break;
    case 'E': m_screen->carriageReturn(); m_screen->index(); break;
    case 'M': m_screen->reverseIndex(); break;
    default: break;
    }
}

void VtParser::handleCsi(unsigned char b)
{
    if (m_csi.isEmpty() && (b == '?' || b == '>' || b == '=')) {
        m_csiPriv = (b == '?');
        m_csi.push_back(static_cast<char>(b));
        return;
    }
    if ((b >= '0' && b <= '9') || b == ';' || b == ':') {
        m_csi.push_back(static_cast<char>(b));
        return;
    }
    m_csi.push_back(static_cast<char>(b));
    execCsi();
    m_state = State::Ground;
}

void VtParser::handleOsc(unsigned char b)
{
    if (b == 0x07 || b == 0x9c) {
        finishOsc();
        m_state = State::Ground;
        return;
    }
    if (b == 0x1b) {
        m_state = State::OscSt;
        return;
    }
    m_osc.push_back(static_cast<char>(b));
    if (m_osc.size() > 4096)
        m_state = State::Ground;
}

void VtParser::finishOsc()
{
    const QByteArray payload = m_osc;
    m_osc.clear();
    const int semi = payload.indexOf(';');
    if (semi <= 0)
        return;
    const int cmd = payload.left(semi).toInt();
    const QByteArray arg = payload.mid(semi + 1);
    if (cmd == 0 || cmd == 2) {
        m_screen->setTitle(QString::fromUtf8(arg));
        return;
    }
    if (cmd != 7)
        return;
    QString uri = QString::fromUtf8(arg);
    if (!uri.startsWith(QLatin1String("file://")))
        return;
    uri = uri.mid(7);
    const int slash = uri.indexOf(QLatin1Char('/'));
    if (slash < 0)
        return;
    const QByteArray path = uri.mid(slash).toUtf8();
    m_screen->setCwd(QString::fromUtf8(QByteArray::fromPercentEncoding(path)));
}

int VtParser::param(int index, int fallback) const
{
    QByteArray body = m_csi;
    if (!body.isEmpty() && (body[0] == '?' || body[0] == '>' || body[0] == '='))
        body = body.mid(1);
    if (body.isEmpty())
        return fallback;
    body.chop(1);
    const QList<QByteArray> parts = body.split(';');
    if (index >= parts.size() || parts[index].isEmpty())
        return fallback;
    bool ok = false;
    const int v = parts[index].toInt(&ok);
    return ok ? v : fallback;
}

void VtParser::execCsi()
{
    if (m_csi.isEmpty())
        return;
    const char cmd = m_csi.at(m_csi.size() - 1);
    const bool priv = m_csiPriv;

    auto params = [this]() {
        QVector<int> out;
        QByteArray body = m_csi;
        if (!body.isEmpty() && (body[0] == '?' || body[0] == '>' || body[0] == '='))
            body = body.mid(1);
        body.chop(1);
        if (body.isEmpty())
            return out;
        for (const auto &p : body.split(';'))
            out.push_back(p.isEmpty() ? 0 : p.toInt());
        return out;
    };

    switch (cmd) {
    case 'A': m_screen->cuu(param(0, 1)); break;
    case 'B': m_screen->cud(param(0, 1)); break;
    case 'C': m_screen->cuf(param(0, 1)); break;
    case 'D': m_screen->cub(param(0, 1)); break;
    case 'E': m_screen->cud(param(0, 1)); m_screen->cha(1); break;
    case 'F': m_screen->cuu(param(0, 1)); m_screen->cha(1); break;
    case 'G': m_screen->cha(param(0, 1)); break;
    case 'H':
    case 'f': m_screen->cup(param(0, 1), param(1, 1)); break;
    case 'J': m_screen->ed(param(0, 0)); break;
    case 'K': m_screen->el(param(0, 0)); break;
    case 'L': m_screen->il(param(0, 1)); break;
    case 'M': m_screen->dl(param(0, 1)); break;
    case 'P': m_screen->dch(param(0, 1)); break;
    case 'S': m_screen->su(param(0, 1)); break;
    case 'T': m_screen->sd(param(0, 1)); break;
    case 'X': m_screen->ech(param(0, 1)); break;
    case '@': m_screen->ich(param(0, 1)); break;
    case 'd': m_screen->vpa(param(0, 1)); break;
    case 'm': m_screen->sgr(params()); break;
    case 'r': m_screen->setScrollRegion(param(0, 1), param(1, 0)); break;
    case 's': m_screen->saveCursor(); break;
    case 'u': m_screen->restoreCursor(); break;
    case 'h':
    case 'l': {
        const QVector<int> ps = params();
        if (priv) {
            if (ps.isEmpty())
                m_screen->setDecMode(0, cmd == 'h');
            else {
                for (int p : ps)
                    m_screen->setDecMode(p, cmd == 'h');
            }
        } else {
            for (int p : ps) {
                if (p == 47 || p == 1047 || p == 1049)
                    m_screen->setDecMode(p, cmd == 'h');
            }
        }
        break;
    }
    case 'n':
        if (param(0, 0) == 6 && m_writeBack) {
            QString seq;
            m_screen->reportCursor(&seq);
            m_writeBack(seq.toLatin1());
        }
        break;
    default:
        break;
    }
}
