package hal

import (
	"fmt"
	"os"
	"regexp"
	"strconv"
	"strings"
)

// tokenizeLine splits a single HAL config line into raw string tokens.
// It handles double-quoted strings (with backslash escapes), single-quoted
// strings (no escaping inside), backslash escapes outside quotes, and
// comment stripping (# outside quotes). Returns an error for unterminated
// quotes.
func tokenizeLine(line string) ([]string, error) {
	var tokens []string
	var current strings.Builder
	inToken := false
	i := 0

	for i < len(line) {
		ch := line[i]

		if ch == '#' {
			// Comment: discard everything from here to end of line
			break
		}

		if ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' {
			// Whitespace ends current token
			if inToken {
				tokens = append(tokens, current.String())
				current.Reset()
				inToken = false
			}
			i++
			continue
		}

		if ch == '"' {
			// Double-quoted string — backslash escapes are processed
			inToken = true
			i++ // skip opening quote
			for {
				if i >= len(line) {
					return nil, fmt.Errorf("unterminated double quote")
				}
				c := line[i]
				if c == '"' {
					i++ // skip closing quote
					break
				}
				if c == '\\' {
					i++
					if i >= len(line) {
						return nil, fmt.Errorf("unterminated double quote")
					}
					switch line[i] {
					case '"':
						current.WriteByte('"')
					case '\\':
						current.WriteByte('\\')
					case 'n':
						current.WriteByte('\n')
					case 't':
						current.WriteByte('\t')
					case 'r':
						current.WriteByte('\r')
					default:
						current.WriteByte('\\')
						current.WriteByte(line[i])
					}
					i++
					continue
				}
				current.WriteByte(c)
				i++
			}
			continue
		}

		if ch == '\'' {
			// Single-quoted string — no escape processing inside
			inToken = true
			i++ // skip opening quote
			for {
				if i >= len(line) {
					return nil, fmt.Errorf("unterminated single quote")
				}
				c := line[i]
				if c == '\'' {
					i++ // skip closing quote
					break
				}
				current.WriteByte(c)
				i++
			}
			continue
		}

		if ch == '\\' {
			// Backslash escape outside quotes: next character is literal
			inToken = true
			i++
			if i >= len(line) {
				// Trailing backslash (file should have had continuation join already)
				break
			}
			current.WriteByte(line[i])
			i++
			continue
		}

		// Regular character
		inToken = true
		current.WriteByte(ch)
		i++
	}

	if inToken {
		tokens = append(tokens, current.String())
	}

	return tokens, nil
}

// joinContinuationLines normalizes line endings and joins lines ending with \.
// A space is inserted at the join point to preserve token boundaries when the
// continuation line has no leading whitespace.
func joinContinuationLines(content string) string {
	content = strings.ReplaceAll(content, "\r\n", "\n")
	content = strings.ReplaceAll(content, "\\\n", " ")
	return content
}

// iniVarPattern matches [SECTION]KEY in a HAL line.
// Section names and keys may contain letters, digits, underscores, or hyphens
// (e.g. [JOINT_0]MIN-LIMIT, [DISPLAY]PYVCP-PANEL).
var iniVarPattern = regexp.MustCompile(`\[([A-Za-z0-9_-]+)\]([A-Za-z0-9_-]+)`)

// envVarPattern matches ${VARNAME} and $VARNAME in a HAL line.
var envVarPattern = regexp.MustCompile(`\$\{([A-Za-z_][A-Za-z0-9_]*)\}|\$([A-Za-z_][A-Za-z0-9_]*)`)

// substituteVars replaces [SECTION]KEY with INI values and $ENV_VAR / ${ENV_VAR}
// with environment variable values. Called per-line before tokenizing.
func substituteVars(line string, ini INILookup) string {
	// Replace [SECTION]KEY with INI values; leave as-is if not found
	line = iniVarPattern.ReplaceAllStringFunc(line, func(match string) string {
		groups := iniVarPattern.FindStringSubmatch(match)
		if len(groups) < 3 {
			return match
		}
		val, err := ini.Get(groups[1], groups[2])
		if err != nil {
			return match
		}
		return val
	})

	// Replace ${VARNAME} and $VARNAME with environment variables
	line = envVarPattern.ReplaceAllStringFunc(line, func(match string) string {
		groups := envVarPattern.FindStringSubmatch(match)
		if len(groups) > 1 && groups[1] != "" {
			return os.Getenv(groups[1]) // ${VARNAME}
		}
		if len(groups) > 2 && groups[2] != "" {
			return os.Getenv(groups[2]) // $VARNAME
		}
		return ""
	})

	return line
}

