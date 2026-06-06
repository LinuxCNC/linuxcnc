package comp

import (
	"fmt"
	"strconv"
	"strings"

	"github.com/sittner/linuxcnc/src/gomc/internal/modcompile/ast"
)

// Parse parses a .comp file and returns a Package.
func Parse(filename, src string) (*ast.Package, error) {
	header, userCode := splitComp(src)

	sc := NewScanner(filename, header)
	p := &parser{
		sc:   sc,
		file: filename,
		pkg: &ast.Package{
			Component: ast.Component{
				Options: make(map[string]string),
			},
		},
	}
	p.next() // prime the lookahead

	if err := p.parseFile(); err != nil {
		return nil, err
	}

	p.pkg.Component.VerbatimC = userCode

	// Reject unsupported RTAPI_MP_ARRAY_* macros.
	// cmod/gomod allows multiple 'load' commands with different parameters instead.
	for _, macro := range []string{"RTAPI_MP_ARRAY_STRING", "RTAPI_MP_ARRAY_INT"} {
		if strings.Contains(userCode, macro) {
			return nil, fmt.Errorf("%s: '%s' is not supported in cmod/gomod; "+
				"use multiple 'load' commands with different parameters instead", filename, macro)
		}
	}

	return p.pkg, nil
}

// splitComp splits the source at the first "\n;;\n" separator into
// a header part (declarations) and a user-code part (verbatim C).
// If no separator is found, the entire source is the header.
func splitComp(src string) (header, userCode string) {
	const sep = "\n;;\n"
	idx := strings.Index(src, sep)
	if idx < 0 {
		// Try ";;\n" at the very start of the file.
		if strings.HasPrefix(src, ";;\n") {
			return "", src[3:]
		}
		// Try "\n;;" at EOF.
		if strings.HasSuffix(src, "\n;;") {
			return src[:len(src)-3], ""
		}
		// No separator — whole file is header.
		return src, ""
	}
	return src[:idx], src[idx+len(sep):]
}

// ---------------------------------------------------------------------------
// parser
// ---------------------------------------------------------------------------

type parser struct {
	sc   *Scanner
	file string
	cur  Token
	pkg  *ast.Package
}

func (p *parser) next() Token {
	prev := p.cur
	p.cur = p.sc.Next()
	return prev
}

func (p *parser) errorf(format string, args ...interface{}) error {
	msg := fmt.Sprintf(format, args...)
	return fmt.Errorf("%s: %s", p.cur.Pos, msg)
}

// expect consumes the current token if it matches kind, otherwise returns error.
func (p *parser) expect(kind TokenKind) (Token, error) {
	if p.cur.Kind != kind {
		return Token{}, p.errorf("expected %s, got %s (%q)", kind, p.cur.Kind, p.cur.Val)
	}
	return p.next(), nil
}

// expectSemi consumes a semicolon.
func (p *parser) expectSemi() error {
	_, err := p.expect(TokSemi)
	return err
}

// ---------------------------------------------------------------------------
// Top-level parsing
// ---------------------------------------------------------------------------

func (p *parser) parseFile() error {
	if err := p.parseComponentDecl(); err != nil {
		return err
	}
	for p.cur.Kind != TokEOF {
		if err := p.parseDeclaration(); err != nil {
			return err
		}
	}
	return nil
}

func (p *parser) parseComponentDecl() error {
	if p.cur.Kind != TokIdent || p.cur.Val != "component" {
		return p.errorf("expected 'component', got %q", p.cur.Val)
	}
	p.pkg.Component.Pos = p.cur.Pos
	p.next() // skip "component"

	name, err := p.expectName()
	if err != nil {
		return err
	}
	p.pkg.Component.Name = name

	p.pkg.Component.Summary = p.parseOptString()

	return p.expectSemi()
}

