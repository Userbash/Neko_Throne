#include "include/configs/outbounds/vless.h"
#include "include/configs/sub/clash.hpp"

#include <QUrlQuery>
#include <QJsonArray>
#include <QJsonObject>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"

namespace Configs {
    bool vless::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

        outbound::ParseFromLink(link);
        uuid = url.userName();
        if (server_port == 0) server_port = 443;

        flow = GetQueryValue(query, "flow", "");

        transport->ParseFromLink(link);
        
        tls->ParseFromLink(link);
        if (!tls->server_name.isEmpty()) {
            tls->enabled = true;
        }
        
        packet_encoding = GetQueryValue(query, "packetEncoding", "xudp");
        multiplex->ParseFromLink(link);

        return !(uuid.isEmpty() || server.isEmpty());
    }

    bool vless::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "vless") return false;
        outbound::ParseFromJson(object);
        if (object.contains("uuid")) uuid = object["uuid"].toString();
        if (object.contains("flow")) flow = object["flow"].toString();
        if (object.contains("packet_encoding")) packet_encoding = object["packet_encoding"].toString();
        if (object.contains("tls")) tls->ParseFromJson(object["tls"].toObject());
        if (object.contains("transport")) transport->ParseFromJson(object["transport"].toObject());
        if (object.contains("multiplex")) multiplex->ParseFromJson(object["multiplex"].toObject());
        return true;
    }

    bool vless::ParseFromClash(const clash::Proxies& object)
    {
        if (object.type != "vless") return false;
        outbound::ParseFromClash(object);
        uuid = QString::fromStdString(object.uuid);
        if (!object.flow.empty()) flow = QString::fromStdString(object.flow);
        if (!object.packet_encoding.empty()) packet_encoding = QString::fromStdString(object.packet_encoding);

        tls->ParseFromClash(object);
        transport->ParseFromClash(object);
        multiplex->ParseFromClash(object);
        return true;
    }

    QString vless::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setScheme("vless");
        url.setUserName(uuid);
        url.setHost(server);
        url.setPort(server_port);
        if (!name.isEmpty()) url.setFragment(name);

        query.addQueryItem("encryption", "none");
        if (!flow.isEmpty()) query.addQueryItem("flow", flow);

        mergeUrlQuery(query, tls->ExportToLink());
        mergeUrlQuery(query, transport->ExportToLink());
        mergeUrlQuery(query, multiplex->ExportToLink());
        
        if (!packet_encoding.isEmpty()) query.addQueryItem("packetEncoding", packet_encoding);
        
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QJsonObject vless::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "vless";
        mergeJsonObjects(object, outbound::ExportToJson());
        if (!uuid.isEmpty()) object["uuid"] = uuid;
        if (!flow.isEmpty()) object["flow"] = flow;
        if (!packet_encoding.isEmpty()) object["packet_encoding"] = packet_encoding;
        if (tls->enabled) object["tls"] = tls->ExportToJson();
        if (!transport->type.isEmpty()) object["transport"] = transport->ExportToJson();
        if (multiplex->enabled) object["multiplex"] = multiplex->ExportToJson();
        return object;
    }

    BuildResult vless::Build()
    {
        QJsonObject object;
        object["type"] = "vless";
        mergeJsonObjects(object, outbound::Build().object);
        if (!uuid.isEmpty()) object["uuid"] = uuid;
        if (!flow.isEmpty()) object["flow"] = flow;
        if (!packet_encoding.isEmpty()) object["packet_encoding"] = packet_encoding;
        if (tls->enabled) object["tls"] = tls->Build().object;
        if (!transport->type.isEmpty()) object["transport"] = transport->Build().object;
        if (auto obj = multiplex->Build().object; !obj.isEmpty()) object["multiplex"] = obj;
        return {object, ""};
    }

    BuildResult vless::BuildXray()
    {
        QJsonObject object;
        object["protocol"] = "vless";
        QJsonObject userObj;
        userObj["id"] = uuid;
        userObj["encryption"] = "none";
        if (!flow.isEmpty()) userObj["flow"] = flow;
        QJsonObject vnextEntry;
        vnextEntry["address"] = server;
        vnextEntry["port"] = server_port;
        vnextEntry["users"] = QJsonArray{userObj};
        object["settings"] = QJsonObject{{"vnext", QJsonArray{vnextEntry}}};

        QJsonObject streamSettings;
        QString network = transport->type;
        if (network == "http") network = "h2";
        if (network.isEmpty()) network = "tcp";
        streamSettings["network"] = network;

        if (tls->enabled) {
            if (tls->reality->enabled) {
                streamSettings["security"] = "reality";
                QJsonObject realitySettings;
                realitySettings["serverName"] = tls->server_name;
                realitySettings["publicKey"] = tls->reality->public_key;
                realitySettings["shortId"] = tls->reality->short_id;
                realitySettings["fingerprint"] = tls->utls->fingerPrint;
                streamSettings["realitySettings"] = realitySettings;
            } else {
                streamSettings["security"] = "tls";
                QJsonObject tlsSettings;
                tlsSettings["serverName"] = tls->server_name;
                tlsSettings["allowInsecure"] = tls->insecure;
                if (!tls->alpn.isEmpty()) tlsSettings["alpn"] = QJsonArray::fromStringList(tls->alpn);
                tlsSettings["fingerprint"] = tls->utls->fingerPrint;
                streamSettings["tlsSettings"] = tlsSettings;
            }
        }

        if (network == "ws") {
            QJsonObject wsSettings;
            wsSettings["path"] = transport->path;
            if (!transport->host.isEmpty()) wsSettings["headers"] = QJsonObject{{"Host", transport->host}};
            streamSettings["wsSettings"] = wsSettings;
        } else if (network == "h2") {
            QJsonObject httpSettings;
            httpSettings["path"] = transport->path;
            if (!transport->host.isEmpty()) httpSettings["host"] = QJsonArray{transport->host};
            streamSettings["httpSettings"] = httpSettings;
        } else if (network == "grpc") {
            QJsonObject grpcSettings;
            grpcSettings["serviceName"] = transport->service_name;
            streamSettings["grpcSettings"] = grpcSettings;
        } else if (network == "xhttp") {
            QJsonObject xhttpSettings;
            xhttpSettings["path"] = transport->path;
            if (!transport->host.isEmpty()) xhttpSettings["host"] = transport->host;
            streamSettings["xhttpSettings"] = xhttpSettings;
        }

        object["streamSettings"] = streamSettings;
        return {object, ""};
    }

    QString vless::DisplayType()
    {
        return "VLESS";
    }
}