// --- helper converters ---

func parsePinType(s string, loc SourceLoc) (PinType, *ParseError) {
	switch strings.ToLower(s) {
	case "bit":
		return TypeBit, nil
	case "float":
		return TypeFloat, nil
	case "s32":
		return TypeS32, nil
	case "u32":
		return TypeU32, nil
	default:
		return 0, &ParseError{Loc: loc, Msg: fmt.Sprintf("unknown pin type: %q", s)}
	}
}

func parseLockLevelStr(s string, loc SourceLoc) (LockLevel, *ParseError) {
	switch strings.ToLower(s) {
	case "none":
		return LockNone, nil
	case "load":
		return LockLoad, nil
	case "config":
		return LockConfig, nil
	case "tune":
		return LockTune, nil
	case "params":
		return LockParams, nil
	case "run":
		return LockRun, nil
	case "all":
		return LockAll, nil
	default:
		return 0, &ParseError{Loc: loc, Msg: fmt.Sprintf("unknown lock level: %q", s)}
	}
}

func parseHalObjType(s string, loc SourceLoc) (HalObjType, *ParseError) {
	switch strings.ToLower(s) {
	case "pin":
		return ObjPin, nil
	case "sig":
		return ObjSig, nil
	case "param":
		return ObjParam, nil
	case "funct":
		return ObjFunct, nil
	case "thread":
		return ObjThread, nil
	case "comp":
		return ObjComp, nil
	case "all":
		return ObjAll, nil
	default:
		return 0, &ParseError{Loc: loc, Msg: fmt.Sprintf("unknown HAL object type: %q", s)}
	}
}

func parseSaveTypeStr(s string, loc SourceLoc) (SaveType, *ParseError) {
	switch strings.ToLower(s) {
	case "comp":
		return SaveComp, nil
	case "sig":
		return SaveSig, nil
	case "link":
		return SaveLink, nil
	case "net":
		return SaveNet, nil
	case "param":
		return SaveParam, nil
	case "thread":
		return SaveThread, nil
	case "all":
		return SaveAll, nil
	default:
		return 0, &ParseError{Loc: loc, Msg: fmt.Sprintf("unknown save type: %q", s)}
	}
}

func parseAliasKind(s string, loc SourceLoc) (AliasKind, *ParseError) {
	switch strings.ToLower(s) {
	case "pin":
		return AliasPin, nil
	case "param":
		return AliasParam, nil
	default:
		return 0, &ParseError{Loc: loc, Msg: fmt.Sprintf("alias: unknown kind %q (want \"pin\" or \"param\")", s)}
	}
}

// --- per-command parse functions ---

func parseLoadRT(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) < 1 {
		return Token{}, &ParseError{Loc: loc, Msg: "loadrt: missing module name"}
	}
	tok := &LoadRTToken{
		Comp:   tokens[0],
		Params: make(map[string]string),
	}
	for _, arg := range tokens[1:] {
		key, value, hasEquals := strings.Cut(arg, "=")
		if !hasEquals {
			continue
		}
		switch key {
		case "count":
			n, err := strconv.Atoi(strings.TrimSpace(value))
			if err != nil {
				return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("loadrt: invalid count value: %q", value)}
			}
			tok.Count = n
		case "names":
			for _, name := range strings.Split(value, ",") {
				name = strings.TrimSpace(name)
				if name != "" {
					tok.Names = append(tok.Names, name)
				}
			}
		default:
			tok.Params[key] = value
		}
	}
	return Token{Location: loc, Data: tok}, nil
}

