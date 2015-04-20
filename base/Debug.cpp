/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2010-2011 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "Debug.h"
#include "ResourceFinder.h"

#include <QMutex>
#include <QDir>
#include <QUrl>
#include <QCoreApplication>

#ifndef NDEBUG

static SVDebug *debug = 0;
static QMutex mutex;

SVDebug &getSVDebug() {
    mutex.lock();
    if (!debug) {
        debug = new SVDebug();
    }
    mutex.unlock();
    return *debug;
}

SVDebug::SVDebug() :
    m_prefix(0),
    m_ok(false),
    m_eol(false)
{
    QString pfx = ResourceFinder().getUserResourcePrefix();
    QDir logdir(QString("%1/%2").arg(pfx).arg("log"));

    m_prefix = strdup(QString("[%1]")
                      .arg(QCoreApplication::applicationPid())
                      .toLatin1().data());

    //!!! what to do if mkpath fails?
    if (!logdir.exists()) logdir.mkpath(logdir.path());

    QString fileName = logdir.path() + "/sv-debug.log";

    m_stream.open(fileName.toLocal8Bit().data(), std::ios_base::out);

    if (!m_stream) {
        QDebug(QtWarningMsg) << (const char *)m_prefix
                             << "Failed to open debug log file "
                             << fileName << " for writing";
    } else {
        cerr << m_prefix << ": Log file is " << fileName << endl;
        m_ok = true;
    }
}

SVDebug::~SVDebug()
{
    m_stream.close();
}

QDebug &
operator<<(QDebug &dbg, const std::string &s)
{
    dbg << QString::fromUtf8(s.c_str());
    return dbg;
}

#endif

std::ostream &
operator<<(std::ostream &target, const QString &str)
{
    return target << str.toStdString();
}

std::ostream &
operator<<(std::ostream &target, const QUrl &u)
{
    return target << "<" << u.toString().toStdString() << ">";
}

