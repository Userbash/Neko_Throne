package boxbox

import (
	"context"
	"fmt"
	"io"
	"time"
)

func (s *Box) CloseWithTimeout(cancel context.CancelFunc, d time.Duration, logFunc func(v ...any), block bool) {
	start := time.Now()
	t := time.NewTimer(d)
	done := make(chan struct{})
	defer func() {
		// Ensure timer is stopped and drained to prevent resource leak
		if !t.Stop() {
			<-t.C // Drain the timer channel if it already fired
		}
	}()

	printCloseTime := func() {
		logFunc("[Info] sing-box closed in", fmt.Sprintf("%d ms", time.Since(start).Milliseconds()))
	}

	go func(cancelFunc context.CancelFunc, closer io.Closer) {
		cancelFunc()
		closer.Close()
		close(done)
	}(cancel, s)

	select {
	case <-t.C:
		logFunc("[Warning] sing-box close takes longer than expected")
		if block {
			select {
			case <-done:
				printCloseTime()
			}
		}
	case <-done:
		printCloseTime()
	}
}