func parseLoadUSR(tokens []string, loc SourceLoc) (Token, *ParseError) {
	tok := &LoadUSRToken{}
	i := 0
	for i < len(tokens) {
		arg := tokens[i]
		matched := true
		switch arg {
		case "-W":
			tok.WaitReady = true
		case "-w":
			tok.WaitExit = true
		case "-i":
			tok.NoStdin = true
		case "-Wn":
			i++
			if i >= len(tokens) {
				return Token{}, &ParseError{Loc: loc, Msg: "loadusr: -Wn requires a name argument"}
			}
			tok.WaitName = tokens[i]
		case "-T":
			i++
			if i >= len(tokens) {
				return Token{}, &ParseError{Loc: loc, Msg: "loadusr: -T requires a seconds argument"}
			}
			n, err := strconv.Atoi(tokens[i])
			if err != nil {
				return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("loadusr: invalid timeout: %q", tokens[i])}
			}
			tok.Timeout = n
		default:
			matched = false
		}
		if !matched {
			break
		}
		i++
	}
	if i >= len(tokens) {
		return Token{}, &ParseError{Loc: loc, Msg: "loadusr: missing program name"}
	}
	tok.Prog = tokens[i]
	if i+1 < len(tokens) {
		tok.Args = tokens[i+1:]
	}
	return Token{Location: loc, Data: tok}, nil
}

func parseNet(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) < 1 {
		return Token{}, &ParseError{Loc: loc, Msg: "net: missing signal name"}
	}
	tok := &NetToken{Signal: tokens[0]}
	for _, arg := range tokens[1:] {
		if arg == "=>" || arg == "<=" || arg == "<=>" {
			continue
		}
		tok.Pins = append(tok.Pins, arg)
	}
	return Token{Location: loc, Data: tok}, nil
}

func parseSetP(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) != 2 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("setp: expected 2 arguments, got %d", len(tokens))}
	}
	return Token{Location: loc, Data: &SetPToken{Name: tokens[0], Value: tokens[1]}}, nil
}

func parseSetS(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) != 2 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("sets: expected 2 arguments, got %d", len(tokens))}
	}
	return Token{Location: loc, Data: &SetSToken{Name: tokens[0], Value: tokens[1]}}, nil
}

func parseGetP(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) != 1 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("getp: expected 1 argument, got %d", len(tokens))}
	}
	return Token{Location: loc, Data: &GetPToken{Name: tokens[0]}}, nil
}

func parseGetS(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) != 1 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("gets: expected 1 argument, got %d", len(tokens))}
	}
	return Token{Location: loc, Data: &GetSToken{Name: tokens[0]}}, nil
}

func parseAddF(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) < 2 || len(tokens) > 3 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("addf: expected 2-3 arguments, got %d", len(tokens))}
	}
	tok := &AddFToken{Funct: tokens[0], Thread: tokens[1], Pos: -1}
	if len(tokens) == 3 {
		n, err := strconv.Atoi(tokens[2])
		if err != nil {
			return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("addf: invalid position: %q", tokens[2])}
		}
		tok.Pos = n
	}
	return Token{Location: loc, Data: tok}, nil
}

func parseDelF(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) != 2 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("delf: expected 2 arguments, got %d", len(tokens))}
	}
	return Token{Location: loc, Data: &DelFToken{Funct: tokens[0], Thread: tokens[1]}}, nil
}

func parseNewSig(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) != 2 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("newsig: expected 2 arguments, got %d", len(tokens))}
	}
	pt, err := parsePinType(tokens[1], loc)
	if err != nil {
		return Token{}, err
	}
	return Token{Location: loc, Data: &NewSigToken{Name: tokens[0], SigType: pt}}, nil
}

func parseDelSig(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) != 1 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("delsig: expected 1 argument, got %d", len(tokens))}
	}
	return Token{Location: loc, Data: &DelSigToken{Name: tokens[0]}}, nil
}

func parseLinkPS(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) != 2 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("linkps: expected 2 arguments, got %d", len(tokens))}
	}
	return Token{Location: loc, Data: &LinkPSToken{Pin: tokens[0], Sig: tokens[1]}}, nil
}

func parseLinkSP(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) != 2 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("linksp: expected 2 arguments, got %d", len(tokens))}
	}
	return Token{Location: loc, Data: &LinkSPToken{Sig: tokens[0], Pin: tokens[1]}}, nil
}

func parseLinkPP(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) != 2 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("linkpp: expected 2 arguments, got %d", len(tokens))}
	}
	return Token{Location: loc, Data: &LinkPPToken{Pin1: tokens[0], Pin2: tokens[1]}}, nil
}

func parseUnlinkP(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) != 1 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("unlinkp: expected 1 argument, got %d", len(tokens))}
	}
	return Token{Location: loc, Data: &UnlinkPToken{Pin: tokens[0]}}, nil
}

