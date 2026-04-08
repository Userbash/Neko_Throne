#include <csignal>

// Compile-time architecture enforcement — must be first
#include "include/global/ArchGuard.hpp"

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QTranslator>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QStandardPaths>
#include <QLocalSocket>
#include <QLocalServer>
#include <QThread>
#include <3rdparty/WinCommander.hpp>

#include "include/global/Configs.hpp"
#include "include/ui/core/TranslationManager.hpp"
#include "include/sys/platform/SystemThemeWatcher.hpp"

#include "include/ui/mainwindow_interface.h"

#ifdef Q_OS_WIN
#include "include/sys/windows/MiniDump.h"
#include "include/sys/windows/eventHandler.h"
#include "include/sys/windows/WinVersion.h"
#include <qfontdatabase.h>
#endif
#ifdef Q_OS_LINUX
#include <include/sys/linux/coreDump.h>
#include <qfontdatabase.h>
#endif

/**
 * Graceful exit handler.
 * Ensures proxy core is stopped and state is saved before termination.
 */
void signal_handler(int signum) {
    if (GetMainWindow()) {
        GetMainWindow()->prepare_exit();
        qApp->quit();
    }
}

QTranslator* trans = nullptr;
QTranslator* trans_qt = nullptr;

/**
 * Fallback translation loader.
 * Used when external .qm files are missing or on initial startup.
 */
void loadTranslate(const QString& locale) {
    if (trans != nullptr) {
        QCoreApplication::removeTranslator(trans);
        trans->deleteLater();
    }
    if (trans_qt != nullptr) {
        QCoreApplication::removeTranslator(trans_qt);
        trans_qt->deleteLater();
    }
    trans = new QTranslator;
    trans_qt = new QTranslator;
    QLocale::setDefault(QLocale(locale));
    
    // Load app-specific translations
    const QString langDir = QCoreApplication::applicationDirPath() + QStringLiteral("/lang");
    if (trans->load(langDir + "/" + locale + ".qm")) {
        QCoreApplication::installTranslator(trans);
    } else if (trans->load(":/translations/" + locale + ".qm")) {
        QCoreApplication::installTranslator(trans);
    }
    
    // Load standard Qt system translations
    const QString qtTransDir = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    if (trans_qt->load("qtbase_" + locale, qtTransDir)) {
        QCoreApplication::installTranslator(trans_qt);
    } else if (trans_qt->load("qt_" + locale, qtTransDir)) {
        QCoreApplication::installTranslator(trans_qt);
    }
}

#define LOCAL_SERVER_PREFIX "throne-"

