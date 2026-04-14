#pragma once

#ifndef Q_MOC_RUN
#include "libcore.pb.h"
#endif
#include <QString>
#include <mutex>
#include <memory>

namespace QtGrpc {
    class Http2GrpcChannelPrivate;
}

namespace API {
    class Client {
    public:
        explicit Client(std::function<void(const QString &)> onError, const QString &target);

        ~Client();

        // QString returns is error string

        QString Start(bool *rpcOK, const libcore::LoadConfigReq &request);

        QString Stop(bool *rpcOK);

        void StopPendingOperations();

        libcore::QueryStatsResp QueryStats();

        libcore::TestResp Test(bool *rpcOK, const libcore::TestReq &request);

        void StopTests(bool *rpcOK);

        libcore::QueryURLTestResponse QueryURLTest(bool *rpcOK);

        QString SetSystemDNS(bool *rpcOK, bool clear) const;

        libcore::ListConnectionsResp ListConnections() const;

        QString CheckConfig(bool *rpcOK, const QString& config) const;

        bool IsPrivileged(bool *rpcOK) const;

        libcore::SpeedTestResponse SpeedTest(bool *rpcOK, const libcore::SpeedTestRequest &request);

        libcore::QuerySpeedTestResponse QueryCurrentSpeedTests(bool *rpcOK);

        libcore::QueryCountryTestResponse QueryCountryTestResults(bool *rpcOK);

        libcore::IPTestResp IPTest(bool *rpcOK, const libcore::IPTestRequest &request);

        libcore::QueryIPTestResponse QueryIPTest(bool *rpcOK);

        libcore::GenWgKeyPairResponse GenWgKeyPair(bool *rpcOK);

    private:
        std::function<std::unique_ptr<QtGrpc::Http2GrpcChannelPrivate>()> make_grpc_channel;
        std::shared_ptr<QtGrpc::Http2GrpcChannelPrivate> default_grpc_channel;
        mutable std::mutex channel_mutex;
        std::function<void(const QString &)> onError;
    };

    inline Client *defaultClient;
} // namespace API