func (p *parser) parseDeclaration() error {
	if p.cur.Kind != TokIdent {
		return p.errorf("expected declaration keyword, got %s (%q)", p.cur.Kind, p.cur.Val)
	}
	switch p.cur.Val {
	case "pin":
		return p.parsePin()
	case "param":
		return p.parseParam()
	case "function":
		return p.parseFunction()
	case "variable":
		return p.parseVariable()
	case "option":
		return p.parseOption()
	case "modparam":
		return p.parseModparam()
	case "include":
		return p.parseInclude()
	case "description":
		return p.parseDocField(&p.pkg.Component.Description)
	case "license":
		return p.parseDocField(&p.pkg.Component.License)
	case "author":
		return p.parseDocField(&p.pkg.Component.Author)
	case "see_also":
		return p.parseDocField(&p.pkg.Component.SeeAlso)
	case "notes":
		return p.parseDocField(&p.pkg.Component.Notes)
	case "examples":
		return p.parseDocField(&p.pkg.Component.Examples)
	case "gmi_provide":
		return p.parseGMIProvide()
	case "gmi_consume":
		return p.parseGMIConsume()
	default:
		return p.errorf("unknown declaration keyword %q", p.cur.Val)
	}
}

// ---------------------------------------------------------------------------
// Pin: "pin" PINDIRECTION TYPE HALNAME OptArray OptSAssign OptPersonality OptString ";"
// ---------------------------------------------------------------------------

func (p *parser) parsePin() error {
	pos := p.cur.Pos
	p.next() // skip "pin"

	dir, err := p.parsePinDir()
	if err != nil {
		return err
	}
	typ, err := p.parseHALType()
	if err != nil {
		return err
	}
	name, err := p.expectHALName()
	if err != nil {
		return err
	}

	arrSize, arrPers := p.parseOptArray()
	def := p.parseOptSAssign()
	pers := p.parseOptPersonality()
	doc := p.parseOptString()

	if err := p.expectSemi(); err != nil {
		return err
	}

	p.pkg.Component.Pins = append(p.pkg.Component.Pins, ast.Pin{
		Pos:              pos,
		Name:             name,
		Type:             typ,
		Dir:              dir,
		ArraySize:        arrSize,
		ArrayPersonality: arrPers,
		Default:          def,
		Personality:      pers,
		Doc:              doc,
	})
	return nil
}

// ---------------------------------------------------------------------------
// Param: "param" PARAMDIRECTION TYPE HALNAME OptArray OptSAssign OptPersonality OptString ";"
// ---------------------------------------------------------------------------

func (p *parser) parseParam() error {
	pos := p.cur.Pos
	p.next() // skip "param"

	dir, err := p.parseParamDir()
	if err != nil {
		return err
	}
	typ, err := p.parseHALType()
	if err != nil {
		return err
	}
	name, err := p.expectHALName()
	if err != nil {
		return err
	}

	arrSize, arrPers := p.parseOptArray()
	def := p.parseOptSAssign()
	pers := p.parseOptPersonality()
	doc := p.parseOptString()

	if err := p.expectSemi(); err != nil {
		return err
	}

	p.pkg.Component.Params = append(p.pkg.Component.Params, ast.Param{
		Pos:              pos,
		Name:             name,
		Type:             typ,
		Dir:              dir,
		ArraySize:        arrSize,
		ArrayPersonality: arrPers,
		Default:          def,
		Personality:      pers,
		Doc:              doc,
	})
	return nil
}

// ---------------------------------------------------------------------------
// Function: "function" NAME OptFP OptString ";"
// ---------------------------------------------------------------------------

func (p *parser) parseFunction() error {
	pos := p.cur.Pos
	p.next() // skip "function"

	name, err := p.expectName()
	if err != nil {
		return err
	}

	fp := true // default: uses floating point
	if p.cur.Kind == TokIdent {
		switch p.cur.Val {
		case "fp":
			fp = true
			p.next()
		case "nofp":
			fp = false
			p.next()
		}
	}

	doc := p.parseOptString()

	if err := p.expectSemi(); err != nil {
		return err
	}

	p.pkg.Component.Functions = append(p.pkg.Component.Functions, ast.Function{
		Pos:  pos,
		Name: name,
		FP:   fp,
		Doc:  doc,
	})
	return nil
}

// ---------------------------------------------------------------------------
// Variable: "variable" NAME STARREDNAME OptSimpleArray OptAssign ";"
// ---------------------------------------------------------------------------

