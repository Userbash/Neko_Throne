#include "include/configs/outbounds/xrayVmess.h"
#include "include/configs/sub/clash.hpp"

#include <QJsonArray>
#include <QUrlQuery>

#include "include/configs/common/utils.h"

namespace Configs {
    bool xrayVmess::ParseFromLink(const QString &link) {
        // Try V2RayN format first (base64 encoded JSON)
        QString linkN = DecodeB64IfValid(SubStrAfter(link, "vmess://"));
        if (!linkN.isEmpty()) {
            auto objN = QString2QJsonObject(linkN);
            if (!objN.isEmpty()) {
                uuid = objN["id"].toString();
                server = objN["add"].toString();
                server_port = objN["port"].toVariant().toInt();
                name = objN["ps"].toString();
                alter_id = objN["aid"].toVariant().toInt();
                security = objN["scy"].toString();
                if (security.isEmpty()) security = "auto";
                
                streamSetting->ParseFromJson(objN);
                return !(uuid.isEmpty() || server.isEmpty());
            }
        }

        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

        outbound::ParseFromLink(link);
        uuid = url.userName();
        security = GetQueryValue(query, "encryption", "auto");
        if (query.hasQueryItem("alterId")) alter_id = query.queryItemValue("alterId").toInt();
        
        streamSetting->ParseFromLink(link);
        multiplex->ParseFromLink(link);
        return !(uuid.isEmpty() || server.isEmpty());
    }

    bool xrayVmess::ParseFromJson(const QJsonObject &object) {
        if (object.isEmpty() || object["protocol"].toString() != "vmess") return false;
        if (object.contains("tag")) name = object["tag"].toString();
        if (auto settingsObj = object["settings"].toObject(); !settingsObj.isEmpty()) {
            if (auto vnext = settingsObj["vnext"].toArray(); !vnext.isEmpty()) {
                auto entry = vnext[0].toObject();
                server = entry["address"].toString();
                server_port = entry["port"].toInt();
                if (auto users = entry["users"].toArray(); !users.isEmpty()) {
                    auto user = users[0].toObject();
                    uuid = user["id"].toString();
                    security = user["security"].toString();
                    alter_id = user["alterId"].toInt();
                }
            }
        }
        if (auto streamSettings = object["streamSettings"].toObject(); !streamSettings.isEmpty()) {
            streamSetting->ParseFromJson(streamSettings);
        }
        if (auto muxObj = object["mux"].toObject(); !muxObj.isEmpty()) {
            multiplex->ParseFromJson(muxObj);
        }
        return true;
    }

    bool xrayVmess::ParseFromClash(const clash::Proxies& object) {
        if (object.type != "vmess") return false;
        outbound::ParseFromClash(object);
        uuid = QString::fromStdString(object.uuid);
        security = QString::fromStdString(object.cipher);
        alter_id = object.alterId;
        streamSetting->ParseFromClash(object);
        multiplex->ParseFromClash(object);
        return true;
    }

    QString xrayVmess::ExportToLink() {
        QUrl url;
        QUrlQuery query;
        url.setScheme("vmess");
        url.setUserName(uuid);
        url.setHost(server);
        url.setPort(server_port);
        if (!name.isEmpty()) url.setFragment(name);

        if (security != "auto") query.addQueryItem("encryption", security);
        if (alter_id > 0) query.addQueryItem("alterId", QString::number(alter_id));

        mergeUrlQuery(query, streamSetting->ExportToLink());
        mergeUrlQuery(query, multiplex->ExportToLink());

        if (!query.isEmpty()) url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QJsonObject xrayVmess::ExportToJson() {
        QJsonObject object;
        if (!name.isEmpty()) object["tag"] = name;
        object["protocol"] = "vmess";
        QJsonObject settings;
        QJsonObject userObj;
        userObj["id"] = uuid;
        userObj["security"] = security;
        userObj["alterId"] = alter_id;
        QJsonObject vnextEntry;
        vnextEntry["address"] = server;
        vnextEntry["port"] = server_port;
        vnextEntry["users"] = QJsonArray{userObj};
        settings["vnext"] = QJsonArray{vnextEntry};
        object["settings"] = settings;
        if (auto streamObj = streamSetting->ExportToJson(); !streamObj.isEmpty()) object["streamSettings"] = streamObj;
        if (auto muxObj = multiplex->ExportToJson(); !muxObj.isEmpty()) object["mux"] = muxObj;
        return object;
    }

    BuildResult xrayVmess::Build() {
        QJsonObject object;
        object["type"] = "socks";
        object["server"] = "127.0.0.1";
        return {object, ""};
    }

    BuildResult xrayVmess::BuildXray() {
        QJsonObject object;
        object["protocol"] = "vmess";
        QJsonObject userObj;
        userObj["id"] = uuid;
        userObj["security"] = security;
        userObj["alterId"] = alter_id;
        QJsonObject vnextEntry;
        vnextEntry["address"] = server;
        vnextEntry["port"] = server_port;
        vnextEntry["users"] = QJsonArray{userObj};
        object["settings"] = QJsonObject{{"vnext", QJsonArray{vnextEntry}}};
        if (auto streamObj = streamSetting->Build().object; !streamObj.isEmpty()) object["streamSettings"] = streamObj;
        if (auto muxObj = multiplex->Build().object; !muxObj.isEmpty()) object["mux"] = muxObj;
        return {object, ""};
    }
}
