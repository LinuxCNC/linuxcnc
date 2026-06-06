package classicladder

/*
#include "classicladder_rt.h"
*/
import "C"
import (
	"fmt"
	"strconv"
	"strings"
	"unicode"
)

// compileExpression compiles a ClassicLadder arithmetic expression string
// into bytecode for the RT evaluator. kind: 0=compare, 1=operate.
func compileExpression(expr string, kind int) (C.cl_compiled_expr_t, error) {
	var ce C.cl_compiled_expr_t
	ce.kind = C.uint8_t(kind)

	expr = strings.TrimSpace(expr)
	if expr == "" {
		return ce, nil
	}

	c := &compiler{src: expr}
	var err error

	switch kind {
	case 0: // COMPAR: "LHS comparator RHS" — evaluates to bool
		err = c.compileCompare()
	case 1: // OPERATE: "@target@ := expression"
		err = c.compileOperate()
	}

	if err != nil {
		return ce, fmt.Errorf("compile %q: %w", expr, err)
	}

	if len(c.code) > C.CL_EXPR_MAX_CODE {
		return ce, fmt.Errorf("expression too complex: %d instructions (max %d)", len(c.code), C.CL_EXPR_MAX_CODE)
	}

	ce.len = C.uint8_t(len(c.code))
	for i, ins := range c.code {
		ce.code[i].opcode = C.uint8_t(ins.opcode)
		ce.code[i].operand = C.int32_t(ins.operand)
	}
	ce.valid = 1
	return ce, nil
}

// instruction represents a single bytecode instruction.
type instruction struct {
	opcode  uint8
	operand int32
}

// Opcode constants (must match C enum cl_opcode)
const (
	opNOP         = 0
	opPushConst   = 1
	opLoadVar     = 2
	opLoadVarIdx  = 3
	opStoreVar    = 4
	opStoreVarIdx = 5
	opAdd         = 6
	opSub         = 7
	opMul         = 8
	opDiv         = 9
	opMod         = 10
	opPow         = 11
	opAnd         = 12
	opOr          = 13
	opXor         = 14
	opNot         = 15
	opNeg         = 16
	opCmpLT       = 17
	opCmpGT       = 18
	opCmpEQ       = 19
	opCmpLE       = 20
	opCmpGE       = 21
	opCmpNE       = 22
	opAbs         = 23
	opMini        = 24
	opMaxi        = 25
)

// compiler is the expression-to-bytecode compiler.
type compiler struct {
	src  string
	pos  int
	code []instruction
}

func (c *compiler) emit(op uint8, operand int32) {
	c.code = append(c.code, instruction{opcode: op, operand: operand})
}

func (c *compiler) peek() byte {
	c.skipSpaces()
	if c.pos >= len(c.src) {
		return 0
	}
	return c.src[c.pos]
}

func (c *compiler) skipSpaces() {
	for c.pos < len(c.src) && c.src[c.pos] == ' ' {
		c.pos++
	}
}

func (c *compiler) eof() bool {
	c.skipSpaces()
	return c.pos >= len(c.src)
}

// compileCompare: parse "expr comparator expr" → bytecode that leaves bool on stack.
func (c *compiler) compileCompare() error {
	// Parse LHS
	if err := c.parseExprOr(); err != nil {
		return err
	}

	// Parse comparator
	c.skipSpaces()
	var cmpOp uint8
	if c.pos < len(c.src) {
		switch {
		case strings.HasPrefix(c.src[c.pos:], "<="):
			cmpOp = opCmpLE
			c.pos += 2
		case strings.HasPrefix(c.src[c.pos:], ">="):
			cmpOp = opCmpGE
			c.pos += 2
		case strings.HasPrefix(c.src[c.pos:], "<>"):
			cmpOp = opCmpNE
			c.pos += 2
		case strings.HasPrefix(c.src[c.pos:], "!="):
			cmpOp = opCmpNE
			c.pos += 2
		case c.src[c.pos] == '<':
			cmpOp = opCmpLT
			c.pos++
		case c.src[c.pos] == '>':
			cmpOp = opCmpGT
			c.pos++
		case c.src[c.pos] == '=':
			cmpOp = opCmpEQ
			c.pos++
		default:
			return fmt.Errorf("expected comparator at pos %d", c.pos)
		}
	} else {
		return fmt.Errorf("expected comparator, got EOF")
	}

	// Parse RHS
	if err := c.parseExprOr(); err != nil {
		return err
	}

	c.emit(cmpOp, 0)
	return nil
}

