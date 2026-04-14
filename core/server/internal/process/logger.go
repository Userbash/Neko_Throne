package process

import (
	"log"
	"strings"
)

const maxLogLineSize = 4096 // Limit log line to 4KB to prevent memory exhaustion

type pipeLogger struct {
	prefix string
	noOut  bool
	buffer strings.Builder // Reusable buffer to reduce GC pressure
}

func (p *pipeLogger) Write(b []byte) (int, error) {
	if p.noOut {
		return len(b), nil
	}

	nBytes := len(b)

	// Prevent excessively large log lines from exhausting memory
	if nBytes > maxLogLineSize {
		// Log in chunks to avoid single huge log line
		for i := 0; i < nBytes; i += maxLogLineSize {
			end := i + maxLogLineSize
			if end > nBytes {
				end = nBytes
			}
			chunk := string(b[i:end])
			if i == 0 {
				log.Println(p.prefix + ": " + chunk)
			} else {
				log.Println(p.prefix + ": (cont) " + chunk)
			}
		}
	} else {
		// Normal case: log the line with size check
		logLine := p.prefix + ": " + string(b)
		log.Println(logLine)
	}

	return nBytes, nil
}