func (p *parser) parseVariable() error {
	pos := p.cur.Pos
	p.next() // skip "variable"

	// Type name (a single NAME).
	typeName, err := p.expectName()
	if err != nil {
		return err
	}

	// Variable name, possibly with leading * for pointers.
	varName := ""
	for p.cur.Kind == TokStar {
		varName += "*"
		p.next()
	}
	n, err := p.expectName()
	if err != nil {
		return err
	}
	varName += n

	// Optional array size: [ NUMBER ]
	arrSize := 0
	if p.cur.Kind == TokLBrack {
		p.next()
		sz, err := p.expectNumber()
		if err != nil {
			return err
		}
		arrSize = sz
		if _, err := p.expect(TokRBrack); err != nil {
			return err
		}
	}

	// Optional default value: = Value
	def := p.parseOptAssign()

	if err := p.expectSemi(); err != nil {
		return err
	}

	p.pkg.Component.Variables = append(p.pkg.Component.Variables, ast.Variable{
		Pos:     pos,
		CType:   typeName,
		Name:    varName,
		Array:   arrSize,
		Default: def,
	})
	return nil
}

// ---------------------------------------------------------------------------
// Option: "option" NAME OptValue ";"
// ---------------------------------------------------------------------------

func (p *parser) parseOption() error {
	p.next() // skip "option"

	name, err := p.expectName()
	if err != nil {
		return err
	}

	// Reject unsupported legacy options.
	// cmod/gomod is always multi-instance; use multiple 'load' commands instead.
	if name == "singleton" {
		return fmt.Errorf("%s: 'option singleton' is not supported in cmod/gomod; "+
			"use multiple 'load' commands instead (each creates a separate instance)", p.file)
	}

	val := p.parseOptValue()

	if err := p.expectSemi(); err != nil {
		return err
	}

	p.pkg.Component.Options[name] = val
	return nil
}

// ---------------------------------------------------------------------------
// Modparam: "modparam" NAME NAME OptSAssign OptString ";"
// ---------------------------------------------------------------------------

func (p *parser) parseModparam() error {
	pos := p.cur.Pos
	p.next() // skip "modparam"

	typeName, err := p.expectName()
	if err != nil {
		return err
	}
	paramName, err := p.expectName()
	if err != nil {
		return err
	}

	def := p.parseOptSAssign()
	doc := p.parseOptString()

	if err := p.expectSemi(); err != nil {
		return err
	}

	p.pkg.Component.Modparams = append(p.pkg.Component.Modparams, ast.Modparam{
		Pos:     pos,
		Type:    typeName,
		Name:    paramName,
		Default: def,
		Doc:     doc,
	})
	return nil
}

// ---------------------------------------------------------------------------
// Include: "include" Header ";"
// ---------------------------------------------------------------------------

func (p *parser) parseInclude() error {
	p.next() // skip "include"

	var header string
	if p.cur.Kind == TokString || p.cur.Kind == TokTString {
		header = "\"" + p.cur.Val + "\""
		p.next()
	} else {
		// Angle-bracket header: the scanner hasn't consumed '<' yet
		// because Next() would return TokLT.  Use the special method.
		if p.cur.Kind == TokLT {
			// Back up: ScanAngleHeader needs the '<' not yet consumed.
			// Since we already consumed it via Next(), we reconstruct.
			// Actually — we need to handle this differently.
			// The '<' was already consumed by Next() into p.cur.
			// Scan remaining content until '>'.
			var b strings.Builder
			b.WriteString("<")
			for p.cur.Kind != TokGT && p.cur.Kind != TokSemi && p.cur.Kind != TokEOF {
				p.next()
				if p.cur.Kind == TokGT {
					break
				}
				b.WriteString(p.cur.Val)
			}
			b.WriteString(">")
			header = b.String()
			if p.cur.Kind == TokGT {
				p.next() // consume '>'
			}
		} else {
			return p.errorf("expected string or <header> in include, got %s", p.cur.Kind)
		}
	}

	if err := p.expectSemi(); err != nil {
		return err
	}

	p.pkg.Component.Includes = append(p.pkg.Component.Includes, header)
	return nil
}

// ---------------------------------------------------------------------------
// GMI API bindings: "gmi_provide" NAME ";" / "gmi_consume" NAME ";"
// ---------------------------------------------------------------------------

