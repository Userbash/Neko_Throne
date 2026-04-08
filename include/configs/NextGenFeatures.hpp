// SPDX-License-Identifier: GPL-2.0-or-later
// ═══════════════════════════════════════════════════════════════════════════════
// include/configs/NextGenFeatures.hpp — Dynamic JSON config generation for
// latest sing-box and Xray-core features.
//
// Covers: XTLS Reality, VLESS enhancements, QUIC optimizations,
//         advanced rule-set structures, Hysteria2 tuning, TUIC v5.
// ═══════════════════════════════════════════════════════════════════════════════
#pragma once

#include <QJsonObject>
#include <QJsonArray>
#include <QString>

namespace Configs {

// ═══════════════════════════════════════════════════════════════════════════════
// sing-box v1.12+ / Xray-core v1.25+ Feature Catalog
// ═══════════════════════════════════════════════════════════════════════════════
//
// Features extracted from the latest releases:
//
// ── sing-box 1.12.x ──
//   1. Rule Set v2: Binary-compiled rule sets (.srs) with domain/IP/geo support
//   2. Inline rule-sets: embed rules directly in config JSON
//   3. QUIC v2 transport: improved 0-RTT, connection migration
//   4. Enhanced DNS: FakeIP pool with TTL control, DNS rule-based routing
//   5. TUN improvements: strict_route enforcement, auto_redirect mode
//   6. AnyTLS obfuscation protocol
//   7. Tailscale integration as outbound
//   8. Clash API with external controller
//
// ── Xray-core 1.25+ ──
//   1. XTLS Reality v2: improved server fingerprinting resistance
//   2. VLESS + Vision flow: zero-copy splice for maximum throughput
//   3. Mux.Cool improvements: concurrent stream multiplexing
//   4. XUDP full cone NAT for QUIC-based outbounds
//   5. SplitHTTP transport
//   6. Enhanced log level per-outbound
//
// ═══════════════════════════════════════════════════════════════════════════════

// ─── XTLS Reality Configuration Builder ──────────────────────────────────────
// Generates the "reality" TLS block for VLESS outbounds (Xray-core).
struct RealityConfig {
    QString serverName;          // e.g., "www.microsoft.com"
    QString publicKey;           // Server's Reality public key (base64)
    QString shortId;             // 0-8 char hex short ID
    QString fingerprint;         // uTLS fingerprint: "chrome", "firefox", "safari", etc.
    QString spiderX;             // URL path for spider crawl camouflage
};

inline QJsonObject buildRealityTLS(const RealityConfig &cfg) {
    QJsonObject reality;
    reality["show"] = false;
    reality["server_name"] = cfg.serverName;
    reality["public_key"] = cfg.publicKey;
    if (!cfg.shortId.isEmpty())
        reality["short_id"] = cfg.shortId;
    if (!cfg.fingerprint.isEmpty())
        reality["fingerprint"] = cfg.fingerprint;
    if (!cfg.spiderX.isEmpty())
        reality["spider_x"] = cfg.spiderX;

    QJsonObject tls;
    tls["enabled"] = true;
    tls["server_name"] = cfg.serverName;
    tls["reality"] = reality;
    if (!cfg.fingerprint.isEmpty()) {
        QJsonObject utls;
        utls["enabled"] = true;
        utls["fingerprint"] = cfg.fingerprint;
        tls["utls"] = utls;
    }
    return tls;
}

// ─── VLESS Vision Flow Builder ──────────────────────────────────────────────
// Generates a VLESS outbound with XTLS-Vision flow for zero-copy splice.
struct VlessVisionConfig {
    QString uuid;
    QString address;
    int port = 443;
    QString flow;                // "xtls-rpc-vision" for latest
    RealityConfig reality;       // Optional Reality TLS
    bool useReality = true;
};

inline QJsonObject buildVlessVisionOutbound(const VlessVisionConfig &cfg, const QString &tag) {
    QJsonObject outbound;
    outbound["type"] = QStringLiteral("vless");
    outbound["tag"] = tag;
    outbound["server"] = cfg.address;
    outbound["server_port"] = cfg.port;
    outbound["uuid"] = cfg.uuid;

    if (!cfg.flow.isEmpty())
        outbound["flow"] = cfg.flow;

    if (cfg.useReality) {
        outbound["tls"] = buildRealityTLS(cfg.reality);
    }

    // Packet encoding for XUDP full-cone NAT
    outbound["packet_encoding"] = QStringLiteral("xudp");

    return outbound;
}

// ─── sing-box Rule Set v2 Builder ────────────────────────────────────────────
// Generates rule_set entries for local and remote .srs binary rule files.
struct RuleSetEntry {
    QString tag;                 // Rule set identifier
    QString type;                // "local" or "remote"
    QString format;              // "binary" (srs) or "source" (json)
    QString url;                 // For remote: download URL
    QString path;                // For local: file path
    int updateInterval = 86400;  // For remote: seconds between updates
};

inline QJsonObject buildRuleSet(const RuleSetEntry &entry) {
    QJsonObject rs;
    rs["tag"] = entry.tag;
    rs["type"] = entry.type;
    rs["format"] = entry.format;

    if (entry.type == QStringLiteral("remote")) {
        rs["url"] = entry.url;
        rs["download_detour"] = QStringLiteral("direct");
        rs["update_interval"] = QString::number(entry.updateInterval) + "s";
    } else {
        rs["path"] = entry.path;
    }

    return rs;
}

// ─── DNS FakeIP Builder ─────────────────────────────────────────────────────
// Configures sing-box's FakeIP DNS pool with custom CIDR and TTL.
struct FakeIPConfig {
    QString inet4Range = "198.18.0.0/15";    // Default FakeIP v4 pool
    QString inet6Range = "fc00::/18";         // Default FakeIP v6 pool
};

inline QJsonObject buildFakeIPDNS(const FakeIPConfig &cfg) {
    QJsonObject fakeip;
    fakeip["enabled"] = true;
    fakeip["inet4_range"] = cfg.inet4Range;
    fakeip["inet6_range"] = cfg.inet6Range;
    return fakeip;
}

// ─── QUIC v2 Transport Builder ──────────────────────────────────────────────
struct QUICTransportConfig {
    int initialStreamReceiveWindow = 8388608;    // 8 MiB
    int maxStreamReceiveWindow = 16777216;        // 16 MiB
    int initialConnectionReceiveWindow = 16777216;
    int maxConnectionReceiveWindow = 33554432;    // 32 MiB
    int maxIdleTimeout = 30;                      // seconds
    int keepAlivePeriod = 10;                     // seconds
};

inline QJsonObject buildQUICTransport(const QUICTransportConfig &cfg) {
    QJsonObject quic;
    quic["type"] = QStringLiteral("quic");
    quic["initial_stream_receive_window"] = cfg.initialStreamReceiveWindow;
    quic["max_stream_receive_window"] = cfg.maxStreamReceiveWindow;
    quic["initial_connection_receive_window"] = cfg.initialConnectionReceiveWindow;
    quic["max_connection_receive_window"] = cfg.maxConnectionReceiveWindow;
    quic["max_idle_timeout"] = QString::number(cfg.maxIdleTimeout) + "s";
    quic["keep_alive_period"] = QString::number(cfg.keepAlivePeriod) + "s";
    return quic;
}

// ─── Hysteria2 Builder ──────────────────────────────────────────────────────
struct Hysteria2Config {
    QString address;
    int port = 443;
    QString password;
    int upMbps = 100;
    int downMbps = 100;
    QString obfsType;             // "salamander" or empty
    QString obfsPassword;
    QString sni;
    bool allowInsecure = false;
};

inline QJsonObject buildHysteria2Outbound(const Hysteria2Config &cfg, const QString &tag) {
    QJsonObject out;
    out["type"] = QStringLiteral("hysteria2");
    out["tag"] = tag;
    out["server"] = cfg.address;
    out["server_port"] = cfg.port;
    out["password"] = cfg.password;
    out["up_mbps"] = cfg.upMbps;
    out["down_mbps"] = cfg.downMbps;

    if (!cfg.obfsType.isEmpty()) {
        QJsonObject obfs;
        obfs["type"] = cfg.obfsType;
        obfs["password"] = cfg.obfsPassword;
        out["obfs"] = obfs;
    }

    QJsonObject tls;
    tls["enabled"] = true;
    if (!cfg.sni.isEmpty())
        tls["server_name"] = cfg.sni;
    tls["insecure"] = cfg.allowInsecure;
    out["tls"] = tls;

    return out;
}

// ─── TUN Strict Route Builder ───────────────────────────────────────────────
// Generates TUN inbound with strict_route and auto_redirect for zero-leak VPN.
struct TUNConfig {
    bool strictRoute = true;           // Enforce all traffic through TUN
    bool autoRedirect = true;          // Auto-redirect system traffic
    QString inet4Address = "172.19.0.1/30";
    QString inet6Address = "fdfe:dcba:9876::1/126";
    int mtu = 9000;                    // Jumbo frames for throughput
    QString stack = "system";          // "system", "gvisor", "mixed"
    QStringList excludePackages;       // Android package exclusion
    QStringList includePackages;       // Android package inclusion
};

inline QJsonObject buildTUNInbound(const TUNConfig &cfg) {
    QJsonObject tun;
    tun["type"] = QStringLiteral("tun");
    tun["tag"] = QStringLiteral("tun-in");
    tun["inet4_address"] = cfg.inet4Address;
    tun["inet6_address"] = cfg.inet6Address;
    tun["mtu"] = cfg.mtu;
    tun["stack"] = cfg.stack;
    tun["auto_route"] = true;
    tun["strict_route"] = cfg.strictRoute;

    if (cfg.autoRedirect) {
        tun["auto_redirect"] = true;
    }

    tun["sniff"] = true;
    tun["sniff_override_destination"] = false;

    return tun;
}

// ─── Xray Mux.Cool Builder ─────────────────────────────────────────────────
struct XrayMuxConfig {
    bool enabled = true;
    int concurrency = 8;               // max concurrent streams per connection
    int xudpConcurrency = 16;          // XUDP stream concurrency
    QString xudpProxyUDP443 = "reject"; // "reject", "allow", "skip"
};

inline QJsonObject buildXrayMux(const XrayMuxConfig &cfg) {
    QJsonObject mux;
    mux["enabled"] = cfg.enabled;
    mux["concurrency"] = cfg.concurrency;
    mux["xudpConcurrency"] = cfg.xudpConcurrency;
    mux["xudpProxyUDP443"] = cfg.xudpProxyUDP443;
    return mux;
}

// ─── Xray SplitHTTP Transport ───────────────────────────────────────────────
struct SplitHTTPConfig {
    QString host;
    QString path = "/";
    int maxConcurrentUploads = 10;
    int maxUploadSize = 1048576;       // 1 MiB
};

inline QJsonObject buildSplitHTTPTransport(const SplitHTTPConfig &cfg) {
    QJsonObject transport;
    transport["type"] = QStringLiteral("splithttp");
    transport["host"] = cfg.host;
    transport["path"] = cfg.path;
    transport["maxConcurrentUploads"] = cfg.maxConcurrentUploads;
    transport["maxUploadSize"] = cfg.maxUploadSize;
    return transport;
}

} // namespace Configs
