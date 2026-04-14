#include "include/configs/common/utils.h"

namespace Configs
{
    void mergeUrlQuery(QUrlQuery& baseQuery, const QString& strQuery)
    {
        QUrlQuery query = QUrlQuery(strQuery);
        for (const auto& item : query.queryItems())
        {
            baseQuery.addQueryItem(item.first, item.second);
        }
    }

    void mergeJsonObjects(QJsonObject& baseObject, const QJsonObject& obj)
    {
        for (const auto& key : obj.keys())
        {
            baseObject[key] = obj[key];
        }
    }

    QStringList jsonObjectToQStringList(const QJsonObject& obj)
    {
        auto result = QStringList();
        for (const auto& key : obj.keys())
        {
            result << key << obj[key].toString();
        }
        return result;
    }

    QJsonObject qStringListToJsonObject(const QStringList& list)
    {
        auto result = QJsonObject();
        if (list.count() %2 != 0)
        {
            qDebug() << "QStringList of odd length in qStringListToJsonObject:" << list;
            return result;
        }
        for (int i=0;i<list.size();i+=2)
        {
            result[list[i]] = list[i+1];
        }
        return result;
    }

    // TODO add setting items and use them here
    bool useXrayCore(const QString& link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

        auto type = query.queryItemValue("type").toLower();
        auto net = query.queryItemValue("net").toLower();
        auto network = query.queryItemValue("network").toLower();
        auto security = query.queryItemValue("security").toLower();
        auto encryption = query.queryItemValue("encryption").toLower();

        if (type == "xhttp" || net == "xhttp" || network == "xhttp"
            || security == "reality"
            || (encryption != "none" && !encryption.isEmpty())
            || query.queryItemValue("extra") != "") return true;

        // VMess base64 parsing (some clients encode everything in base64)
        if (link.startsWith("vmess://")) {
            auto b64 = link.mid(8);
            auto json = QJsonDocument::fromJson(QByteArray::fromBase64(b64.toUtf8())).object();
            if (!json.isEmpty()) {
                auto jnet = json["net"].toString().toLower();
                auto jtls = json["tls"].toString().toLower();
                auto jsc = json["sc"].toString().toLower();
                if (jnet == "xhttp" || jtls == "reality" || jsc == "reality") return true;
            }
        }

        return false;
    }

    QString getHeadersString(QStringList headers) {
        QString result;
        if (headers.length()%2 != 0) {
            return "";
        }
        for (int i=0;i<headers.length();i+=2) {
            result += headers[i]+"=";
            result += "\""+headers[i+1]+"\" ";
        }
        return result;
    }

    QStringList parseHeaderPairs(const QString& rawHeader) {
        bool inQuote = false;
        QString curr;
        QStringList list;
        for (const auto &ch: rawHeader) {
            if (inQuote) {
                if (ch == '"') {
                    inQuote = false;
                    list << curr;
                    curr = "";
                    continue;
                } else {
                    curr += ch;
                    continue;
                }
            }
            if (ch == '"') {
                inQuote = true;
                continue;
            }
            if (ch == ' ') {
                if (!curr.isEmpty()) {
                    list << curr;
                    curr = "";
                }
                continue;
            }
            if (ch == '=') {
                if (!curr.isEmpty()) {
                    list << curr;
                    curr = "";
                }
                continue;
            }
            curr+=ch;
        }
        if (!curr.isEmpty()) list<<curr;

        if (list.size()%2 != 0) {
            return {};
        }

        return list;
    }
}
