/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

/*
   This is a modified version of a source file from the 
   Rosegarden MIDI and audio sequencer and notation editor.
   This file copyright 2005-2011 Chris Cannam and the Rosegarden
   development team.
*/

#include "ResourceFinder.h"

#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QProcess>
#include <QCoreApplication>

#include <cstdlib>
#include <iostream>

/**
   Resource files may be found in three places:

   * Bundled into the application as Qt4 resources.  These may be
     opened using Qt classes such as QFile, with "fake" file paths
     starting with a colon.  For example ":icons/fileopen.png".

   * Installed with the package, or in the user's equivalent home
     directory location.  For example,

     - on Linux, in /usr/share/<appname> or /usr/local/share/<appname>
     - on Linux, in $HOME/.local/share/<appname>

     - on OS/X, in /Library/Application Support/<appname>
     - on OS/X, in $HOME/Library/Application Support/<appname>

     - on Windows, in %ProgramFiles%/<company>/<appname>
     - on Windows, in (where?)

   These locations are searched in reverse order (user-installed
   copies take priority over system-installed copies take priority
   over bundled copies).  Also, /usr/local takes priority over /usr.
*/

QStringList
ResourceFinder::getSystemResourcePrefixList()
{
    // returned in order of priority

    QStringList list;

#ifdef Q_OS_WIN32
    char *programFiles = getenv("ProgramFiles");
    if (programFiles && programFiles[0]) {
        list << QString("%1/%2/%3")
            .arg(programFiles)
            .arg(qApp->organizationName())
            .arg(qApp->applicationName());
    } else {
        list << QString("C:/Program Files/%1/%2")
            .arg(qApp->organizationName())
            .arg(qApp->applicationName());
    }
#else
#ifdef Q_OS_MAC
    list << QString("/Library/Application Support/%1/%2")
        .arg(qApp->organizationName())
        .arg(qApp->applicationName());
#else
    list << QString("/usr/local/share/%1")
        .arg(qApp->applicationName());
    list << QString("/usr/share/%1")
        .arg(qApp->applicationName());
#endif
#endif    

    return list;
}

QString
ResourceFinder::getUserResourcePrefix()
{
    char *home = getenv("HOME");
    if (!home || !home[0]) return "";

#ifdef Q_OS_WIN32
    return QString(); //!!!???
#else
#ifdef Q_OS_MAC
    return QString("%1/Library/Application Support/%2/%3")
        .arg(home)
        .arg(qApp->organizationName())
        .arg(qApp->applicationName());
#else
    return QString("%1/.local/share/%2")
        .arg(home)
        .arg(qApp->applicationName());
#endif
#endif    
}

QStringList
ResourceFinder::getResourcePrefixList()
{
    // returned in order of priority

    QStringList list;

    QString user = getUserResourcePrefix();
    if (user != "") list << user;

    list << getSystemResourcePrefixList();

    list << ":"; // bundled resource location

    return list;
}

QString
ResourceFinder::getResourcePath(QString resourceCat, QString fileName)
{
    // We don't simply call getResourceDir here, because that returns
    // only the "installed file" location.  We also want to search the
    // bundled resources and user-saved files.

    QStringList prefixes = getResourcePrefixList();
    
    if (resourceCat != "") resourceCat = "/" + resourceCat;

    for (QStringList::const_iterator i = prefixes.begin();
         i != prefixes.end(); ++i) {
        
        QString prefix = *i;

        std::cerr << "ResourceFinder::getResourcePath: Looking up file \"" << fileName.toStdString() << "\" for category \"" << resourceCat.toStdString() << "\" in prefix \"" << prefix.toStdString() << "\"" << std::endl;

        QString path =
            QString("%1%2/%3").arg(prefix).arg(resourceCat).arg(fileName);
        if (QFileInfo(path).exists() && QFileInfo(path).isReadable()) {
            std::cerr << "Found it!" << std::endl;
            return path;
        }
    }

    return "";
}

