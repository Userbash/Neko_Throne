#include <QtTest>
#include <QProcess>
#include <QCoreApplication>
#include <QFile>
#include <QDir>

class ArtifactTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        // Пути к артефактам. Тест запускается из папки build/
        QDir rootDir = QDir::current();
        if (rootDir.dirName() == "build") {
            rootDir.cdUp();
        }
        
        // Сначала ищем в папке сборки (для локального ctest)
        QString localCore = rootDir.absoluteFilePath("build/NekoCore");
        if (QFile::exists(localCore)) {
            corePath = localCore;
        } else {
            // Фолбэк на деплой-путь
            corePath = rootDir.absoluteFilePath("deployment/linux-amd64/NekoCore");
        }
        
        guiPath = rootDir.absoluteFilePath("build/Neko_Throne");
    }

    // Тест 1: Проверка существования файлов
    void testBinariesExist() {
        QVERIFY2(QFile::exists(corePath), qPrintable(QString("Core not found at: %1").arg(corePath)));
        QVERIFY2(QFile::exists(guiPath), qPrintable(QString("GUI not found at: %1").arg(guiPath)));
    }

    // Тест 2: Проверка версии ядра (Go интеграция)
    void testCoreExecution() {
        QProcess process;
        process.start(corePath, {"--version"});
        QVERIFY(process.waitForFinished());
        QString output = process.readAllStandardOutput();
        // Принимаем как sing-box так и Xray-core
        bool ok = output.contains("sing-box", Qt::CaseInsensitive) || 
                  output.contains("Xray-core", Qt::CaseInsensitive);
        QVERIFY2(ok, qPrintable(QString("Unexpected core output: %1").arg(output)));
    }

    // Тест 3: Проверка линковки GUI (Qt зависимости)
    void testGuiDependencies() {
#ifdef Q_OS_LINUX
        QProcess ldd;
        ldd.start("ldd", {guiPath});
        QVERIFY(ldd.waitForFinished());
        QString output = ldd.readAllStandardError() + ldd.readAllStandardOutput();
        QVERIFY2(!output.contains("not found"), qPrintable(QString("Missing libraries:\n%1").arg(output)));
#endif
    }

    // Тест 4: Валидация базовой конфигурации
    void testConfigValidation() {
        // Здесь можно добавить проверку парсинга JSON через ваши классы (например, DataStore)
        QVERIFY(true); 
    }

private:
    QString corePath;
    QString guiPath;
};

QTEST_MAIN(ArtifactTest)
#include "ArtifactTest.moc"
