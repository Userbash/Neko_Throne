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
#include <QPointer>
#include <QSocketNotifier>
#include <3rdparty/WinCommander.hpp>

#include "include/global/Configs.hpp"
#include "include/dataStore/Database.hpp"
#include "include/ui/core/TranslationManager.hpp"
#include "include/sys/platform/SystemThemeWatcher.hpp"

#include "include/ui/mainwindow_interface.h"

// Optional: Valgrind support for local memory profiling (not available in CI/CD)
#ifdef HAVE_VALGRIND
#include <valgrind/valgrind.h>
#else
// Valgrind disabled - not available in CI/CD environments
#define RUNNING_ON_VALGRIND 0
#endif

#ifdef Q_OS_WIN
#include "include/sys/windows/MiniDump.h"
#include "include/sys/windows/eventHandler.h"
#include "include/sys/windows/WinVersion.h"
#include <qfontdatabase.h>
#endif
#ifdef Q_OS_LINUX
#include <include/sys/linux/coreDump.h>
#include <qfontdatabase.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

static bool is_exiting_global = false;

#ifdef Q_OS_LINUX
static int sig_pipe_fd[2];

/**
 * Low-level signal handler.
 * Writes a byte to a pipe to notify the main thread.
 */
void unix_signal_handler(int signum) {
    char a = (char)signum;
    if (::write(sig_pipe_fd[0], &a, 1) == -1) {
        // Ignore errors
    }
}
#endif

/**
 * Graceful exit handler.
 * Ensures proxy core is stopped and state is saved before termination.
 */
void signal_handler(int signum) {
    if (is_exiting_global) return;
    is_exiting_global = true;
    
    fprintf(stderr, "Signal %d received, exiting...\n", signum);
    if (QCoreApplication::instance()) {
        if (GetMainWindow()) {
            GetMainWindow()->prepare_exit();
        }
        QCoreApplication::quit();
    } else {
        exit(0);
    }
}

// Fixed: Using QPointer to avoid dangling pointers and potential memory issues
QPointer<QTranslator> trans = nullptr;
QPointer<QTranslator> trans_qt = nullptr;

/**
 * Fallback translation loader.
 * Used when external .qm files are missing or on initial startup.
 */
void loadTranslate(const QString& locale) {
    if (trans) {
        QCoreApplication::removeTranslator(trans);
        delete trans;
    }
    if (trans_qt) {
        QCoreApplication::removeTranslator(trans_qt);
        delete trans_qt;
    }
    
    trans = new QTranslator(qApp);
    trans_qt = new QTranslator(qApp);
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

#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QPushButton>

/**
 * Global Message Handler.
 * Intercepts all qDebug, qWarning, etc., and writes them to application.log
 */
void myMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    // If QCoreApplication is already destroyed, we can't get applicationDirPath
    if (!QCoreApplication::instance()) {
        fprintf(stderr, "Log during/after shutdown: %s\n", msg.toStdString().c_str());
        return;
    }

    static QString logPath;
    if (logPath.isEmpty()) {
        logPath = QCoreApplication::applicationDirPath() + "/application.log";
    }
    
    QFile logFile(logPath);
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&logFile);
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        const char* level;
        switch (type) {
            case QtDebugMsg:    level = "DEBUG"; break;
            case QtInfoMsg:     level = "INFO "; break;
            case QtWarningMsg:  level = "WARN "; break;
            case QtCriticalMsg: level = "CRIT "; break;
            case QtFatalMsg:    level = "FATAL"; break;
            default:            level = "UNKN "; break;
        }
        out << timestamp << " [" << level << "] " << (context.category ? context.category : "default") << ": " << msg << "\n";
        out.flush();
        logFile.close();
    }
    
    // Always print to stderr for console/GDB
    fprintf(stderr, "[%d] %s\n", (int)type, msg.toStdString().c_str());
}

/**
 * UI Event Filter.
 * Logs every button click in the application.
 */
class UIEventFilter : public QObject {
public:
    explicit UIEventFilter(QObject *parent = nullptr) : QObject(parent) {}
protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::MouseButtonRelease) {
            auto *button = qobject_cast<QPushButton*>(obj);
            if (button) {
                QString name = button->objectName();
                if (name.isEmpty()) {
                    name = QString("Unnamed %1 (parent: %2)").arg(button->metaObject()->className()).arg(button->parent() ? button->parent()->metaObject()->className() : "none");
                }
                qDebug() << "[UI Interaction] Button clicked:" << name << "Text:" << button->text();
            }
        }
        return QObject::eventFilter(obj, event);
    }
};

// Helper for clean exit from any point in main()
void performCleanup() {
    static bool cleaned = false;
    if (cleaned) return;
    cleaned = true;

    TranslationManager::destroy();
    
    // Clean up global data structures to avoid leaks reported by Valgrind
    delete Configs::profileManager;
    Configs::profileManager = nullptr;
    delete Configs::dataStore;
    Configs::dataStore = nullptr;

    if (DS_cores) {
        DS_cores->quit();
        if (!DS_cores->wait(2000)) {
            DS_cores->terminate();
            DS_cores->wait();
        }
        delete DS_cores;
        DS_cores = nullptr;
    }
}

