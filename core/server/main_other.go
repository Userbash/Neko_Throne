//go:build !linux
// +build !linux

package main

import (
	"ThroneCore/gen"
	"ThroneCore/internal/boxmain"
	"ThroneCore/test_utils"
	"context"
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
			// macOS/other Unix: Use polling for parent death detection
			// (unix.Prctl is Linux-only, so we use polling fallback)
			for {
				time.Sleep(time.Second)
				err = parent.Signal(syscall.Signal(0))
				if err != nil {
					log.Fatalln("parent exited:", err)
				}
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
			ppid := os.Getppid()
			if ppid > 1 {
				if parent, err := os.FindProcess(ppid); err == nil {
					_ = parent.Signal(syscall.SIGTERM)
				}
			}

			os.Exit(1)
		}
	}()

	fmt.Println("sing-box:", C.Version)
	fmt.Println("Xray-core:", core.Version())
	fmt.Println()
	runtimeDebug.SetMemoryLimit(2 * 1024 * 1024 * 1024) // 2GB

	test_utils.TestCtx, test_utils.CancelTests = context.WithCancel(context.Background())
	RunCore()
	return
}
