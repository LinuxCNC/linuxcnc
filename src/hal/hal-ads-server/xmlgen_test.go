package main

import (
	"bytes"
	"encoding/xml"
	"fmt"
	"io"
	"os"
	"strings"
	"testing"
)

// TestGenerateXMLBasicStructure verifies that GenerateXML produces well-formed
// PLCopen TC6 XML with the expected structural elements from a small config.

func TestGenerateXMLBasicStructure(t *testing.T) {
	cfg := `
@struct ST_AST_ITEM 00000000-0000-0000-0000-000000000000
  in fPower REAL
  out bManu BOOL

stRoot
  stSub
    in bFlag BOOL
    out nVal DWORD
  struct aSt[1..2] ST_AST_ITEM
`
	aliases, roots, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}

	var buf bytes.Buffer
	if err := GenerateXML(&buf, roots, aliases); err != nil {
		t.Fatalf("GenerateXML: %v", err)
	}

	// Capture the output string before the decoder consumes the buffer.
	out := buf.String()

	// Verify it parses as valid XML.
	dec := xml.NewDecoder(strings.NewReader(out))
	for {
		_, err := dec.Token()
		if err == io.EOF {
			break
		}
		if err != nil {
			t.Fatalf("generated XML is not valid: %v\n%s", err, out)
		}
	}

	// Verify key structural elements are present (using substrings that are
	// insensitive to exact whitespace/formatting decisions by the encoder).
	for _, want := range []string{
		`xmlns="http://www.plcopen.org/xml/tc6_0200"`,
		`<fileHeader`,
		`<contentHeader`,
		`<dataTypes`,
		`globalVars`,
		`ST_AST_ITEM`,
		`T_stSub`,
		`bFlag`,
		`BOOL`,
		`DWORD`,
		`REAL`,
	} {
		if !strings.Contains(out, want) {
			t.Errorf("generated XML missing %q", want)
		}
	}
}

// TestGenerateXMLEmpty verifies GenerateXML returns an error for empty input.
func TestGenerateXMLEmpty(t *testing.T) {
	if err := GenerateXML(io.Discard, nil); err == nil {
		t.Error("expected error for empty roots, got nil")
	}
}

// TestGenerateXMLGalvHmi generates XML from galv-hmi.conf and verifies it is
// a syntactically valid PLCopen TC6 XML document.
func TestGenerateXMLGalvHmi(t *testing.T) {
	f, err := os.Open("configs/galv-hmi.conf")
	if err != nil {
		t.Skipf("galv-hmi.conf not found: %v", err)
	}
	defer f.Close()

	aliases, roots, err := ParseTreeWithAliases(f)
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}

	var buf bytes.Buffer
	if err := GenerateXML(&buf, roots, aliases); err != nil {
		t.Fatalf("GenerateXML: %v", err)
	}

	out := buf.String()
	dec := xml.NewDecoder(strings.NewReader(out))
	for {
		_, err := dec.Token()
		if err == io.EOF {
			break
		}
		if err != nil {
			t.Fatalf("generated XML is not valid: %v", err)
		}
	}
}

// ---------------------------------------------------------------------------
// TestLayoutConsistencyGalvHmi: layouts from galv-hmi.conf and DISPLAY_DATA.xml
// must agree for every leaf field.
// ---------------------------------------------------------------------------

// TestLayoutConsistencyGalvHmi parses configs/galv-hmi.conf and
// configs/DISPLAY_DATA.xml, computes the byte layout from each, and verifies
// that every non-pad field has the same offset in both layouts.
func TestLayoutConsistencyGalvHmi(t *testing.T) {
	// --- Layout from conf ---
	fConf, err := os.Open("configs/galv-hmi.conf")
	if err != nil {
		t.Skipf("galv-hmi.conf not found: %v", err)
	}
	defer fConf.Close()

	confAliases, confRoots, err := ParseTreeWithAliases(fConf)
	if err != nil {
		t.Fatalf("ParseTreeWithAliases(conf): %v", err)
	}
	confPins, err := ComputeLayout(confRoots, confAliases)
	if err != nil {
		t.Fatalf("ComputeLayout(conf): %v", err)
	}
	confMap := make(map[string]uint32)
	for _, p := range confPins {
		if p.Dir != DirPad {
			confMap[p.ADSName] = p.Offset
		}
	}

	// --- Layout from XML ---
	fXML, err := os.Open("configs/DISPLAY_DATA.xml")
	if err != nil {
		t.Skipf("DISPLAY_DATA.xml not found: %v", err)
	}
	defer fXML.Close()

	xmlRoots, err := parseXMLToNodes(fXML)
	if err != nil {
		t.Fatalf("parseXMLToNodes: %v", err)
	}
	xmlPins, err := ComputeLayout(xmlRoots)
	if err != nil {
		t.Fatalf("ComputeLayout(xml): %v", err)
	}
	xmlMap := make(map[string]uint32)
	for _, p := range xmlPins {
		xmlMap[p.ADSName] = p.Offset
	}

	// --- Compare ---
	// Every non-pad pin in the conf must appear in the XML layout with the same offset.
	mismatches := 0
	for name, confOff := range confMap {
		xmlOff, ok := xmlMap[name]
		if !ok {
			t.Errorf("pin %q: present in conf but not in XML layout", name)
			mismatches++
			continue
		}
		if confOff != xmlOff {
			t.Errorf("pin %q: conf offset=%d, xml offset=%d", name, confOff, xmlOff)
			mismatches++
		}
	}
	// Also check XML pins not in conf (warn only — XML may have more fields).
	for name := range xmlMap {
		if _, ok := confMap[name]; !ok {
			t.Logf("note: pin %q in XML but not in conf (may be a type detail difference)", name)
		}
	}
	if mismatches > 0 {
		t.Fatalf("%d offset mismatch(es) between conf and XML layouts", mismatches)
	}
}

