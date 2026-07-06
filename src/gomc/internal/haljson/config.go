// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package haljson

import (
	"encoding/xml"
	"fmt"
	"os"
	"strings"

	halparse "github.com/sittner/linuxcnc/src/gomc/internal/halparse"
)

// xmlConfig is the top-level XML document element.
type xmlConfig struct {
	XMLName xml.Name  `xml:"halJson"`
	Roots   []xmlRoot `xml:"halJsonRoot"`
}

// xmlRoot represents one REST/WS endpoint root.
type xmlRoot struct {
	Path     string    `xml:"path,attr"`
	Children []xmlItem `xml:",any"`
}

// xmlItem represents a pin, param, object, or array element in the XML.
type xmlItem struct {
	XMLName  xml.Name
	Name     string    `xml:"name,attr"`
	Type     string    `xml:"type,attr"`
	Dir      string    `xml:"dir,attr"`
	Size     int       `xml:"size,attr"`
	Children []xmlItem `xml:",any"`
}

// parseConfig parses an XML configuration file and returns the root definitions.
// The file is first rendered through Go's text/template engine using tmplData as
// the context (matching HAL file templating); a nil tmplData disables rendering.
func parseConfig(filename string, tmplData *halparse.HalTemplateData) ([]*jsonRoot, error) {
	data, err := os.ReadFile(filename)
	if err != nil {
		return nil, err
	}

	content := string(data)
	if tmplData != nil {
		content, err = halparse.RenderHalTemplate(filename, content, tmplData)
		if err != nil {
			return nil, err
		}
	}

	var cfg xmlConfig
	if err := xml.Unmarshal([]byte(content), &cfg); err != nil {
		return nil, fmt.Errorf("XML parse error: %w", err)
	}

	if len(cfg.Roots) == 0 {
		return nil, fmt.Errorf("no halJsonRoot elements found")
	}

	roots := make([]*jsonRoot, 0, len(cfg.Roots))
	for _, xr := range cfg.Roots {
		if xr.Path == "" {
			return nil, fmt.Errorf("halJsonRoot missing path attribute")
		}
		root := &jsonRoot{path: xr.Path}
		if err := root.parseItems(xr.Children); err != nil {
			return nil, fmt.Errorf("root %q: %w", xr.Path, err)
		}
		roots = append(roots, root)
	}

	return roots, nil
}

// parseHalType converts an XML type string to a pinType.
func parseHalType(s string) (pinType, error) {
	switch strings.ToLower(s) {
	case "bit":
		return pinTypeBit, nil
	case "float":
		return pinTypeFloat, nil
	case "s32":
		return pinTypeS32, nil
	case "u32":
		return pinTypeU32, nil
	default:
		return 0, fmt.Errorf("unsupported pin type %q", s)
	}
}

// parseHalDir converts an XML dir string to a hal.Direction.
func parseHalDir(s string) (pinDir, error) {
	switch strings.ToLower(s) {
	case "in":
		return pinDirIn, nil
	case "out":
		return pinDirOut, nil
	case "io":
		return pinDirIO, nil
	default:
		return 0, fmt.Errorf("unsupported pin direction %q", s)
	}
}
