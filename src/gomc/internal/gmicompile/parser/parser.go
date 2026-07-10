// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package parser

import (
	"fmt"
	"strconv"

	"github.com/sittner/linuxcnc/src/gomc/internal/gmicompile/ast"
)

// Parser parses GMI source into an AST.
type Parser struct {
	scanner   *Scanner
	cur       Token
	file      string
	errors    []string
	consts    map[string]int  // named constants for array size resolution
	callbacks map[string]bool // declared callback names for type resolution
	imports   map[string]bool // imported API names for type resolution
}

// Parse parses a GMI file and returns the AST and any errors.
func Parse(filename, src string) (*ast.API, []string) {
	p := &Parser{
		scanner:   NewScanner(src),
		file:      filename,
		consts:    make(map[string]int),
		callbacks: make(map[string]bool),
		imports:   make(map[string]bool),
	}
	p.advance()
	api := p.parseAPI()
	return api, p.errors
}

func (p *Parser) advance() {
	p.cur = p.scanner.Scan()
}

func (p *Parser) pos() ast.Pos {
	return ast.Pos{File: p.file, Line: p.cur.Line, Col: p.cur.Col}
}

func (p *Parser) errorf(format string, args ...interface{}) {
	msg := fmt.Sprintf("%s:%d:%d: %s", p.file, p.cur.Line, p.cur.Col, fmt.Sprintf(format, args...))
	p.errors = append(p.errors, msg)
}

func (p *Parser) expect(t TokenType) bool {
	if p.cur.Type != t {
		p.errorf("expected %v, got %q", t, p.cur.Text)
		return false
	}
	p.advance()
	return true
}

func (p *Parser) parseAPI() *ast.API {
	api := &ast.API{}

	// Pending function-level annotations collected before a func declaration
	var pendingAnns []annotation

	for p.cur.Type != EOF {
		switch {
		case p.cur.Type == AT:
			ann := p.parseAnnotation()
			if isAPIDirective(ann.name) {
				p.applyAPIDirective(api, ann)
			} else {
				pendingAnns = append(pendingAnns, ann)
			}
		case p.cur.Type == ENUM:
			if len(pendingAnns) > 0 {
				p.errorf("annotations before enum are not supported")
				pendingAnns = nil
			}
			api.Enums = append(api.Enums, p.parseEnum())
		case p.cur.Type == CONST:
			if len(pendingAnns) > 0 {
				p.errorf("annotations before const are not supported")
				pendingAnns = nil
			}
			api.Consts = append(api.Consts, p.parseConst())
		case p.cur.Type == TYPE:
			if len(pendingAnns) > 0 {
				p.errorf("annotations before type are not supported")
				pendingAnns = nil
			}
			api.Types = append(api.Types, p.parseType())
		case p.cur.Type == CALLBACK:
			if len(pendingAnns) > 0 {
				p.errorf("annotations before callback are not supported")
				pendingAnns = nil
			}
			api.Callbacks = append(api.Callbacks, p.parseCallback())
		case p.cur.Type == STREAM_SERVER:
			if len(pendingAnns) > 0 {
				p.errorf("annotations before stream_server are not supported")
				pendingAnns = nil
			}
			api.StreamServers = append(api.StreamServers, p.parseStreamServer())
		case p.cur.Type == FUNC:
			fn := p.parseFunc(pendingAnns)
			pendingAnns = nil
			api.Funcs = append(api.Funcs, fn)
		default:
			p.errorf("unexpected token %q", p.cur.Text)
			p.advance()
		}
	}

	if len(pendingAnns) > 0 {
		p.errorf("trailing annotations without func declaration")
	}

	return api
}

// annotation is a parsed @name value pair.
type annotation struct {
	name  string
	value string
	pos   ast.Pos
}

// isAPIDirective returns true for top-level API directives.
func isAPIDirective(name string) bool {
	switch name {
	case "api", "version", "prefix", "rest_export", "import", "author", "license":
		return true
	}
	return false
}

