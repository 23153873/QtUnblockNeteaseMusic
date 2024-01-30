#include <QCommandLineParser>
#include <QDir>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QTimer>
#include <QTranslator>
#include <SingleApplication>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

#include "mainwindow.h"
#include "tray.h"
#include "updatechecker.h"
#include "version.h"

using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    SingleApplication a(argc, argv, true);
    a.setApplicationName(PROJECT_NAME);
    a.setApplicationVersion(PROJECT_VERSION);
    a.setOrganizationName(PROJECT_AUTHOR);
    a.setOrganizationDomain(PROJECT_HOMEPAGE_URL);

    QCommandLineParser parser;
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    const QCommandLineOption silentOption(u"silent"_s);
    parser.addOption(silentOption);
    parser.process(a);

    if (a.isSecondary())
    {
#ifdef Q_OS_WIN
        AllowSetForegroundWindow((DWORD)a.primaryPid());
#endif
        if (a.sendMessage(" "_qba))
        {
            return -1;
        }
    }

    QDir::setCurrent(QApplication::applicationDirPath());

    const QLocale locale = QLocale();
    const QString translationsPath =
        QLibraryInfo::path(QLibraryInfo::TranslationsPath);

    // load app translations
    QTranslator appTranslator;
    // look up e.g. :/i18n/QtUnblockNeteaseMusic_en.qm
    if (appTranslator.load(locale, u"QtUnblockNeteaseMusic"_s, u"_"_s, u":/i18n"_s))
    {
        a.installTranslator(&appTranslator);
    }

    // load Qt base translations
    QTranslator baseTranslator;
    // look up e.g. {current_path}/translations/qt_en.qm
    if (baseTranslator.load(locale, u"qt"_s, u"_"_s, translationsPath) ||
        baseTranslator.load(locale, u"qt"_s, u"_"_s, u"translations"_s))
    {
        a.installTranslator(&baseTranslator);
    }

    Config config;

    Server server(&config);

    MainWindow w(&config);

    QObject::connect(&server, &Server::log, &w, &MainWindow::log);
    QObject::connect(&server, &Server::logClear, &w, &MainWindow::logClear);
    QObject::connect(&w, &MainWindow::serverClose, &server, &Server::close);
    QObject::connect(&w, &MainWindow::serverRestart, &server, &Server::restart);

    Tray tray(&w);

    UpdateChecker updateChecker;
    QObject::connect(&updateChecker, &UpdateChecker::ready,
                     [&w, &updateChecker](const bool &isNewVersion, const QString &version)
                     { if (isNewVersion) w.showVersionStatus(version);
                       // Check again after 24 hours
                       QTimer::singleShot(24*60*60*1000, &updateChecker, &UpdateChecker::checkUpdate); });
    QTimer::singleShot(1000, &updateChecker, &UpdateChecker::checkUpdate);

    // Start server in another thread
    qDebug("---Starting server---");
    QTimer::singleShot(0, &server, &Server::start);

    // Open when second instance started
    QObject::connect(&a, &SingleApplication::receivedMessage, &w, [&w]
                     {
#ifdef Q_OS_WIN
                         ShowWindow((HWND)w.winId(), SW_RESTORE);
#endif
                         w.show();
                         w.activateWindow(); });

    // Disable proxy before quit or shutdown
    QObject::connect(&a, &QApplication::aboutToQuit, &w, [&w]
                     { if (w.isProxy()) w.setProxy(false); });

    // Don't quit when closing dialog
    a.setQuitOnLastWindowClosed(false);

    tray.setVisible(true);

    // don't show window if "--silent" in arguments
    if (!parser.isSet(silentOption) && !config.startMinimized)
    {
        w.show();
    }

    return a.exec();
}
