package main

import (
	"ThroneCore/gen"
	"ThroneCore/internal/boxbox"
	"ThroneCore/internal/boxmain"
	"ThroneCore/internal/process"
	"ThroneCore/internal/sys"
	"ThroneCore/internal/wg"
	"ThroneCore/internal/xray"
	"ThroneCore/test_utils"
	"bufio"
	"context"
	"errors"
	"fmt"
	"log"
	"net/netip"
	"os"
	"os/exec"
	"runtime"
	"strconv"
	"strings"
	"time"

	"github.com/google/shlex"
	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/experimental/clashapi"
	"github.com/sagernet/sing/common"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/service"
	"github.com/xtls/xray-core/core"
)

var boxInstance *boxbox.Box
var extraProcess *process.Process
var needUnsetDNS bool
var ipv6Blocked bool
var instanceCancel context.CancelFunc
var debug bool

// Xray core
var xrayInstance *core.Instance

type server struct {
	gen.UnimplementedLibcoreServiceServer
}

// To returns a pointer to the given value.
func To[T any](v T) *T {
	return &v
}

func (s *server) Start(ctx context.Context, in *gen.LoadConfigReq) (out *gen.ErrorResp, _ error) {
	var err error
	out = &gen.ErrorResp{}

	if runtime.GOOS == "linux" {
		linuxNetworkCleanup()
	}

	defer func() {
		if err != nil {
			// Preserve error response and ensure boxInstance is cleaned up
			out.Error = To(err.Error())
			boxInstance = nil
			if runtime.GOOS == "linux" && ipv6Blocked {
				restoreIPv6()
			}
		}
	}()

	// Fix permission issue for cache.db created when running with SUID/root
	if runtime.GOOS == "linux" {
		uid := os.Getuid()
		gid := os.Getgid()

		// Ensure cache files belong to the real user, not root
		files := []string{"cache.db", "cache.db-shm", "cache.db-wal"}
		for _, f := range files {
			// Touch file if not exists so we can chown/chmod it
			if _, err := os.Stat(f); os.IsNotExist(err) {
				// Create file and ensure proper cleanup on error
				file, createErr := os.Create(f)
				if createErr != nil {
					log.Printf("warning: failed to create %s: %v (continuing anyway)", f, createErr)
					continue
				}
				// Strict file descriptor management: ensure Close() is called
				if closeErr := file.Close(); closeErr != nil {
					log.Printf("warning: failed to close %s: %v", f, closeErr)
				}
			}
			// Chown/Chmod errors are non-fatal (may fail on some systems)
			_ = os.Chown(f, uid, gid)
			_ = os.Chmod(f, 0666)
		}
	}

	if debug {
		log.Println("Start:", *in.CoreConfig)
	}

	if boxInstance != nil {
		err = errors.New("instance already started")
		return
	}

	if *in.NeedExtraProcess {
		args, e := shlex.Split(in.GetExtraProcessArgs())
		if e != nil {
			err = E.Cause(e, "Failed to parse args")
			return
		}
		if in.ExtraProcessConf != nil {
			// Strict nil check: ExtraProcessConfDir must be non-nil and non-empty
			if in.ExtraProcessConfDir == nil || *in.ExtraProcessConfDir == "" {
				err = errors.New("ExtraProcessConfDir is required when ExtraProcessConf is set")
				return
			}
			extraConfPath := *in.ExtraProcessConfDir + string(os.PathSeparator) + "extra.conf"
			f, e := os.OpenFile(extraConfPath, os.O_CREATE|os.O_TRUNC|os.O_RDWR, 700)
			if e != nil {
				err = E.Cause(e, "Failed to open extra.conf")
				return
			}
			_, e = f.WriteString(*in.ExtraProcessConf)
			if e != nil {
				err = E.Cause(e, "Failed to write extra.conf")
				return
			}
			_ = f.Close()
			for idx, arg := range args {
				if strings.Contains(arg, "%s") {
					args[idx] = fmt.Sprintf(arg, extraConfPath)
					break
				}
			}
		}

		extraProcess = process.NewProcess(*in.ExtraProcessPath, args, *in.ExtraNoOut)
		err = extraProcess.Start()
		if err != nil {
			return
		}
	}

	if *in.NeedXray {
		xrayInstance, err = xray.CreateXrayInstance(*in.XrayConfig)
		if err != nil {
			return
		}
		err = xrayInstance.Start()
		if err != nil {
			xrayInstance = nil
			return
		}
	}

	if in == nil || in.CoreConfig == nil {
		err = errors.New("empty core config")
		return
	}

	boxInstance, instanceCancel, err = boxmain.Create([]byte(*in.CoreConfig), in.GetDisableDnsRouting())
	if err != nil {
		log.Printf("[Core] FATAL: Failed to create sing-box instance: %v", err)
		log.Printf("[Core] Config used: %s", *in.CoreConfig)
		if extraProcess != nil {
			extraProcess.Stop()
			extraProcess = nil
		}
		if xrayInstance != nil {
			xrayInstance.Close()
			xrayInstance = nil
		}
		return
	}

	if runtime.GOOS == "darwin" && in.GetTunIpv4Cidr() != "" {
		stopAllCores := func() {
			boxInstance.CloseWithTimeout(instanceCancel, time.Second*2, log.Println, true)
			boxInstance = nil
			if extraProcess != nil {
				extraProcess.Stop()
				extraProcess = nil
			}
			if xrayInstance != nil {
				xrayInstance.Close()
				xrayInstance = nil
			}
		}

		tunCIDR := in.GetTunIpv4Cidr()
		tunPrefix, parseErr := netip.ParsePrefix(tunCIDR)
		if parseErr != nil || !tunPrefix.Addr().Is4() {
			err = fmt.Errorf("invalid tun_ipv4_cidr %q", tunCIDR)
			stopAllCores()
			return
		}

		tunDNS := tunPrefix.Addr().Next()
		if !tunDNS.IsValid() || !tunDNS.Is4() {
			err = fmt.Errorf("got invalid DNS IP from tun_ipv4_cidr: %s", tunDNS)
			stopAllCores()
			return
		}

		if err := sys.SetSystemDNS(tunDNS.String(), boxInstance.Network().InterfaceMonitor()); err != nil {
			log.Println("Failed to set system DNS:", err)
		}

		needUnsetDNS = true
	}

	if in.GetBlock_Ipv6() && runtime.GOOS == "linux" {
		blockIPv6Leaks()
	}

	return
}