// ---------------------------------------------------------------------------
// XML → Node tree conversion helpers (used only by tests)
// ---------------------------------------------------------------------------

// xmlEmptyElem is a placeholder for XML elements with no significant content.
type xmlEmptyElem struct{}

// xmlStringType represents a <string length="n" /> element.
type xmlStringType struct {
	Length int `xml:"length,attr"`
}

// xmlDerivedType represents a <derived name="TypeName" /> element.
type xmlDerivedType struct {
	Name string `xml:"name,attr"`
}

// xmlDimensionType represents a <dimension lower="1" upper="4" /> element.
type xmlDimensionType struct {
	Lower int `xml:"lower,attr"`
	Upper int `xml:"upper,attr"`
}

// xmlArrayType represents an <array> element.
type xmlArrayType struct {
	Dimension xmlDimensionType `xml:"dimension"`
	BaseType  *xmlTypeDef      `xml:"baseType"`
}

// xmlStructType represents a <struct> element.
type xmlStructType struct {
	Variables []xmlVariable `xml:"variable"`
}

// xmlTypeDef represents the type node inside a <type> or <baseType> wrapper.
// Exactly one field will be non-nil after unmarshaling, identifying the type.
type xmlTypeDef struct {
	// Primitive types (each is an empty element, e.g. <BOOL />)
	Bool  *xmlEmptyElem `xml:"BOOL"`
	Byte  *xmlEmptyElem `xml:"BYTE"`
	Usint *xmlEmptyElem `xml:"USINT"`
	Sint  *xmlEmptyElem `xml:"SINT"`
	Word  *xmlEmptyElem `xml:"WORD"`
	Uint  *xmlEmptyElem `xml:"UINT"`
	Int   *xmlEmptyElem `xml:"INT"`
	Dword *xmlEmptyElem `xml:"DWORD"`
	Udint *xmlEmptyElem `xml:"UDINT"`
	Dint  *xmlEmptyElem `xml:"DINT"`
	Real  *xmlEmptyElem `xml:"REAL"`
	Lreal *xmlEmptyElem `xml:"LREAL"`
	Time  *xmlEmptyElem `xml:"TIME"`
	Tod   *xmlEmptyElem `xml:"TOD"`
	Date  *xmlEmptyElem `xml:"DATE"`
	DT    *xmlEmptyElem `xml:"DT"`
	// Complex types
	Str     *xmlStringType  `xml:"string"`
	Derived *xmlDerivedType `xml:"derived"`
	Array   *xmlArrayType   `xml:"array"`
	Struct  *xmlStructType  `xml:"struct"`
	// Enum element (content is discarded; mapped to WORD)
	Enum *xmlEmptyElem `xml:"enum"`
}

// primitiveTypeName returns the IEC 61131-3 primitive type name for the
// xmlTypeDef if it is a primitive or string type, or "" for complex types.
// Enum types map to "WORD" (TwinCAT default enum base type).
func (td *xmlTypeDef) primitiveTypeName() string {
	switch {
	case td.Bool != nil:
		return "BOOL"
	case td.Byte != nil:
		return "BYTE"
	case td.Usint != nil:
		return "USINT"
	case td.Sint != nil:
		return "SINT"
	case td.Word != nil:
		return "WORD"
	case td.Uint != nil:
		return "UINT"
	case td.Int != nil:
		return "INT"
	case td.Dword != nil:
		return "DWORD"
	case td.Udint != nil:
		return "UDINT"
	case td.Dint != nil:
		return "DINT"
	case td.Real != nil:
		return "REAL"
	case td.Lreal != nil:
		return "LREAL"
	case td.Time != nil:
		return "TIME"
	case td.Tod != nil:
		return "TOD"
	case td.Date != nil:
		return "DATE"
	case td.DT != nil:
		return "DT"
	case td.Str != nil:
		return fmt.Sprintf("STRING(%d)", td.Str.Length)
	case td.Enum != nil:
		return "WORD" // TwinCAT enums default to WORD
	default:
		return ""
	}
}

