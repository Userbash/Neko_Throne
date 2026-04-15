#include <mutex>
#include "include/api/RPC.h"
#include <utility>
#include <mutex>

#include "include/global/Configs.hpp"

#include <QCoreApplication>
#include <QNetworkReply>
#include <QTimer>
#include <QtEndian>
#include <QThread>
#include <QMutex>
#include <QSemaphore>
#include <QAbstractNetworkCache>
#include <QtConcurrent>

namespace QtGrpc {
    const char *GrpcAcceptEncodingHeader = "grpc-accept-encoding";
    const char *AcceptEncodingHeader = "accept-encoding";
    const char *TEHeader = "te";
    const char *GrpcStatusHeader = "grpc-status";
    const char *GrpcStatusMessage = "grpc-message";
    const int GrpcMessageSizeHeaderSize = 5;

    class NoCache : public QAbstractNetworkCache {
    public:
        QNetworkCacheMetaData metaData(const QUrl &) override { return {}; }
        void updateMetaData(const QNetworkCacheMetaData &) override {}
        QIODevice *data(const QUrl &) override { return nullptr; }
        bool remove(const QUrl &) override { return false; }
        [[nodiscard]] qint64 cacheSize() const override { return 0; }
        QIODevice *prepare(const QNetworkCacheMetaData &) override { return nullptr; }
        void insert(QIODevice *) override {}
        void clear() override {}
    };

    class Http2GrpcChannelPrivate {
    private:
        QThread *thread;
        QNetworkAccessManager *nm;

        QString url_base;
        QString serviceName;

        QNetworkReply *post(const QString &method, const QString &service, const QByteArray &args) {
            QUrl callUrl = url_base + "/" + service + "/" + method;

            QNetworkRequest request(callUrl);
            request.setAttribute(QNetworkRequest::Http2DirectAttribute, true);
            request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String{"application/grpc"});
            request.setRawHeader("Cache-Control", "no-store");
            request.setRawHeader("x-client-version", NKR_VERSION);
            request.setRawHeader(GrpcAcceptEncodingHeader, QByteArray{"identity,deflate,gzip"});
            request.setRawHeader(AcceptEncodingHeader, QByteArray{"identity,gzip"});
            request.setRawHeader(TEHeader, QByteArray{"trailers"});

            QByteArray msg(GrpcMessageSizeHeaderSize, '\0');
            *reinterpret_cast<int *>(msg.data() + 1) = qToBigEndian(static_cast<int>(args.size()));
            msg += args;

            return nm->post(request, msg);
        }

        static QByteArray processReply(QPointer<QNetworkReply> networkReply, QNetworkReply::NetworkError &statusCode) {
            if (networkReply.isNull()) {
                statusCode = QNetworkReply::NetworkError::UnknownNetworkError;
                return {};
            }

            statusCode = networkReply->error();
            if (statusCode != QNetworkReply::NoError) {
                MW_show_log(QString("[gRPC] Network Error: %1 (%2)").arg(networkReply->errorString()).arg(static_cast<int>(statusCode)));
                return {};
            }

            if (!networkReply->isReadable()) {
                statusCode = QNetworkReply::NetworkError::ProtocolUnknownError;
                return {};
            }

            auto errCode = networkReply->rawHeader(GrpcStatusHeader).toInt();
            if (errCode != 0) {
                QString errMessage = QLatin1String(networkReply->rawHeader(GrpcStatusMessage));
                MW_show_log(QString("[gRPC] Status Error: %1, Message: %2").arg(errCode).arg(errMessage));
                statusCode = QNetworkReply::NetworkError::ProtocolUnknownError;
                return {};
            }
            statusCode = QNetworkReply::NetworkError::NoError;
            return networkReply->readAll().mid(GrpcMessageSizeHeaderSize);
        }

