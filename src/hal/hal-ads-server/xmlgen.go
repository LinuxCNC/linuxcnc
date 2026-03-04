package main

import (
	"encoding/xml"
	"fmt"
	"io"
	"strings"
)

// errEncoder wraps *xml.Encoder to accumulate the first error and short-circuit
// subsequent calls after a failure.
type errEncoder struct {
	enc *xml.Encoder
	err error
}

func (e *errEncoder) token(t xml.Token) {
	if e.err == nil {
		e.err = e.enc.EncodeToken(t)
	}
}

func (e *errEncoder) start(name string, attrs ...xml.Attr) {
	e.token(xml.StartElement{Name: xml.Name{Local: name}, Attr: attrs})
}

func (e *errEncoder) end(name string) {
	e.token(xml.EndElement{Name: xml.Name{Local: name}})
}

func (e *errEncoder) flush() error {
	if e.err != nil {
		return e.err
	}
	return e.enc.Flush()
}

// genTypeDef associates a generated type name with a container Node.
// Types are emitted post-order so inner types are defined before they are
// referenced by outer types.
type genTypeDef struct {
	typeName string // generated type name, e.g. "T_stData", "T_aPools_ITEM"
	node     *Node  // the container node (struct or array)
}

// nodeTypeMap maps container node pointers to their generated type names.
type nodeTypeMap map[*Node]string

// GenerateXML writes a PLCopen TC6 XML document to w from the given Node tree.
//
// The first root node is used as the GVL name; its immediate children become
// GVL variables. Each container node in the tree becomes a named <dataType>
// (struct). Type names are derived from the node name with a "T_" prefix;
// array element types get an additional "_ITEM" suffix.
//
// Usage:
//
//	hal-ads-server -xml configs/galv-hmi.conf > output.xml
func GenerateXML(w io.Writer, roots []*Node) error {
	if len(roots) == 0 {
		return fmt.Errorf("xmlgen: no nodes to generate XML from")
	}

	gvlNode := roots[0]
	gvlName := gvlNode.Name

	// Collect struct type definitions (post-order: inner types first).
	tm := make(nodeTypeMap)
	nameSet := make(map[string]int)
	var typeDefs []genTypeDef
	for _, child := range gvlNode.Children {
		collectTypeDefs(child, nameSet, tm, &typeDefs)
	}

	// Write the XML processing instruction first (encoder doesn't add it).
	if _, err := io.WriteString(w, `<?xml version="1.0" encoding="utf-8"?>`+"\n"); err != nil {
		return err
	}

	e := &errEncoder{enc: xml.NewEncoder(w)}
	e.enc.Indent("", "  ")

	// <project xmlns="...">
	e.start("project", xml.Attr{Name: xml.Name{Local: "xmlns"}, Value: "http://www.plcopen.org/xml/tc6_0200"})

	// <fileHeader ... />
	e.start("fileHeader",
		xml.Attr{Name: xml.Name{Local: "companyName"}, Value: ""},
		xml.Attr{Name: xml.Name{Local: "productName"}, Value: "hal-ads-server"},
		xml.Attr{Name: xml.Name{Local: "productVersion"}, Value: "1.0"},
		xml.Attr{Name: xml.Name{Local: "creationDateTime"}, Value: ""},
	)
	e.end("fileHeader")

	// <contentHeader name="GVLName"> ... </contentHeader>
	e.start("contentHeader", xml.Attr{Name: xml.Name{Local: "name"}, Value: gvlName})
	e.start("coordinateInfo")
	for _, elem := range []string{"fbd", "ld", "sfc"} {
		e.start(elem)
		e.start("scaling",
			xml.Attr{Name: xml.Name{Local: "x"}, Value: "1"},
			xml.Attr{Name: xml.Name{Local: "y"}, Value: "1"},
		)
		e.end("scaling")
		e.end(elem)
	}
	e.end("coordinateInfo")
	e.end("contentHeader")

	// <types> <dataTypes> ... </dataTypes> <pous /> </types>
	e.start("types")
	e.start("dataTypes")
	for _, td := range typeDefs {
		if err := emitDataType(e, td, tm); err != nil {
			return err
		}
	}
	e.end("dataTypes")
	e.start("pous")
	e.end("pous")
	e.end("types")

	// <instances> <configurations /> </instances>
	e.start("instances")
	e.start("configurations")
	e.end("configurations")
	e.end("instances")

	// <addData> <data name="...globalvars"> <globalVars name="..."> ...
	e.start("addData")
	e.start("data",
		xml.Attr{Name: xml.Name{Local: "name"}, Value: "http://www.3s-software.com/plcopenxml/globalvars"},
		xml.Attr{Name: xml.Name{Local: "handleUnknown"}, Value: "implementation"},
	)
	e.start("globalVars", xml.Attr{Name: xml.Name{Local: "name"}, Value: gvlName})
	for _, child := range gvlNode.Children {
		if err := emitVariable(e, child, tm); err != nil {
			return err
		}
	}
	e.end("globalVars")
	e.end("data")
	e.end("addData")

	e.end("project")
	return e.flush()
}