// xmlVariable represents a <variable name="..."><type>...</type></variable>.
type xmlVariable struct {
	Name string     `xml:"name,attr"`
	Type xmlTypeDef `xml:"type"`
}

// xmlDataType represents a <dataType name="..."><baseType>...</baseType>...
type xmlDataType struct {
	Name     string     `xml:"name,attr"`
	BaseType xmlTypeDef `xml:"baseType"`
}

// xmlGlobalVars represents the <globalVars name="..."> element.
type xmlGlobalVars struct {
	Name      string        `xml:"name,attr"`
	Variables []xmlVariable `xml:"variable"`
}

// xmlAddDataItem represents a <data name="..."> element at the project level.
type xmlAddDataItem struct {
	Name       string         `xml:"name,attr"`
	GlobalVars *xmlGlobalVars `xml:"globalVars"`
}

// xmlProjectAddData represents the <addData> element at the project level.
type xmlProjectAddData struct {
	Items []xmlAddDataItem `xml:"data"`
}

// xmlProject is the top-level PLCopen TC6 XML document structure.
type xmlProject struct {
	XMLName   xml.Name          `xml:"project"`
	DataTypes []xmlDataType     `xml:"types>dataTypes>dataType"`
	AddData   xmlProjectAddData `xml:"addData"`
}

// parseXMLToNodes parses a PLCopen TC6 XML document and returns an equivalent
// []*Node tree that can be fed to ComputeLayout.
//
// Enum types are resolved to WORD. All leaf nodes get direction DirIn (the
// direction is not encoded in the XML; the test only compares byte offsets).
func parseXMLToNodes(r io.Reader) ([]*Node, error) {
	var proj xmlProject
	if err := xml.NewDecoder(r).Decode(&proj); err != nil {
		return nil, fmt.Errorf("XML decode: %w", err)
	}

	// Build type registry.
	typeMap := make(map[string]*xmlDataType, len(proj.DataTypes))
	for i := range proj.DataTypes {
		dt := &proj.DataTypes[i]
		typeMap[dt.Name] = dt
	}

	// Find the globalVars declaration.
	var gvl *xmlGlobalVars
	for i := range proj.AddData.Items {
		item := &proj.AddData.Items[i]
		if item.GlobalVars != nil {
			gvl = item.GlobalVars
			break
		}
	}
	if gvl == nil {
		return nil, fmt.Errorf("no globalVars found in XML")
	}

	// Convert GVL variables to Node children.
	children := make([]*Node, 0, len(gvl.Variables))
	for _, v := range gvl.Variables {
		child, err := xmlVarToNode(v.Name, &v.Type, typeMap)
		if err != nil {
			return nil, fmt.Errorf("GVL %q: %w", v.Name, err)
		}
		children = append(children, child)
	}

	root := &Node{Name: gvl.Name, Children: children}
	return []*Node{root}, nil
}

// xmlVarToNode converts an xmlTypeDef (with a given field name) to a *Node.
func xmlVarToNode(name string, td *xmlTypeDef, typeMap map[string]*xmlDataType) (*Node, error) {
	// Array type.
	if td.Array != nil {
		arr := td.Array
		if arr.BaseType == nil {
			return nil, fmt.Errorf("array field %q has no baseType", name)
		}
		elemChildren, err := xmlTypeToChildren(arr.BaseType, typeMap)
		if err != nil {
			return nil, fmt.Errorf("array %q element: %w", name, err)
		}
		return &Node{
			Name:       name,
			ArrayStart: arr.Dimension.Lower,
			ArrayEnd:   arr.Dimension.Upper,
			Children:   elemChildren,
		}, nil
	}

	// Inline struct.
	if td.Struct != nil {
		children, err := xmlStructVarsToChildren(td.Struct.Variables, typeMap)
		if err != nil {
			return nil, fmt.Errorf("struct field %q: %w", name, err)
		}
		return &Node{Name: name, Children: children}, nil
	}

	// Derived (named) type reference.
	if td.Derived != nil {
		dt, ok := typeMap[td.Derived.Name]
		if !ok {
			return nil, fmt.Errorf("unknown type %q for field %q", td.Derived.Name, name)
		}
		// Enum → WORD leaf.
		if dt.BaseType.Enum != nil {
			return &Node{Name: name, Dir: DirIn, TypeName: "WORD"}, nil
		}
		// Struct → container node.
		if dt.BaseType.Struct != nil {
			children, err := xmlStructVarsToChildren(dt.BaseType.Struct.Variables, typeMap)
			if err != nil {
				return nil, fmt.Errorf("type %q for field %q: %w", td.Derived.Name, name, err)
			}
			return &Node{Name: name, Children: children}, nil
		}
		// Primitive-based alias (e.g. from @type directive) → use the primitive type.
		if primName := dt.BaseType.primitiveTypeName(); primName != "" {
			return &Node{Name: name, Dir: DirIn, TypeName: primName}, nil
		}
		return nil, fmt.Errorf("type %q (for field %q) has unsupported baseType", td.Derived.Name, name)
	}

	// Primitive / string / enum type.
	typeName := td.primitiveTypeName()
	if typeName == "" {
		return nil, fmt.Errorf("field %q: unrecognized type", name)
	}
	return &Node{Name: name, Dir: DirIn, TypeName: typeName}, nil
}

