#include <include/dataStore/ProxyEntity.hpp>

namespace Configs
{
    ProxyEntity::ProxyEntity(Configs::outbound *outbound, Configs::AbstractBean *bean, const QString &type_) : JsonStore()
    {
        schema_version = CURRENT_SCHEMA_VERSION;
        if (type_ != nullptr) this->type = type_;

        _add(new configItem("type", &type, itemType::string));
        _add(new configItem("id", &id, itemType::integer));
        _add(new configItem("gid", &gid, itemType::integer));
        _add(new configItem("yc", &latency, itemType::integer));
        _add(new configItem("dl", &dl_speed, itemType::string));
        _add(new configItem("ul", &ul_speed, itemType::string));
        _add(new configItem("report", &full_test_report, itemType::string));
        _add(new configItem("country", &test_country, itemType::string));

        if (bean != nullptr) {
            // Use make_shared for safer ownership transfer, fallback to direct construction if needed
            this->_bean = std::make_shared<Configs::AbstractBean>(*bean);
            auto beanStore = dynamic_cast<JsonStore *>(bean);
            if (beanStore != nullptr) {
                _add(new configItem("bean", beanStore, itemType::jsonStore));
            }
        }

        if (outbound != nullptr) {
            // FIXED: Don't copy - preserve derived class type!
            // Use std::move to avoid copying and maintain polymorphic type
            // This prevents object slicing that was causing empty {} outbounds
            this->outbound = std::shared_ptr<Configs::outbound>(outbound);
            auto outboundStore = dynamic_cast<JsonStore *>(outbound);
            if (outboundStore != nullptr) {
                _add(new configItem("outbound", outboundStore, itemType::jsonStore));
            }
            auto trafficStore = dynamic_cast<JsonStore *>(traffic_data.get());
            if (trafficStore != nullptr) {
                _add(new configItem("traffic", trafficStore, itemType::jsonStore));
            }
        }
    }

    QString ProxyEntity::DisplayTestResult() const {
        QString result;
        if (latency < 0) {
            result = "Unavailable";
        } else if (latency > 0) {
            if (!test_country.isEmpty()) result += UNICODE_LRO + CountryCodeToFlag(test_country) + " ";
            result += QString("%1 ms").arg(latency);
        }
        if (!dl_speed.isEmpty() && dl_speed != "N/A") result += " ↓" + dl_speed;
        if (!ul_speed.isEmpty() && ul_speed != "N/A") result += " ↑" + ul_speed;
        return result;
    }

    QColor ProxyEntity::DisplayLatencyColor() const {
        if (latency < 0) {
            return Qt::darkGray;
        } else if (latency > 0) {
            if (latency <= 100) {
                return Qt::darkGreen;
            } else if (latency <= 300)
            {
                return Qt::darkYellow;
            } else {
                return Qt::red;
            }
        } else {
            return {};
        }
    }

    void ProxyEntity::RunMigrations(QJsonObject &root) {
        int version = root.contains("schema_version") ? root["schema_version"].toInt() : 1;
        if (version >= CURRENT_SCHEMA_VERSION) return;

        // V1 -> V2 Migration
        if (root.contains("type")) {
            QString type_str = root["type"].toString();
            if (type_str == "vless" || type_str == "xrayvless") {
                if (root.contains("outbound")) {
                    QJsonObject outbound_obj = root["outbound"].toObject();
                    bool needs_vision = false;
                    bool changed = false;

                    if (type_str == "vless") {
                        // Sing-box VLESS
                        if (outbound_obj.contains("tls")) {
                            QJsonObject tls = outbound_obj["tls"].toObject();
                            bool tlsEnabled = tls["enabled"].toBool();
                            bool realityEnabled = false;
                            if (tls.contains("reality")) {
                                realityEnabled = tls["reality"].toObject()["enabled"].toBool();
                            }
                            if (tlsEnabled || realityEnabled) {
                                needs_vision = true;
                            }
                        }
                        // XHTTP -> http (v2) for Sing-box
                        if (outbound_obj.contains("transport")) {
                            QJsonObject transport = outbound_obj["transport"].toObject();
                            if (transport["type"].toString() == "xhttp") {
                                transport["type"] = "http";
                                // Sing-box http transport uses "version" field
                                transport["version"] = "2";
                                outbound_obj["transport"] = transport;
                                changed = true;
                            }
                        }
                    } else {
                        // Xray VLESS
                        if (outbound_obj.contains("streamSetting")) {
                            QJsonObject ss = outbound_obj["streamSetting"].toObject();
                            QString security = ss["security"].toString();
                            if (security == "tls" || security == "reality") {
                                needs_vision = true;
                            }
                            // XHTTP migration for Xray
                            if (ss["network"].toString() == "xhttp") {
                                ss["network"] = "http";
                                QJsonObject xhttpSettings = ss["xhttpSettings"].toObject();
                                xhttpSettings["version"] = "2";
                                ss["httpSettings"] = xhttpSettings;
                                ss.remove("xhttpSettings");
                                outbound_obj["streamSetting"] = ss;
                                changed = true;
                            }
                        }
                    }

                    if (needs_vision) {
                        if (!outbound_obj.contains("flow") || outbound_obj["flow"].toString().isEmpty()) {
                            outbound_obj["flow"] = "xtls-rprx-vision";
                            changed = true;
                        }
                    }

                    if (changed) {
                        root["outbound"] = outbound_obj;
                    }
                }
            }
        }

        root["schema_version"] = CURRENT_SCHEMA_VERSION;
        this->schema_version = CURRENT_SCHEMA_VERSION;
    }
}
