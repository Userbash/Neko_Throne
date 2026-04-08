# Privacy and Security Improvements

## DNS Leak Prevention

### Current Protection Mechanisms:

1. **VPN Strict Route** ✅
   - Enabled by default on Windows 10+ and other platforms.
   - Prevents traffic from bypassing the VPN tunnel.
   - Location: `src/configs/generate.cpp`.

2. **Split DNS** ✅
   - Remote DNS via proxy (for proxied domains).
   - Direct DNS for local/whitelisted domains.
   - Prevents DNS query leakage.

3. **DNS over H3/HTTPS/TLS/QUIC** ✅
   - Support for encrypted DNS protocols.
   - Protects against DNS snooping by ISPs.
   - Examples: `h3://1.1.1.1/dns-query`, `https://dns.google/dns-query`.

4. **FakeIP Support** ✅
   - Prevents real DNS requests for proxied traffic.
   - Reduces latency.
   - Enhances protection against DNS leaks via fake addresses.

5. **Localhost Protection in TUN Mode** ✅
   - Automatic fallback to 8.8.8.8 in Linux+TUN mode when localhost is specified.
   - Prevents "No default interface" errors.
   - Logic in `generate.cpp`.

## Verification Procedures

### DNS Leak Test:
```bash
# Run with an active profile:
curl -s https://1.1.1.1/cdn-cgi/trace | grep fl=
# Result should show the proxy server's IP, not your real IP.
```

### WebRTC Leak Test:
- Visit: https://browserleaks.com/webrtc
- Verify that your local/private IP addresses are not exposed.

### DNS Resolution Test:
```bash
# With proxy enabled:
nslookup google.com
# The DNS server used should match your proxy configuration.
```

## Recommendations for Maximum Privacy:

1. **Enable VPN Strict Route**
   - Settings → VPN Settings → Strict Route (Check) ✓

2. **Use Encrypted Remote DNS**
   ```
   Remote DNS: h3://1.1.1.1/dns-query
   OR
   Remote DNS: https://dns.google/dns-query
   ```

3. **Enable FakeIP**
   - Routing Settings → DNS Settings → Enable FakeIP (Check) ✓

4. **Configure Sniffing**
   - Routes → Common → Sniffing Mode: "Sniff result for routing"
   - Ensures correct protocol identification for rules.

5. **Verify Routing Rules**
   - Ensure the DNS hijack rule is active.
   - Confirm that local domains are routed through "Direct".

## Technical Details

### Sing-box DNS Architecture:

```
Application → Sing-box DNS Router → [Rule Engine] → DNS Server Selection
                                        ↓
                                   [Remote/Direct/Local]
                                        ↓
                                   [Protocol: UDP/TCP/DoH/DoT/DoQ/H3]
                                        ↓
                                   [Encrypted Channel]
```

### DNS Resolution Order:

1. FakeIP (if enabled).
2. DNS Rules (by domain/geosite).
3. Direct DNS (for whitelisted domains).
4. Remote DNS (routed through proxy).
5. Fallback server (if specified).

### Prevention of Leaks in TUN Mode:

```cpp
// generate.cpp
inboundObj["strict_route"] = dataStore->vpn_strict_route;
inboundObj["stack"] = dataStore->vpn_implementation;
```

**Strict Route** ensures:
- All traffic is forced through the TUN interface.
- No bypassing via physical network interfaces.
- DNS requests cannot circumvent the proxy core.

## Memory Safety

### Resolved Memory Leaks:

1. **QSystemTrayIcon** - Proper parent assigned in `mainwindow.cpp`.
2. **QMenu** - Parent assigned for tray context menu.
3. **QWidget/QDialog** - `WA_DeleteOnClose` attribute applied where necessary.
4. **Tab widgets** - Ensured proper parentage during dynamic creation.

### Qt Memory Management Principles:

Qt employs a parent-child hierarchy:
- Objects with a parent are automatically deleted when the parent is destroyed.
- Objects without a parent must be managed via `deleteLater()` or explicit `delete`.
- Dialogs should use `Qt::WA_DeleteOnClose` if not explicitly deleted after use.

## Verification Checklist:

- [x] Memory Leak Check (Linux): `valgrind --leak-check=full ./nekoray`
- [x] DNS Leak Check: `curl https://ipleak.net/json/`
- [x] Connection Check: `netstat -tunp | grep nekoray`

## Status:

✅ DNS leak protection - Implemented.
✅ VPN strict route - Enabled.
✅ Memory leaks - Fixed.
✅ H3 DNS support - Functional.
✅ Encrypted DNS - Supported (H3, HTTPS, TLS, QUIC).
✅ FakeIP - Available.
✅ Split DNS - Functional (Remote/Direct).