func (s *server) Stop(ctx context.Context, in *gen.EmptyReq) (out *gen.ErrorResp, _ error) {
	var err error

	defer func() {
		out = &gen.ErrorResp{}
		if err != nil {
			out.Error = To(err.Error())
		}
	}()

	if boxInstance == nil {
		return
	}

	if needUnsetDNS {
		// Retry DNS cleanup up to 3 times before giving up
		// Only reset flag if cleanup succeeds
		var dnsErr error
		for attempt := 1; attempt <= 3; attempt++ {
			dnsErr = sys.SetSystemDNS("Empty", boxInstance.Network().InterfaceMonitor())
			if dnsErr == nil {
				needUnsetDNS = false
				break
			}
			if attempt < 3 {
				log.Printf("Failed to unset system DNS (attempt %d/3): %v, retrying...", attempt, dnsErr)
				time.Sleep(time.Millisecond * 100 * time.Duration(attempt))
			}
		}
		if dnsErr != nil {
			log.Printf("CRITICAL: Failed to unset system DNS after 3 attempts: %v", dnsErr)
			err = E.Cause(dnsErr, "DNS cleanup failed")
		}
	}
	boxInstance.CloseWithTimeout(instanceCancel, time.Second*2, log.Println, true)

	boxInstance = nil

	if extraProcess != nil {
		extraProcess.Stop()
		extraProcess = nil
	}

	if xrayInstance != nil {
		xrayInstance.Close()
		xrayInstance = nil
	}

	if runtime.GOOS == "linux" && ipv6Blocked {
		restoreIPv6()
	}

	return
}

func (s *server) CheckConfig(ctx context.Context, in *gen.LoadConfigReq) (out *gen.ErrorResp, _ error) {
	err := boxmain.Check([]byte(*in.CoreConfig))
	out = &gen.ErrorResp{}
	if err != nil {
		out.Error = To(err.Error())
	}
	return
}