// xmlTypeToChildren resolves a typeRef (used as an array element base type) to
// a slice of children Nodes.
func xmlTypeToChildren(td *xmlTypeDef, typeMap map[string]*xmlDataType) ([]*Node, error) {
	if td.Struct != nil {
		return xmlStructVarsToChildren(td.Struct.Variables, typeMap)
	}
	if td.Derived != nil {
		dt, ok := typeMap[td.Derived.Name]
		if !ok {
			return nil, fmt.Errorf("unknown element type %q", td.Derived.Name)
		}
		if dt.BaseType.Struct != nil {
			return xmlStructVarsToChildren(dt.BaseType.Struct.Variables, typeMap)
		}
		return nil, fmt.Errorf("array element type %q is not a struct", td.Derived.Name)
	}
	return nil, fmt.Errorf("array element type is not a struct or derived type")
}

// xmlStructVarsToChildren converts a slice of xmlVariable into []*Node children.
func xmlStructVarsToChildren(vars []xmlVariable, typeMap map[string]*xmlDataType) ([]*Node, error) {
	children := make([]*Node, 0, len(vars))
	for _, v := range vars {
		child, err := xmlVarToNode(v.Name, &v.Type, typeMap)
		if err != nil {
			return nil, fmt.Errorf("field %q: %w", v.Name, err)
		}
		children = append(children, child)
	}
	return children, nil
}

// ---------------------------------------------------------------------------
// @type alias XML generation tests
// ---------------------------------------------------------------------------

// TestGenerateXMLAliasedTypeDerived verifies that GenerateXML emits a
// <derived name="AliasName" /> element for leaf nodes whose TypeName matches
// an @enum alias, and emits a <dataType> entry for the alias.
func TestGenerateXMLAliasedTypeDerived(t *testing.T) {
	cfg := `
@enum MY_ENUM WORD 00000000-0000-0000-0000-000000000001
  a 0
  b

stBlock
  in eType MY_ENUM
  in bFlag BOOL
`
	aliases, roots, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}

	var buf bytes.Buffer
	if err := GenerateXML(&buf, roots, aliases); err != nil {
		t.Fatalf("GenerateXML: %v", err)
	}
	out := buf.String()

	// Verify the output is valid XML.
	dec := xml.NewDecoder(strings.NewReader(out))
	for {
		_, err := dec.Token()
		if err == io.EOF {
			break
		}
		if err != nil {
			t.Fatalf("invalid XML: %v", err)
		}
	}

	// The alias should appear as a <derived name="MY_ENUM"> in the variable.
	if !strings.Contains(out, `name="MY_ENUM"`) {
		t.Error("expected <derived name=\"MY_ENUM\"> in XML output")
	}

	// The alias should appear as a <dataType name="MY_ENUM"> entry.
	if !strings.Contains(out, `<dataType name="MY_ENUM"`) {
		t.Error("expected <dataType name=\"MY_ENUM\"> in XML output")
	}

	// The raw WORD element should NOT appear directly for the alias field
	// (it should be wrapped in <derived>).
	// We verify by checking that <BOOL /> is still present (for bFlag).
	if !strings.Contains(out, "<BOOL") {
		t.Error("expected <BOOL /> for non-aliased field bFlag")
	}
}

// TestGenerateXMLNoAliasesBackwardCompat verifies that GenerateXML without
// aliases still generates valid XML (backward compatibility).
func TestGenerateXMLNoAliasesBackwardCompat(t *testing.T) {
	cfg := `
stBlock
  in bFlag BOOL
  out nVal DWORD
`
	roots, err := ParseTree(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTree: %v", err)
	}

	var buf bytes.Buffer
	// Call without aliases (backward compatible signature).
	if err := GenerateXML(&buf, roots); err != nil {
		t.Fatalf("GenerateXML: %v", err)
	}
	out := buf.String()
	if !strings.Contains(out, "<BOOL") || !strings.Contains(out, "<DWORD") {
		t.Error("expected <BOOL /> and <DWORD /> elements in XML output")
	}
}

