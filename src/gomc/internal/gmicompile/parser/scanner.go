// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// Package parser implements the GMI file parser.
package parser

import (
	"unicode"
)

// TokenType represents the type of a lexical token.
type TokenType int

const (
	EOF TokenType = iota
	ERROR

	// Literals
	IDENT
	INT
	FLOAT
	STRING

	// Keywords
	ENUM
	TYPE
	FUNC
	CONST
	CALLBACK
	STREAM_SERVER

	// Punctuation
	LBRACE   // {
	RBRACE   // }
	LPAREN   // (
	RPAREN   // )
	LBRACKET // [
	RBRACKET // ]
	COLON    // :
	COMMA    // ,
	SEMI     // ;
	EQ       // =
	ARROW    // ->
	QUESTION // ?
	AT       // @
)

// Token represents a lexical token.
type Token struct {
	Type TokenType
	Text string
	Line int
	Col  int
}

// Scanner tokenizes GMI source code.
type Scanner struct {
	src  []rune
	pos  int
	line int
	col  int
}

// NewScanner creates a new scanner for the given source.
func NewScanner(src string) *Scanner {
	return &Scanner{src: []rune(src), line: 1, col: 1}
}

// Scan returns the next token.
func (s *Scanner) Scan() Token {
	s.skipWhitespaceAndComments()
	if s.pos >= len(s.src) {
		return Token{Type: EOF, Line: s.line, Col: s.col}
	}

	start := s.pos
	line, col := s.line, s.col
	ch := s.src[s.pos]

	// Single-char tokens
	switch ch {
	case '{':
		s.advance()
		return Token{LBRACE, "{", line, col}
	case '}':
		s.advance()
		return Token{RBRACE, "}", line, col}
	case '(':
		s.advance()
		return Token{LPAREN, "(", line, col}
	case ')':
		s.advance()
		return Token{RPAREN, ")", line, col}
	case '[':
		s.advance()
		return Token{LBRACKET, "[", line, col}
	case ']':
		s.advance()
		return Token{RBRACKET, "]", line, col}
	case ':':
		s.advance()
		return Token{COLON, ":", line, col}
	case ',':
		s.advance()
		return Token{COMMA, ",", line, col}
	case ';':
		s.advance()
		return Token{SEMI, ";", line, col}
	case '=':
		s.advance()
		return Token{EQ, "=", line, col}
	case '?':
		s.advance()
		return Token{QUESTION, "?", line, col}
	case '@':
		s.advance()
		return Token{AT, "@", line, col}
	case '-':
		if s.peek(1) == '>' {
			s.advance()
			s.advance()
			return Token{ARROW, "->", line, col}
		}
		// Fall through to number (negative)
	}

	// Numbers (including negative)
	if ch == '-' || unicode.IsDigit(ch) {
		return s.scanNumber(line, col)
	}

	// Strings
	if ch == '"' {
		return s.scanString(line, col)
	}

	// Identifiers and keywords
	if unicode.IsLetter(ch) || ch == '_' {
		for s.pos < len(s.src) && (unicode.IsLetter(s.src[s.pos]) || unicode.IsDigit(s.src[s.pos]) || s.src[s.pos] == '_') {
			s.advance()
		}
		text := string(s.src[start:s.pos])
		return Token{s.keyword(text), text, line, col}
	}

	s.advance()
	return Token{ERROR, string(ch), line, col}
}

func (s *Scanner) advance() {
	if s.pos < len(s.src) {
		if s.src[s.pos] == '\n' {
			s.line++
			s.col = 1
		} else {
			s.col++
		}
		s.pos++
	}
}

func (s *Scanner) peek(offset int) rune {
	if s.pos+offset < len(s.src) {
		return s.src[s.pos+offset]
	}
	return 0
}

func (s *Scanner) skipWhitespaceAndComments() {
	for s.pos < len(s.src) {
		ch := s.src[s.pos]
		if ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' {
			s.advance()
		} else if ch == '#' {
			// Line comment
			for s.pos < len(s.src) && s.src[s.pos] != '\n' {
				s.advance()
			}
		} else {
			break
		}
	}
}

func (s *Scanner) scanNumber(line, col int) Token {
	start := s.pos
	if s.src[s.pos] == '-' {
		s.advance()
	}
	for s.pos < len(s.src) && unicode.IsDigit(s.src[s.pos]) {
		s.advance()
	}
	typ := INT
	// Fractional part: a '.' followed by digits makes this a FLOAT literal.
	if s.pos+1 < len(s.src) && s.src[s.pos] == '.' && unicode.IsDigit(s.src[s.pos+1]) {
		typ = FLOAT
		s.advance() // consume '.'
		for s.pos < len(s.src) && unicode.IsDigit(s.src[s.pos]) {
			s.advance()
		}
	}
	return Token{typ, string(s.src[start:s.pos]), line, col}
}

func (s *Scanner) scanString(line, col int) Token {
	s.advance() // skip opening "
	start := s.pos
	for s.pos < len(s.src) && s.src[s.pos] != '"' {
		if s.src[s.pos] == '\\' {
			s.advance()
		}
		s.advance()
	}
	text := string(s.src[start:s.pos])
	if s.pos < len(s.src) {
		s.advance() // skip closing "
	}
	return Token{STRING, text, line, col}
}

func (s *Scanner) keyword(text string) TokenType {
	switch text {
	case "enum":
		return ENUM
	case "type":
		return TYPE
	case "func":
		return FUNC
	case "const":
		return CONST
	case "callback":
		return CALLBACK
	case "stream_server":
		return STREAM_SERVER
	}
	return IDENT
}