func (s *server) Test(ctx context.Context, in *gen.TestReq) (*gen.TestResp, error) {
	var testInstance *boxbox.Box
	var xrayTestIntance *core.Instance
	var cancel context.CancelFunc
	var err error
	var twice = true
	if *in.TestCurrent {
		if boxInstance == nil {
			return &gen.TestResp{Results: []*gen.URLTestResp{{
				OutboundTag: To("proxy"),
				LatencyMs:   To(int32(0)),
				Error:       To("Instance is not running"),
			}}}, nil
		}
		testInstance = boxInstance
		twice = false
	} else {
		if *in.NeedXray {
			xrayTestIntance, err = xray.CreateXrayInstance(*in.XrayConfig)
			if err != nil {
				return nil, err
			}
			err = xrayTestIntance.Start()
			if err != nil {
				return nil, err
			}
			defer func() {
				common.Must(xrayTestIntance.Close())
			}() // crash in case it does not close properly
		}
		testInstance, cancel, err = boxmain.Create([]byte(*in.Config), false)
		if err != nil {
			return nil, err
		}
		defer testInstance.CloseWithTimeout(cancel, 2*time.Second, log.Println, false)
	}

	outboundTags := in.OutboundTags
	if *in.UseDefaultOutbound || *in.TestCurrent {
		outbound := testInstance.Outbound().Default()
		outboundTags = []string{outbound.Tag()}
	}

	var maxConcurrency = *in.MaxConcurrency
	if maxConcurrency >= 500 || maxConcurrency == 0 {
		maxConcurrency = test_utils.MaxConcurrentTests
	}

	// Create a per-operation context instead of using the global reusable context
	// This prevents "operation canceled" errors when StopTest() cancels the global context
	testOpCtx, testOpCancel := context.WithCancel(context.Background())
	defer testOpCancel()

	results := test_utils.BatchURLTest(testOpCtx, testInstance, outboundTags, *in.Url, int(maxConcurrency), twice, time.Duration(*in.TestTimeoutMs)*time.Millisecond)

	res := make([]*gen.URLTestResp, 0)
	for idx, data := range results {
		errStr := ""
		if data.Error != nil {
			errStr = data.Error.Error()
		}
		res = append(res, &gen.URLTestResp{
			OutboundTag: To(outboundTags[idx]),
			LatencyMs:   To(int32(data.Duration.Milliseconds())),
			Error:       To(errStr),
		})
	}

	return &gen.TestResp{Results: res}, nil
}

func (s *server) StopTest(ctx context.Context, in *gen.EmptyReq) (*gen.EmptyResp, error) {
	test_utils.CancelAllTests()

	return &gen.EmptyResp{}, nil
}

func (s *server) QueryURLTest(ctx context.Context, in *gen.EmptyReq) (out *gen.QueryURLTestResponse, _ error) {
	results := test_utils.URLReporter.Results()
	out = &gen.QueryURLTestResponse{}
	for _, r := range results {
		errStr := ""
		if r.Error != nil {
			errStr = r.Error.Error()
		}
		out.Results = append(out.Results, &gen.URLTestResp{
			OutboundTag: To(r.Tag),
			LatencyMs:   To(int32(r.Duration.Milliseconds())),
			Error:       To(errStr),
		})
	}
	return
}

func (s *server) QueryStats(ctx context.Context, in *gen.EmptyReq) (out *gen.QueryStatsResp, err error) {
	out = &gen.QueryStatsResp{}
	out.Ups = make(map[string]int64)
	out.Downs = make(map[string]int64)
	if boxInstance != nil {
		clash := service.FromContext[adapter.ClashServer](boxInstance.Context())
		if clash != nil {
			cApi, ok := clash.(*clashapi.Server)
			if !ok {
				log.Println("Failed to assert clash server")
				err = E.New("invalid clash server type")
				return
			}
			outbounds := service.FromContext[adapter.OutboundManager](boxInstance.Context())
			if outbounds == nil {
				log.Println("Failed to get outbound manager")
				err = E.New("nil outbound manager")
				return
			}
			endpoints := service.FromContext[adapter.EndpointManager](boxInstance.Context())
			if endpoints == nil {
				log.Println("Failed to get endpoint manager")
				err = E.New("nil endpoint manager")
				return
			}
			for _, ob := range outbounds.Outbounds() {
				u, d := cApi.TrafficManager().TotalOutbound(ob.Tag())
				out.Ups[ob.Tag()] = u
				out.Downs[ob.Tag()] = d
			}
			for _, ep := range endpoints.Endpoints() {
				u, d := cApi.TrafficManager().TotalOutbound(ep.Tag())
				out.Ups[ep.Tag()] = u
				out.Downs[ep.Tag()] = d
			}
		}
	}
	return
}