int main(int argc, char* argv[]) {
    if (RUNNING_ON_VALGRIND) {
        fprintf(stderr, "[DIAGNOSTIC] Deep memory tracing detected (Valgrind is active).\n");
    }
    qInstallMessageHandler(myMessageHandler);
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

#ifdef Q_OS_WIN
    Windows_SetCrashHandler();
#endif
#ifdef Q_OS_LINUX
    setup_crash_handlers();
#endif

    if (args.contains("--crash-now")) {
        printf("DEBUG: Triggering manual SIGSEGV...\n");
        int *p = nullptr;
        *p = 123;
        return 0;
    }

    QApplication a(argc, argv);
    a.installEventFilter(new UIEventFilter(&a)); // Fixed leak: added parent
    a.setQuitOnLastWindowClosed(false);

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

    // Workspace initialization
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
    if (!wd.exists("config")) {
        if (!wd.mkdir("config")) {
            qWarning() << "Failed to create config directory";
        }
    }
    QDir::setCurrent(wd.absoluteFilePath("config"));
    if (!QDir("temp").removeRecursively()) {
        qWarning() << "Failed to remove temp directory";
    } 

#ifdef Q_OS_LINUX
    QApplication::addLibraryPath(QApplication::applicationDirPath() + "/usr/plugins");
#endif

    // Core gRPC service thread
    DS_cores = new QThread;
    DS_cores->start();

    // Single instance check via local socket
    QByteArray hashBytes = QCryptographicHash::hash(wd.absolutePath().toUtf8(), QCryptographicHash::Md5).toBase64(QByteArray::OmitTrailingEquals);
    hashBytes.replace('+', '0').replace('/', '1');
    auto serverName = LOCAL_SERVER_PREFIX + QString::fromUtf8(hashBytes);
    
    {
        QLocalSocket socket;
        socket.connectToServer(serverName);
        if (socket.waitForConnected(250)) {
            socket.disconnectFromServer();
            qInfo() << "Another instance is already running. Exiting.";
            performCleanup();
            return 0;
        }
        // If we failed to connect, but the file exists, it's a stale socket
        if (QLocalServer::removeServer(serverName)) {
            qInfo() << "Cleaned up stale local server socket:" << serverName;
        }
    }

    QIcon::setFallbackSearchPaths(QStringList{ ":/icon" });
    if (QIcon::themeName().isEmpty()) {
        QIcon::setThemeName("breeze");
    }

    QDir dir;
    bool dir_success = true;
    if (!dir.exists("profiles")) dir_success &= dir.mkdir("profiles");
    if (!dir.exists("groups"))   dir_success &= dir.mkdir("groups");
    if (!dir.exists(ROUTES_PREFIX_NAME)) dir_success &= dir.mkdir(ROUTES_PREFIX_NAME);
    if (!dir_success) {
        QMessageBox::critical(nullptr, QObject::tr("Error"), QObject::tr("No permission to write in %1").arg(dir.absolutePath()));
        performCleanup();
        return 1;
    }

    if (QFile::exists("groups/nekobox.json")) {
        QFile::rename("groups/nekobox.json", "configs.json");
    }

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
        performCleanup();
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

    QLocalServer *server = new QLocalServer(qApp);
    server->setSocketOptions(QLocalServer::WorldAccessOption);
    if (!server->listen(serverName)) {
        delete server;
        performCleanup();
        return 1;
    }
    
    QObject::connect(server, &QLocalServer::newConnection, qApp, [server] {
        auto s = server->nextPendingConnection();
        if (s) {
            s->close();
            s->deleteLater();
        }
        MW_dialog_message("", "Raise"); 
    });
    
    QObject::connect(qApp, &QApplication::aboutToQuit, [server, serverName] {
        server->close();
        QLocalServer::removeServer(serverName);
        server->deleteLater();
    });

#ifdef Q_OS_LINUX
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sig_pipe_fd) == 0) {
        auto *notifier = new QSocketNotifier(sig_pipe_fd[1], QSocketNotifier::Read, qApp);
        QObject::connect(notifier, &QSocketNotifier::activated, qApp, [] {
            char a;
            if (::read(sig_pipe_fd[1], &a, 1) > 0) {
                signal_handler((int)a);
            }
        });
        
        struct sigaction sa;
        sa.sa_handler = unix_signal_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sigaction(SIGTERM, &sa, NULL);
        sigaction(SIGINT, &sa, NULL);
    }
#endif

#ifdef Q_OS_WIN
    auto eventFilter = new PowerOffTaskkillFilter(signal_handler);
    a.installNativeEventFilter(eventFilter);
#endif

    UI_InitMainWindow();
    int ret = QApplication::exec();
    
    performCleanup();

    return ret;
}
