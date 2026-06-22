// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// halstreamer — reads data from stdin and streams it to the HAL streamer
// component via the stream_server WebSocket endpoint.
//
// Usage:
//   halstreamer [-n num_lines] [instance_name] < data.txt
//
// Input format: space-separated values, one sample per line.
// Values are interpreted according to the pin configuration of the streamer.
//
// Environment:
//   GMC_REST_URL — base URL of gomc-server (default: http://127.0.0.1:5080)

package main

import (
	"bufio"
	"context"
	"encoding/binary"
	"flag"
	"fmt"
	"math"
	"net/url"
	"os"
	"os/signal"
	"strconv"
	"strings"

	"nhooyr.io/websocket"
)

const (
	defaultRestURL = "http://127.0.0.1:5080"
	envRestURL     = "GMC_REST_URL"
)

func main() {
	var numLines int

	flag.IntVar(&numLines, "n", -1, "number of lines to send (-1 = all)")
	flag.Parse()

	instance := "streamer"
	if flag.NArg() > 0 {
		instance = flag.Arg(0)
	}

	restURL := os.Getenv(envRestURL)
	if restURL == "" {
		restURL = defaultRestURL
	}

	wsURL := httpToWS(restURL) + "/api/v1/stream/hal_streamer_stream/" + instance

	ctx, cancel := signal.NotifyContext(context.Background(), os.Interrupt)
	defer cancel()

	conn, _, err := websocket.Dial(ctx, wsURL, nil)
	if err != nil {
		fmt.Fprintf(os.Stderr, "halstreamer: connect failed: %v\n", err)
		os.Exit(1)
	}
	defer conn.CloseNow()

	// Read header from server: "cfg:<types>"
	_, headerMsg, err := conn.Read(ctx)
	if err != nil {
		fmt.Fprintf(os.Stderr, "halstreamer: read header: %v\n", err)
		os.Exit(1)
	}

	cfg := string(headerMsg)
	if !strings.HasPrefix(cfg, "cfg:") {
		fmt.Fprintf(os.Stderr, "halstreamer: unexpected header: %s\n", cfg)
		os.Exit(1)
	}
	pinTypes := cfg[4:]
	numPins := len(pinTypes)

	scanner := bufio.NewScanner(os.Stdin)
	lineNum := 0

	for scanner.Scan() {
		if numLines >= 0 && lineNum >= numLines {
			break
		}

		line := strings.TrimSpace(scanner.Text())
		if line == "" {
			continue
		}

		fields := strings.Fields(line)
		if len(fields) < numPins {
			fmt.Fprintf(os.Stderr, "halstreamer: line %d: expected %d values, got %d\n",
				lineNum+1, numPins, len(fields))
			os.Exit(1)
		}

		// Encode one sample as binary (numPins * 8 bytes)
		buf := make([]byte, numPins*8)
		for i := 0; i < numPins; i++ {
			var raw uint64
			switch pinTypes[i] {
			case 'f':
				v, err := strconv.ParseFloat(fields[i], 64)
				if err != nil {
					fmt.Fprintf(os.Stderr, "halstreamer: line %d pin %d: %v\n",
						lineNum+1, i, err)
					os.Exit(1)
				}
				raw = math.Float64bits(v)
			case 'b':
				v, err := strconv.ParseUint(fields[i], 10, 1)
				if err != nil {
					fmt.Fprintf(os.Stderr, "halstreamer: line %d pin %d: %v\n",
						lineNum+1, i, err)
					os.Exit(1)
				}
				raw = v
			case 'u':
				v, err := strconv.ParseUint(fields[i], 10, 32)
				if err != nil {
					fmt.Fprintf(os.Stderr, "halstreamer: line %d pin %d: %v\n",
						lineNum+1, i, err)
					os.Exit(1)
				}
				raw = v
			case 's':
				v, err := strconv.ParseInt(fields[i], 10, 32)
				if err != nil {
					fmt.Fprintf(os.Stderr, "halstreamer: line %d pin %d: %v\n",
						lineNum+1, i, err)
					os.Exit(1)
				}
				raw = uint64(v)
			}
			binary.LittleEndian.PutUint64(buf[i*8:], raw)
		}

		err := conn.Write(ctx, websocket.MessageBinary, buf)
		if err != nil {
			if ctx.Err() != nil {
				break
			}
			fmt.Fprintf(os.Stderr, "halstreamer: write: %v\n", err)
			os.Exit(1)
		}

		lineNum++
	}

	if err := scanner.Err(); err != nil {
		fmt.Fprintf(os.Stderr, "halstreamer: stdin read error: %v\n", err)
		os.Exit(1)
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