// parseAnnotation parses @ name value and returns the pair.
func (p *Parser) parseAnnotation() annotation {
	p.advance() // skip @
	pos := p.pos()
	name := p.cur.Text
	nameLine := p.cur.Line
	p.advance()
	// Collect value tokens on the same line as the annotation name.
	// This handles compound values like "100ms" (tokenized as "100" + "ms").
	var parts []string
	for p.cur.Type != EOF && p.cur.Line == nameLine &&
		p.cur.Type != AT && p.cur.Type != FUNC &&
		p.cur.Type != TYPE && p.cur.Type != ENUM && p.cur.Type != CONST {
		parts = append(parts, p.cur.Text)
		p.advance()
	}
	value := ""
	for _, part := range parts {
		value += part
	}
	return annotation{name: name, value: value, pos: pos}
}

func (p *Parser) parseConst() ast.Const {
	pos := p.pos()
	p.advance() // skip "const"
	name := p.cur.Text
	p.advance()
	p.expect(EQ)
	val, err := strconv.Atoi(p.cur.Text)
	if err != nil {
		p.errorf("const value must be integer, got %q", p.cur.Text)
	}
	p.advance()
	p.consts[name] = val
	return ast.Const{Name: name, Value: val, Pos: pos}
}

func (p *Parser) applyAPIDirective(api *ast.API, ann annotation) {
	switch ann.name {
	case "api":
		api.Name = ann.value
		api.Pos = ann.pos
	case "version":
		if v, err := strconv.Atoi(ann.value); err == nil {
			api.Version = v
		}
	case "prefix":
		api.Prefix = ann.value
	case "rest_export":
		api.RestExport = ann.value == "true"
	case "import":
		p.imports[ann.value] = true
		api.Imports = append(api.Imports, ast.Import{Name: ann.value, Pos: ann.pos})
	case "author":
		api.Authors = append(api.Authors, ann.value)
	case "license":
		api.License = ann.value
	}
}

func (p *Parser) parseEnum() ast.Enum {
	pos := p.pos()
	p.advance() // skip "enum"
	name := p.cur.Text
	p.advance()

	enum := ast.Enum{Name: name, Pos: pos}
	p.expect(LBRACE)

	for p.cur.Type != RBRACE && p.cur.Type != EOF {
		vpos := p.pos()
		vname := p.cur.Text
		p.advance()
		p.expect(EQ)
		val, _ := strconv.Atoi(p.cur.Text)
		p.advance()
		enum.Values = append(enum.Values, ast.EnumValue{Name: vname, Value: val, Pos: vpos})
	}
	p.expect(RBRACE)
	return enum
}

func (p *Parser) parseType() ast.Type {
	pos := p.pos()
	p.advance() // skip "type"
	name := p.cur.Text
	p.advance()

	typ := ast.Type{Name: name, Pos: pos}
	p.expect(LBRACE)

	for p.cur.Type != RBRACE && p.cur.Type != EOF {
		fpos := p.pos()
		fname := p.cur.Text
		p.advance()
		p.expect(COLON)
		ftype := p.parseTypeRef()
		constraints := p.parseConstraints()
		typ.Fields = append(typ.Fields, ast.Field{Name: fname, Type: ftype, Constraints: constraints, Pos: fpos})
	}
	p.expect(RBRACE)
	return typ
}

// parseConstraints reads zero or more inline @constraints that follow a field
// or parameter type, e.g.  toolno: i32 @min(1) @max(99999)
func (p *Parser) parseConstraints() []ast.Constraint {
	var cs []ast.Constraint
	for p.cur.Type == AT {
		cpos := p.pos()
		p.advance() // skip @
		name := p.cur.Text
		p.advance()
		c := ast.Constraint{Pos: cpos}
		switch name {
		case "min":
			c.Kind, c.Num = ast.ConstraintMin, p.constraintNum()
		case "max":
			c.Kind, c.Num = ast.ConstraintMax, p.constraintNum()
		case "minlen":
			c.Kind, c.Num = ast.ConstraintMinLen, p.constraintNum()
		case "maxlen":
			c.Kind, c.Num = ast.ConstraintMaxLen, p.constraintNum()
		case "notempty":
			c.Kind = ast.ConstraintNotEmpty
		case "notnull":
			c.Kind = ast.ConstraintNotNull
		case "regex":
			c.Kind, c.Str = ast.ConstraintRegex, p.constraintStr()
		case "enum_open":
			c.Kind = ast.ConstraintEnumOpen
		default:
			p.errorf("unknown constraint @%s", name)
			continue
		}
		cs = append(cs, c)
	}
	return cs
}