int main(int argc, char* argv[]) {
    QStringList args;
    for (int i = 0; i < argc; ++i) args << QString::fromLocal8Bit(argv[i]);

    if (args.contains("--help") || args.contains("-h")) {
        printf("Neko Throne - Unified Proxy Manager\n");
        printf("Usage: %s [options]\n", argv[0]);
        printf("Options:\n");
        printf("  -h, --help     Show help\n");
        printf("  -v, --version  Show version\n");
        printf("  --offscreen    Offscreen mode\n");
        return 0;
    }

    if (args.contains("--version") || args.contains("-v")) {
        printf("Neko Throne v1.0.0 (Qt 6.10.2)\n");
        return 0;
    }

#ifdef Q_OS_LINUX
    // Headless environment fallback
    if (!args.contains("--offscreen") && qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
#endif

#ifdef Q_OS_WIN
    Windows_SetCrashHandler();
#endif
#ifdef Q_OS_LINUX
    setup_crash_handlers();
#endif

    QApplication::setQuitOnLastWindowClosed(false);
    QApplication a(argc, argv);

#if (QT_VERSION >= QT_VERSION_CHECK(6,9,0))
    // Global emoji font support
#ifdef Q_OS_WIN
    int fontId = QFontDatabase::addApplicationFont(WinVersion::IsBuildNumGreaterOrEqual(BuildNumber::Windows_11_22H2) ? ":/font/notoEmoji" : ":/font/Twemoji");
#else
    int fontId = QFontDatabase::addApplicationFont(":/font/notoEmoji");
#endif
    if (fontId >= 0)
    {
        QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
        QFontDatabase::setApplicationEmojiFontFamilies(fontFamilies);
    }
#endif

    QDir::setCurrent(QApplication::applicationDirPath());
    if (QFile::exists("updater.old")) {
        QFile::remove("updater.old");
    }

    // Argument handling
    Configs::dataStore->argv = QApplication::arguments();
    if (Configs::dataStore->argv.contains("-many")) Configs::dataStore->flag_many = true;
    if (Configs::dataStore->argv.contains("-appdata")) {
        Configs::dataStore->flag_use_appdata = true;
        int appdataIndex = Configs::dataStore->argv.indexOf("-appdata");
        if (Configs::dataStore->argv.size() > appdataIndex + 1 && !Configs::dataStore->argv.at(appdataIndex + 1).startsWith("-")) {
            Configs::dataStore->appdataDir = Configs::dataStore->argv.at(appdataIndex + 1);
        }
    }
    if (Configs::dataStore->argv.contains("-tray")) Configs::dataStore->flag_tray = true;
    if (Configs::dataStore->argv.contains("-debug")) Configs::dataStore->flag_debug = true;
    if (Configs::dataStore->argv.contains("-flag_restart_tun_on")) Configs::dataStore->flag_restart_tun_on = true;
    if (Configs::dataStore->argv.contains("-flag_restart_dns_set")) Configs::dataStore->flag_dns_set = true;
#ifdef NKR_CPP_USE_APPDATA
    Configs::dataStore->flag_use_appdata = true;
#endif
#ifdef NKR_CPP_DEBUG
    Configs::dataStore->flag_debug = true;
#endif

    // Workspace initialization (Portable vs AppData)
    auto wd = QDir(QApplication::applicationDirPath());
    if (Configs::dataStore->flag_use_appdata) {
        QApplication::setApplicationName("Neko Throne");
        if (!Configs::dataStore->appdataDir.isEmpty()) {
            wd.setPath(Configs::dataStore->appdataDir);
        } else {
            wd.setPath(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
        }
    }
    if (!wd.exists()) wd.mkpath(wd.absolutePath());
    if (!wd.exists("config")) wd.mkdir("config");
    QDir::setCurrent(wd.absoluteFilePath("config"));
    QDir("temp").removeRecursively(); 

#ifdef Q_OS_LINUX
    QApplication::addLibraryPath(QApplication::applicationDirPath() + "/usr/plugins");
#endif

    // Core gRPC service thread
    DS_cores = new QThread;
    DS_cores->start();

    QIcon::setFallbackSearchPaths(QStringList{ ":/icon" });
    if (QIcon::themeName().isEmpty()) {
        QIcon::setThemeName("breeze");
    }

    // Check write permissions for essential directories
    QDir dir;
    bool dir_success = true;
    if (!dir.exists("profiles")) dir_success &= dir.mkdir("profiles");
    if (!dir.exists("groups"))   dir_success &= dir.mkdir("groups");
    if (!dir.exists(ROUTES_PREFIX_NAME)) dir_success &= dir.mkdir(ROUTES_PREFIX_NAME);
    if (!dir_success) {
        QMessageBox::critical(nullptr, QObject::tr("Error"), QObject::tr("No permission to write in %1").arg(dir.absolutePath()));
        return 1;
    }

    // Migration from older versions
    if (QFile::exists("groups/nekobox.json")) {
        QFile::rename("groups/nekobox.json", "configs.json");
    }

    // Load global state
    Configs::dataStore->fn = "configs.json";
    if (!Configs::dataStore->Load()) {
        Configs::dataStore->Save();
    }

#ifdef Q_OS_WIN
    if (Configs::dataStore->windows_set_admin && !Configs::IsAdmin() && !Configs::dataStore->disable_run_admin)
    {
        Configs::dataStore->windows_set_admin = false;
        Configs::dataStore->Save();
        WinCommander::runProcessElevated(QApplication::applicationFilePath(), {}, "", WinCommander::SW_NORMAL, false);
        return 0;
    }
#endif

    if (Configs::dataStore->start_minimal) Configs::dataStore->flag_tray = true;

    Configs::dataStore->routing = std::make_unique<Configs::Routing>();
    Configs::dataStore->routing->fn = ROUTES_PREFIX + "Default";
    if (!Configs::dataStore->routing->Load()) Configs::dataStore->routing->Save();

    Configs::dataStore->shortcuts = std::make_unique<Configs::Shortcuts>();
    Configs::dataStore->shortcuts->fn = "shortcuts.json";
    if (!Configs::dataStore->shortcuts->Load()) Configs::dataStore->shortcuts->Save();

    TranslationManager::instance()->initialize();
    SystemThemeWatcher::instance()->start();

    QString locale;
    switch (Configs::dataStore->language) {
        case 1: locale = ""; break; 
        case 2: locale = "zh_CN"; break;
        case 3: locale = "fa_IR"; break;
        case 4: locale = "ru_RU"; break;
        default: locale = QLocale().name();
    }
    
    if (!locale.isEmpty() && !TranslationManager::instance()->switchLanguage(locale)) {
        loadTranslate(locale);
    }
    
    if (QGuiApplication::tr("QT_LAYOUT_DIRECTION") == QLatin1String("RTL")) {
        QGuiApplication::setLayoutDirection(Qt::RightToLeft);
    }

    // Single instance check via local socket
    QByteArray hashBytes = QCryptographicHash::hash(wd.absolutePath().toUtf8(), QCryptographicHash::Md5).toBase64(QByteArray::OmitTrailingEquals);
    hashBytes.replace('+', '0').replace('/', '1');
    auto serverName = LOCAL_SERVER_PREFIX + QString::fromUtf8(hashBytes);
    
    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (socket.waitForConnected(250)) {
        socket.disconnectFromServer();
        return 0;
    }

    QLocalServer server(qApp);
    server.setSocketOptions(QLocalServer::WorldAccessOption);
    if (!server.listen(serverName)) {
        return 1;
    }
    
    QObject::connect(&server, &QLocalServer::newConnection, qApp, [&] {
        auto s = server.nextPendingConnection();
        s->close();
        MW_dialog_message("", "Raise"); 
    });
    
    QObject::connect(qApp, &QApplication::aboutToQuit, [&] {
        server.close();
        QLocalServer::removeServer(serverName);
    });

#ifdef Q_OS_LINUX
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
#endif

#ifdef Q_OS_WIN
    auto eventFilter = new PowerOffTaskkillFilter(signal_handler);
    a.installNativeEventFilter(eventFilter);
#endif

    UI_InitMainWindow();
    return QApplication::exec();
}