func (p *parser) parseGMIProvide() error {
	p.next() // skip "gmi_provide"

	name, err := p.expectName()
	if err != nil {
		return err
	}
	if err := p.expectSemi(); err != nil {
		return err
	}

	p.pkg.Component.GMIProvide = append(p.pkg.Component.GMIProvide, name)
	return nil
}

func (p *parser) parseGMIConsume() error {
	p.next() // skip "gmi_consume"

	name, err := p.expectName()
	if err != nil {
		return err
	}

	// Optional "from <module>" clause for default provider instance.
	var from string
	if p.cur.Kind == TokIdent && p.cur.Val == "from" {
		p.next() // skip "from"
		from, err = p.expectName()
		if err != nil {
			return err
		}
	}

	if err := p.expectSemi(); err != nil {
		return err
	}

	p.pkg.Component.GMIConsume = append(p.pkg.Component.GMIConsume, ast.GMIConsumeEntry{
		API:  name,
		From: from,
	})
	return nil
}

// ---------------------------------------------------------------------------
// Doc fields: "description" String ";" etc.
// ---------------------------------------------------------------------------

func (p *parser) parseDocField(target *string) error {
	p.next() // skip keyword

	s, err := p.expectString()
	if err != nil {
		return err
	}
	*target = s

	return p.expectSemi()
}

// ---------------------------------------------------------------------------
// Sub-parsers for types, directions, values
// ---------------------------------------------------------------------------

func (p *parser) parsePinDir() (ast.PinDir, error) {
	if p.cur.Kind != TokIdent {
		return 0, p.errorf("expected pin direction (in/out/io), got %s", p.cur.Kind)
	}
	switch p.cur.Val {
	case "in":
		p.next()
		return ast.PinIn, nil
	case "out":
		p.next()
		return ast.PinOut, nil
	case "io":
		p.next()
		return ast.PinIO, nil
	default:
		return 0, p.errorf("expected pin direction (in/out/io), got %q", p.cur.Val)
	}
}

func (p *parser) parseParamDir() (ast.ParamDir, error) {
	if p.cur.Kind != TokIdent {
		return 0, p.errorf("expected param direction (r/rw), got %s", p.cur.Kind)
	}
	switch p.cur.Val {
	case "r":
		p.next()
		return ast.ParamR, nil
	case "rw":
		p.next()
		return ast.ParamRW, nil
	default:
		return 0, p.errorf("expected param direction (r/rw), got %q", p.cur.Val)
	}
}

func (p *parser) parseHALType() (ast.HALType, error) {
	if p.cur.Kind != TokIdent {
		return 0, p.errorf("expected HAL type, got %s", p.cur.Kind)
	}
	var t ast.HALType
	switch p.cur.Val {
	case "bit":
		t = ast.HALBit
	case "float":
		t = ast.HALFloat
	case "s32", "signed":
		t = ast.HALS32
	case "u32", "unsigned":
		t = ast.HALU32
	case "port":
		t = ast.HALPort
	default:
		return 0, p.errorf("expected HAL type (bit/float/s32/u32/port), got %q", p.cur.Val)
	}
	p.next()
	return t, nil
}

// expectName expects a plain C identifier (NAME).
func (p *parser) expectName() (string, error) {
	if p.cur.Kind != TokIdent || !IsName(p.cur.Val) {
		return "", p.errorf("expected identifier, got %s (%q)", p.cur.Kind, p.cur.Val)
	}
	val := p.cur.Val
	p.next()
	return val, nil
}

// expectHALName expects a HAL-style name (HALNAME).
func (p *parser) expectHALName() (string, error) {
	if p.cur.Kind != TokIdent {
		return "", p.errorf("expected HAL name, got %s (%q)", p.cur.Kind, p.cur.Val)
	}
	val := p.cur.Val
	p.next()
	return val, nil
}

// expectNumber expects an integer number literal and returns its value.
func (p *parser) expectNumber() (int, error) {
	if p.cur.Kind != TokNumber {
		return 0, p.errorf("expected number, got %s (%q)", p.cur.Kind, p.cur.Val)
	}
	val := p.cur.Val
	p.next()
	n, err := strconv.ParseInt(val, 0, 64)
	if err != nil {
		return 0, fmt.Errorf("%s: invalid number %q: %w", p.cur.Pos, val, err)
	}
	return int(n), nil
}