QString
ResourceFinder::getResourceDir(QString resourceCat)
{
    // Returns only the "installed file" location

    QStringList prefixes = getSystemResourcePrefixList();
    
    if (resourceCat != "") resourceCat = "/" + resourceCat;

    for (QStringList::const_iterator i = prefixes.begin();
         i != prefixes.end(); ++i) {
        
        QString prefix = *i;
        QString path = QString("%1%2").arg(prefix).arg(resourceCat);
        if (QFileInfo(path).exists() &&
            QFileInfo(path).isDir() &&
            QFileInfo(path).isReadable()) {
            return path;
        }
    }

    return "";
}

QString
ResourceFinder::getResourceSavePath(QString resourceCat, QString fileName)
{
    QString dir = getResourceSaveDir(resourceCat);
    if (dir == "") return "";

    return dir + "/" + fileName;
}

QString
ResourceFinder::getResourceSaveDir(QString resourceCat)
{
    // Returns the "user" location

    QString user = getUserResourcePrefix();
    if (user == "") return "";

    if (resourceCat != "") resourceCat = "/" + resourceCat;

    QDir userDir(user);
    if (!userDir.exists()) {
        if (!userDir.mkpath(user)) {
            std::cerr << "ResourceFinder::getResourceSaveDir: ERROR: Failed to create user resource path \"" << user.toStdString() << "\"" << std::endl;
            return "";
        }
    }

    if (resourceCat != "") {
        QString save = QString("%1%2").arg(user).arg(resourceCat);
        QDir saveDir(save);
        if (!saveDir.exists()) {
            if (!userDir.mkpath(save)) {
                std::cerr << "ResourceFinder::getResourceSaveDir: ERROR: Failed to create user resource path \"" << save.toStdString() << "\"" << std::endl;
                return "";
            }
        }
        return save;
    } else {
        return user;
    }
}

QStringList
ResourceFinder::getResourceFiles(QString resourceCat, QString fileExt)
{
    QStringList results;
    QStringList prefixes = getResourcePrefixList();

    QStringList filters;
    filters << QString("*.%1").arg(fileExt);

    for (QStringList::const_iterator i = prefixes.begin();
         i != prefixes.end(); ++i) {
        
        QString prefix = *i;
        QString path;

        if (resourceCat != "") {
            path = QString("%1/%2").arg(prefix).arg(resourceCat);
        } else {
            path = prefix;
        }
        
        QDir dir(path);
        if (!dir.exists()) continue;

        dir.setNameFilters(filters);
        QStringList entries = dir.entryList
            (QDir::Files | QDir::Readable, QDir::Name);
        
        for (QStringList::const_iterator j = entries.begin();
             j != entries.end(); ++j) {
            results << QString("%1/%2").arg(path).arg(*j);
        }
    }

    return results;
}

bool
ResourceFinder::unbundleResource(QString resourceCat, QString fileName)
{
    QString path = getResourcePath(resourceCat, fileName);
    
    if (!path.startsWith(':')) return true;

    // This is the lowest-priority alternative path for this
    // resource, so we know that there must be no installed copy.
    // Install one to the user location.
    std::cerr << "ResourceFinder::unbundleResource: File " << fileName.toStdString() << " is bundled, un-bundling it" << std::endl;
    QString target = getResourceSavePath(resourceCat, fileName);
    QFile file(path);
    if (!file.copy(target)) {
        std::cerr << "ResourceFinder::unbundleResource: ERROR: Failed to un-bundle resource file \"" << fileName.toStdString() << "\" to user location \"" << target.toStdString() << "\"" << std::endl;
        return false;
    }

    // Now since the file is in the user's editable space, the user should get
    // to edit it.  The chords.xml file I unbundled came out 444 instead of 644
    // which won't do.  Rather than put the chmod code there, I decided to put
    // it here, because I think it will always be appropriate to make unbundled
    // files editable.  That's rather the point in many cases, and for the rest,
    // nobody will likely notice they could have edited their font files or what
    // have you that were unbundled to improve performance.  (Dissenting
    // opinions welcome.  We can always shuffle this somewhere else if
    // necessary.  There are many possibilities.)
    QFile chmod(target);
    chmod.setPermissions(QFile::ReadOwner |
                         QFile::ReadUser  | /* for potential platform-independence */
                         QFile::ReadGroup |
                         QFile::ReadOther |
                         QFile::WriteOwner|
                         QFile::WriteUser); /* for potential platform-independence */

    return true;
}