func (s *server) ListConnections(ctx context.Context, in *gen.EmptyReq) (*gen.ListConnectionsResp, error) {
	if boxInstance == nil {
		return &gen.ListConnectionsResp{}, nil
	}
	if service.FromContext[adapter.ClashServer](boxInstance.Context()) == nil {
		return &gen.ListConnectionsResp{}, errors.New("no clash server found")
	}
	clash, ok := service.FromContext[adapter.ClashServer](boxInstance.Context()).(*clashapi.Server)
	if !ok {
		return &gen.ListConnectionsResp{}, errors.New("invalid state, should not be here")
	}
	connections := clash.TrafficManager().Connections()

	res := make([]*gen.ConnectionMetaData, 0)
	for _, c := range connections {
		process := ""
		if c.Metadata.ProcessInfo != nil {
			spl := strings.Split(c.Metadata.ProcessInfo.ProcessPath, string(os.PathSeparator))
			process = spl[len(spl)-1]
		}
		r := &gen.ConnectionMetaData{
			Id:        To(c.ID.String()),
			CreatedAt: To(c.CreatedAt.UnixMilli()),
			Upload:    To(c.Upload.Load()),
			Download:  To(c.Download.Load()),
			Outbound:  To(c.Outbound),
			Network:   To(c.Metadata.Network),
			Dest:      To(c.Metadata.Destination.String()),
			Protocol:  To(c.Metadata.Protocol),
			Domain:    To(c.Metadata.Domain),
			Process:   To(process),
			WifiSsid:  To(""),
			WifiBssid: To(""),
		}
		res = append(res, r)
	}
	return &gen.ListConnectionsResp{Connections: res}, nil
}

func (s *server) IsPrivileged(ctx context.Context, _ *gen.EmptyReq) (*gen.IsPrivilegedResponse, error) {
	if runtime.GOOS == "windows" {
		return &gen.IsPrivilegedResponse{HasPrivilege: To(true)}, nil
	}

	hasPriv := os.Geteuid() == 0

	// On Linux, we may not be root but still hold CAP_NET_ADMIN via file capabilities
	// or an ambient set. Read /proc/self/status, which is cheap, side-effect-free,
	// and does not require any external binary (iproute2 may not be installed).
	if !hasPriv && runtime.GOOS == "linux" {
		hasPriv = linuxHasCapNetAdmin()
	}

	return &gen.IsPrivilegedResponse{HasPrivilege: To(hasPriv)}, nil
}

// linuxHasCapNetAdmin returns true if the current process has CAP_NET_ADMIN in
// its effective capability set. CAP_NET_ADMIN is bit 12 per <linux/capability.h>.
func linuxHasCapNetAdmin() bool {
	f, err := os.Open("/proc/self/status")
	if err != nil {
		return false
	}
	defer f.Close()

	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		line := scanner.Text()
		if !strings.HasPrefix(line, "CapEff:") {
			continue
		}
		hex := strings.TrimSpace(strings.TrimPrefix(line, "CapEff:"))
		caps, err := strconv.ParseUint(hex, 16, 64)
		if err != nil {
			return false
		}
		const capNetAdmin = 12
		return (caps>>capNetAdmin)&1 == 1
	}
	return false
}

