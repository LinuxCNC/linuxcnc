// halsampler — reads HAL sample data from the stream_server WebSocket endpoint
// and writes it to stdout in the same format as the legacy halsampler.
//
// Usage:
//   halsampler [-n num_samples] [-t] [instance_name]
//
// Environment:
//   GMC_REST_URL — base URL of gomc-server (default: http://127.0.0.1:5080)

package main

import (
	"context"
	"encoding/binary"
	"flag"
	"fmt"
	"math"
	"net/url"
	"os"
	"os/signal"
	"strings"

	"nhooyr.io/websocket"
)

const (
	defaultRestURL = "http://127.0.0.1:5080"
	envRestURL     = "GMC_REST_URL"
)

func main() {
	var (
		numSamples int
		showTag    bool
	)

	flag.IntVar(&numSamples, "n", -1, "number of samples to capture (-1 = infinite)")
	flag.BoolVar(&showTag, "t", false, "print sample number")
	flag.Parse()

	instance := "sampler"
	if flag.NArg() > 0 {
		instance = flag.Arg(0)
	}

	restURL := os.Getenv(envRestURL)
	if restURL == "" {
		restURL = defaultRestURL
	}

	// Convert http(s) URL to ws(s) URL
	wsURL := httpToWS(restURL) + "/api/v1/stream/hal_sampler_stream/" + instance

	ctx, cancel := signal.NotifyContext(context.Background(), os.Interrupt)
	defer cancel()

	conn, _, err := websocket.Dial(ctx, wsURL, nil)
	if err != nil {
		fmt.Fprintf(os.Stderr, "halsampler: connect failed: %v\n", err)
		os.Exit(1)
	}
	defer conn.CloseNow()

	// The first message from the server is a header with pin types
	// Format: "cfg:<types>" e.g. "cfg:uffb"
	// We read this to know how to decode subsequent binary frames.
	_, headerMsg, err := conn.Read(ctx)
	if err != nil {
		fmt.Fprintf(os.Stderr, "halsampler: read header: %v\n", err)
		os.Exit(1)
	}

	cfg := string(headerMsg)
	if !strings.HasPrefix(cfg, "cfg:") {
		fmt.Fprintf(os.Stderr, "halsampler: unexpected header: %s\n", cfg)
		os.Exit(1)
	}
	pinTypes := cfg[4:]
	numPins := len(pinTypes)
	sampleSize := numPins * 8 // each value is 8 bytes

	sampleNum := 0
	for numSamples != 0 {
		_, data, err := conn.Read(ctx)
		if err != nil {
			if ctx.Err() != nil {
				break // clean shutdown
			}
			fmt.Fprintf(os.Stderr, "halsampler: read: %v\n", err)
			os.Exit(1)
		}

		// Process all samples in the frame
		for offset := 0; offset+sampleSize <= len(data); offset += sampleSize {
			if showTag {
				fmt.Printf("%d ", sampleNum)
			}

			for i := 0; i < numPins; i++ {
				raw := binary.LittleEndian.Uint64(data[offset+i*8:])
				switch pinTypes[i] {
				case 'f':
					fmt.Printf("%f ", math.Float64frombits(raw))
				case 'b':
					if raw != 0 {
						fmt.Print("1 ")
					} else {
						fmt.Print("0 ")
					}
				case 'u':
					fmt.Printf("%d ", uint32(raw))
				case 's':
					fmt.Printf("%d ", int32(raw))
				}
			}
			fmt.Println()

			sampleNum++
			if numSamples > 0 {
				numSamples--
				if numSamples == 0 {
					conn.Close(websocket.StatusNormalClosure, "done")
					return
				}
			}
		}
	}

	conn.Close(websocket.StatusNormalClosure, "done")
}

func httpToWS(httpURL string) string {
	u, err := url.Parse(httpURL)
	if err != nil {
		return strings.Replace(strings.Replace(httpURL, "https://", "wss://", 1), "http://", "ws://", 1)
	}
	switch u.Scheme {
	case "https":
		u.Scheme = "wss"
	default:
		u.Scheme = "ws"
	}
	return u.String()
}
