package sys

import (
	tun "github.com/sagernet/sing-tun"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/shell"
	"net/netip"
	"strings"
)

func SetSystemDNS(addr string, interfaceMonitor tun.DefaultInterfaceMonitor) error {
	// Validate that addr is a valid IP address to prevent command injection
	if addr != "Empty" {
		if _, err := netip.ParseAddr(addr); err != nil {
			return E.New("invalid DNS address: ", err)
		}
	}

	interfaceName := interfaceMonitor.DefaultInterface().Name
	interfaceDisplayName, err := getInterfaceDisplayName(interfaceName)
	if err != nil {
		return err
	}

	err = shell.Exec("/usr/sbin/networksetup", "-setdnsservers", interfaceDisplayName, addr).Attach().Run()
	if err != nil {
		return err
	}

	return nil
}

func getInterfaceDisplayName(name string) (string, error) {
	content, err := shell.Exec("/usr/sbin/networksetup", "-listallhardwareports").ReadOutput()
	if err != nil {
		return "", err
	}
	for _, deviceSpan := range strings.Split(string(content), "Ethernet Address") {
		if strings.Contains(deviceSpan, "Device: "+name) {
			substr := "Hardware Port: "
			startIdx := strings.Index(deviceSpan, substr)
			if startIdx == -1 {
				continue
			}
			deviceSpan = deviceSpan[startIdx+len(substr):]
			endIdx := strings.Index(deviceSpan, "\n")
			if endIdx == -1 {
				endIdx = len(deviceSpan)
			}
			return strings.TrimSpace(deviceSpan[:endIdx]), nil
		}
	}
	return "", E.New(name, " not found in networksetup -listallhardwareports")
}
