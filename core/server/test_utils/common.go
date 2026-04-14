package test_utils

import (
	"context"
	"errors"
	"github.com/Mahdi-zarei/speedtest-go/speedtest"
	"github.com/sagernet/sing/common/metadata"
	"net"
	"sync"
	"time"
)

var (
	testContextMutex   sync.Mutex
	activeTestContexts map[string]context.CancelFunc = make(map[string]context.CancelFunc)
)

func CreateTestContext(testID string) context.Context {
	testContextMutex.Lock()
	defer testContextMutex.Unlock()

	ctx, cancel := context.WithCancel(context.Background())
	activeTestContexts[testID] = cancel
	return ctx
}

func CancelTest(testID string) {
	testContextMutex.Lock()
	defer testContextMutex.Unlock()

	if cancel, exists := activeTestContexts[testID]; exists {
		cancel()
		delete(activeTestContexts, testID)
	}
}

func CancelAllTests() {
	testContextMutex.Lock()
	defer testContextMutex.Unlock()

	for _, cancel := range activeTestContexts {
		cancel()
	}
	activeTestContexts = make(map[string]context.CancelFunc)
}

const FetchServersTimeout = 8 * time.Second
const MaxConcurrentTests = 100

func getNetDialer(dialer func(ctx context.Context, network string, destination metadata.Socksaddr) (net.Conn, error)) func(ctx context.Context, network string, address string) (net.Conn, error) {
	return func(ctx context.Context, network string, address string) (net.Conn, error) {
		return dialer(ctx, network, metadata.ParseSocksaddr(address))
	}
}

func getSpeedtestServer(ctx context.Context, dialer func(ctx context.Context, network string, address string) (net.Conn, error)) (*speedtest.Server, error) {
	clt := speedtest.New(speedtest.WithUserConfig(&speedtest.UserConfig{
		DialContextFunc: dialer,
		PingMode:        speedtest.HTTP,
		MaxConnections:  8,
	}))
	fetchCtx, cancel := context.WithTimeout(ctx, FetchServersTimeout)
	defer cancel()
	srv, err := clt.FetchServerListContext(fetchCtx)
	if err != nil {
		return nil, err
	}
	srv, err = srv.FindServer(nil)
	if err != nil {
		return nil, err
	}

	if srv.Len() == 0 {
		return nil, errors.New("no server found for speedTest")
	}

	return srv[0], nil
}
