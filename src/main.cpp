/***************************************************************************
 *   Copyright (C) 2003-2007 by Joachim Eibl <joachim.eibl at gmx.de>      *
 *   Copyright (C) 2018 Michael Reeves reeves.87@gmail.com                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "kdiff3_shell.h"
#include "version.h"

#include "Logging.h"

#ifndef Q_OS_WIN
#include <unistd.h>
#endif

#include <KAboutData>
#include <KCrash/KCrash>
#include <KLocalizedString>
#include <KMessageBox>

#include <QApplication>
#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFile>
#include <QPointer>
#include <QStringList>
#include <QStandardPaths>
#include <QTextStream>

void initialiseCmdLineArgs(QCommandLineParser* cmdLineParser)
{
    QString configFileName = QStandardPaths::locate(QStandardPaths::GenericConfigLocation, "kdiff3rc");
    QFile configFile(configFileName);
    QString ignorableOptionsLine = "-u;-query;-html;-abort";
    if(configFile.open(QIODevice::ReadOnly))
    {
        QTextStream ts(&configFile);
        while(!ts.atEnd())
        {
            QString line = ts.readLine();
            if(line.startsWith(QLatin1String("IgnorableCmdLineOptions=")))
            {
                int pos = line.indexOf('=');
                if(pos >= 0)
                {
                    ignorableOptionsLine = line.mid(pos + 1);
                }
                break;
            }
        }
    }
    //support our own old preferences this is obsolete
    QStringList sl = ignorableOptionsLine.split(',');

    if(!sl.isEmpty())
    {
        QStringList ignorableOptions = sl.front().split(';');
        for(QStringList::iterator i = ignorableOptions.begin(); i != ignorableOptions.end(); ++i)
        {
            (*i).remove('-');
            if(!(*i).isEmpty())
            {
                if(i->length() == 1) {
                    cmdLineParser->addOption(QCommandLineOption(QStringList() << *i << QLatin1String("ignore"), i18n("Ignored. (User defined.)")));
                }
                else
                {
                    cmdLineParser->addOption(QCommandLineOption(QStringList() << *i, i18n("Ignored. (User defined.)")));
                }
            }
        }
    }
}

int main(int argc, char* argv[])
{
    const QLatin1String appName("kdiff3");

    QApplication app(argc, argv); // KAboutData and QCommandLineParser depend on this being setup.
    KLocalizedString::setApplicationDomain(appName.data());

    KCrash::initialize();

    const QString i18nName = i18n("KDiff3");
    QString appVersion(KDIFF3_VERSION_STRING);

    if(sizeof(void*) == 8)
        appVersion += i18n(" (64 bit)");
    else if(sizeof(void*) == 4)
        appVersion += i18n(" (32 bit)");
    const QString description = i18n("Tool for Comparison and Merge of Files and Directories");
    const QString copyright = i18n("(c) 2002-2014 Joachim Eibl, (c) 2017 Michael Reeves KF5/Qt5 port");
    const QString homePage = QStringLiteral("https://kde.org/applications/development/kdiff3");

    KAboutData aboutData(appName, i18nName,
                         appVersion, description, KAboutLicense::GPL_V2, copyright, QString(),
                         homePage);

    KAboutData::setApplicationData(aboutData);

    QCommandLineParser* cmdLineParser = KDiff3Shell::getParser();
    cmdLineParser->setApplicationDescription(aboutData.shortDescription());

    aboutData.setupCommandLine(cmdLineParser);

    initialiseCmdLineArgs(cmdLineParser);
    // ignorable command options
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("m") << QLatin1String("merge"), i18n("Merge the input.")));
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("b") << QLatin1String("base"), i18n("Explicit base file. For compatibility with certain tools."), QLatin1String("file")));
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("o") << QLatin1String("output"), i18n("Output file. Implies -m. E.g.: -o newfile.txt"), QLatin1String("file")));
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("out"), i18n("Output file, again. (For compatibility with certain tools.)"), QLatin1String("file")));
#ifdef ENABLE_AUTO
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("auto"), i18n("No GUI if all conflicts are auto-solvable. (Needs -o file)")));
#else
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("auto"), i18n("Ignored.")));
#endif
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("qall"), i18n("Do not solve conflicts automatically.")));
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("L1"), i18n("Visible name replacement for input file 1 (base)."), QLatin1String("alias1")));
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("L2"), i18n("Visible name replacement for input file 2."), QLatin1String("alias2")));
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("L3"), i18n("Visible name replacement for input file 3."), QLatin1String("alias3")));
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("L") << QLatin1String("fname"), i18n("Alternative visible name replacement. Supply this once for every input."), QLatin1String("alias")));
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("cs"), i18n("Override a config setting. Use once for every setting. E.g.: --cs \"AutoAdvance=1\""), QLatin1String("string")));
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("confighelp"), i18n("Show list of config settings and current values.")));
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("config"), i18n("Use a different config file."), QLatin1String("file")));

    // other command options
    cmdLineParser->addPositionalArgument(QLatin1String("[File1]"), i18n("file1 to open (base, if not specified via --base)"));
    cmdLineParser->addPositionalArgument(QLatin1String("[File2]"), i18n("file2 to open"));
    cmdLineParser->addPositionalArgument(QLatin1String("[File3]"), i18n("file3 to open"));

    bool isAtty = true;

#ifndef Q_OS_WIN
    isAtty = isatty(fileno(stderr)) == 1;//will be true for redirected output as well
#endif
    /*
        QCommandLineParser::process does what is expected on windows or when running from a commandline.
        However, it only accounts for a lackof terminal output on windows.
    */
    if(isAtty)
    {
        cmdLineParser->process(QCoreApplication::arguments());
    }
    else
    {
        /*
            There is no terminal connected so don't just exit mysteriously on error.
        */
        if(!cmdLineParser->parse(QCoreApplication::arguments()))
        {
            QString errorMessage = cmdLineParser->errorText();

            KMessageBox::error(nullptr, "<html><head/><body><h2>" + errorMessage + "</h2><pre>" + i18n("See kdiff3 --help for supported options.") + "</pre></body></html>", aboutData.displayName());
            exit(1);
        }

        if(cmdLineParser->isSet(QStringLiteral("version"))) {
            KMessageBox::information(nullptr,
                                    aboutData.displayName() + ' ' + aboutData.version(), aboutData.displayName());
            exit(0);
        }
        if(cmdLineParser->isSet(QStringLiteral("help"))) {
            KMessageBox::information(nullptr, "<html><head/><body><pre>" + cmdLineParser->helpText() + "</pre></body></html>", aboutData.displayName());

            exit(0);
        }
    }

    aboutData.processCommandLine(cmdLineParser);

    /*
      Do not attempt to call show here that will be done later.
      This variable exists solely to insure the KDiff3Shell is deleted on exit.
    */
    QPointer<KDiff3Shell> p(new KDiff3Shell());
    Q_UNUSED(p);

    //p->setWindowState( p->windowState() | Qt::WindowActive ); // Patch for ubuntu: window not active on startup
    //app.installEventFilter( new CFilter );
    int retVal = app.exec();
    /*  if (QApplication::clipboard()->text().size() == 0)
     QApplication::clipboard()->clear(); // Patch for Ubuntu: Fix issue with Qt clipboard*/
    return retVal;
}

