// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package mqttbridge

import (
	"encoding/xml"
	"fmt"
	"os"
	"strings"
	"time"
)

// --- XML config structures ---

// xmlConfig is the top-level <mqttBridge> element.
type xmlConfig struct {
	XMLName  xml.Name   `xml:"mqttBridge"`
	Broker   string     `xml:"broker,attr"`
	ClientID string     `xml:"clientId,attr"`
	Username string     `xml:"username,attr"`
	Password string     `xml:"password,attr"`
	TLS      bool       `xml:"tls,attr"`
	Topics   []xmlTopic `xml:"topic"`
}

// xmlTopic represents one <topic> element.
type xmlTopic struct {
	Path    string   `xml:"path,attr"`
	Dir     string   `xml:"dir,attr"`     // "in" or "out"
	Type    string   `xml:"type,attr"`    // "pin" or "json"
	HalType string   `xml:"halType,attr"` // for type="pin": bit, float, s32, u32
	Rate    int      `xml:"rate,attr"`    // publish rate in ms (default 100)
	QoS     int      `xml:"qos,attr"`     // MQTT QoS (0, 1, 2)
	Retain  bool     `xml:"retain,attr"`  // MQTT retain flag
	Publish string   `xml:"publish,attr"` // "full", "delta", "always" (json mode)
	Pins    []xmlPin `xml:"pin"`          // child pins for json mode
}

// xmlPin represents a <pin> element inside a json topic.
type xmlPin struct {
	Name string `xml:"name,attr"`
	Type string `xml:"type,attr"` // bit, float, s32, u32
	Dir  string `xml:"dir,attr"`  // in or out (relative to HAL component)
}

// --- Parsed config ---

// Config holds the parsed MQTT bridge configuration.
type Config struct {
	Broker   string
	ClientID string
	Username string
	Password string
	TLS      bool
	Topics   []TopicConfig
}

// TopicConfig holds one topic's configuration.
type TopicConfig struct {
	Path        string
	Dir         TopicDir
	Mode        TopicMode
	Rate        time.Duration
	QoS         byte
	Retain      bool
	PublishMode PublishMode
	Pins        []PinConfig // for json mode
	// for pin mode (single pin)
	HalType PinType
}

// PinConfig holds one pin's configuration within a json topic.
type PinConfig struct {
	Name string
	Type PinType
	Dir  TopicDir
}

// TopicDir is the direction of a topic (publish or subscribe).
type TopicDir int

const (
	DirOut TopicDir = iota // publish: HAL → MQTT
	DirIn                  // subscribe: MQTT → HAL
)

// TopicMode selects between single-pin and JSON-grouped topics.
type TopicMode int

const (
	ModePin  TopicMode = iota // one topic = one pin, raw value
	ModeJSON                  // one topic = JSON object with multiple pins
)

// PublishMode controls when JSON publish messages are sent.
type PublishMode int

const (
	PublishFull   PublishMode = iota // send all values when any changed
	PublishDelta                     // send only changed values
	PublishAlways                    // send at every tick regardless
)

// PinType is the HAL pin data type.
type PinType int

const (
	PinTypeBit PinType = iota
	PinTypeFloat
	PinTypeS32
	PinTypeU32
)

// parseConfig reads and parses an XML configuration file.
func parseConfig(filename string) (*Config, error) {
	data, err := os.ReadFile(filename)
	if err != nil {
		return nil, fmt.Errorf("reading config: %w", err)
	}

	var xc xmlConfig
	if err := xml.Unmarshal(data, &xc); err != nil {
		return nil, fmt.Errorf("XML parse error: %w", err)
	}

	if xc.Broker == "" {
		return nil, fmt.Errorf("mqttBridge: missing broker attribute")
	}

	cfg := &Config{
		Broker:   xc.Broker,
		ClientID: xc.ClientID,
		Username: xc.Username,
		Password: xc.Password,
		TLS:      xc.TLS,
	}

	if cfg.ClientID == "" {
		cfg.ClientID = "linuxcnc-mqtt"
	}

	for i, xt := range xc.Topics {
		tc, err := parseTopic(xt, i)
		if err != nil {
			return nil, err
		}
		cfg.Topics = append(cfg.Topics, tc)
	}

	return cfg, nil
}

func parseTopic(xt xmlTopic, idx int) (TopicConfig, error) {
	if xt.Path == "" {
		return TopicConfig{}, fmt.Errorf("topic[%d]: missing path attribute", idx)
	}

	tc := TopicConfig{
		Path:   xt.Path,
		QoS:    byte(xt.QoS),
		Retain: xt.Retain,
	}

	// Direction
	switch strings.ToLower(xt.Dir) {
	case "out", "publish", "":
		tc.Dir = DirOut
	case "in", "subscribe":
		tc.Dir = DirIn
	default:
		return TopicConfig{}, fmt.Errorf("topic %q: invalid dir %q", xt.Path, xt.Dir)
	}

	// Mode
	switch strings.ToLower(xt.Type) {
	case "pin":
		tc.Mode = ModePin
		pt, err := parsePinType(xt.HalType)
		if err != nil {
			return TopicConfig{}, fmt.Errorf("topic %q: %w", xt.Path, err)
		}
		tc.HalType = pt
	case "json", "":
		tc.Mode = ModeJSON
		for j, xp := range xt.Pins {
			pc, err := parsePin(xp, xt.Path, j)
			if err != nil {
				return TopicConfig{}, err
			}
			tc.Pins = append(tc.Pins, pc)
		}
		if len(tc.Pins) == 0 {
			return TopicConfig{}, fmt.Errorf("topic %q: json mode requires at least one <pin>", xt.Path)
		}
	default:
		return TopicConfig{}, fmt.Errorf("topic %q: invalid type %q (use \"pin\" or \"json\")", xt.Path, xt.Type)
	}

	// Rate
	rate := xt.Rate
	if rate <= 0 {
		rate = 100
	}
	tc.Rate = time.Duration(rate) * time.Millisecond

	// Publish mode
	switch strings.ToLower(xt.Publish) {
	case "delta":
		tc.PublishMode = PublishDelta
	case "always":
		tc.PublishMode = PublishAlways
	default:
		tc.PublishMode = PublishFull
	}

	return tc, nil
}

func parsePin(xp xmlPin, topicPath string, idx int) (PinConfig, error) {
	if xp.Name == "" {
		return PinConfig{}, fmt.Errorf("topic %q pin[%d]: missing name", topicPath, idx)
	}
	pt, err := parsePinType(xp.Type)
	if err != nil {
		return PinConfig{}, fmt.Errorf("topic %q pin %q: %w", topicPath, xp.Name, err)
	}
	pc := PinConfig{
		Name: xp.Name,
		Type: pt,
	}
	switch strings.ToLower(xp.Dir) {
	case "in":
		pc.Dir = DirIn
	case "out", "":
		pc.Dir = DirOut
	default:
		return PinConfig{}, fmt.Errorf("topic %q pin %q: invalid dir %q", topicPath, xp.Name, xp.Dir)
	}
	return pc, nil
}

func parsePinType(s string) (PinType, error) {
	switch strings.ToLower(s) {
	case "bit":
		return PinTypeBit, nil
	case "float":
		return PinTypeFloat, nil
	case "s32":
		return PinTypeS32, nil
	case "u32":
		return PinTypeU32, nil
	default:
		return 0, fmt.Errorf("unsupported pin type %q", s)
	}
}
