package main

import (
	"encoding/xml"
	"fmt"
	"io"
	"strings"
)

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

	enc := xml.NewEncoder(w)
	enc.Indent("", "  ")

	// <project xmlns="...">
	project := xml.StartElement{
		Name: xml.Name{Local: "project"},
		Attr: []xml.Attr{{Name: xml.Name{Local: "xmlns"}, Value: "http://www.plcopen.org/xml/tc6_0200"}},
	}
	enc.EncodeToken(project)

	// <fileHeader ... />
	enc.EncodeToken(xml.StartElement{
		Name: xml.Name{Local: "fileHeader"},
		Attr: []xml.Attr{
			{Name: xml.Name{Local: "companyName"}, Value: ""},
			{Name: xml.Name{Local: "productName"}, Value: "hal-ads-server"},
			{Name: xml.Name{Local: "productVersion"}, Value: "1.0"},
			{Name: xml.Name{Local: "creationDateTime"}, Value: ""},
		},
	})
	enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: "fileHeader"}})

	// <contentHeader name="GVLName"> ... </contentHeader>
	enc.EncodeToken(xml.StartElement{
		Name: xml.Name{Local: "contentHeader"},
		Attr: []xml.Attr{{Name: xml.Name{Local: "name"}, Value: gvlName}},
	})
	enc.EncodeToken(xml.StartElement{Name: xml.Name{Local: "coordinateInfo"}})
	for _, elem := range []string{"fbd", "ld", "sfc"} {
		enc.EncodeToken(xml.StartElement{Name: xml.Name{Local: elem}})
		enc.EncodeToken(xml.StartElement{
			Name: xml.Name{Local: "scaling"},
			Attr: []xml.Attr{
				{Name: xml.Name{Local: "x"}, Value: "1"},
				{Name: xml.Name{Local: "y"}, Value: "1"},
			},
		})
		enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: "scaling"}})
		enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: elem}})
	}
	enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: "coordinateInfo"}})
	enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: "contentHeader"}})

	// <types> <dataTypes> ... </dataTypes> <pous /> </types>
	enc.EncodeToken(xml.StartElement{Name: xml.Name{Local: "types"}})
	enc.EncodeToken(xml.StartElement{Name: xml.Name{Local: "dataTypes"}})
	for _, td := range typeDefs {
		if err := emitDataType(enc, td, tm); err != nil {
			return err
		}
	}
	enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: "dataTypes"}})
	enc.EncodeToken(xml.StartElement{Name: xml.Name{Local: "pous"}})
	enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: "pous"}})
	enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: "types"}})

	// <instances> <configurations /> </instances>
	enc.EncodeToken(xml.StartElement{Name: xml.Name{Local: "instances"}})
	enc.EncodeToken(xml.StartElement{Name: xml.Name{Local: "configurations"}})
	enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: "configurations"}})
	enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: "instances"}})

	// <addData> <data name="...globalvars"> <globalVars name="..."> ...
	enc.EncodeToken(xml.StartElement{Name: xml.Name{Local: "addData"}})
	enc.EncodeToken(xml.StartElement{
		Name: xml.Name{Local: "data"},
		Attr: []xml.Attr{
			{Name: xml.Name{Local: "name"}, Value: "http://www.3s-software.com/plcopenxml/globalvars"},
			{Name: xml.Name{Local: "handleUnknown"}, Value: "implementation"},
		},
	})
	enc.EncodeToken(xml.StartElement{
		Name: xml.Name{Local: "globalVars"},
		Attr: []xml.Attr{{Name: xml.Name{Local: "name"}, Value: gvlName}},
	})
	for _, child := range gvlNode.Children {
		if err := emitVariable(enc, child, tm); err != nil {
			return err
		}
	}
	enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: "globalVars"}})
	enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: "data"}})
	enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: "addData"}})

	enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: "project"}})
	return enc.Flush()
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
func emitDataType(enc *xml.Encoder, td genTypeDef, tm nodeTypeMap) error {
	enc.EncodeToken(xml.StartElement{
		Name: xml.Name{Local: "dataType"},
		Attr: []xml.Attr{{Name: xml.Name{Local: "name"}, Value: td.typeName}},
	})
	enc.EncodeToken(xml.StartElement{Name: xml.Name{Local: "baseType"}})
	enc.EncodeToken(xml.StartElement{Name: xml.Name{Local: "struct"}})

	for _, child := range td.node.Children {
		if err := emitVariable(enc, child, tm); err != nil {
			return err
		}
	}

	enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: "struct"}})
	enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: "baseType"}})
	enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: "dataType"}})
	return nil
}