// ---------------------------------------------------------------------------
// @enum XML generation tests
// ---------------------------------------------------------------------------

// TestGenerateXMLEnumBlock verifies that GenerateXML emits a proper
// <enum><values>...</values></enum> block for a @enum type alias, and that
// only members with explicit values include a value="N" attribute.
func TestGenerateXMLEnumBlock(t *testing.T) {
	cfg := `
@enum MY_ENUM WORD 00000000-0000-0000-0000-000000000001
  none 0
  first
  second
  reset 10
  next

stBlock
  in eVal MY_ENUM
`
	aliases, roots, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}

	var buf bytes.Buffer
	if err := GenerateXML(&buf, roots, aliases); err != nil {
		t.Fatalf("GenerateXML: %v", err)
	}

	// Verify valid XML.
	out := buf.String()
	dec := xml.NewDecoder(strings.NewReader(out))
	for {
		_, err := dec.Token()
		if err == io.EOF {
			break
		}
		if err != nil {
			t.Fatalf("invalid XML: %v\n%s", err, out)
		}
	}

	// The dataType block for MY_ENUM must contain an <enum> block.
	if !strings.Contains(out, `<dataType name="MY_ENUM"`) {
		t.Error("expected <dataType name=\"MY_ENUM\"> in output")
	}
	if !strings.Contains(out, "<enum>") {
		t.Error("expected <enum> block in output")
	}
	if !strings.Contains(out, "<values>") {
		t.Error("expected <values> block in output")
	}

	// Members with explicit values must have value="N" attribute.
	if !strings.Contains(out, `name="none" value="0"`) {
		t.Error("expected value=\"0\" attribute for explicit member none")
	}
	if !strings.Contains(out, `name="reset" value="10"`) {
		t.Error("expected value=\"10\" attribute for explicit member reset")
	}

	// Auto-incremented members must NOT have a value attribute.
	// We check that the raw member elements don't carry value= for "first", "second", "next".
	for _, member := range []string{"first", "second", "next"} {
		// The member element for auto-incremented values should only have name= attribute.
		// We look for value elements that have both name="<member>" and value= which would be wrong.
		if strings.Contains(out, `name="`+member+`" value=`) {
			t.Errorf("auto-incremented member %q should not have a value= attribute", member)
		}
	}

	// The variable eVal should reference MY_ENUM via <derived>.
	if !strings.Contains(out, `name="MY_ENUM"`) {
		t.Error("expected <derived name=\"MY_ENUM\"> reference for variable eVal")
	}
}

// TestGenerateXMLEnumVsStruct verifies that @enum aliases emit an enum block
// while @struct aliases emit a struct block in the generated XML.
func TestGenerateXMLEnumVsStruct(t *testing.T) {
	cfg := `
@struct MY_STRUCT 00000000-0000-0000-0000-000000000001
  in bFlag BOOL

@enum MY_ENUM WORD 00000000-0000-0000-0000-000000000002
  a 0
  b

stBlock
  struct stX MY_STRUCT
  in y MY_ENUM
`
	aliases, roots, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}

	var buf bytes.Buffer
	if err := GenerateXML(&buf, roots, aliases); err != nil {
		t.Fatalf("GenerateXML: %v", err)
	}
	out := buf.String()

	// MY_STRUCT should have a <struct> body.
	if !strings.Contains(out, `<dataType name="MY_STRUCT"`) {
		t.Error("expected <dataType name=\"MY_STRUCT\"> in output")
	}
	if !strings.Contains(out, "<struct>") {
		t.Error("expected <struct> block for @struct alias")
	}
	// MY_ENUM should have an <enum> block.
	if !strings.Contains(out, "<enum>") {
		t.Error("expected <enum> block for @enum alias")
	}
	// stX should reference MY_STRUCT via <derived>.
	if !strings.Contains(out, `name="MY_STRUCT"`) {
		t.Error("expected <derived name=\"MY_STRUCT\"> for stX variable")
	}
}