func parseAlias(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) != 3 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("alias: expected 3 arguments, got %d", len(tokens))}
	}
	kind, err := parseAliasKind(tokens[0], loc)
	if err != nil {
		return Token{}, err
	}
	return Token{Location: loc, Data: &AliasToken{Kind: kind, Name: tokens[1], Alias: tokens[2]}}, nil
}

func parseUnAlias(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) != 2 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("unalias: expected 2 arguments, got %d", len(tokens))}
	}
	kind, err := parseAliasKind(tokens[0], loc)
	if err != nil {
		return Token{}, err
	}
	return Token{Location: loc, Data: &UnAliasToken{Kind: kind, Name: tokens[1]}}, nil
}

func parseStart(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) != 0 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("start: expected 0 arguments, got %d", len(tokens))}
	}
	return Token{Location: loc, Data: &StartToken{}}, nil
}

func parseStop(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) != 0 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("stop: expected 0 arguments, got %d", len(tokens))}
	}
	return Token{Location: loc, Data: &StopToken{}}, nil
}

func parseLock(tokens []string, loc SourceLoc) (Token, *ParseError) {
	level := LockAll
	if len(tokens) == 1 {
		var err *ParseError
		level, err = parseLockLevelStr(tokens[0], loc)
		if err != nil {
			return Token{}, err
		}
	} else if len(tokens) > 1 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("lock: expected 0-1 arguments, got %d", len(tokens))}
	}
	return Token{Location: loc, Data: &LockToken{Level: level}}, nil
}

func parseUnlock(tokens []string, loc SourceLoc) (Token, *ParseError) {
	level := LockAll
	if len(tokens) == 1 {
		var err *ParseError
		level, err = parseLockLevelStr(tokens[0], loc)
		if err != nil {
			return Token{}, err
		}
	} else if len(tokens) > 1 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("unlock: expected 0-1 arguments, got %d", len(tokens))}
	}
	return Token{Location: loc, Data: &UnlockToken{Level: level}}, nil
}

func parseUnloadRT(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) != 1 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("unloadrt: expected 1 argument, got %d", len(tokens))}
	}
	return Token{Location: loc, Data: &UnloadRTToken{Comp: tokens[0]}}, nil
}

func parseUnloadUSR(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) != 1 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("unloadusr: expected 1 argument, got %d", len(tokens))}
	}
	return Token{Location: loc, Data: &UnloadUSRToken{Comp: tokens[0]}}, nil
}

func parseUnload(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) != 1 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("unload: expected 1 argument, got %d", len(tokens))}
	}
	return Token{Location: loc, Data: &UnloadToken{Comp: tokens[0]}}, nil
}

func parseWaitUSR(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) != 1 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("waitusr: expected 1 argument, got %d", len(tokens))}
	}
	return Token{Location: loc, Data: &WaitUSRToken{Comp: tokens[0]}}, nil
}

func parseList(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) < 1 {
		return Token{}, &ParseError{Loc: loc, Msg: "list: missing object type"}
	}
	objType, err := parseHalObjType(tokens[0], loc)
	if err != nil {
		return Token{}, err
	}
	return Token{Location: loc, Data: &ListToken{ObjType: objType, Patterns: tokens[1:]}}, nil
}

func parseShow(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) == 0 {
		return Token{Location: loc, Data: &ShowToken{ObjType: ObjAll}}, nil
	}
	// "all" is also accepted as explicit type for show
	if strings.ToLower(tokens[0]) == "all" {
		return Token{Location: loc, Data: &ShowToken{ObjType: ObjAll, Patterns: tokens[1:]}}, nil
	}
	objType, err := parseHalObjType(tokens[0], loc)
	if err != nil {
		return Token{}, err
	}
	return Token{Location: loc, Data: &ShowToken{ObjType: objType, Patterns: tokens[1:]}}, nil
}

func parseSave(tokens []string, loc SourceLoc) (Token, *ParseError) {
	tok := &SaveToken{SaveType: SaveAll}
	if len(tokens) == 0 {
		return Token{Location: loc, Data: tok}, nil
	}
	st, err := parseSaveTypeStr(tokens[0], loc)
	if err != nil {
		return Token{}, err
	}
	tok.SaveType = st
	if len(tokens) >= 2 {
		tok.File = tokens[1]
	}
	return Token{Location: loc, Data: tok}, nil
}

func parseStatus(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) != 0 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("status: expected 0 arguments, got %d", len(tokens))}
	}
	return Token{Location: loc, Data: &StatusToken{}}, nil
}