// compileOperate: parse "@target@ := expr" OR "@target[idx]@ := expr"
func (c *compiler) compileOperate() error {
	c.skipSpaces()
	if c.peek() != '@' {
		return fmt.Errorf("operate expression must start with @target@")
	}

	// Parse the assignment target
	c.pos++ // skip @
	varType, varOffset, indexed, idxType, idxOffset, err := c.parseVarRef()
	if err != nil {
		return fmt.Errorf("target: %w", err)
	}

	// Expect :=
	c.skipSpaces()
	if !strings.HasPrefix(c.src[c.pos:], ":=") {
		return fmt.Errorf("expected ':=' at pos %d", c.pos)
	}
	c.pos += 2

	if indexed {
		// Push index value first (for STORE_VAR_IDX: stack = [index, value])
		// We need to emit index load, then expression, then store_var_idx
		c.emitVarLoad(idxType, idxOffset, false, 0, 0)
	}

	// Parse the RHS expression
	if err := c.parseExprOr(); err != nil {
		return err
	}

	// Store result
	packed := packVar(varType, varOffset)
	if indexed {
		c.emit(opStoreVarIdx, packed)
	} else {
		c.emit(opStoreVar, packed)
	}
	return nil
}

func packVar(varType, offset int) int32 {
	return int32((varType&0xFFFF)<<16 | (offset & 0xFFFF))
}

// Expression parsing — recursive descent with operator precedence:
// Or > Xor > And > AddSub > MulDivMod > Pow > Unary > Term

func (c *compiler) parseExprOr() error {
	if err := c.parseExprXor(); err != nil {
		return err
	}
	for c.peek() == '|' {
		c.pos++
		if err := c.parseExprXor(); err != nil {
			return err
		}
		c.emit(opOr, 0)
	}
	return nil
}

func (c *compiler) parseExprXor() error {
	if err := c.parseExprAnd(); err != nil {
		return err
	}
	for c.peek() == '^' {
		c.pos++
		if err := c.parseExprAnd(); err != nil {
			return err
		}
		c.emit(opXor, 0)
	}
	return nil
}

func (c *compiler) parseExprAnd() error {
	if err := c.parseExprAdd(); err != nil {
		return err
	}
	for c.peek() == '&' {
		c.pos++
		if err := c.parseExprAdd(); err != nil {
			return err
		}
		c.emit(opAnd, 0)
	}
	return nil
}

func (c *compiler) parseExprAdd() error {
	if err := c.parseExprMul(); err != nil {
		return err
	}
	for {
		ch := c.peek()
		if ch == '+' {
			c.pos++
			if err := c.parseExprMul(); err != nil {
				return err
			}
			c.emit(opAdd, 0)
		} else if ch == '-' {
			c.pos++
			if err := c.parseExprMul(); err != nil {
				return err
			}
			c.emit(opSub, 0)
		} else {
			break
		}
	}
	return nil
}

func (c *compiler) parseExprMul() error {
	if err := c.parseExprPow(); err != nil {
		return err
	}
	for {
		ch := c.peek()
		if ch == '*' {
			c.pos++
			if err := c.parseExprPow(); err != nil {
				return err
			}
			c.emit(opMul, 0)
		} else if ch == '/' {
			c.pos++
			if err := c.parseExprPow(); err != nil {
				return err
			}
			c.emit(opDiv, 0)
		} else if ch == '%' {
			c.pos++
			if err := c.parseExprPow(); err != nil {
				return err
			}
			c.emit(opMod, 0)
		} else {
			break
		}
	}
	return nil
}

func (c *compiler) parseExprPow() error {
	if err := c.parseUnary(); err != nil {
		return err
	}
	if c.peek() == '*' && c.pos+1 < len(c.src) && c.src[c.pos+1] == '*' {
		c.pos += 2
		if err := c.parseExprPow(); err != nil { // right-associative
			return err
		}
		c.emit(opPow, 0)
	}
	return nil
}