        QNetworkReply::NetworkError call(const QString &method, const QString &service, const QByteArray &args, QByteArray &qByteArray, int timeout_ms) {
            QPointer<QNetworkReply> networkReply = post(method, service, args);

            if (timeout_ms > 0 && !networkReply.isNull()) {
                auto *abortTimer = new QTimer(nullptr);
                abortTimer->setSingleShot(true);
                abortTimer->setInterval(timeout_ms);
                QObject::connect(abortTimer, &QTimer::timeout, networkReply.data(), &QNetworkReply::abort);
                QObject::connect(networkReply.data(), &QNetworkReply::finished, abortTimer, &QTimer::deleteLater);
                abortTimer->start();
            }

            QEventLoop loop;
            if (!networkReply.isNull()) {
                QObject::connect(networkReply.data(), &QNetworkReply::finished, &loop, &QEventLoop::quit);
                loop.exec();
            }

            auto grpcStatus = QNetworkReply::NetworkError::ProtocolUnknownError;
            qByteArray = processReply(networkReply, grpcStatus);

            if (!networkReply.isNull()) networkReply->deleteLater();
            return grpcStatus;
        }

    public:
        Http2GrpcChannelPrivate(const QString &url_, const QString &serviceName_) {
            url_base = "http://" + url_;
            serviceName = serviceName_;
            thread = new QThread;
            nm = new QNetworkAccessManager();
            nm->setCache(new NoCache);
            nm->moveToThread(thread);
            thread->start();
        }

        ~Http2GrpcChannelPrivate() {
            // Forcefully abort and cleanup pending replies
            for (auto *reply : nm->findChildren<QNetworkReply*>()) {
                reply->abort();
                reply->deleteLater();
            }
            nm->deleteLater();
            thread->quit();
            thread->wait();
            thread->deleteLater();
        }

        QNetworkReply::NetworkError Call(const QString &methodName,
                                         const std::string req, std::vector<uint8_t> &rsp,
                                         int timeout_ms = 15000) {
            if (!Configs::dataStore->core_running) return QNetworkReply::NetworkError(-1919);

            auto requestArray = QByteArray::fromStdString(req);

            // Use shared_ptr to keep semaphore, responseArray, and err alive across async operations
            auto semaphore = std::make_shared<QSemaphore>();
            auto responseArray = std::make_shared<QByteArray>();
            auto err = std::make_shared<QNetworkReply::NetworkError>(QNetworkReply::NetworkError::TimeoutError);

            QMetaObject::invokeMethod(nm, [semaphore, responseArray, err, methodName, requestArray, this, timeout_ms] {
                // Lambda captures shared_ptr by value - keeps objects alive
                *err = call(methodName, serviceName, requestArray, *responseArray, timeout_ms);
                semaphore->release();
            });

            if (!semaphore->tryAcquire(1, timeout_ms > 0 ? timeout_ms + 5000 : 20000)) {
                return QNetworkReply::NetworkError::TimeoutError;
            }

            if (*err != QNetworkReply::NetworkError::NoError) {
                return *err;
            }
            rsp.assign(responseArray->begin(), responseArray->end());
            return QNetworkReply::NetworkError::NoError;
        }
    };
} // namespace QtGrpc

namespace API {

    Client::Client(std::function<void(const QString &)> onError, const QString &target) {
        this->onError = std::move(onError);
        std::lock_guard<std::mutex> lock(channel_mutex);
        this->default_grpc_channel = std::make_shared<QtGrpc::Http2GrpcChannelPrivate>(target, "libcore.LibcoreService");
    }

    Client::~Client() {
        StopPendingOperations();
    }

    void Client::StopPendingOperations() {
        std::lock_guard<std::mutex> lock(channel_mutex);
        default_grpc_channel.reset();
    }

#define NOT_OK(channel_ptr) \
    *rpcOK = false; \
    onError(QString("[gRPC] Call failed. NetworkError code: %1\n").arg(static_cast<int>(status)));

#define GET_CHANNEL(channel_ptr) \
    std::shared_ptr<QtGrpc::Http2GrpcChannelPrivate> channel_ptr; \
    { \
        std::lock_guard<std::mutex> lock(channel_mutex); \
        channel_ptr = default_grpc_channel; \
    } \
    if (!channel_ptr) { *rpcOK = false; return {}; }

#define GET_CHANNEL_VOID(channel_ptr) \
    std::shared_ptr<QtGrpc::Http2GrpcChannelPrivate> channel_ptr; \
    { \
        std::lock_guard<std::mutex> lock(channel_mutex); \
        channel_ptr = default_grpc_channel; \
    } \
    if (!channel_ptr) { *rpcOK = false; return; }