// constraintNum parses a parenthesized numeric argument: (INT|FLOAT).
func (p *Parser) constraintNum() string {
	p.expect(LPAREN)
	if p.cur.Type != INT && p.cur.Type != FLOAT {
		p.errorf("constraint argument must be numeric, got %q", p.cur.Text)
	}
	v := p.cur.Text
	p.advance()
	p.expect(RPAREN)
	return v
}

// constraintStr parses a parenthesized string-literal argument (for @regex).
func (p *Parser) constraintStr() string {
	p.expect(LPAREN)
	if p.cur.Type != STRING {
		p.errorf("constraint argument must be a string literal, got %q", p.cur.Text)
	}
	v := p.cur.Text
	p.advance()
	p.expect(RPAREN)
	return v
}

func (p *Parser) parseCallback() ast.Callback {
	pos := p.pos()
	p.advance() // skip "callback"
	name := p.cur.Text
	p.advance()

	cb := ast.Callback{Name: name, Pos: pos}
	p.callbacks[name] = true

	// Parameters
	p.expect(LPAREN)
	for p.cur.Type != RPAREN && p.cur.Type != EOF {
		ppos := p.pos()
		pname := p.cur.Text
		p.advance()
		p.expect(COLON)
		ptype := p.parseTypeRef()
		byref := false
		isPtr := false
		isOut := false
		if p.cur.Type == IDENT && p.cur.Text == "byref" {
			byref = true
			p.advance()
		} else if p.cur.Type == IDENT && p.cur.Text == "out" {
			isOut = true
			p.advance()
		} else if p.cur.Type == IDENT && p.cur.Text == "ptr" {
			isPtr = true
			p.advance()
		}
		cb.Params = append(cb.Params, ast.Param{Name: pname, Type: ptype, ByRef: byref, IsOut: isOut, IsPtr: isPtr, Pos: ppos})
		if p.cur.Type == COMMA {
			p.advance()
		}
	}
	p.expect(RPAREN)

	// Return type
	if p.cur.Type == ARROW {
		p.advance()
		ret := p.parseTypeRef()
		cb.Return = &ret
	}

	return cb
}

func (p *Parser) parseStreamServer() ast.StreamServer {
	pos := p.pos()
	p.advance() // skip "stream_server"
	name := p.cur.Text
	p.advance()

	ss := ast.StreamServer{Name: name, Pos: pos}
	p.expect(LBRACE)

	for p.cur.Type != RBRACE && p.cur.Type != EOF {
		sf := p.parseStreamFunc()
		ss.Funcs = append(ss.Funcs, sf)
	}
	p.expect(RBRACE)
	return ss
}

func (p *Parser) parseStreamFunc() ast.StreamFunc {
	pos := p.pos()
	name := p.cur.Text
	p.advance()

	sf := ast.StreamFunc{Name: name, Pos: pos}

	p.expect(LPAREN)
	for p.cur.Type != RPAREN && p.cur.Type != EOF {
		ppos := p.pos()
		pname := p.cur.Text
		p.advance()
		p.expect(COLON)
		ptype := p.parseTypeRef()
		byref := false
		isPtr := false
		isOut := false
		if p.cur.Type == IDENT && p.cur.Text == "byref" {
			byref = true
			p.advance()
		} else if p.cur.Type == IDENT && p.cur.Text == "out" {
			isOut = true
			p.advance()
		} else if p.cur.Type == IDENT && p.cur.Text == "ptr" {
			isPtr = true
			p.advance()
		}
		sf.Params = append(sf.Params, ast.Param{Name: pname, Type: ptype, ByRef: byref, IsOut: isOut, IsPtr: isPtr, Pos: ppos})
		if p.cur.Type == COMMA {
			p.advance()
		}
	}
	p.expect(RPAREN)

	if p.cur.Type == ARROW {
		p.advance()
		ret := p.parseTypeRef()
		sf.Return = &ret
	}

	return sf
}

