package adsconfig

import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
)

// ServerConf holds ADS server instance configuration parsed from $ directives
// in a .conf file.
type ServerConf struct {
	// Name is the HAL component name (from $name or the config filename stem).
	Name string
	// AMSNetID is the local AMS Net ID (from $ams-net-id, default "192.168.0.99.1.1").
	AMSNetID string
	// Bind is the IP address to bind the TCP listener to (from $bind, default "0.0.0.0").
	Bind string
	// Port is the TCP port for the ADS server (from $port, default 48898).
	Port int
}

// ParseConfFile reads an ADS .conf file, extracts $ directives for server
// configuration, and parses the remaining content as the symbol tree.
//
// $ directives are single-line key-value pairs at depth 0:
//
//	$name my-hmi-server
//	$ams-net-id 192.168.0.99.1.1
//	$bind 0.0.0.0
//	$port 48898
//
// The component name defaults to the filename stem (e.g., "hmi-server" from
// "hmi-server.conf") if $name is not specified.
//
// All remaining lines (including @enum, @struct, and the symbol tree) are
// passed to ParseTreeWithAliases.
func ParseConfFile(path string) (*ServerConf, TypeAliasMap, []*Node, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, nil, nil, fmt.Errorf("opening config %q: %w", path, err)
	}
	defer f.Close()

	conf := &ServerConf{
		Name:     strings.TrimSuffix(filepath.Base(path), filepath.Ext(path)),
		AMSNetID: "192.168.0.99.1.1",
		Bind:     "0.0.0.0",
		Port:     48898,
	}

	// First pass: extract $ directives and collect remaining lines.
	var remaining strings.Builder
	scanner := bufio.NewScanner(f)
	lineNo := 0
	for scanner.Scan() {
		lineNo++
		line := scanner.Text()
		trimmed := strings.TrimSpace(line)

		if strings.HasPrefix(trimmed, "$") {
			parts := strings.SplitN(trimmed, " ", 2)
			if len(parts) != 2 || strings.TrimSpace(parts[1]) == "" {
				return nil, nil, nil, fmt.Errorf("line %d: $ directive requires a value", lineNo)
			}
			key := parts[0]
			val := strings.TrimSpace(parts[1])

			switch key {
			case "$name":
				conf.Name = val
			case "$ams-net-id":
				conf.AMSNetID = val
			case "$bind":
				conf.Bind = val
			case "$port":
				p, err := strconv.Atoi(val)
				if err != nil {
					return nil, nil, nil, fmt.Errorf("line %d: invalid $port value %q: %w", lineNo, val, err)
				}
				if p <= 0 || p > 65535 {
					return nil, nil, nil, fmt.Errorf("line %d: $port %d out of range (1-65535)", lineNo, p)
				}
				conf.Port = p
			default:
				return nil, nil, nil, fmt.Errorf("line %d: unknown directive %q", lineNo, key)
			}
			continue
		}

		remaining.WriteString(line)
		remaining.WriteByte('\n')
	}
	if err := scanner.Err(); err != nil {
		return nil, nil, nil, fmt.Errorf("reading config %q: %w", path, err)
	}

	// Parse the remaining content as the symbol tree.
	aliases, tree, err := ParseTreeWithAliases(strings.NewReader(remaining.String()))
	if err != nil {
		return nil, nil, nil, err
	}

	return conf, aliases, tree, nil
}