    QString Client::Start(bool *rpcOK, const libcore::LoadConfigReq &request) {
        GET_CHANNEL(channel)
        libcore::ErrorResp reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("Start", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            reply = spb::pb::deserialize<libcore::ErrorResp>(resp);
            *rpcOK = true;
            return reply.error.has_value() ? QString::fromStdString(reply.error.value()) : "";
        } else {
            *rpcOK = false;
            onError(QString("[gRPC] Call failed. NetworkError code: %1\n").arg(static_cast<int>(status)));
            return "";
        }
    }

    QString Client::Stop(bool *rpcOK) {
        GET_CHANNEL(channel)
        libcore::EmptyReq request;
        libcore::ErrorResp reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("Stop", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            reply = spb::pb::deserialize<libcore::ErrorResp>(resp);
            *rpcOK = true;
            return reply.error.has_value() ? QString::fromStdString(reply.error.value()) : "";
        } else {
            *rpcOK = false;
            onError(QString("[gRPC] Call failed. NetworkError code: %1\n").arg(static_cast<int>(status)));
            return "";
        }
    }

    libcore::QueryStatsResp Client::QueryStats() {
        std::shared_ptr<QtGrpc::Http2GrpcChannelPrivate> channel;
        {
            std::lock_guard<std::mutex> lock(channel_mutex);
            channel = default_grpc_channel;
        }
        if (!channel) return {};

        libcore::EmptyReq request;
        libcore::QueryStatsResp reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("QueryStats", spb::pb::serialize<std::string>(request), resp, 500);

        if (status == QNetworkReply::NoError) {
            reply = spb::pb::deserialize<libcore::QueryStatsResp>(resp);
            return reply;
        } else {
            return {};
        }
    }

    libcore::TestResp Client::Test(bool *rpcOK, const libcore::TestReq &request) {
        GET_CHANNEL(channel)
        libcore::TestResp reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("Test", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            reply = spb::pb::deserialize<libcore::TestResp>(resp);
            *rpcOK = true;
            return reply;
        } else {
            *rpcOK = false;
            onError(QString("[gRPC] Call failed. NetworkError code: %1\n").arg(static_cast<int>(status)));
            return {};
        }
    }

    void Client::StopTests(bool *rpcOK) {
        GET_CHANNEL_VOID(channel)
        const libcore::EmptyReq request;
        std::vector<uint8_t> resp;
        auto status = channel->Call("StopTest", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            *rpcOK = true;
        } else {
            NOT_OK(channel)
        }
    }

    libcore::QueryURLTestResponse Client::QueryURLTest(bool *rpcOK)
    {
        GET_CHANNEL(channel)
        libcore::EmptyReq request;
        libcore::QueryURLTestResponse reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("QueryURLTest", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            reply = spb::pb::deserialize<libcore::QueryURLTestResponse>(resp);
            *rpcOK = true;
            return reply;
        } else {
            *rpcOK = false;
            onError(QString("[gRPC] Call failed. NetworkError code: %1\n").arg(static_cast<int>(status)));
            return {};
        }
    }

    QString Client::SetSystemDNS(bool *rpcOK, const bool clear) const {
        std::shared_ptr<QtGrpc::Http2GrpcChannelPrivate> channel;
        {
            std::lock_guard<std::mutex> lock(channel_mutex);
            channel = default_grpc_channel;
        }
        if (!channel) { *rpcOK = false; return {}; }
        libcore::SetSystemDNSRequest request{clear};
        std::vector<uint8_t> resp;
        auto status = channel->Call("SetSystemDNS", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            *rpcOK = true;
            return "";
        } else {
            *rpcOK = false;
            onError(QString("[gRPC] Call failed. NetworkError code: %1\n").arg(static_cast<int>(status)));
            return QString("QNetworkReply error: %1").arg(status);
        }
    }

    libcore::ListConnectionsResp Client::ListConnections() const
    {
        std::shared_ptr<QtGrpc::Http2GrpcChannelPrivate> channel;
        {
            std::lock_guard<std::mutex> lock(channel_mutex);
            channel = default_grpc_channel;
        }
        if (!channel) return {};
        libcore::EmptyReq request;
        libcore::ListConnectionsResp reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("ListConnections", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            reply = spb::pb::deserialize<libcore::ListConnectionsResp>(resp);
            return reply;
        } else {
            MW_show_log(QString("Failed to list connections: QNetworkReply error %1").arg(status));
            return {};
        }
    }