// linuxNetworkCleanup tears down any stale sing-box / throne state left over from
// a previous crash or unclean shutdown. This is the mitigation for two classes of
// post-start failures observed in the logs:
//
//  1. "post-start inbound/tun[tun-in]: auto-redirect: conn.Receive: netlink
//     receive: file exists" — sing-box tried to create an nftables table/chain
//     that was already present from a previous run.
//  2. "start inbound/tun[tun-in]: configure tun interface: device or resource
//     busy" — the previous TUN interface is still registered with the kernel.
//
// The cleanup is silent about missing binaries (nft/ip) so the call is safe on
// systems where iproute2/nftables aren't installed, and bounded so a pathological
// kernel state can never wedge the call.
func linuxNetworkCleanup() {
	log.Println("[Core] Initiating network cleanup...")

	nft, nftErr := exec.LookPath("nft")
	ipBin, ipErr := exec.LookPath("ip")
	if nftErr != nil && ipErr != nil {
		log.Println("[Core] Neither nft nor ip found; skipping cleanup.")
		return
	}

	// 1. nftables cleanup
	if nftErr == nil {
		out, err := exec.Command(nft, "list", "tables").Output()
		if err == nil {
			scanner := bufio.NewScanner(strings.NewReader(string(out)))
			for scanner.Scan() {
				fields := strings.Fields(scanner.Text())
				if len(fields) < 3 || fields[0] != "table" {
					continue
				}
				family, name := fields[1], fields[2]
				if strings.Contains(name, "sing-box") || strings.Contains(name, "throne") {
					err := exec.Command(nft, "delete", "table", family, name).Run()
					if err != nil && os.IsPermission(err) {
						log.Printf("[Core] Permission denied deleting nftables table %s", name)
					}
				}
			}
		}
	}

	if ipErr == nil {
		// 2. Interfaces cleanup
		knownInterfaces := []string{"throne-tun", "sing-tun", "singtun0", "tun0", "tun1", "tun2", "tun3", "utun0", "utun1"}
		detectProc := exec.Command("ip", "link", "show", "type", "tun")
		if output, err := detectProc.CombinedOutput(); err == nil {
			for _, line := range strings.Split(string(output), "\n") {
				parts := strings.Fields(line)
				if len(parts) >= 2 && (strings.HasPrefix(parts[1], "tun") || strings.HasPrefix(parts[1], "utun")) {
					ifaceName := strings.TrimSuffix(parts[1], ":")
					knownInterfaces = append(knownInterfaces, ifaceName)
				}
			}
		}

		for _, iface := range knownInterfaces {
			_ = exec.Command(ipBin, "link", "set", iface, "down").Run()
			err := exec.Command(ipBin, "link", "delete", iface).Run()
			if err != nil && os.IsPermission(err) {
				log.Printf("[Core] Permission denied deleting interface %s", iface)
			}
		}

		// 3. Routing Rules
		const maxRuleIters = 32
		for i := 0; i < maxRuleIters; i++ {
			removed := false
			for _, args := range [][]string{
				{"-4", "rule", "del", "priority", "9000"},
				{"-4", "rule", "del", "fwmark", "1", "lookup", "100"},
				{"-6", "rule", "del", "priority", "9000"},
				{"-6", "rule", "del", "fwmark", "1", "lookup", "100"},
			} {
				err := exec.Command(ipBin, args...).Run()
				if err == nil {
					removed = true
				} else if os.IsPermission(err) {
					log.Println("[Core] Permission denied removing routing rules")
					goto skipRules
				}
			}
			if !removed {
				break
			}
		}
	skipRules:

		// 4. Routing tables
		_ = exec.Command(ipBin, "-4", "route", "flush", "table", "100").Run()
		_ = exec.Command(ipBin, "-6", "route", "flush", "table", "100").Run()
	}

	log.Println("[Core] Network cleanup finished.")
	time.Sleep(250 * time.Millisecond)
}