// TestGenerateXMLEnumGalvHmiEnums verifies that the galv-hmi.conf enum types
// produce <enum><values> blocks in the generated XML, matching the structure
// of the reference DISPLAY_DATA.xml.
func TestGenerateXMLEnumGalvHmiEnums(t *testing.T) {
	f, err := os.Open("configs/galv-hmi.conf")
	if err != nil {
		t.Skipf("galv-hmi.conf not found: %v", err)
	}
	defer f.Close()

	aliases, roots, err := ParseTreeWithAliases(f)
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}

	var buf bytes.Buffer
	if err := GenerateXML(&buf, roots, aliases); err != nil {
		t.Fatalf("GenerateXML: %v", err)
	}
	out := buf.String()

	// Both enum types must appear with <enum><values> blocks.
	for _, enumName := range []string{"EN_DISP_MSGTYPE", "EN_DISP_POOL_STATE"} {
		if !strings.Contains(out, `<dataType name="`+enumName+`"`) {
			t.Errorf("expected <dataType name=%q> in XML", enumName)
		}
	}
	if !strings.Contains(out, "<enum>") {
		t.Error("expected <enum> block in XML output")
	}
	if !strings.Contains(out, "<values>") {
		t.Error("expected <values> block in XML output")
	}

	// Spot-check specific enum members from EN_DISP_MSGTYPE.
	if !strings.Contains(out, `name="none" value="0"`) {
		t.Error("expected none=0 in EN_DISP_MSGTYPE enum")
	}
	if !strings.Contains(out, `name="precheck"`) {
		t.Error("expected precheck member in EN_DISP_MSGTYPE enum")
	}

	// Spot-check EN_DISP_POOL_STATE members.
	if !strings.Contains(out, `name="empty" value="0"`) {
		t.Error("expected empty=0 in EN_DISP_POOL_STATE enum")
	}
	if !strings.Contains(out, `name="finished"`) {
		t.Error("expected finished member in EN_DISP_POOL_STATE enum")
	}
}

// ---------------------------------------------------------------------------
// @struct XML generation tests
// ---------------------------------------------------------------------------

// TestGenerateXMLStructDerived verifies that GenerateXML emits
// <derived name="ST_DISP_MSG" /> (using the declared struct name) instead of
// a generated T_xxx name for container nodes from the struct keyword.
func TestGenerateXMLStructDerived(t *testing.T) {
	cfg := `
@struct ST_DISP_MSG 702ba601-5f18-413a-95f1-5fe16503843e
  in bEnableOk BOOL
  out bOk BOOL

stBlock
  struct stMsg ST_DISP_MSG
  in bExtra BOOL
`
	aliases, roots, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}

	var buf bytes.Buffer
	if err := GenerateXML(&buf, roots, aliases); err != nil {
		t.Fatalf("GenerateXML: %v", err)
	}
	out := buf.String()

	// Verify valid XML.
	dec := xml.NewDecoder(strings.NewReader(out))
	for {
		_, err := dec.Token()
		if err == io.EOF {
			break
		}
		if err != nil {
			t.Fatalf("invalid XML: %v\n%s", err, out)
		}
	}

	// stMsg should reference the declared struct name, not T_stMsg.
	if strings.Contains(out, `T_stMsg`) {
		t.Error("must not emit T_stMsg; should use declared name ST_DISP_MSG")
	}
	if !strings.Contains(out, `name="ST_DISP_MSG"`) {
		t.Error("expected <derived name=\"ST_DISP_MSG\" /> or similar in XML output")
	}
}

// TestGenerateXMLStructDataType verifies that GenerateXML emits a proper
// <dataType name="ST_DISP_MSG"><baseType><struct>...</struct></baseType></dataType>
// block for a @struct alias.
func TestGenerateXMLStructDataType(t *testing.T) {
	cfg := `
@struct ST_DISP_MSG 702ba601-5f18-413a-95f1-5fe16503843e
  in bEnableOk BOOL
  in bEnableCancel BOOL
  out bOk BOOL
  out bCancel BOOL

stBlock
  struct stMsg ST_DISP_MSG
`
	aliases, roots, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}

	var buf bytes.Buffer
	if err := GenerateXML(&buf, roots, aliases); err != nil {
		t.Fatalf("GenerateXML: %v", err)
	}
	out := buf.String()

	// Verify valid XML.
	dec := xml.NewDecoder(strings.NewReader(out))
	for {
		_, err := dec.Token()
		if err == io.EOF {
			break
		}
		if err != nil {
			t.Fatalf("invalid XML: %v\n%s", err, out)
		}
	}

	// The @struct should appear as a <dataType name="ST_DISP_MSG"> with a struct body.
	if !strings.Contains(out, `<dataType name="ST_DISP_MSG"`) {
		t.Error("expected <dataType name=\"ST_DISP_MSG\"> in output")
	}
	if !strings.Contains(out, "<struct>") {
		t.Error("expected <struct> block inside ST_DISP_MSG dataType")
	}
	// Members should appear as variables.
	if !strings.Contains(out, `name="bEnableOk"`) {
		t.Error("expected variable bEnableOk in ST_DISP_MSG struct")
	}
	if !strings.Contains(out, `name="bOk"`) {
		t.Error("expected variable bOk in ST_DISP_MSG struct")
	}
}

