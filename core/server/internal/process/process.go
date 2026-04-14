package process

import (
	"fmt"
	"github.com/sagernet/sing/common/atomic"
	"os/exec"
	"sync"
	"syscall"
	"time"
)

type Process struct {
	path      string
	args      []string
	noOut     bool
	cmd       *exec.Cmd
	stopped   atomic.Bool
	stopOnce  sync.Once  // Ensures Stop() is called exactly once
}

func NewProcess(path string, args []string, noOut bool) *Process {
	return &Process{path: path, args: args, noOut: noOut}
}

func (p *Process) Start() error {
	p.cmd = exec.Command(p.path, p.args...)

	p.cmd.Stdout = &pipeLogger{prefix: "Extra Core", noOut: p.noOut}
	p.cmd.Stderr = &pipeLogger{prefix: "Extra Core", noOut: p.noOut}

	err := p.cmd.Start()
	if err != nil {
		return err
	}
	p.stopped.Store(false)

	go func() {
		fmt.Println(p.path, ":", "process started, waiting for it to end")
		_ = p.cmd.Wait()

		// Atomic check: if process exited unexpectedly (not via Stop()),
		// log it. This prevents race condition where Stop() is called
		// between Wait() and Load().
		if !p.stopped.Load() {
			fmt.Println("Extra process exited unexpectedly")
		}
	}()
	return nil
}

func (p *Process) Stop() {
	// Use sync.Once to ensure Stop() logic runs exactly once,
	// preventing race conditions with concurrent Stop() calls
	p.stopOnce.Do(func() {
		p.stopped.Store(true)
		if p.cmd != nil && p.cmd.Process != nil {
			// Graceful shutdown with timeout escalation:
			// 1. Send SIGTERM and wait 5 seconds for graceful shutdown
			// 2. If still running, send SIGKILL and wait 2 seconds
			// 3. Force cleanup after timeout

			// Step 1: Try graceful shutdown with SIGTERM
			fmt.Println(p.path, ":", "sending SIGTERM for graceful shutdown")
			if err := p.cmd.Process.Signal(syscall.SIGTERM); err == nil {
				// Wait up to 5 seconds for graceful termination
				done := make(chan error, 1)
				go func() {
					done <- p.cmd.Wait()
				}()

				select {
				case <-done:
					// Process terminated gracefully
					fmt.Println(p.path, ":", "process terminated gracefully")
					return
				case <-time.After(time.Second * 5):
					// Graceful timeout reached, escalate to SIGKILL
					fmt.Println(p.path, ":", "graceful shutdown timeout, sending SIGKILL")
					_ = p.cmd.Process.Kill()

					// Wait for kill to complete (max 2 seconds)
					select {
					case <-done:
						fmt.Println(p.path, ":", "process killed")
						return
					case <-time.After(time.Second * 2):
						fmt.Println(p.path, ":", "force kill timeout exceeded")
						return
					}
				}
			} else {
				// Signal() failed, force kill immediately
				fmt.Println(p.path, ":", "SIGTERM failed, force killing")
				_ = p.cmd.Process.Kill()
			}
		}
	})
}