func (c *compiler) parseUnary() error {
	ch := c.peek()
	if ch == '!' {
		c.pos++
		if err := c.parseUnary(); err != nil {
			return err
		}
		c.emit(opNot, 0)
		return nil
	}
	if ch == '-' {
		// Distinguish unary minus from subtraction: only if next is digit, @, or (
		next := c.pos + 1
		for next < len(c.src) && c.src[next] == ' ' {
			next++
		}
		if next < len(c.src) && (c.src[next] == '@' || c.src[next] == '(' || unicode.IsDigit(rune(c.src[next])) || c.src[next] == '$') {
			c.pos++
			if err := c.parseUnary(); err != nil {
				return err
			}
			c.emit(opNeg, 0)
			return nil
		}
	}
	return c.parseTerm()
}

func (c *compiler) parseTerm() error {
	c.skipSpaces()
	if c.pos >= len(c.src) {
		return fmt.Errorf("unexpected end of expression")
	}

	ch := c.src[c.pos]

	// Parenthesized sub-expression
	if ch == '(' {
		c.pos++
		if err := c.parseExprOr(); err != nil {
			return err
		}
		c.skipSpaces()
		if c.pos >= len(c.src) || c.src[c.pos] != ')' {
			return fmt.Errorf("missing closing ')'")
		}
		c.pos++
		return nil
	}

	// Variable reference: @type/offset@ or @type/offset[idx_type/idx_offset]@
	if ch == '@' {
		c.pos++
		varType, varOffset, indexed, idxType, idxOffset, err := c.parseVarRef()
		if err != nil {
			return err
		}
		if indexed {
			c.emitVarLoad(varType, varOffset, true, idxType, idxOffset)
		} else {
			c.emitVarLoad(varType, varOffset, false, 0, 0)
		}
		return nil
	}

	// Hex constant: $xxxx
	if ch == '$' {
		c.pos++
		start := c.pos
		for c.pos < len(c.src) && isHexDigit(c.src[c.pos]) {
			c.pos++
		}
		if c.pos == start {
			return fmt.Errorf("expected hex digits after '$'")
		}
		val, err := strconv.ParseInt(c.src[start:c.pos], 16, 32)
		if err != nil {
			return fmt.Errorf("invalid hex: %w", err)
		}
		c.emit(opPushConst, int32(val))
		return nil
	}

	// Decimal constant
	if unicode.IsDigit(rune(ch)) {
		start := c.pos
		for c.pos < len(c.src) && unicode.IsDigit(rune(c.src[c.pos])) {
			c.pos++
		}
		val, err := strconv.ParseInt(c.src[start:c.pos], 10, 32)
		if err != nil {
			return fmt.Errorf("invalid number: %w", err)
		}
		c.emit(opPushConst, int32(val))
		return nil
	}

	// Function call: ABS(x), MINI(a,b), MAXI(a,b), MOY/AVG(a,b)
	if unicode.IsLetter(rune(ch)) {
		start := c.pos
		for c.pos < len(c.src) && (unicode.IsLetter(rune(c.src[c.pos])) || unicode.IsDigit(rune(c.src[c.pos]))) {
			c.pos++
		}
		fname := strings.ToUpper(c.src[start:c.pos])
		c.skipSpaces()
		if c.pos >= len(c.src) || c.src[c.pos] != '(' {
			return fmt.Errorf("expected '(' after function name %q", fname)
		}
		c.pos++ // skip (

		switch fname {
		case "ABS":
			if err := c.parseExprOr(); err != nil {
				return err
			}
			c.skipSpaces()
			if c.pos >= len(c.src) || c.src[c.pos] != ')' {
				return fmt.Errorf("missing ')' for ABS")
			}
			c.pos++
			c.emit(opAbs, 0)
		case "MINI":
			if err := c.parseFuncArgs2(); err != nil {
				return err
			}
			c.emit(opMini, 0)
		case "MAXI":
			if err := c.parseFuncArgs2(); err != nil {
				return err
			}
			c.emit(opMaxi, 0)
		case "MOY", "AVG":
			// Average of two values: (a+b)/2
			if err := c.parseFuncArgs2(); err != nil {
				return err
			}
			c.emit(opAdd, 0)
			c.emit(opPushConst, 2)
			c.emit(opDiv, 0)
		default:
			return fmt.Errorf("unknown function %q", fname)
		}
		return nil
	}

	return fmt.Errorf("unexpected character %q at pos %d", ch, c.pos)
}