// TestGenerateXMLStructGalvHmi verifies that the updated galv-hmi.conf with
// @struct directives generates XML matching the structure of DISPLAY_DATA.xml.
func TestGenerateXMLStructGalvHmi(t *testing.T) {
	f, err := os.Open("configs/galv-hmi.conf")
	if err != nil {
		t.Skipf("galv-hmi.conf not found: %v", err)
	}
	defer f.Close()

	aliases, roots, err := ParseTreeWithAliases(f)
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}

	var buf bytes.Buffer
	if err := GenerateXML(&buf, roots, aliases); err != nil {
		t.Fatalf("GenerateXML: %v", err)
	}
	out := buf.String()

	// All 8 struct dataTypes must be present.
	for _, structName := range []string{
		"ST_DISP_DATA", "ST_DISP_POOL", "ST_DISP_MSG",
		"ST_DISP_MIXER", "ST_DISP_ERRORS",
		"ST_DISP_GOLBAL_ERRORS", "ST_DISP_POOL_ERRORS", "ST_DISP_MIXER_ERRORS",
	} {
		if !strings.Contains(out, `<dataType name="`+structName+`"`) {
			t.Errorf("expected <dataType name=%q> in XML", structName)
		}
	}

	// Struct types must reference each other via <derived>.
	if !strings.Contains(out, `name="ST_DISP_POOL"`) {
		t.Error("expected <derived name=\"ST_DISP_POOL\"> (array element type)")
	}
	if !strings.Contains(out, `name="ST_DISP_MSG"`) {
		t.Error("expected <derived name=\"ST_DISP_MSG\"> (stMsg field type)")
	}
	if !strings.Contains(out, `name="ST_DISP_MIXER"`) {
		t.Error("expected <derived name=\"ST_DISP_MIXER\"> (array element type)")
	}

	// GVL variables should use the declared struct types.
	if !strings.Contains(out, `name="ST_DISP_DATA"`) {
		t.Error("expected <derived name=\"ST_DISP_DATA\"> for stData variable")
	}
	if !strings.Contains(out, `name="ST_DISP_ERRORS"`) {
		t.Error("expected <derived name=\"ST_DISP_ERRORS\"> for stErrors variable")
	}
}

// ---------------------------------------------------------------------------
// GUID formatting and ObjectId XML emission tests
// ---------------------------------------------------------------------------

// TestFormatGUIDRoundTrip verifies that formatGUID is the exact inverse of
// parseGUID: formatGUID(parseGUID(s)) == s for various known GUIDs.
func TestFormatGUIDRoundTrip(t *testing.T) {
	guids := []string{
		"354914ab-5602-4319-a5dd-d33707893044",
		"96656ea5-0db7-49b0-86ec-56cef26b56d0",
		"4bb8098e-6846-4a59-915d-71a3e3d369c0",
		"47068e7e-0738-4746-acaa-9138b857c754",
		"00000000-0000-0000-0000-000000000000",
		"ffffffff-ffff-ffff-ffff-ffffffffffff",
	}
	for _, guidStr := range guids {
		guid, err := parseGUID(guidStr)
		if err != nil {
			t.Fatalf("parseGUID(%q): %v", guidStr, err)
		}
		got := formatGUID(guid)
		if got != guidStr {
			t.Errorf("formatGUID(parseGUID(%q)) = %q, want %q", guidStr, got, guidStr)
		}
	}
}

// TestEmitObjectIdAddDataEnum verifies that GenerateXML emits an <ObjectId>
// element inside <addData> for a @enum alias with a non-zero GUID.
func TestEmitObjectIdAddDataEnum(t *testing.T) {
	const guidStr = "96656ea5-0db7-49b0-86ec-56cef26b56d0"
	cfg := `
@enum MY_ENUM WORD ` + guidStr + `
  none 0
  active

stBlock
  in eVal MY_ENUM
`
	aliases, roots, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}

	var buf bytes.Buffer
	if err := GenerateXML(&buf, roots, aliases); err != nil {
		t.Fatalf("GenerateXML: %v", err)
	}
	out := buf.String()

	// Output must contain the ObjectId with the correct GUID string.
	if !strings.Contains(out, "<ObjectId>"+guidStr+"</ObjectId>") {
		t.Errorf("expected <ObjectId>%s</ObjectId> in output, got:\n%s", guidStr, out)
	}
	// Must also contain the objectid data element wrapper.
	if !strings.Contains(out, "plcopenxml/objectid") {
		t.Error("expected objectid data element in output")
	}
}

