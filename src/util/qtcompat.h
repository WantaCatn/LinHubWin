#pragma once

#include <QDateTime>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QtGlobal>

inline QStringList splitSkipEmpty(const QString &text, QChar sep)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    return text.split(sep, Qt::SkipEmptyParts);
#else
    return text.split(sep, QString::SkipEmptyParts);
#endif
}

inline QStringList splitSkipEmpty(const QString &text, const QRegularExpression &re)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    return text.split(re, Qt::SkipEmptyParts);
#else
    return text.split(re, QString::SkipEmptyParts);
#endif
}

inline QString ucs4ToString(char32_t ch)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QString::fromUcs4(&ch, 1);
#else
    const uint u = static_cast<uint>(ch);
    return QString::fromUcs4(&u, 1);
#endif
}

inline QDateTime parseSqliteDateTime(const QString &text)
{
    QDateTime dt = QDateTime::fromString(text, Qt::ISODate);
    if (!dt.isValid())
        dt = QDateTime::fromString(text, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    return dt;
}