func (p *Parser) parseFunc(anns []annotation) ast.Func {
	pos := p.pos()
	p.advance() // skip "func"
	name := p.cur.Text
	p.advance()

	fn := ast.Func{Name: name, Pos: pos}

	// Parameters
	p.expect(LPAREN)
	for p.cur.Type != RPAREN && p.cur.Type != EOF {
		ppos := p.pos()
		pname := p.cur.Text
		p.advance()
		p.expect(COLON)
		ptype := p.parseTypeRef()
		byref := false
		isPtr := false
		isOut := false
		if p.cur.Type == IDENT && p.cur.Text == "byref" {
			byref = true
			p.advance()
		} else if p.cur.Type == IDENT && p.cur.Text == "out" {
			isOut = true
			p.advance()
		} else if p.cur.Type == IDENT && p.cur.Text == "ptr" {
			isPtr = true
			p.advance()
		}
		constraints := p.parseConstraints()
		fn.Params = append(fn.Params, ast.Param{Name: pname, Type: ptype, ByRef: byref, IsOut: isOut, IsPtr: isPtr, Constraints: constraints, Pos: ppos})
		if p.cur.Type == COMMA {
			p.advance()
		}
	}
	p.expect(RPAREN)

	// Return type
	if p.cur.Type == ARROW {
		p.advance()
		ret := p.parseTypeRef()
		fn.Return = &ret
	}

	// Apply preceding annotations
	for _, ann := range anns {
		switch ann.name {
		case "method":
			fn.Method = ann.value
		case "path":
			fn.Path = ann.value
		case "rt_safe":
			fn.RTSafe = ann.value == "true"
		case "doc":
			fn.Doc = ann.value
		case "watch":
			fn.Watch = ann.value == "true"
		case "watch_default_rate":
			fn.WatchDefaultRate = ann.value
		case "watch_factory":
			fn.WatchFactory = ann.value == "true"
		case "publish":
			fn.Publish = ann.value == "true"
		case "publish_ring_size":
			if n, err := strconv.Atoi(ann.value); err == nil {
				fn.PublishRingSize = n
			}
		case "watch_source":
			fn.WatchSource = ann.value
		case "returns_value":
			fn.ReturnsValue = true
		}
	}

	return fn
}

func (p *Parser) parseTypeRef() ast.TypeRef {
	// []T slice or [N]T / [NAME]T array
	if p.cur.Type == LBRACKET {
		p.advance()
		if p.cur.Type == RBRACKET {
			// []T — slice
			p.advance()
			elem := p.parseTypeRef()
			return ast.TypeRef{Kind: ast.TypeSlice, Elem: &elem}
		}
		// [N]T or [NAME]T — array
		var size int
		var sizeName string
		if p.cur.Type == INT {
			size, _ = strconv.Atoi(p.cur.Text)
			p.advance()
		} else if p.cur.Type == IDENT {
			sizeName = p.cur.Text
			var ok bool
			size, ok = p.consts[sizeName]
			if !ok {
				p.errorf("undefined constant %q in array size", sizeName)
			}
			p.advance()
		} else {
			p.errorf("expected integer or constant name for array size, got %q", p.cur.Text)
			p.advance()
		}
		p.expect(RBRACKET)
		elem := p.parseTypeRef()
		return ast.TypeRef{Kind: ast.TypeArray, Elem: &elem, ArrayLen: size, ArrayLenName: sizeName}
	}

	name := p.cur.Text
	p.advance()

	nullable := false
	if p.cur.Type == QUESTION {
		nullable = true
		p.advance()
	}

	kind := ast.TypeNamed
	if ast.Primitives[name] {
		kind = ast.TypePrimitive
	} else if p.callbacks[name] {
		kind = ast.TypeCallback
	} else if p.imports[name] {
		kind = ast.TypeImport
	}
	return ast.TypeRef{Kind: kind, Name: name, Nullable: nullable}
}