func (s *server) SpeedTest(ctx context.Context, in *gen.SpeedTestRequest) (*gen.SpeedTestResponse, error) {
	if !*in.TestDownload && !*in.TestUpload && !*in.SimpleDownload && !*in.OnlyCountry {
		return nil, errors.New("cannot run empty test")
	}
	var testInstance *boxbox.Box
	var xrayTestIntance *core.Instance
	var cancel context.CancelFunc
	outboundTags := in.OutboundTags
	var err error
	if *in.TestCurrent {
		if boxInstance == nil {
			return &gen.SpeedTestResponse{Results: []*gen.SpeedTestResult{{
				OutboundTag: To("proxy"),
				Error:       To("Instance is not running"),
			}}}, nil
		}
		testInstance = boxInstance
	} else {
		if *in.NeedXray {
			xrayTestIntance, err = xray.CreateXrayInstance(*in.XrayConfig)
			if err != nil {
				return nil, err
			}
			err = xrayTestIntance.Start()
			if err != nil {
				return nil, err
			}
			defer xrayTestIntance.Close()
		}
		testInstance, cancel, err = boxmain.Create([]byte(*in.Config), false)
		if err != nil {
			return nil, err
		}
		defer cancel()
		defer testInstance.Close()
	}

	if *in.UseDefaultOutbound || *in.TestCurrent {
		outbound := testInstance.Outbound().Default()
		outboundTags = []string{outbound.Tag()}
	}

	testID := fmt.Sprintf("speedtest-%d", time.Now().UnixNano())
	testCtx := test_utils.CreateTestContext(testID)
	defer test_utils.CancelTest(testID)
	results := test_utils.BatchSpeedTest(testCtx, testInstance, outboundTags, *in.TestDownload, *in.TestUpload, *in.SimpleDownload, *in.SimpleDownloadAddr, time.Duration(*in.TimeoutMs)*time.Millisecond, *in.OnlyCountry, *in.CountryConcurrency)

	res := make([]*gen.SpeedTestResult, 0)
	for _, data := range results {
		errStr := ""
		if data.Error != nil {
			errStr = data.Error.Error()
		}
		res = append(res, &gen.SpeedTestResult{
			DlSpeed:       To(data.DlSpeed),
			UlSpeed:       To(data.UlSpeed),
			Latency:       To(data.Latency),
			OutboundTag:   To(data.Tag),
			Error:         To(errStr),
			ServerName:    To(data.ServerName),
			ServerCountry: To(data.ServerCountry),
			Cancelled:     To(data.Cancelled),
		})
	}

	return &gen.SpeedTestResponse{Results: res}, nil
}

func (s *server) QuerySpeedTest(context.Context, *gen.EmptyReq) (*gen.QuerySpeedTestResponse, error) {
	res, isRunning := test_utils.SpTQuerier.Result()
	errStr := ""
	if res.Error != nil {
		errStr = res.Error.Error()
	}
	return &gen.QuerySpeedTestResponse{
		Result: &gen.SpeedTestResult{
			DlSpeed:       To(res.DlSpeed),
			UlSpeed:       To(res.UlSpeed),
			Latency:       To(res.Latency),
			OutboundTag:   To(res.Tag),
			Error:         To(errStr),
			ServerName:    To(res.ServerName),
			ServerCountry: To(res.ServerCountry),
			Cancelled:     To(res.Cancelled),
		},
		IsRunning: To(isRunning),
	}, nil
}

func (s *server) QueryCountryTest(ctx context.Context, _ *gen.EmptyReq) (out *gen.QueryCountryTestResponse, _ error) {
	results := test_utils.CountryResults.Results()
	out = &gen.QueryCountryTestResponse{}
	for _, res := range results {
		var errStr string
		if res.Error != nil {
			errStr = res.Error.Error()
		}
		out.Results = append(out.Results, &gen.SpeedTestResult{
			DlSpeed:       To(res.DlSpeed),
			UlSpeed:       To(res.UlSpeed),
			Latency:       To(res.Latency),
			OutboundTag:   To(res.Tag),
			Error:         To(errStr),
			ServerName:    To(res.ServerName),
			ServerCountry: To(res.ServerCountry),
			Cancelled:     To(res.Cancelled),
		})
	}
	return
}