// emitVariable emits a <variable name="..."><type>...</type></variable> element.
// Pad nodes (Dir == DirPad) are intentionally emitted as variables in the
// PLCopen XML output. They represent explicit padding fields that TwinCAT
// expects in the struct layout to maintain correct alignment.
func emitVariable(enc *xml.Encoder, node *Node, tm nodeTypeMap) error {
	enc.EncodeToken(xml.StartElement{
		Name: xml.Name{Local: "variable"},
		Attr: []xml.Attr{{Name: xml.Name{Local: "name"}, Value: node.Name}},
	})
	enc.EncodeToken(xml.StartElement{Name: xml.Name{Local: "type"}})
	if err := emitTypeRef(enc, node, tm); err != nil {
		return err
	}
	enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: "type"}})
	enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: "variable"}})
	return nil
}

// emitTypeRef emits the type element(s) for a node: a primitive element
// (e.g. <BOOL />), a <string length="n" /> element, a <derived name="..." />
// element for struct containers, or an <array> block for array containers.
func emitTypeRef(enc *xml.Encoder, node *Node, tm nodeTypeMap) error {
	if len(node.Children) == 0 {
		// Leaf: primitive or string type.
		return emitPrimitiveTypeElem(enc, node.TypeName)
	}
	if node.ArrayStart > 0 {
		// Array container.
		elemTypeName := tm[node]
		enc.EncodeToken(xml.StartElement{Name: xml.Name{Local: "array"}})
		enc.EncodeToken(xml.StartElement{
			Name: xml.Name{Local: "dimension"},
			Attr: []xml.Attr{
				{Name: xml.Name{Local: "lower"}, Value: fmt.Sprintf("%d", node.ArrayStart)},
				{Name: xml.Name{Local: "upper"}, Value: fmt.Sprintf("%d", node.ArrayEnd)},
			},
		})
		enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: "dimension"}})
		enc.EncodeToken(xml.StartElement{Name: xml.Name{Local: "baseType"}})
		enc.EncodeToken(xml.StartElement{
			Name: xml.Name{Local: "derived"},
			Attr: []xml.Attr{{Name: xml.Name{Local: "name"}, Value: elemTypeName}},
		})
		enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: "derived"}})
		enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: "baseType"}})
		enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: "array"}})
		return nil
	}
	// Struct container: reference by generated type name.
	structTypeName := tm[node]
	enc.EncodeToken(xml.StartElement{
		Name: xml.Name{Local: "derived"},
		Attr: []xml.Attr{{Name: xml.Name{Local: "name"}, Value: structTypeName}},
	})
	enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: "derived"}})
	return nil
}

// emitPrimitiveTypeElem emits the XML element for a primitive IEC 61131-3 type.
// For STRING(n) it emits <string length="n" />; for all other types it emits
// the type name as an empty element, e.g. <BOOL />, <REAL />, <DT />.
func emitPrimitiveTypeElem(enc *xml.Encoder, typeName string) error {
	if strings.HasPrefix(typeName, "STRING(") {
		var n int
		if _, err := fmt.Sscanf(typeName, "STRING(%d)", &n); err != nil || n <= 0 {
			return fmt.Errorf("xmlgen: invalid string type %q", typeName)
		}
		enc.EncodeToken(xml.StartElement{
			Name: xml.Name{Local: "string"},
			Attr: []xml.Attr{{Name: xml.Name{Local: "length"}, Value: fmt.Sprintf("%d", n)}},
		})
		enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: "string"}})
		return nil
	}
	enc.EncodeToken(xml.StartElement{Name: xml.Name{Local: typeName}})
	enc.EncodeToken(xml.EndElement{Name: xml.Name{Local: typeName}})
	return nil
}
