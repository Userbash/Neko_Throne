#pragma once
#include "include/configs/common/Outbound.h"
#include "include/configs/common/xrayMultiplex.h"
#include "include/configs/common/xrayStreamSetting.h"

namespace Configs {
    class xrayVmess : public outbound {
        public:
        QString uuid;
        QString security = "auto";
        int alter_id = 0;
        std::shared_ptr<xrayStreamSetting> streamSetting = std::make_shared<xrayStreamSetting>();
        std::shared_ptr<xrayMultiplex> multiplex = std::make_shared<xrayMultiplex>();

        xrayVmess() : outbound() {
            _add(new configItem("uuid", &uuid, string));
            _add(new configItem("security", &security, string));
            _add(new configItem("alter_id", &alter_id, integer));
            _add(new configItem("streamSetting", dynamic_cast<JsonStore *>(streamSetting.get()), jsonStore));
            _add(new configItem("multiplex", dynamic_cast<JsonStore *>(multiplex.get()), jsonStore));
        }

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        bool ParseFromClash(const clash::Proxies& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
        BuildResult BuildXray() override;

        std::shared_ptr<xrayStreamSetting> GetXrayStream() override { return streamSetting; }
        std::shared_ptr<xrayMultiplex> GetXrayMultiplex() override { return multiplex; }

        QString DisplayType() override {
            return "VMess (Xray)";
        }
        bool IsXray() override {
           return true;
        }
        bool MustXray() override {
           return true;
        }
    };
}