func (s *server) IPTest(ctx context.Context, in *gen.IPTestRequest) (*gen.IPTestResp, error) {
	var testInstance *boxbox.Box
	var xrayTestInstance *core.Instance
	var cancel context.CancelFunc
	var err error
	if *in.NeedXray {
		xrayTestInstance, err = xray.CreateXrayInstance(*in.XrayConfig)
		if err != nil {
			return nil, err
		}
		err = xrayTestInstance.Start()
		if err != nil {
			return nil, err
		}
		defer func() {
			common.Must(xrayTestInstance.Close())
		}()
	}
	testInstance, cancel, err = boxmain.Create([]byte(*in.Config), false)
	if err != nil {
		return nil, err
	}
	defer testInstance.CloseWithTimeout(cancel, 2*time.Second, log.Println, false)

	outboundTags := in.OutboundTags
	if *in.UseDefaultOutbound {
		outbound := testInstance.Outbound().Default()
		outboundTags = []string{outbound.Tag()}
	}

	maxConcurrency := *in.MaxConcurrency
	if maxConcurrency >= 500 || maxConcurrency == 0 {
		maxConcurrency = test_utils.MaxConcurrentTests
	}
	timeout := time.Duration(*in.TestTimeoutMs) * time.Millisecond
	testID := fmt.Sprintf("iptest-%d", time.Now().UnixNano())
	testCtx := test_utils.CreateTestContext(testID)
	defer test_utils.CancelTest(testID)
	results := test_utils.BatchIPTest(testCtx, testInstance, outboundTags, int(maxConcurrency), timeout)

	res := make([]*gen.IPTestRes, 0, len(results))
	for idx, data := range results {
		errStr := ""
		if data.Error != nil {
			errStr = data.Error.Error()
		}
		tag := outboundTags[idx]
		res = append(res, &gen.IPTestRes{
			OutboundTag: To(tag),
			Ip:          To(data.Result.IP),
			CountryCode: To(data.Result.CountryCode),
			Error:       To(errStr),
		})
	}
	return &gen.IPTestResp{Results: res}, nil
}

func (s *server) QueryIPTest(ctx context.Context, in *gen.EmptyReq) (out *gen.QueryIPTestResponse, _ error) {
	results := test_utils.IPReporter.Results()
	out = &gen.QueryIPTestResponse{}
	for _, r := range results {
		errStr := ""
		if r.Error != nil {
			errStr = r.Error.Error()
		}
		out.Results = append(out.Results, &gen.IPTestRes{
			OutboundTag: To(r.Tag),
			Ip:          To(r.Result.IP),
			CountryCode: To(r.Result.CountryCode),
			Error:       To(errStr),
		})
	}
	return
}

func (s *server) GenWgKeyPair(ctx context.Context, _ *gen.EmptyReq) (out *gen.GenWgKeyPairResponse, _ error) {
	var res gen.GenWgKeyPairResponse
	privateKey, err := wg.GeneratePrivateKey()
	if err != nil {
		res.Error = To(err.Error())
		return &res, nil
	}
	res.PrivateKey = To(privateKey.String())
	res.PublicKey = To(privateKey.PublicKey().String())
	return &res, nil
}

func blockIPv6Leaks() {
	if runtime.GOOS != "linux" {
		return
	}
	log.Println("[LeakGuard] Blocking IPv6 leaks...")
	
	files := []string{
		"/proc/sys/net/ipv6/conf/all/disable_ipv6",
		"/proc/sys/net/ipv6/conf/default/disable_ipv6",
	}

	for _, path := range files {
		err := os.WriteFile(path, []byte("1"), 0644)
		if err != nil {
			if os.IsPermission(err) {
				log.Printf("[LeakGuard] Skipped IPv6 blocking for %s: permission denied. Your IPv6 traffic might leak!", path)
			} else {
				log.Printf("[LeakGuard] IPv6 block sysctl not found or other error for %s: %v", path, err)
			}
		} else {
			ipv6Blocked = true
		}
	}
	if ipv6Blocked {
		log.Println("[LeakGuard] Successfully blocked IPv6 to prevent leaks.")
	}
}

func restoreIPv6() {
	if runtime.GOOS != "linux" {
		return
	}
	log.Println("[LeakGuard] Restoring IPv6...")
	
	files := []string{
		"/proc/sys/net/ipv6/conf/all/disable_ipv6",
		"/proc/sys/net/ipv6/conf/default/disable_ipv6",
	}

	for _, path := range files {
		err := os.WriteFile(path, []byte("0"), 0644)
		if err != nil {
			if os.IsPermission(err) {
				log.Printf("[LeakGuard] Permission denied restoring IPv6 for %s", path)
			} else {
				log.Printf("[LeakGuard] Error restoring IPv6 for %s: %v", path, err)
			}
		}
	}
	ipv6Blocked = false
}