func parseDebug(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) != 1 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("debug: expected 1 argument, got %d", len(tokens))}
	}
	n, err := strconv.Atoi(tokens[0])
	if err != nil {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("debug: invalid level: %q", tokens[0])}
	}
	return Token{Location: loc, Data: &DebugToken{Level: n}}, nil
}

func parsePType(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) != 1 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("ptype: expected 1 argument, got %d", len(tokens))}
	}
	return Token{Location: loc, Data: &PTypeToken{Name: tokens[0]}}, nil
}

func parseSType(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) != 1 {
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("stype: expected 1 argument, got %d", len(tokens))}
	}
	return Token{Location: loc, Data: &STypeToken{Name: tokens[0]}}, nil
}

func parseEcho(_ []string, loc SourceLoc) (Token, *ParseError) {
	return Token{Location: loc, Data: &EchoToken{}}, nil
}

func parseUnEcho(_ []string, loc SourceLoc) (Token, *ParseError) {
	return Token{Location: loc, Data: &UnEchoToken{}}, nil
}

func parsePrint(tokens []string, loc SourceLoc) (Token, *ParseError) {
	return Token{Location: loc, Data: &PrintToken{Message: strings.Join(tokens, " ")}}, nil
}

// parseLine dispatches a tokenized line to the appropriate per-command parse
// function. The command name (tokens[0]) is matched case-insensitively.
// The "source" command is NOT handled here — it is resolved by SingleFileParser.
func parseLine(tokens []string, loc SourceLoc) (Token, *ParseError) {
	if len(tokens) == 0 {
		return Token{}, &ParseError{Loc: loc, Msg: "empty token list"}
	}
	cmd := strings.ToLower(tokens[0])
	args := tokens[1:]

	switch cmd {
	case "loadrt":
		return parseLoadRT(args, loc)
	case "loadusr":
		return parseLoadUSR(args, loc)
	case "net":
		return parseNet(args, loc)
	case "setp":
		return parseSetP(args, loc)
	case "sets":
		return parseSetS(args, loc)
	case "getp":
		return parseGetP(args, loc)
	case "gets":
		return parseGetS(args, loc)
	case "addf":
		return parseAddF(args, loc)
	case "delf":
		return parseDelF(args, loc)
	case "newsig":
		return parseNewSig(args, loc)
	case "delsig":
		return parseDelSig(args, loc)
	case "linkps":
		return parseLinkPS(args, loc)
	case "linksp":
		return parseLinkSP(args, loc)
	case "linkpp":
		return parseLinkPP(args, loc)
	case "unlinkp":
		return parseUnlinkP(args, loc)
	case "alias":
		return parseAlias(args, loc)
	case "unalias":
		return parseUnAlias(args, loc)
	case "start":
		return parseStart(args, loc)
	case "stop":
		return parseStop(args, loc)
	case "lock":
		return parseLock(args, loc)
	case "unlock":
		return parseUnlock(args, loc)
	case "unloadrt":
		return parseUnloadRT(args, loc)
	case "unloadusr":
		return parseUnloadUSR(args, loc)
	case "unload":
		return parseUnload(args, loc)
	case "waitusr":
		return parseWaitUSR(args, loc)
	case "list":
		return parseList(args, loc)
	case "show":
		return parseShow(args, loc)
	case "save":
		return parseSave(args, loc)
	case "status":
		return parseStatus(args, loc)
	case "debug":
		return parseDebug(args, loc)
	case "ptype":
		return parsePType(args, loc)
	case "stype":
		return parseSType(args, loc)
	case "echo":
		return parseEcho(args, loc)
	case "unecho":
		return parseUnEcho(args, loc)
	case "print":
		return parsePrint(args, loc)
	case "source":
		return Token{}, &ParseError{Loc: loc, Msg: "source: not a halcmd command (resolved at parse time)"}
	default:
		return Token{}, &ParseError{Loc: loc, Msg: fmt.Sprintf("unknown command: %q", tokens[0])}
	}
}

// SingleFileParser parses a single HAL file into a ParseResult. It handles
// template rendering, INI/ENV variable substitution, line continuation, and
// recursive source inclusion.
type SingleFileParser struct {
	ini          INILookup
	templateData *HalTemplateData
	resolver     PathResolver
	depth        int
	readFile     func(path string) (string, error) // injectable for testing
}