    QString Client::CheckConfig(bool* rpcOK, const QString& config) const
    {
        std::shared_ptr<QtGrpc::Http2GrpcChannelPrivate> channel;
        {
            std::lock_guard<std::mutex> lock(channel_mutex);
            channel = default_grpc_channel;
        }
        if (!channel) { *rpcOK = false; return {}; }
        libcore::LoadConfigReq request{.core_config = config.toStdString()};
        libcore::ErrorResp reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("CheckConfig", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError)
        {
            reply = spb::pb::deserialize<libcore::ErrorResp>(resp);
            *rpcOK = true;
            return reply.error.has_value() ? QString::fromStdString(reply.error.value()) : "";
        } else
        {
            *rpcOK = false;
            onError(QString("[gRPC] Call failed. NetworkError code: %1\n").arg(static_cast<int>(status)));
            return QString("QNetworkReply error: %1").arg(status);
        }
    }

    bool Client::IsPrivileged(bool* rpcOK) const
    {
        std::shared_ptr<QtGrpc::Http2GrpcChannelPrivate> channel;
        {
            std::lock_guard<std::mutex> lock(channel_mutex);
            channel = default_grpc_channel;
        }
        if (!channel) { *rpcOK = false; return false; }
        libcore::EmptyReq request;
        libcore::IsPrivilegedResponse reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("IsPrivileged", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError)
        {
            reply = spb::pb::deserialize<libcore::IsPrivilegedResponse>(resp);
            *rpcOK = true;
            return reply.has_privilege.has_value() ? reply.has_privilege.value() : false;
        } else
        {
            *rpcOK = false;
            onError(QString("[gRPC] Call failed. NetworkError code: %1\n").arg(static_cast<int>(status)));
            return false;
        }
    }

    libcore::SpeedTestResponse Client::SpeedTest(bool *rpcOK, const libcore::SpeedTestRequest &request)
    {
        GET_CHANNEL(channel)
        libcore::SpeedTestResponse reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("SpeedTest", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            reply = spb::pb::deserialize<libcore::SpeedTestResponse>(resp);
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK(channel)
            return {};
        }
    }

    libcore::QuerySpeedTestResponse Client::QueryCurrentSpeedTests(bool *rpcOK)
    {
        GET_CHANNEL(channel)
        const libcore::EmptyReq request;
        libcore::QuerySpeedTestResponse reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("QuerySpeedTest", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            reply = spb::pb::deserialize<libcore::QuerySpeedTestResponse>(resp);
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK(channel)
            return {};
        }
    }

    libcore::QueryCountryTestResponse Client::QueryCountryTestResults(bool* rpcOK)
    {
        GET_CHANNEL(channel)
        const libcore::EmptyReq request;
        libcore::QueryCountryTestResponse reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("QueryCountryTest", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            reply = spb::pb::deserialize<libcore::QueryCountryTestResponse>(resp);
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK(channel)
            return {};
        }
    }

    libcore::IPTestResp Client::IPTest(bool *rpcOK, const libcore::IPTestRequest &request)
    {
        GET_CHANNEL(channel)
        libcore::IPTestResp reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("IPTest", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            reply = spb::pb::deserialize<libcore::IPTestResp>(resp);
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK(channel)
            return {};
        }
    }

    libcore::QueryIPTestResponse Client::QueryIPTest(bool *rpcOK)
    {
        GET_CHANNEL(channel)
        const libcore::EmptyReq request;
        libcore::QueryIPTestResponse reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("QueryIPTest", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            reply = spb::pb::deserialize<libcore::QueryIPTestResponse>(resp);
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK(channel)
            return {};
        }
    }

    libcore::GenWgKeyPairResponse Client::GenWgKeyPair(bool *rpcOK)
    {
        GET_CHANNEL(channel)
        const libcore::EmptyReq request;
        libcore::GenWgKeyPairResponse reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("GenWgKeyPair", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            reply = spb::pb::deserialize<libcore::GenWgKeyPairResponse>(resp);
            *rpcOK = true;
            QString error = QString::fromStdString(reply.error.value());
            if (!error.isEmpty()) {
                MW_show_log(QString("Failed to generate WireGuard key pair:\n") + error);
            }
            return reply;
        } else {
            NOT_OK(channel)
            return {};
        }
    }

} // namespace API
