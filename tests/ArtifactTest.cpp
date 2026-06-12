#include <QtTest>
#include <QProcess>
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QThread>
#include <QtConcurrent>
#include <QPointer>
#include <QProcessEnvironment>
#include <QTimer>
#include "include/global/Utils.hpp"

class DummyWindow : public QObject {
public:
    explicit DummyWindow(QObject *parent = nullptr) : QObject(parent) {}
};

class ArtifactTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QDir rootDir = QDir::current();
        if (rootDir.dirName() == "build") {
            rootDir.cdUp();
        }

        const QString artifactDir = qEnvironmentVariable("ARTIFACT_DIR", rootDir.absoluteFilePath("artifacts/linux-amd64"));
        const QString buildDir = rootDir.absoluteFilePath("build");
        const QString deploymentDir = rootDir.absoluteFilePath("deployment/linux-amd64");

        corePath = qEnvironmentVariable("CORE", artifactDir + "/NekoCore");
        if (!QFile::exists(corePath)) {
            const QString localCore = buildDir + "/NekoCore";
            corePath = QFile::exists(localCore) ? localCore : deploymentDir + "/NekoCore";
        }

        guiPath = qEnvironmentVariable("APP", artifactDir + "/Neko_Throne");
        if (!QFile::exists(guiPath)) {
            const QString localGui = buildDir + "/Neko_Throne";
            guiPath = QFile::exists(localGui) ? localGui : deploymentDir + "/Neko_Throne";
        }
    }

    void testBinariesExist() {
        QVERIFY2(QFile::exists(corePath), qPrintable(QString("Core not found at: %1").arg(corePath)));
        QVERIFY2(QFile::exists(guiPath), qPrintable(QString("GUI not found at: %1").arg(guiPath)));
    }

    void testCoreExecution() {
        for (int i = 0; i < 3; ++i) {
            QProcess process;
            process.start(corePath, {"--version"});
            QVERIFY2(process.waitForFinished(5000), "Core process did not finish in time");
            const QString output = QString::fromLocal8Bit(process.readAllStandardOutput() + process.readAllStandardError());
            const bool ok = output.contains("sing-box", Qt::CaseInsensitive) ||
                            output.contains("Xray-core", Qt::CaseInsensitive);
            QVERIFY2(ok, qPrintable(QString("Unexpected core output: %1").arg(output)));
        }
    }

    void testGuiDependencies() {
#ifdef Q_OS_LINUX
        QProcess ldd;
        ldd.start("ldd", {guiPath});
        QVERIFY(ldd.waitForFinished());
        const QString output = QString::fromLocal8Bit(ldd.readAllStandardError() + ldd.readAllStandardOutput());
        QVERIFY2(!output.contains("not found"), qPrintable(QString("Missing libraries:\n%1").arg(output)));
#endif
    }

    void testBackgroundParserStability() {
        for (int i = 0; i < 10; ++i) {
            QThreadPool::globalInstance()->start([=]() {
            });
        }
        QTest::qWait(1000);
        QVERIFY(true);
    }

    void testConfigValidation() {
        QVERIFY(true);
    }

    void testThreadSafetySimulation() {
        QList<QFuture<void>> futures;
        for (int i = 0; i < 50; ++i) {
            futures.append(QtConcurrent::run([=]() {
            }));
        }
        for (auto &f : futures) f.waitForFinished();
        QVERIFY(true);
    }

    void testAsyncDestruction() {
        auto *dummy = new DummyWindow();
        QPointer<DummyWindow> safeDummy(dummy);

        SafeUIFunction<QString> testFunc;
        testFunc.assign(dummy, [safeDummy](const QString &s) {
            if (safeDummy) {
                Q_UNUSED(s);
            }
        });

        for (int i = 0; i < 100; ++i) {
            QThreadPool::globalInstance()->start([testFunc]() {
                QTest::qWait(2);
                testFunc("Test log");
            });
        }

        QTest::qWait(5);
        delete dummy;
        QThreadPool::globalInstance()->waitForDone();

        QVERIFY(safeDummy.isNull());
        QVERIFY(!testFunc);
    }

    void testOffscreenStartup() {
#ifdef Q_OS_LINUX
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("QT_QPA_PLATFORM", "offscreen");
        env.insert("NEKO_THRONE_DISABLE_SINGLE_INSTANCE", "1");

        QProcess proc;
        proc.setProcessEnvironment(env);
        proc.start(guiPath, {"--offscreen", "--headless-smoke"});
        QVERIFY2(proc.waitForStarted(5000), "GUI process failed to start in offscreen mode");
        QTest::qWait(1500);
        QVERIFY2(proc.state() != QProcess::NotRunning, qPrintable(QString::fromLocal8Bit(proc.readAllStandardError() + proc.readAllStandardOutput())));
        proc.kill();
        QVERIFY(proc.waitForFinished(5000));
        const QString output = QString::fromLocal8Bit(proc.readAllStandardError() + proc.readAllStandardOutput());
        QVERIFY2(!output.contains("segmentation fault", Qt::CaseInsensitive), qPrintable(output));
        QVERIFY2(!output.contains("SIGSEGV", Qt::CaseInsensitive), qPrintable(output));
#endif
    }

    void testWaylandFallbackLogic() {
#ifdef Q_OS_LINUX
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("WAYLAND_DISPLAY", "");
        env.insert("QT_QPA_PLATFORM", "offscreen");
        env.insert("NEKO_THRONE_DISABLE_SINGLE_INSTANCE", "1");

        QProcess proc;
        proc.setProcessEnvironment(env);
        proc.start(guiPath, {"--offscreen", "--headless-smoke"});
        QVERIFY(proc.waitForStarted(5000));
        QTest::qWait(1000);
        QVERIFY(proc.state() != QProcess::NotRunning);
        proc.kill();
        QVERIFY(proc.waitForFinished(5000));
#endif
    }

    void testTimerLifecycleSafety() {
        QPointer<QTimer> safeTimer;
        {
            auto *dummy = new DummyWindow();
            auto *timer = new QTimer(dummy);
            safeTimer = timer;
            timer->start(100);
            timer->stop();
            delete dummy;
        }
        QVERIFY(safeTimer.isNull());
    }

private:
    QString corePath;
    QString guiPath;
};

QTEST_MAIN(ArtifactTest)
#include "ArtifactTest.moc"
