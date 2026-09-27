#pragma once

#include <QByteArray>
#include <QString>
#include <functional>
#include <utility>

class Screen;

class VtParser
{
public:
    explicit VtParser(Screen *screen);

    using WriteBack = std::function<void(const QByteArray &)>;
    void setWriteBack(WriteBack cb) { m_writeBack = std::move(cb); }

    void feed(const QByteArray &data);
    void reset();

private:
    enum class State {
        Ground,
        Escape,
        Csi,
        Osc,
        OscSt,
        Charset,
        Dcs
    };

    void handleByte(unsigned char b);
    void handleGround(unsigned char b);
    void handleEscape(unsigned char b);
    void handleCsi(unsigned char b);
    void handleOsc(unsigned char b);
    void finishOsc();
    void flushUtf8();
    void execCsi();
    int param(int index, int fallback) const;

    Screen *m_screen = nullptr;
    State m_state = State::Ground;
    QByteArray m_csi;
    QByteArray m_osc;
    QByteArray m_utf8;
    int m_utfNeed = 0;
    bool m_csiPriv = false;
    WriteBack m_writeBack;
};
