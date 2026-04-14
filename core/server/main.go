//go:build linux
// +build linux

package main

import (
	"ThroneCore/gen"
	"ThroneCore/internal/boxmain"
	"flag"
	"fmt"
	"github.com/xtls/xray-core/core"
	"google.golang.org/grpc"
	"log"
	"net"
	"os"
	"runtime"
	runtimeDebug "runtime/debug"
	"strconv"
	"syscall"
	"time"

	_ "ThroneCore/internal/distro/all"
	C "github.com/sagernet/sing-box/constant"
	"golang.org/x/sys/unix"
)

func RunCore() {
	_port := flag.Int("port", 19810, "")
	_debug := flag.Bool("debug", false, "")
	flag.CommandLine.Parse(os.Args[1:])
	debug = *_debug

	go func() {
		parent, err := os.FindProcess(os.Getppid())
		if err != nil {
			log.Fatalln("find parent:", err)
		}
		if runtime.GOOS == "windows" {
			state, err := parent.Wait()
			log.Fatalln("parent exited:", state, err)
		} else {
			// Linux: Use prctl(PR_SET_PDEATHSIG) for event-driven parent death notification
			// instead of polling every 10 seconds. This eliminates 10-second delay on parent crash.
			if err := unix.Prctl(unix.PR_SET_PDEATHSIG, uintptr(syscall.SIGTERM), 0, 0, 0); err != nil {
				log.Println("warning: prctl(PR_SET_PDEATHSIG) failed, falling back to polling:", err)
				// Fallback: Polling with 1-second interval instead of 10 (faster crash detection)
				for {
					time.Sleep(time.Second)
					err = parent.Signal(syscall.Signal(0))
					if err != nil {
						log.Fatalln("parent exited:", err)
					}
				}
			} else {
				// If prctl succeeded, block until SIGTERM is received
				// Parent death will trigger automatic termination of this process
				select {}
			}
		}
	}()
	boxmain.DisableColor()

	// GRPC
	lis, err := net.Listen("tcp", "127.0.0.1:"+strconv.Itoa(*_port))
	if err != nil {
		log.Fatalf("failed to listen: %v", err)
	}

	s := grpc.NewServer(
		grpc.MaxRecvMsgSize(1024*1024*1024), // 1 gigaByte
		grpc.MaxSendMsgSize(1024*1024*1024), // 1 gigaByte
	)
	gen.RegisterLibcoreServiceServer(s, &server{})

	fmt.Printf("Core listening at %v\n", lis.Addr())
	if err := s.Serve(lis); err != nil {
		log.Fatalf("failed to serve: %v", err)
	}
}

func main() {
	defer func() {
		if err := recover(); err != nil {
			fmt.Fprintf(os.Stderr, "Core panicked: %v\n", err)
			runtimeDebug.PrintStack()

			// Notify parent process (if exists) that core crashed
			// This ensures parent detects the crash immediately instead of waiting
			ppid := os.Getppid()
			if ppid > 1 {
				if parent, err := os.FindProcess(ppid); err == nil {
					// Send SIGTERM to parent to signal abnormal termination
					_ = parent.Signal(syscall.SIGTERM)
				}
			}

			os.Exit(1)
		}
	}()

	// DEBUG: Manual panic testing (disabled for production builds)
	// if len(os.Args) > 1 && os.Args[1] == "--crash-now" {
	// 	fmt.Println("DEBUG: Triggering Go panic...")
	// 	panic("Manual panic for testing")
	// }

	fmt.Println("sing-box:", C.Version)
	fmt.Println("Xray-core:", core.Version())
	fmt.Println()
	runtimeDebug.SetMemoryLimit(2 * 1024 * 1024 * 1024) // 2GB

	// Memory monitoring (disabled for CI environments - may cause false positives)
	// Uncomment below if needed for local development
	/*
		go func() {
			var memStats runtime.MemStats
			for {
				time.Sleep(2 * time.Second)
				runtime.ReadMemStats(&memStats)
				if memStats.HeapAlloc > 1.5*1024*1024*1024 {
					// too much memory for sing-box, crash
					panic("Memory has reached 1.5 GB, this is not normal")
				}
			}
		}()
	*/

	RunCore()
	return
}