// collectTypeDefs walks the node tree (post-order) and appends a genTypeDef
// for each container node. It also populates tm with type name mappings.
func collectTypeDefs(node *Node, nameSet map[string]int, tm nodeTypeMap, typeDefs *[]genTypeDef) {
	if len(node.Children) == 0 {
		return // leaf; no type definition needed
	}

	// Post-order: collect children first so inner types are emitted first.
	for _, child := range node.Children {
		collectTypeDefs(child, nameSet, tm, typeDefs)
	}

	// Determine base name for this type.
	baseName := node.Name
	if node.ArrayStart > 0 {
		baseName += "_ITEM"
	}
	typeName := uniqueName("T_"+baseName, nameSet)

	tm[node] = typeName
	*typeDefs = append(*typeDefs, genTypeDef{typeName: typeName, node: node})
}

// uniqueName returns "prefix" if it hasn't been used, otherwise "prefix_2",
// "prefix_3", … until a free name is found.
func uniqueName(prefix string, nameSet map[string]int) string {
	if _, exists := nameSet[prefix]; !exists {
		nameSet[prefix] = 1
		return prefix
	}
	for {
		nameSet[prefix]++
		candidate := fmt.Sprintf("%s_%d", prefix, nameSet[prefix])
		if _, ok := nameSet[candidate]; !ok {
			nameSet[candidate] = 1
			return candidate
		}
	}
}

// emitDataType emits a <dataType> element for one genTypeDef.
func emitDataType(e *errEncoder, td genTypeDef, tm nodeTypeMap) error {
	e.start("dataType", xml.Attr{Name: xml.Name{Local: "name"}, Value: td.typeName})
	e.start("baseType")
	e.start("struct")

	for _, child := range td.node.Children {
		if err := emitVariable(e, child, tm); err != nil {
			return err
		}
	}

	e.end("struct")
	e.end("baseType")
	e.end("dataType")
	return e.err
}

// emitVariable emits a <variable name="..."><type>...</type></variable> element.
// Pad nodes (Dir == DirPad) are intentionally emitted as variables in the
// PLCopen XML output. They represent explicit padding fields that TwinCAT
// expects in the struct layout to maintain correct alignment.
func emitVariable(e *errEncoder, node *Node, tm nodeTypeMap) error {
	e.start("variable", xml.Attr{Name: xml.Name{Local: "name"}, Value: node.Name})
	e.start("type")
	if err := emitTypeRef(e, node, tm); err != nil {
		return err
	}
	e.end("type")
	e.end("variable")
	return e.err
}

// emitTypeRef emits the type element(s) for a node: a primitive element
// (e.g. <BOOL />), a <string length="n" /> element, a <derived name="..." />
// element for struct containers, or an <array> block for array containers.
func emitTypeRef(e *errEncoder, node *Node, tm nodeTypeMap) error {
	if len(node.Children) == 0 {
		// Leaf: primitive or string type.
		return emitPrimitiveTypeElem(e, node.TypeName)
	}
	if node.ArrayStart > 0 {
		// Array container.
		elemTypeName := tm[node]
		e.start("array")
		e.start("dimension",
			xml.Attr{Name: xml.Name{Local: "lower"}, Value: fmt.Sprintf("%d", node.ArrayStart)},
			xml.Attr{Name: xml.Name{Local: "upper"}, Value: fmt.Sprintf("%d", node.ArrayEnd)},
		)
		e.end("dimension")
		e.start("baseType")
		e.start("derived", xml.Attr{Name: xml.Name{Local: "name"}, Value: elemTypeName})
		e.end("derived")
		e.end("baseType")
		e.end("array")
		return e.err
	}
	// Struct container: reference by generated type name.
	structTypeName := tm[node]
	e.start("derived", xml.Attr{Name: xml.Name{Local: "name"}, Value: structTypeName})
	e.end("derived")
	return e.err
}

// emitPrimitiveTypeElem emits the XML element for a primitive IEC 61131-3 type.
// For STRING(n) it emits <string length="n" />; for all other types it emits
// the type name as an empty element, e.g. <BOOL />, <REAL />, <DT />.
func emitPrimitiveTypeElem(e *errEncoder, typeName string) error {
	if strings.HasPrefix(typeName, "STRING(") {
		var n int
		if _, err := fmt.Sscanf(typeName, "STRING(%d)", &n); err != nil || n <= 0 {
			return fmt.Errorf("xmlgen: invalid string type %q", typeName)
		}
		e.start("string", xml.Attr{Name: xml.Name{Local: "length"}, Value: fmt.Sprintf("%d", n)})
		e.end("string")
		return e.err
	}
	e.start(typeName)
	e.end(typeName)
	return e.err
}
