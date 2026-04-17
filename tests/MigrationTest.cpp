#include <QtTest>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include "include/dataStore/ProxyEntity.hpp"

class MigrationTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QDir().mkpath("route_profiles");
        // ProfileManager::LoadManager expects route_profiles directory
    }

    void cleanupTestCase() {
        QDir("route_profiles").removeRecursively();
    }

    void testVlessMigration() {
        // 1. Create a V1 VLESS profile (Sing-box format)
        QJsonObject outbound;
        outbound["type"] = "vless";
        QJsonObject tls;
        tls["enabled"] = true;
        outbound["tls"] = tls;
        // flow is missing

        QJsonObject root;
        root["type"] = "vless";
        root["outbound"] = outbound;
        // schema_version is missing (defaults to 1)

        QString path = "test_v1_vless.json";
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(root).toJson());
            file.close();
        }

        // 2. Test manual migration logic via ProxyEntity constructor
        // We use ProxyEntity directly because LoadProxyEntity is private
        // and we want to test the RunMigrations call.
        Configs::ProxyEntity ent(nullptr, nullptr, "vless");
        ent.fn = path;
        ent.Load(); // This calls FromJson -> RunMigrations

        // 3. Verify migration
        QCOMPARE(ent.schema_version, Configs::CURRENT_SCHEMA_VERSION);
        
        auto vless = ent.VLESS();
        QVERIFY(vless != nullptr);
        QCOMPARE(vless->flow, QString("xtls-rprx-vision"));

        // 4. Verify file on disk was updated
        QFile updatedFile(path);
        if (updatedFile.open(QIODevice::ReadOnly)) {
            QJsonObject updatedRoot = QJsonDocument::fromJson(updatedFile.readAll()).object();
            QCOMPARE(updatedRoot["schema_version"].toInt(), Configs::CURRENT_SCHEMA_VERSION);
            QCOMPARE(updatedRoot["outbound"].toObject()["flow"].toString(), QString("xtls-rprx-vision"));
        }
        QFile::remove(path);
    }

    void testXrayVlessMigration() {
        // 1. Create a V1 Xray VLESS profile
        QJsonObject outbound;
        QJsonObject ss;
        ss["security"] = "reality";
        ss["network"] = "xhttp";
        QJsonObject xhttpSettings;
        xhttpSettings["path"] = "/test";
        ss["xhttpSettings"] = xhttpSettings;
        
        outbound["streamSetting"] = ss;
        // flow is missing

        QJsonObject root;
        root["type"] = "xrayvless";
        root["outbound"] = outbound;

        QString path = "test_v1_xray.json";
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(root).toJson());
            file.close();
        }

        // 2. Load it
        Configs::ProxyEntity ent(nullptr, nullptr, "xrayvless");
        ent.fn = path;
        ent.Load();

        // 3. Verify migration
        QCOMPARE(ent.schema_version, Configs::CURRENT_SCHEMA_VERSION);
        
        auto xvless = ent.XrayVLESS();
        QVERIFY(xvless != nullptr);
        QCOMPARE(xvless->flow, QString("xtls-rprx-vision"));
        
        // Verify XHTTP migration in the saved JSON (since C++ model might not have all fields)
        QFile updatedFile(path);
        if (updatedFile.open(QIODevice::ReadOnly)) {
            QJsonObject updatedRoot = QJsonDocument::fromJson(updatedFile.readAll()).object();
            QJsonObject ssObj = updatedRoot["outbound"].toObject()["streamSetting"].toObject();
            QCOMPARE(ssObj["network"].toString(), QString("http"));
            QCOMPARE(ssObj["httpSettings"].toObject()["version"].toString(), QString("2"));
        }
        QFile::remove(path);
    }

    void testAtomicSaveFailureSimulation() {
        QString path = "atomic.json";
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write("original content");
            file.close();
        }

        // QSaveFile creates a temporary file and only replaces original on commit()
        QSaveFile saveFile(path);
        QVERIFY(saveFile.open(QIODevice::WriteOnly));
        saveFile.write("new content");
        
        // Before commit, the original should still be "original content"
        QFile check(path);
        if (check.open(QIODevice::ReadOnly)) {
            QCOMPARE(check.readAll(), QByteArray("original content"));
            check.close();
        }
        
        // If we don't call commit, the original stays intact
        // saveFile goes out of scope and cancels automatically
    }
};

QTEST_MAIN(MigrationTest)
#include "MigrationTest.moc"
