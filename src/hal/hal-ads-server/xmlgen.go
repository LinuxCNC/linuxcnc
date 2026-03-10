package main

import (
	"encoding/binary"
	"encoding/xml"
	"fmt"
	"io"
	"sort"
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
// The optional aliases parameter provides the @enum and @struct alias map from
// the config file. Each @enum alias generates a <dataType> entry with an enum
// block, and each @struct alias generates a <dataType> entry with a struct block.
// Leaf nodes whose TypeName matches an alias emit a <derived name="AliasName" />
// element instead of the raw primitive element. Container nodes from the
// "struct varName TypeName" syntax emit <derived name="TypeName" />.
//
// Usage:
//
//	hal-ads-server -xml configs/galv-hmi.conf > output.xml
func GenerateXML(w io.Writer, roots []*Node, aliasOpts ...TypeAliasMap) error {
	var aliases TypeAliasMap
	if len(aliasOpts) > 0 {
		aliases = aliasOpts[0]
	}

	if len(roots) == 0 {
		return fmt.Errorf("xmlgen: no nodes to generate XML from")
	}

	gvlNode := roots[0]
	gvlName := gvlNode.Name

	// Collect struct type definitions (post-order: inner types first).
	tm := make(nodeTypeMap)
	nameSet := make(map[string]int)
	var typeDefs []genTypeDef

	// First walk @struct StructDef member trees so that their internal
	// containers (e.g. arrays inside a struct) are registered in tm.
	// This allows emitAliasDataTypes to look up type names for those nodes.
	if aliases != nil {
		// Walk in sorted order for determinism.
		structNames := make([]string, 0)
		for name, alias := range aliases {
			if alias.StructDef != nil {
				structNames = append(structNames, name)
			}
		}
		sort.Strings(structNames)
		for _, name := range structNames {
			alias := aliases[name]
			for _, child := range alias.StructDef {
				collectTypeDefs(child, nameSet, tm, &typeDefs, aliases)
			}
		}
	}

	for _, child := range gvlNode.Children {
		collectTypeDefs(child, nameSet, tm, &typeDefs, aliases)
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
	// Emit @enum and @struct alias dataType entries first (they may be
	// referenced by container type definitions below).
	if err := emitAliasDataTypes(e, aliases, tm); err != nil {
		return err
	}
	for _, td := range typeDefs {
		if err := emitDataType(e, td, tm, aliases); err != nil {
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
		if err := emitVariable(e, child, tm, aliases); err != nil {
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
// for each container node that needs a generated type definition. Container
// nodes that reference a named @struct type (node.TypeName != "") use the
// declared struct name directly instead of a generated T_xxx name, and are
// not added to typeDefs (their dataType is emitted by emitAliasDataTypes).
func collectTypeDefs(node *Node, nameSet map[string]int, tm nodeTypeMap, typeDefs *[]genTypeDef, aliases TypeAliasMap) {
	if len(node.Children) == 0 {
		return // leaf; no type definition needed
	}

	// If this container references a named @struct, register its type name
	// from the alias map and skip generating a T_xxx entry.
	if node.TypeName != "" && aliases != nil {
		if alias, ok := aliases[node.TypeName]; ok && alias.StructDef != nil {
			tm[node] = node.TypeName
			return
		}
	}

	// Post-order: collect children first so inner types are emitted first.
	for _, child := range node.Children {
		collectTypeDefs(child, nameSet, tm, typeDefs, aliases)
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

// emitAliasDataTypes emits <dataType> entries for each @enum and @struct alias.
//
// For @enum aliases (where alias.EnumValues != nil) it emits:
//
//	<dataType name="EnumName">
//	  <baseType>
//	    <enum><values>
//	      <value name="none" value="0" />
//	      <value name="precheck" />
//	      ...
//	    </values></enum>
//	  </baseType>
//	</dataType>
//
// For @struct aliases (where alias.StructDef != nil) it emits:
//
//	<dataType name="StructName">
//	  <baseType>
//	    <struct>
//	      <variable name="fieldName"><type>...</type></variable>
//	      ...
//	    </struct>
//	  </baseType>
//	</dataType>
//
// Only @enum members with HasExplicitValue == true include a value="N" attribute,
// matching the TwinCAT PLCopen XML convention.
func emitAliasDataTypes(e *errEncoder, aliases TypeAliasMap, tm nodeTypeMap) error {
	if len(aliases) == 0 {
		return nil
	}
	// Emit in stable sorted order for deterministic output.
	names := make([]string, 0, len(aliases))
	for name := range aliases {
		names = append(names, name)
	}
	sort.Strings(names)
	for _, name := range names {
		alias := aliases[name]
		e.start("dataType", xml.Attr{Name: xml.Name{Local: "name"}, Value: name})
		e.start("baseType")
		if alias.EnumValues != nil {
			// Emit <enum><values>...</values></enum> block.
			e.start("enum")
			e.start("values")
			for _, ev := range alias.EnumValues {
				var attrs []xml.Attr
				attrs = append(attrs, xml.Attr{Name: xml.Name{Local: "name"}, Value: ev.Name})
				if ev.HasExplicitValue {
					attrs = append(attrs, xml.Attr{Name: xml.Name{Local: "value"}, Value: fmt.Sprintf("%d", ev.Value)})
				}
				e.start("value", attrs...)
				e.end("value")
			}
			e.end("values")
			e.end("enum")
		} else if alias.StructDef != nil {
			// Emit <struct>...</struct> block with variables for each member.
			e.start("struct")
			for _, child := range alias.StructDef {
				if err := emitVariable(e, child, tm, aliases); err != nil {
					return err
				}
			}
			e.end("struct")
		} else {
			return fmt.Errorf("alias %q has neither EnumValues nor StructDef; @type aliases are no longer supported", name)
		}
		e.end("baseType")
		emitObjectIdAddData(e, alias.GUID)
		e.end("dataType")
	}
	return e.err
}

// emitDataType emits a <dataType> element for one genTypeDef.
func emitDataType(e *errEncoder, td genTypeDef, tm nodeTypeMap, aliases TypeAliasMap) error {
	e.start("dataType", xml.Attr{Name: xml.Name{Local: "name"}, Value: td.typeName})
	e.start("baseType")
	e.start("struct")

	for _, child := range td.node.Children {
		if err := emitVariable(e, child, tm, aliases); err != nil {
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
func emitVariable(e *errEncoder, node *Node, tm nodeTypeMap, aliases TypeAliasMap) error {
	e.start("variable", xml.Attr{Name: xml.Name{Local: "name"}, Value: node.Name})
	e.start("type")
	if err := emitTypeRef(e, node, tm, aliases); err != nil {
		return err
	}
	e.end("type")
	e.end("variable")
	return e.err
}

// emitTypeRef emits the type element(s) for a node: a primitive element
// (e.g. <BOOL />), a <string length="n" /> element, a <derived name="..." />
// element for struct containers or @enum/@struct aliases, or an <array> block
// for array containers.
func emitTypeRef(e *errEncoder, node *Node, tm nodeTypeMap, aliases TypeAliasMap) error {
	// Scalar leaf array (ArrayStart > 0, no Children): emit <array> with a
	// primitive or alias base type directly.
	if node.ArrayStart > 0 && len(node.Children) == 0 {
		e.start("array")
		e.start("dimension",
			xml.Attr{Name: xml.Name{Local: "lower"}, Value: fmt.Sprintf("%d", node.ArrayStart)},
			xml.Attr{Name: xml.Name{Local: "upper"}, Value: fmt.Sprintf("%d", node.ArrayEnd)},
		)
		e.end("dimension")
		e.start("baseType")
		if aliases != nil {
			if _, ok := aliases[node.TypeName]; ok {
				e.start("derived", xml.Attr{Name: xml.Name{Local: "name"}, Value: node.TypeName})
				e.end("derived")
				e.end("baseType")
				e.end("array")
				return e.err
			}
		}
		if err := emitPrimitiveTypeElem(e, node.TypeName); err != nil {
			return err
		}
		e.end("baseType")
		e.end("array")
		return e.err
	}

	if len(node.Children) == 0 {
		// Leaf: check if TypeName is an alias → emit <derived name="AliasName" />.
		if aliases != nil {
			if _, ok := aliases[node.TypeName]; ok {
				e.start("derived", xml.Attr{Name: xml.Name{Local: "name"}, Value: node.TypeName})
				e.end("derived")
				return e.err
			}
		}
		// Primitive or string type.
		return emitPrimitiveTypeElem(e, node.TypeName)
	}

	// Container node with TypeName set (from a "struct varName TypeName" or
	// a "struct varName[s..e] TypeName" inline array): emit a <derived name="TypeName" /> reference.
	if node.TypeName != "" {
		e.start("derived", xml.Attr{Name: xml.Name{Local: "name"}, Value: node.TypeName})
		e.end("derived")
		return e.err
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

// formatGUID converts a 16-byte Microsoft COM wire-format GUID to the standard
// "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" string representation.
// This is the exact inverse of parseGUID in config.go.
func formatGUID(g [16]byte) string {
	data1 := binary.LittleEndian.Uint32(g[0:4])
	data2 := binary.LittleEndian.Uint16(g[4:6])
	data3 := binary.LittleEndian.Uint16(g[6:8])
	return fmt.Sprintf("%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		data1, data2, data3,
		g[8], g[9],
		g[10], g[11], g[12], g[13], g[14], g[15])
}

// emitObjectIdAddData emits the <addData><data name="...objectid"><ObjectId>
// block for the given GUID. If the GUID is all zeros, the block is not emitted.
func emitObjectIdAddData(e *errEncoder, guid [16]byte) {
	var zero [16]byte
	if guid == zero {
		return
	}
	e.start("addData")
	e.start("data",
		xml.Attr{Name: xml.Name{Local: "name"}, Value: "http://www.3s-software.com/plcopenxml/objectid"},
		xml.Attr{Name: xml.Name{Local: "handleUnknown"}, Value: "discard"},
	)
	e.start("ObjectId")
	e.token(xml.CharData(formatGUID(guid)))
	e.end("ObjectId")
	e.end("data")
	e.end("addData")
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