func (c *compiler) parseFuncArgs2() error {
	if err := c.parseExprOr(); err != nil {
		return err
	}
	c.skipSpaces()
	if c.pos >= len(c.src) || c.src[c.pos] != ',' {
		return fmt.Errorf("expected ',' between function arguments")
	}
	c.pos++
	if err := c.parseExprOr(); err != nil {
		return err
	}
	c.skipSpaces()
	if c.pos >= len(c.src) || c.src[c.pos] != ')' {
		return fmt.Errorf("missing ')' for function")
	}
	c.pos++
	return nil
}

// parseVarRef parses "type/offset@" or "type/offset[idx_type/idx_offset]@"
// The leading '@' has already been consumed.
func (c *compiler) parseVarRef() (varType, varOffset int, indexed bool, idxType, idxOffset int, err error) {
	varType, err = c.parseInt()
	if err != nil {
		return 0, 0, false, 0, 0, fmt.Errorf("var type: %w", err)
	}
	if c.pos >= len(c.src) || c.src[c.pos] != '/' {
		return 0, 0, false, 0, 0, fmt.Errorf("expected '/' after var type")
	}
	c.pos++
	varOffset, err = c.parseInt()
	if err != nil {
		return 0, 0, false, 0, 0, fmt.Errorf("var offset: %w", err)
	}

	// Check for indexed: [idx_type/idx_offset]
	if c.pos < len(c.src) && c.src[c.pos] == '[' {
		c.pos++
		indexed = true
		idxType, err = c.parseInt()
		if err != nil {
			return 0, 0, false, 0, 0, fmt.Errorf("idx type: %w", err)
		}
		if c.pos >= len(c.src) || c.src[c.pos] != '/' {
			return 0, 0, false, 0, 0, fmt.Errorf("expected '/' in index ref")
		}
		c.pos++
		idxOffset, err = c.parseInt()
		if err != nil {
			return 0, 0, false, 0, 0, fmt.Errorf("idx offset: %w", err)
		}
		if c.pos >= len(c.src) || c.src[c.pos] != ']' {
			return 0, 0, false, 0, 0, fmt.Errorf("expected ']' after index ref")
		}
		c.pos++
	}

	// Expect closing @
	if c.pos >= len(c.src) || c.src[c.pos] != '@' {
		return 0, 0, false, 0, 0, fmt.Errorf("expected closing '@'")
	}
	c.pos++
	return
}

func (c *compiler) parseInt() (int, error) {
	start := c.pos
	if c.pos < len(c.src) && c.src[c.pos] == '-' {
		c.pos++
	}
	for c.pos < len(c.src) && unicode.IsDigit(rune(c.src[c.pos])) {
		c.pos++
	}
	if c.pos == start {
		return 0, fmt.Errorf("expected integer at pos %d", c.pos)
	}
	val, err := strconv.Atoi(c.src[start:c.pos])
	if err != nil {
		return 0, err
	}
	return val, nil
}

func (c *compiler) emitVarLoad(varType, varOffset int, indexed bool, idxType, idxOffset int) {
	packed := packVar(varType, varOffset)
	if indexed {
		// Push the index variable's value, then LOAD_VAR_IDX
		c.emit(opLoadVar, packVar(idxType, idxOffset))
		c.emit(opLoadVarIdx, packed)
	} else {
		c.emit(opLoadVar, packed)
	}
}

func isHexDigit(b byte) bool {
	return (b >= '0' && b <= '9') || (b >= 'a' && b <= 'f') || (b >= 'A' && b <= 'F')
}

// compileAllExpressions compiles all arithmetic expressions in the RT instance.
// Called from Go after loading a program. Returns errors for invalid expressions
// but does not fail — invalid expressions are simply marked as invalid.
func (cl *classicladder) compileAllExpressions() []error {
	var errs []error
	for i := 0; i < int(cl.rt.sizes.nbr_arithm_expr); i++ {
		expr := C.GoString(&cl.rt.arithm_exprs[i].expr[0])
		if expr == "" {
			cl.rt.compiled_exprs[i].valid = 0
			cl.rt.compiled_exprs[i].len = 0
			continue
		}

		// Determine kind by looking at the expression content:
		// OPERATE expressions have ":=" in them.
		kind := 0 // compare
		if strings.Contains(expr, ":=") {
			kind = 1 // operate
		}

		ce, err := compileExpression(expr, kind)
		if err != nil {
			errs = append(errs, fmt.Errorf("expr[%d]: %w", i, err))
			cl.rt.compiled_exprs[i].valid = 0
			continue
		}
		cl.rt.compiled_exprs[i] = ce
	}
	return errs
}
