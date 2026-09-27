#include "pty/winptyreader.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

void WinPtyReader::run()
{
    HANDLE h = reinterpret_cast<HANDLE>(m_handle);
    if (!h || h == INVALID_HANDLE_VALUE)
        return;
    char buf[4096];
    DWORD n = 0;
    for (;;) {
        if (!ReadFile(h, buf, sizeof(buf), &n, nullptr))
            break;
        if (n == 0)
            break;
        emit chunk(QByteArray(buf, int(n)));
    }
    emit done();
}