func (sp *SingleFileParser) readFileContent(path string) (string, error) {
	if sp.readFile != nil {
		return sp.readFile(path)
	}
	data, err := os.ReadFile(path)
	if err != nil {
		return "", err
	}
	return string(data), nil
}

// Parse reads and parses a HAL file, returning a classified ParseResult.
func (sp *SingleFileParser) Parse(path string) (*ParseResult, error) {
	if strings.HasSuffix(strings.ToLower(path), ".tcl") {
		return nil, &ParseError{Loc: SourceLoc{File: path, Line: 0}, Msg: fmt.Sprintf("source: %q is a Tcl file; use haltcl to execute Tcl HAL files", path)}
	}
	content, err := sp.readFileContent(path)
	if err != nil {
		return nil, err
	}

	// Template rendering (only when templateData is available)
	if strings.Contains(content, "{{") && sp.templateData != nil {
		content, err = RenderHalTemplate(path, content, sp.templateData)
		if err != nil {
			return nil, err
		}
	}

	// Join continuation lines before splitting
	content = joinContinuationLines(content)

	result := &ParseResult{}
	lines := strings.Split(content, "\n")

	for lineNum, line := range lines {
		loc := SourceLoc{File: path, Line: lineNum + 1}

		// Substitute INI and environment variables
		if sp.ini != nil {
			line = substituteVars(line, sp.ini)
		}

		// Tokenize the line
		tokens, tokErr := tokenizeLine(line)
		if tokErr != nil {
			return nil, &ParseError{Loc: loc, Msg: tokErr.Error()}
		}

		if len(tokens) == 0 {
			continue
		}

		// Handle "source" before parseLine (it is not a regular token)
		if strings.ToLower(tokens[0]) == "source" {
			if len(tokens) < 2 {
				return nil, &ParseError{Loc: loc, Msg: "source: missing file argument"}
			}
			if sp.depth >= 20 {
				return nil, &ParseError{Loc: loc, Msg: "source: maximum include depth exceeded"}
			}

			childPath := tokens[1]
			if sp.resolver != nil {
				childPath, err = sp.resolver.Resolve(tokens[1])
				if err != nil {
					return nil, &ParseError{Loc: loc, Msg: fmt.Sprintf("source: cannot resolve %q: %v", tokens[1], err)}
				}
			}

			child := &SingleFileParser{
				ini:          sp.ini,
				templateData: sp.templateData,
				resolver:     sp.resolver,
				depth:        sp.depth + 1,
				readFile:     sp.readFile,
			}
			childResult, parseErr := child.Parse(childPath)
			if parseErr != nil {
				return nil, parseErr
			}
			result.LoadRT = append(result.LoadRT, childResult.LoadRT...)
			result.LoadUSR = append(result.LoadUSR, childResult.LoadUSR...)
			result.HALCmd = append(result.HALCmd, childResult.HALCmd...)
			continue
		}

		// Parse the command line
		tok, parseErr := parseLine(tokens, loc)
		if parseErr != nil {
			return nil, parseErr
		}

		// Classify into the appropriate bucket
		switch d := tok.Data.(type) {
		case *LoadRTToken:
			result.LoadRT = append(result.LoadRT, tok)
		case *LoadUSRToken:
			if d.WaitReady || d.WaitName != "" {
				result.LoadUSR = append(result.LoadUSR, tok)
			} else {
				result.HALCmd = append(result.HALCmd, tok)
			}
		default:
			result.HALCmd = append(result.HALCmd, tok)
		}
	}

	return result, nil
}

// MultiFileParser parses multiple HAL files and merges their ParseResults.
type MultiFileParser struct {
	ini          INILookup
	templateData *HalTemplateData
	resolver     PathResolver
}

// Parse parses each file and returns a single merged ParseResult. The first
// error encountered stops processing and is returned immediately.
func (mp *MultiFileParser) Parse(paths []string) (*ParseResult, error) {
	result := &ParseResult{}
	for _, path := range paths {
		sp := &SingleFileParser{
			ini:          mp.ini,
			templateData: mp.templateData,
			resolver:     mp.resolver,
		}
		fileResult, err := sp.Parse(path)
		if err != nil {
			return nil, err
		}
		result.LoadRT = append(result.LoadRT, fileResult.LoadRT...)
		result.LoadUSR = append(result.LoadUSR, fileResult.LoadUSR...)
		result.HALCmd = append(result.HALCmd, fileResult.HALCmd...)
	}
	return result, nil
}
