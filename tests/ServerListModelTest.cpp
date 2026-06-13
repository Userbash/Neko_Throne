#include <QtTest>
#include <memory>
#include "include/ui/models/ServerListModel.hpp"
#include "include/dataStore/ProxyEntity.hpp"

class ServerListModelTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        // Initialization before all tests
    }

    void testRowCount() {
        ServerListModel model;
        QCOMPARE(model.rowCount(), 0);

        auto p1 = std::make_shared<Configs::ProxyEntity>();
        p1->id = 1;
        p1->_bean = std::make_shared<Configs::AbstractBean>(1);
        p1->_bean->name = "Test 1";
        model.appendProfile(p1);
        QCOMPARE(model.rowCount(), 1);
    }

    void testDataRetrieval() {
        ServerListModel model;
        auto p1 = std::make_shared<Configs::ProxyEntity>();
        p1->id = 42;
        p1->_bean = std::make_shared<Configs::AbstractBean>(1);
        p1->_bean->name = "Neko";
        model.appendProfile(p1);

        QModelIndex idx = model.index(0, 0);
        QCOMPARE(model.data(idx, ServerListModel::NameRole).toString(), QString("Neko"));
        QCOMPARE(model.data(idx, ServerListModel::ProfileIdRole).toInt(), 42);
    }

    void testActiveProfile() {
        ServerListModel model;
        auto p1 = std::make_shared<Configs::ProxyEntity>();
        p1->id = 10;
        p1->_bean = std::make_shared<Configs::AbstractBean>(1);
        model.appendProfile(p1);

        model.setActiveProfileId(10);
        QCOMPARE(model.activeProfileId(), 10);
        QCOMPARE(model.data(model.index(0, 0), ServerListModel::IsActiveRole).toBool(), true);
    }

    void testRemoveProfile() {
        ServerListModel model;
        auto p1 = std::make_shared<Configs::ProxyEntity>();
        p1->id = 1;
        p1->_bean = std::make_shared<Configs::AbstractBean>(1);
        model.appendProfile(p1);
        
        model.removeProfile(1);
        QCOMPARE(model.rowCount(), 0);
    }

    void testPerformance10k() {
        ServerListModel model;
        QList<std::shared_ptr<Configs::ProxyEntity>> bigList;
        for (int i = 0; i < 10000; ++i) {
            auto p = std::make_shared<Configs::ProxyEntity>();
            p->id = i;
            p->_bean = std::make_shared<Configs::AbstractBean>(1);
            p->_bean->name = QString("Server %1").arg(i);
            bigList.append(p);
        }
        
        QBENCHMARK {
            model.setProfiles(bigList);
        }
        QCOMPARE(model.rowCount(), 10000);
    }

    void testInvalidIndex() {
        ServerListModel model;
        QCOMPARE(model.data(model.index(0, 0), Qt::DisplayRole), QVariant());
    }

    void cleanupTestCase() {
        // Cleanup after all tests
    }
};

QTEST_MAIN(ServerListModelTest)
#include "ServerListModelTest.moc"