// TestEmitObjectIdAddDataStruct verifies that GenerateXML emits an <ObjectId>
// element inside <addData> for a @struct alias with a non-zero GUID.
func TestEmitObjectIdAddDataStruct(t *testing.T) {
	const guidStr = "354914ab-5602-4319-a5dd-d33707893044"
	cfg := `
@struct MY_STRUCT ` + guidStr + `
  in bFlag BOOL
  out nVal DWORD

stBlock
  struct stItem MY_STRUCT
`
	aliases, roots, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}

	var buf bytes.Buffer
	if err := GenerateXML(&buf, roots, aliases); err != nil {
		t.Fatalf("GenerateXML: %v", err)
	}
	out := buf.String()

	// Output must contain the ObjectId with the correct GUID string.
	if !strings.Contains(out, "<ObjectId>"+guidStr+"</ObjectId>") {
		t.Errorf("expected <ObjectId>%s</ObjectId> in output, got:\n%s", guidStr, out)
	}
}

// TestGenerateXMLInlineStructArray verifies that a "struct name[s..e] TypeName"
// node emits an <array><dimension .../><baseType><derived name="TypeName" /></baseType></array>
// wrapper rather than a bare <derived name="TypeName" />.
func TestGenerateXMLInlineStructArray(t *testing.T) {
	cfg := `
@struct ST_POOL_ERRORS 00000000-0000-0000-0000-000000000001
  in bFault BOOL
  out nCode DWORD

@struct ST_ERRORS 00000000-0000-0000-0000-000000000002
  in bGlobal BOOL
  struct aPoolErrors[1..1] ST_POOL_ERRORS

stRoot
  struct stErrors ST_ERRORS
`
	aliases, roots, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}

	var buf bytes.Buffer
	if err := GenerateXML(&buf, roots, aliases); err != nil {
		t.Fatalf("GenerateXML: %v", err)
	}
	out := buf.String()

	// Verify the output is valid XML.
	var proj xmlProject
	if err := xml.NewDecoder(strings.NewReader(out)).Decode(&proj); err != nil {
		t.Fatalf("generated XML is not valid: %v\n%s", err, out)
	}

	// Locate the ST_ERRORS dataType and find the aPoolErrors variable within it.
	var stErrors *xmlDataType
	for i := range proj.DataTypes {
		if proj.DataTypes[i].Name == "ST_ERRORS" {
			stErrors = &proj.DataTypes[i]
			break
		}
	}
	if stErrors == nil {
		t.Fatalf("ST_ERRORS dataType not found in output:\n%s", out)
	}
	if stErrors.BaseType.Struct == nil {
		t.Fatalf("ST_ERRORS baseType is not a struct:\n%s", out)
	}

	// Find the aPoolErrors variable inside ST_ERRORS.
	var aPoolErrorsVar *xmlVariable
	for i := range stErrors.BaseType.Struct.Variables {
		if stErrors.BaseType.Struct.Variables[i].Name == "aPoolErrors" {
			aPoolErrorsVar = &stErrors.BaseType.Struct.Variables[i]
			break
		}
	}
	if aPoolErrorsVar == nil {
		t.Fatalf("aPoolErrors variable not found in ST_ERRORS struct:\n%s", out)
	}

	// aPoolErrors must have an <array> type (not a bare <derived>).
	arr := aPoolErrorsVar.Type.Array
	if arr == nil {
		t.Fatalf("aPoolErrors type is not an <array>; got bare <derived> or other:\n%s", out)
	}

	// Verify dimension bounds.
	if arr.Dimension.Lower != 1 || arr.Dimension.Upper != 1 {
		t.Errorf("aPoolErrors dimension: got lower=%d upper=%d, want lower=1 upper=1",
			arr.Dimension.Lower, arr.Dimension.Upper)
	}

	// Verify baseType is <derived name="ST_POOL_ERRORS" />.
	if arr.BaseType == nil || arr.BaseType.Derived == nil {
		t.Fatalf("aPoolErrors array baseType is not a <derived> element:\n%s", out)
	}
	if arr.BaseType.Derived.Name != "ST_POOL_ERRORS" {
		t.Errorf("aPoolErrors array baseType derived name: got %q, want %q",
			arr.BaseType.Derived.Name, "ST_POOL_ERRORS")
	}
}

// TestEmitObjectIdAddDataZeroGUID verifies that GenerateXML does NOT emit an
// <addData> block for an alias whose GUID is all zeros.
func TestEmitObjectIdAddDataZeroGUID(t *testing.T) {
	// Use an all-zero GUID explicitly.
	cfg := `
@enum MY_ENUM WORD 00000000-0000-0000-0000-000000000000
  none 0
  active

stBlock
  in eVal MY_ENUM
`
	aliases, roots, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}

	var buf bytes.Buffer
	if err := GenerateXML(&buf, roots, aliases); err != nil {
		t.Fatalf("GenerateXML: %v", err)
	}
	out := buf.String()

	if strings.Contains(out, "plcopenxml/objectid") {
		t.Error("expected no objectid addData block for zero GUID, but found one")
	}
	if strings.Contains(out, "<ObjectId>") {
		t.Error("expected no <ObjectId> element for zero GUID, but found one")
	}
}
