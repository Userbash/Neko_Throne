package boxmain

import (
	"ThroneCore/internal/boxbox"
	"context"
	"os"
	"os/signal"
	runtimeDebug "runtime/debug"
	"sync"
	"syscall"
	"time"

	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/log"
	"github.com/sagernet/sing-box/option"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/json"
	"github.com/spf13/cobra"
)

var commandRun = &cobra.Command{
	Use:   "run",
	Short: "Run service",
	Run: func(cmd *cobra.Command, args []string) {
		err := run()
		if err != nil {
			log.Fatal(err)
		}
	},
}

func init() {
	mainCommand.AddCommand(commandRun)
}

type OptionsEntry struct {
	content []byte
	path    string
	options option.Options
}

func parseConfig(ctx context.Context, configContent []byte) (*option.Options, error) {
	var (
		err error
	)
	options, err := json.UnmarshalExtendedContext[option.Options](ctx, configContent)
	if err != nil {
		// Limit config size in error message to prevent log overflow (max 512 bytes)
		configPreview := string(configContent)
		if len(configContent) > 512 {
			configPreview = configPreview[:512] + "... (truncated)"
		}
		return nil, E.Cause(err, "decode config failed (size: %d bytes): %s", len(configContent), configPreview)
	}
	return &options, nil
}

func Create(configContent []byte, disableDNS bool) (*boxbox.Box, context.CancelFunc, error) {
	preRun(nil, nil)
	options, err := parseConfig(globalCtx, configContent)
	if err != nil {
		return nil, nil, err
	}
	if disableDNS {
		if options.DNS == nil {
			options.DNS = &option.DNSOptions{}
		}
		options.DNS.DisableCache = true
	}
	if disableColor {
		if options.Log == nil {
			options.Log = &option.LogOptions{}
		}
		options.Log.DisableColor = true
	}
	ctx, cancel := context.WithCancel(globalCtx)
	instance, err := boxbox.New(boxbox.Options{
		Context: ctx,
		Options: *options,
	})
	if err != nil {
		cancel()
		return nil, nil, E.Cause(err, "create service")
	}

	osSignals := make(chan os.Signal, 1)
	signal.Notify(osSignals, os.Interrupt, syscall.SIGTERM, syscall.SIGHUP)

	// Use WaitGroup to synchronize signal handler goroutine cleanup
	var signalWg sync.WaitGroup
	signalWg.Add(1)

	defer func() {
		signal.Stop(osSignals)
		// Wait for signal goroutine to finish before closing channel
		// This prevents "send on closed channel" panic
		signalWg.Wait()
		close(osSignals)
	}()

	startCtx, finishStart := context.WithCancel(context.Background())
	go func() {
		defer signalWg.Done()
		_, loaded := <-osSignals
		if loaded {
			cancel()
			closeMonitor(startCtx)
		}
	}()
	err = instance.Start()
	finishStart()
	if err != nil {
		cancel()
		return nil, nil, E.Cause(err, "start service")
	}
	return instance, cancel, nil
}

func run() error {
	osSignals := make(chan os.Signal, 1)
	signal.Notify(osSignals, os.Interrupt, syscall.SIGTERM)
	defer func() {
		// Proper cleanup: stop signal notification and close channel
		// Wrapped in defer to guarantee execution even on panic
		signal.Stop(osSignals)
		close(osSignals)
	}()

	for {
		instance, cancel, err := Create([]byte{}, false)
		if err != nil {
			return err
		}
		runtimeDebug.FreeOSMemory()
		for {
			// Safe read from signal channel
			// If osSignals is closed, this will receive zero value and loaded=false
			osSignal, loaded := <-osSignals
			if !loaded {
				// Channel closed, exit gracefully
				return nil
			}

			cancel()
			closeCtx, closed := context.WithCancel(context.Background())
			go closeMonitor(closeCtx)
			err = instance.Close()
			closed()
			if osSignal != syscall.SIGHUP {
				if err != nil {
					log.Error(E.Cause(err, "sing-box did not closed properly"))
				}
				return nil
			}
			break
		}
	}
}

func closeMonitor(ctx context.Context) {
	time.Sleep(C.FatalStopTimeout)
	select {
	case <-ctx.Done():
		return
	default:
	}
	log.Fatal("sing-box did not close!")
}