// expectString expects a string or triple-quoted string.
func (p *parser) expectString() (string, error) {
	if p.cur.Kind != TokString && p.cur.Kind != TokTString {
		return "", p.errorf("expected string, got %s (%q)", p.cur.Kind, p.cur.Val)
	}
	val := p.cur.Val
	p.next()
	return val, nil
}

// ---------------------------------------------------------------------------
// Optional components
// ---------------------------------------------------------------------------

// parseOptString returns the string value of the current token if it's a
// string literal, or "" otherwise.
func (p *parser) parseOptString() string {
	if p.cur.Kind == TokString || p.cur.Kind == TokTString {
		val := p.cur.Val
		p.next()
		return val
	}
	return ""
}

// parseOptArray parses OptArray: "[" NUMBER (":" PersonalityExpr)? "]"
// Returns (size, personalityExpr).
func (p *parser) parseOptArray() (int, string) {
	if p.cur.Kind != TokLBrack {
		return 0, ""
	}
	p.next() // skip [

	// Must be a number.
	if p.cur.Kind != TokNumber {
		return 0, ""
	}
	size, _ := strconv.ParseInt(p.cur.Val, 0, 64)
	p.next()

	pers := ""
	if p.cur.Kind == TokColon {
		p.next() // skip :
		pers = p.collectPersonalityExpr(TokRBrack)
	}

	if p.cur.Kind == TokRBrack {
		p.next() // skip ]
	}

	return int(size), pers
}

// parseOptSAssign parses OptSAssign: "=" SValue | empty.
// Returns the value as a string, or "".
func (p *parser) parseOptSAssign() string {
	if p.cur.Kind != TokEq {
		return ""
	}
	p.next() // skip =
	return p.parseSValue()
}

// parseOptAssign parses OptAssign: "=" Value | empty (for variables).
func (p *parser) parseOptAssign() string {
	if p.cur.Kind != TokEq {
		return ""
	}
	p.next() // skip =
	return p.parseValue()
}

// parseOptPersonality parses OptPersonality: "if" PersonalityExpr | empty.
func (p *parser) parseOptPersonality() string {
	if p.cur.Kind != TokIdent || p.cur.Val != "if" {
		return ""
	}
	p.next() // skip "if"
	return p.collectPersonalityExpr(TokString, TokTString, TokSemi)
}

// parseOptValue parses OptValue: Value | String | TString | empty (→ "1").
func (p *parser) parseOptValue() string {
	switch p.cur.Kind {
	case TokString, TokTString:
		val := p.cur.Val
		p.next()
		return val
	case TokNumber, TokFPNum:
		val := p.cur.Val
		p.next()
		return val
	case TokIdent:
		val := p.cur.Val
		p.next()
		return val
	default:
		return "1"
	}
}

// parseSValue parses a single value token (for pin/param defaults).
// Handles optional leading sign for numbers.
func (p *parser) parseSValue() string {
	// Possible leading sign.
	sign := ""
	if p.cur.Kind == TokMinus || p.cur.Kind == TokPlus {
		sign = p.cur.Val
		p.next()
	}
	switch p.cur.Kind {
	case TokNumber, TokFPNum:
		val := sign + p.cur.Val
		p.next()
		return val
	case TokString:
		val := p.cur.Val
		p.next()
		return val
	case TokIdent:
		val := p.cur.Val
		p.next()
		return val
	default:
		if sign != "" {
			return sign // shouldn't happen, but be safe
		}
		return ""
	}
}

// parseValue parses a Value (for variable defaults).
func (p *parser) parseValue() string {
	return p.parseSValue()
}

// collectPersonalityExpr collects a personality expression as a raw string,
// stopping when it encounters any of the given terminator token kinds.
func (p *parser) collectPersonalityExpr(terminators ...TokenKind) string {
	var parts []string
	for {
		for _, t := range terminators {
			if p.cur.Kind == t {
				return strings.Join(parts, " ")
			}
		}
		if p.cur.Kind == TokEOF {
			return strings.Join(parts, " ")
		}
		parts = append(parts, p.cur.Val)
		p.next()
	}
}
