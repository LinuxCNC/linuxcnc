//
// IniFile - Ini-file reader and query class
// Copyright (C) 2026 B.Stultiens
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
//
#include <filesystem>
#include <mutex>

#include <cerrno>
#include <cstring>
#include <fmt/format.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <locale.h>

// FIXME: we don't want to pull in libnml.so
//#include "libnml/rcs/rcs_print.hh"

#include "inifile.hh"

using namespace linuxcnc;

// FIXME:
// This should not be printed directly to stderr, although the previous ini
// reader did that. We also do not want to pull in libnml. A new consistent
// linuxcnc global print library with channels should be established instead.
#include <iostream>
static inline void print_msg(const std::string &str)
{
	std::cerr << str << std::endl;
	//rcs_print((str + "\n").c_str());
}

// Identifier characters
static const char STR_ID[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789";

namespace linuxcnc {
//
//*****************************************************************************
// IniFileTag: Encapsulate a tag of type 'name = value'
//*****************************************************************************
//
class IniFileTag
{
public:
	IniFileTag() {};
	IniFileTag(const std::string &p, unsigned l, size_t s, const std::string &t, const std::string &v)
		: path(p), lineno(l), secidx(s), tagname(t), tagvalue(v)
	{}

	std::string path;		// From which file it came
	unsigned lineno;		// Line number of TAG=...
	size_t secidx;			// Section index locator (index into 'sections')
	std::string tagname;
	std::string tagvalue;	// Content, everything after the '=', but trimmed
};

//
//*****************************************************************************
// IniFileSection: Encapsulate a section of type '[section]'
//*****************************************************************************
//
class IniFileSection
{
public:
	IniFileSection() {};
	IniFileSection(const std::string &p, unsigned l, size_t s, const std::string &n)
		: path(p), lineno(l), secidx(s), secname(n), tags{}
	{}

	std::string path;	// From which file it came
	unsigned lineno;	// Line number of [section]
	size_t secidx;		// Section index locator (for duplicates)
	std::string secname;
	// Note: tags in a section can become discontinuous when equally named
	// sections are merged. Therefore, we cannot depend on a start
	// reference/count and want to register them all.
	std::vector<size_t> tags;	// References of tags into IniFileContent::_tags in this section in-order
};

//
//*****************************************************************************
// IniFileContent: Ini-file section/tag content and parsing
//*****************************************************************************
//
class IniFileContent
{
public:
	IniFileContent(const std::string &path);

	IniFileContent(const IniFileContent &) = delete;
	IniFileContent& operator=(const IniFileContent &) = delete;

	operator bool() const { return _isvalid; }

	int tagCount() const { return _tags.size(); }
	int sectionCount() const { return _sections.size(); }

	std::optional<const IniFileTag *> getTagRef(int idx) const
		{ if(idx < 0 || idx >= tagCount()) return std::nullopt; else return &_tags[idx]; }

	std::optional<const IniFileSection *> getSectionRef(int idx) const
		{ if(idx < 0 || idx >= sectionCount()) return std::nullopt; else return &_sections[idx]; }

	std::optional<const IniFileSection *> getSectionRef(const std::string &s) const
		{ if(!hasSection(s)) return std::nullopt; else return &_sections[sectionIndex(s)]; }

private:
	bool hasSection(const std::string &s) const { return _sectionmap.find(s) != _sectionmap.end(); };
	size_t sectionIndex(const std::string &s) const { return _sectionmap.find(s)->second; };

	std::string _filepath;		// The ini-file path of the loaded file
	ssize_t _currentsection;	// Used during parsing
	bool _isvalid;				// Set to true on successful parse

	// Actual data container
	std::vector<IniFileSection> _sections;	// All section in sequence as found
	std::vector<IniFileTag> _tags;		// All tags in sequence as found

	// Map of sections by name indexing 'sections' vector
	std::map<std::string, size_t> _sectionmap;

	// File loader and parser functions
	bool readFile(const std::string &path, std::string &content);
	bool parseLine(const std::string &line, const std::string &path, unsigned linenr);
	bool processValue(std::string &value, const std::string &path, unsigned linenr);
	bool parseContent(const std::string &content, const std::string &path);

	const size_t MAX_INCLUDE_DEPTH = 16;	// Nesting more than 16 deep is an error
	bool pathPush(const std::string &path) { _pathstack.push_back(path); return _pathstack.size() < MAX_INCLUDE_DEPTH; }
	void pathPop() { if(!_pathstack.empty()) _pathstack.pop_back(); }
	bool pathIsLoading(const std::string &path) { for(auto const &s : _pathstack) { if(s == path) return true; } return false; } 
	std::vector<std::string> _pathstack;
};

}	// namespace linuxcnc

//
// The constructor is the top-level ini-file loader
//
IniFileContent::IniFileContent(const std::string &path) :
	_filepath(path),
	_currentsection(-1),
	_isvalid(false),
	_sections{},
	_tags{},
	_sectionmap{},
	_pathstack{}
{
	std::string content;
	if(!readFile(path, content))
		return;

	// We now have the complete file read in 'content'
	pathPush(path);
	bool rv = parseContent(content, path);
	pathPop();

	// Should not happen. Just checking...
	if(!_pathstack.empty()) {
		print_msg(fmt::format("{}: internal error: Include stack level stuck at {} and should be 0", path, _pathstack.size()));
		return;
	}
	_isvalid = rv;
}

//
// Convert a hex digit string into its value
//
static bool fromHex(const std::string &str, uint32_t &val)
{
	val = 0;
	for(auto ch : str) {
		if(!std::isxdigit(ch & 0xff))
			return false;
		unsigned cval;
		if(std::isdigit(ch))
			cval = ch - '0';
		else
			cval = std::toupper(ch) - 'A' + 10;
		val <<= 4;
		val += cval;
	}
	return true;
}

//
// Convert a Unicode code point into a UTF-8 string
//
static std::string toUTF8(int32_t v)
{
	if(v <= 0x7f) {
		return std::string(1, (char)v);
	}
	if(v <= 0x7ff) {
		std::string seq;
		seq += (char)( (v >>  6) | 0xc0);
		seq += (char)((v & 0x3f) | 0x80);
		return seq;
	}
	if(v <= 0xffff) {
		std::string seq;
		seq += (char)( (v >> 12) | 0xe0);
		seq += (char)(((v >>  6) & 0x3f) | 0x80);
		seq += (char)(((v >>  0) & 0x3f) | 0x80);
		return seq;
	}
	if(v <= 0x10ffff) {
		std::string seq;
		seq += (char)( (v >> 18) | 0xf0);
		seq += (char)(((v >> 12) & 0x3f) | 0x80);
		seq += (char)(((v >>  6) & 0x3f) | 0x80);
		seq += (char)(((v >>  0) & 0x3f) | 0x80);
		return seq;
	}
	// This is an invalid code point. Just ignore.
	return std::string{};
}

//
// Scan the string and determine UTF-8 validity by finding the correct amount
// of high-bit sequences and code point validity. Detects overlong encodings,
// UTF-16 surrogates and code points out-of-range.
//
static bool isValidUTF8(const std::string &s)
{
#define C(i, m)	((s[i]) & (m))
#define M(i)	((s[i]) & 0xff)

	for(size_t i = 0; i < s.size(); i++) {
		size_t n;
		if(C(i, 0x80) == 0x00) {	// 0b0bbbbbbb
			n = 0;	// 0x00-0x7f
		} else if(C(i, 0xe0) == 0xc0) {	// 0b110bbbbb
			if(C(i, 0xfe) == 0xc0)	// 0xc0 and 0xc1
				return false;	// Overlong encoding, overlaps 0x00-0x7f
			n = 1;	// 0xc0-0xdf -> 0x000080-0x0007ff
		} else if(M(i) == 0xed && i+1 < s.size() && M(i+1) >= 0xa0) {
			return false;	// 0xd800-0xdfff (UTF-16 surrogates)
		} else if(C(i, 0xf0) == 0xe0) {	// 0b1110bbbb
			if(M(i) == 0xe0 && i+1 < s.size() && M(i+1) < 0xa0)
				return false;	// Overlong encoding, overlaps 0x000080-0x0007ff
			n = 2;	// 0xe0-0xef -> 0x000800-0x00ffff
		} else if(C(i, 0xf8) == 0xf0) {	// 0b11110bbb
			if(M(i) == 0xf0 && i+1 < s.size() && M(i+1) < 0x90)
				return false; // Overlong encoding, overlaps 0x000800-0x00ffff
			if((M(i) > 0xf4) || (M(i) == 0xf4 && i+1 < s.size() && M(i+1) >= 0x90))
				return false;	// Results in invalid code points 0x110000-0x1fffff
			n = 3;	// 0xf0-0xf7 -> 0x010000-0x10ffff
		} else {	// 0b11111xxx
			return false;	// 5/6 byte UTF-8 not supported
		}
		// Test if there are enough bytes following
		for(size_t j = 0; j < n && i < s.size(); j++) {
			if(++i >= s.size() || C(i, 0xc0) != 0x80)
				return false;	// No more chars available or not 0b10bbbbbb
		}
	}
	return true;

#undef M
#undef C
}

//
// Process a value to find strings
// Modifies the 'value' argument to its final version
// Embedded quotes are no special characters and are copied verbatim to the
// output. Comments character # and ; are not special in the value and also
// copied verbatim.
// Escape substitution is performed on the entire value. Embedding characters
// with \ooo or \xHH escapes may result in invalid UTF-8 strings but that is
// checked later. Possible escapes:
// - \[abfnrtv]         Standard C-type escapes
// - \[0-3][0-7]{0,2}   Standard C-type octal escape
// - \x[a-fA-F0-9]{2}   Hex 8-bit character escape
// - \u[a-fA-F0-9]{4}   UTF-16 escape (with surrogate support)
// - \U[a-fA-F0-9]{8}   UTF-32 escape
//
// UTF-16 surrogate support checks high/low surrogate presence and code points
// are converted into UTF-8. UTF-32 code points checked against the valid range
// (code <= 0x10ffff) and converted into UTF-8.
// Embedded NUL characters (\0) are not allowed and flagged as an error.
//
bool IniFileContent::processValue(std::string &value, const std::string &path, unsigned linenr)
{
	if(value.empty())
		return true;

	// We should be l/r trimmed getting here.

	std::string realstr;
	for(size_t pos = 0; pos < value.size(); pos++) {
		uint32_t hexval;

		char ch = value[pos];
		if(0 == ch) {
			// Trying to add a NUL char
			print_msg(fmt::format("{}:{}: error: Embedded literal NUL character not supported", path, linenr));
			return false;
		}

		// Check for an escape
		if('\\' == ch) {
			// An escape, see what follows
			size_t cleft = value.size() - pos - 1; // Nr of characters after the '\'
			if(cleft < 1) {
				// Last char is an escape
				// This should never happen because it would have been seen
				// as a continuation. The error happens when it fails to
				// detect the end quote.
				print_msg(fmt::format("{}:{}: internal error: End of line while parsing '\\' escape", path, linenr));
				return false;
			}
			auto nch =  value[pos+1];	// The letter after the escape
			switch(nch) {
			case 'a': realstr += '\a'; break;
			case 'b': realstr += '\b'; break;
			case 'f': realstr += '\f'; break;
			case 'n': realstr += '\n'; break;
			case 'r': realstr += '\r'; break;
			case 't': realstr += '\t'; break;
			case 'v': realstr += '\v'; break;
			case '\\': realstr += '\\'; break;
			case '0': // Octal value \0 ... \377
			case '1':
			case '2':
			case '3':
				// Have at least \o, can have \oo or \ooo
				hexval = nch - '0';
				if(cleft >= 2 && value[pos+2] >= '0' && value[pos+2] <= '7') {
					// Have at least \oo, can have \ooo
					hexval <<= 3;
					hexval += value[pos+2] - '0';
					if(cleft >= 3 && value[pos+3] >= '0' && value[pos+3] <= '7') {
						// Have \ooo
						hexval <<= 3;
						hexval += value[pos+3] - '0';
						pos++; // Eat the third digit
					}
					pos++; // Eat the second digit
				}
				if(!hexval) {
					// Trying to embed a NUL char
					print_msg(fmt::format("{}:{}: error: Embedded octal NUL character not supported", path, linenr));
					return false;
				}
				// Note that this can result in invalid UTF-8, but we don't care here
				realstr += (char)hexval;
				break;
			case 'x': // Hex value \xHH
				if(cleft < 3) {
					// Need at least 3 chars for xHH
					print_msg(fmt::format("{}:{}: error: Improper hex escape", path, linenr));
					return false;
				}
				if(!fromHex(value.substr(pos+2, 2), hexval)) {
					// Invalid hex number
					print_msg(fmt::format("{}:{}: error: Invalid hex '{}' in hex escape",
									path, linenr, value.substr(pos+2, 2)));
					return false;
				}
				if(!hexval) {
					// Trying to embed a NUL char
					print_msg(fmt::format("{}:{}: error: Embedded hex NUL character not supported", path, linenr));
					return false;
				}
				// Note that this can result in invalid UTF-8, but we don't care here
				realstr += (char)hexval;
				pos += 2; // Eat the xHH characters
				break;
			case 'u': // 16-bit hex \uXXXX (actually, this is an UTF-16 abomination)
				if(cleft < 5) {
					// Need at least 5 chars for uXXXX
					print_msg(fmt::format("{}:{}: error: Improper UTF-16 escape", path, linenr));
					return false;
				}
				if(!fromHex(value.substr(pos+2, 4), hexval)) {
					// Invalid hex number
					print_msg(fmt::format("{}:{}: error: Invalid hex value '{}' in UTF-16 escape",
									path, linenr, value.substr(pos+2, 4)));
					return false;
				}
				if(!hexval) {
					// Trying to embed a NUL char
					print_msg(fmt::format("{}:{}: error: Embedded UTF-16 NUL character not supported", path, linenr));
					return false;
				}
				if(hexval >= 0xd800 && hexval <= 0xdfff) {
					// We're in UTF-16 surrogate wonderland
					if(hexval >= 0xdc00) {
						// The high surrogate must be 0xd800..0xdbff
						print_msg(fmt::format("{}:{}: error: Invalid high surrogate value u{:04X} in UTF-16",
											path, linenr, hexval));
						return false;
					}
					if(cleft < 5 + 6 || '\\' != value[pos+6] || 'u' != value[pos+7]) {
						// We need to have room for the second surrogate: uXXXX\uYYYY
						print_msg(fmt::format("{}:{}: error: Missing low surrogate in UTF-16",
											path, linenr, pos));
						return false;
					}
					uint32_t hexsur;
					if(!fromHex(value.substr(pos+8, 4), hexsur)) {
						// Invalid hex number
						print_msg(fmt::format("{}:{}: error: Invalid hex '{}' in UTF-16 low surrogate escape",
										path, linenr, value.substr(pos+8, 4)));
						return false;
					}
					if(hexsur < 0xdc00 || hexsur > 0xdfff) {
						// The low surrogate must be 0xdc00..0xdfff
						print_msg(fmt::format("{}:{}: error: Invalid low surrogate value u{:04X} in UTF-16",
											path, linenr, hexsur));
						return false;
					}
					hexval = 0x10000 + ((hexval & 0x03ff) << 10) + (hexsur & 0x3ff);
					pos += 6; // Eat the entire low surrogate \uYYYY
				}
				realstr += toUTF8(hexval);
				pos += 4; // Eat the XXXX characters
				break;
			case 'U': // 32-bit hex \UXXXXXXXX (UTF-32)
				if(cleft < 9) {
					// Need at least 9 chars for UXXXXXXXX
					print_msg(fmt::format("{}:{}: error: Improper UTF-32 escape", path, linenr));
					return false;
				}
				if(!fromHex(value.substr(pos+2, 8), hexval)) {
					// Invalid hex number
					print_msg(fmt::format("{}:{}: error: Invalid hex value '{}' in UTF-32 escape",
									path, linenr, value.substr(pos+2, 8)));
					return false;
				}
				if(!hexval) {
					// Trying to embed a NUL char
					print_msg(fmt::format("{}:{}: error: Embedded UTF-32 NUL character not supported", path, linenr));
					return false;
				}
				if(hexval > 0x10ffff) {
					// Invalid code point
					print_msg(fmt::format("{}:{}: error: Invalid code point U{:08X} in UTF-32 escape",
									path, linenr, hexval));
					return false;
				}
				realstr += toUTF8(hexval);
				pos += 8; // Eat the XXXXXXXX characters
				break;
			default:
				print_msg(fmt::format("{}:{}: warning: Improper escape of '{}', ignored", path, linenr, nch));
				realstr += '\\';	// Just add the literal backslash
				realstr += nch;
				break;
			}
			pos++;	// Eat the escape char
		} else {
			// Not escaped, just copy
			realstr += ch;
		}
	}

	value = realstr;
	return true;
}

bool IniFileContent::parseLine(const std::string &line, const std::string &path, unsigned linenr)
{
	// Handle include files
	if(line.starts_with("#INCLUDE")) {
		// Must have a blank after the #INCLUDE directive
		if(line.size() <= 8 || !(' ' == line[8] || '\t' == line[8])) {
			print_msg(fmt::format("{}:{}: error: Missing filename after #INCLUDE", path, linenr));
			return false;
		}
		size_t start = line.find_first_not_of(IniFile::STR_WS, 8);	// Skip whitespace after #INCLUDE
		// #INCLUDE  myname.inc
		//           ^
		//         start
		if(std::string::npos == start) {
			// This should not be possible. If npos, then we skipped all
			// whitespace and had whitespace left? Should have detected some
			// character because the line is right trimmed before we get here.
			print_msg(fmt::format("{}:{}: internal error: Missing filename after #INCLUDE", path, linenr));
			return false;
		}
		std::string arg = line.substr(start);
		IniFile::rtrim(arg);
		std::string fname;
		if(IniFile::tildeExpand(arg, fname)) {
			print_msg(fmt::format("{}:{}: error: Failed to tilde-expand '{}'", path, linenr, arg));
			return false;
		}

		// Loading from a subdir means that the includes are relative to it too
		size_t pslash = path.find_last_of('/');
		size_t fslash = fname.find_first_of('/');
		if(0 != fslash && std::string::npos != pslash) {
			// We have no absolute path in the include filename but we have a
			// '/' in the current path. Use the dirname of the current path to
			// prepend to the new file to ensure they come from the same
			// directory.
			fname.insert(0, path.substr(0, pslash + 1));
		}

		// Check the file loading stack if this is being processed to detect
		// infinite include loops
		if(pathIsLoading(fname)) {
			print_msg(fmt::format("{}:{}: error: Include file recursion on loading '{}'", path, linenr, fname));
			return false;
		}
		std::string content;
		if(!readFile(fname, content))
			return false;	// Has already printed any message

		// Recurse loading
		if(!pathPush(fname)) {
			print_msg(fmt::format("{}:{}: error: Maximum include depth ({}) reached on loading '{}'",
						path, linenr, MAX_INCLUDE_DEPTH, fname));
			return false;
		}
		bool rv = parseContent(content, fname);
		pathPop();
		return rv;
	}

	size_t start = line.find_first_not_of(IniFile::STR_WS);
	if(std::string::npos == start) {
		// Empty line (only white space)
		return true;
	}
	if('#' == line[start] || ';' == line[start]) {
		// Comment until end-of-line
		return true;
	}
	if('[' == line[start]) {
		// Section header
		//    [ SECTION ]
		//    ^
		//  start
		size_t end = line.find_first_of("]", start);
		if(std::string::npos == end) {
			// note: end cannot be 0 because that is occupied by '['
			print_msg(fmt::format("{}:{}: error: Invalid section. Missing ']'", path, linenr));
			return false;
		}
		// Section header
		//    [ SECTION ]
		//    ^         ^
		//  start      end
		std::string sect = line.substr(start+1, end - start - 1);
		IniFile::trim(sect);
		if(sect.empty()) {
			// This happens on '[]' or '[ ]', when no section ID is present
			print_msg(fmt::format("{}:{}: error: Invalid section. No content between '[' and ']'", path, linenr));
			return false;
		}
		if(std::string::npos != sect.find_first_not_of(STR_ID)) {
			// There are non ID characters in the section identifier
			print_msg(fmt::format("{}:{}: error: Invalid section '{}'. Identifier contains invalid character(s)",
							path, linenr, sect));
			return false;
		}
		if(std::isdigit(sect[0] & 0xff)) {
			print_msg(fmt::format("{}:{}: error: Invalid section '{}'. Cannot start with a digit", path, linenr, sect));
			return false;
		}

		if(_sectionmap.find(sect) != _sectionmap.end()) {
			print_msg(fmt::format("{}:{}: warning: Section '{}' already exists. Merging...", path, linenr, sect));
			_currentsection = _sections[_sectionmap[sect]].secidx;
		} else {
			_sections.push_back(IniFileSection(path, linenr, _sections.size(), sect));
			_sectionmap[sect] = _sections.size() - 1;
			_currentsection = _sections.size() - 1;
		}

		// We ignore everything on the line that follows the [section] marker
		// But we want to warn...
		start = line.find_first_not_of(IniFile::STR_WS, end+1);
		if(std::string::npos != start && !('#' == line[start] || ';' == line[start])) {
			print_msg(fmt::format("{}:{}: warning: Section header has trailing content, ignored", path, linenr));
		}
		return true;
	}

	// We must have a tag when we get here. Well, actually, we have a
	// non-whitespace character, which should be a tag.
	//
	//   TAG  =  whatever
	//   ^
	// start
	size_t end = line.find_first_of('=', start);
	if(std::string::npos == end) {
		// Happens on lines like "VAR"
		print_msg(fmt::format("{}:{}: error: Invalid tag '{}'. Expected '=' after tag identifier",
						path, linenr, line.substr(start)));
		return false;
	}
	if(end == start) {
		// Happens on lines like "= value"
		print_msg(fmt::format("{}:{}: error: Invalid tag. Missing identifier before '='", path, linenr));
		return false;
	}
	//
	//   TAG  =  whatever
	//   ^    ^
	// start end
	std::string tag = line.substr(start, end - start);
	IniFile::rtrim(tag);
	if(std::string::npos != tag.find_first_not_of(STR_ID)) {
		// There are non ID characters in the tag identifier
		print_msg(fmt::format("{}:{}: error: Invalid tag name '{}'. Identifier contains invalid character(s)",
						path, linenr, tag));
		return false;
	}
	if(std::isdigit(tag[0] & 0xff)) {
		print_msg(fmt::format("{}:{}: error: Invalid tag '{}'. Tag identifiers cannot start with a digit", path, linenr, tag));
		return false;
	}

	start = line.find_first_not_of(IniFile::STR_WS, end);	// Skip whitespace before '='
	if(std::string::npos == start || '=' != line[start]) {
		// Happens on lines like "VAR x =..."
		print_msg(fmt::format("{}:{}: error: Invalid tag '{}'. Expected '=' after tag identifier", path, linenr, tag));
		return false;
	}

	if(_currentsection < 0) {
		print_msg(fmt::format("{}:{}: error: Tag '{}' found without prior section definition", path, linenr, tag));
		return false;
	}

	//   TAG = whatever
	//       ^
	//      end
	start = line.find_first_not_of(IniFile::STR_WS, end + 1);	// Skip whitespace after '='
	//   TAG = whatever
	//         ^
	//       start
	std::string value;
	if(std::string::npos != start) {
		// There was something on the line after '='
		// Copy and rtrim whitespace
		value = line.substr(start);
		IniFile::rtrim(value);
	} // else there was no content after the '='

	// The 'value' we have now should be left/right trimmed and may be empty.

	// Handle strings and comments
	if(!processValue(value, path, linenr))
		return false;

	if(!isValidUTF8(value)) {
		print_msg(fmt::format("{}:{}: error: Value contains invalid UTF-8 sequence", path, linenr));
		return false;
	}

	// Add the tag
	_tags.push_back(IniFileTag(path, linenr, (unsigned)_currentsection, tag, value));
	// Add to the section index
	_sections[_currentsection].tags.push_back(_tags.size() - 1);
	return true;
}

bool IniFileContent::parseContent(const std::string &content, const std::string &path)
{
	// Split into lines and parse them
	unsigned linenr = 1;
	std::string line;
	for(size_t pos = 0; pos < content.size();) {
		// Search the newline to find a line
		size_t eol = content.find_first_of("\n", pos);
		if(std::string::npos == eol || eol == content.size()-1) {
			// This is EOF with or without terminating newline
			line += content.substr(pos);
			IniFile::rtrim(line);
			if(line[line.size()-1] == '\\') {
				// This is a continuation on the last line and there are no
				// lines left after this data. A clear error.
				print_msg(fmt::format("{}:{}: error: Last line has a continuation", path, linenr));
				return false;
			}
			return parseLine(line, path, linenr);
		}

		// \n this line\r\n
		//   ^           ^
		//  pos         eol
		// Copy line, without newline
		std::string thisline = content.substr(pos, eol - pos);
		// Remove trailing whitespace, including possible \r. Note that using
		// rtrim here also allows for additional spaces after a continuation,
		// but it should be fine to be tolerant and makes it easier for us.
		IniFile::rtrim(thisline);

		if(!thisline.empty() && thisline[thisline.size()-1] == '\\') {
			// Continuation
			thisline.pop_back();	// Remove trailing '\\'
			line += thisline;		// Add to current line
			pos = eol + 1;			// Prep for next line
			// This will cause continued lines to be counted by the last
			// non-continued line. But else we need to do double tracking.
			linenr++;
			continue;
		} else {
			line += thisline;
		}

		// Not a continuation, we have a whole line to process
		if(!parseLine(line, path, linenr)) {
			return false;	// Message already printed
		}
		linenr++;
		line.clear();
		pos = eol + 1;
	}
	return true;
}

bool IniFileContent::readFile(const std::string &path, std::string &content)
{
	// We read in using syscall/C into the string for speed
	int fd = ::open(path.c_str(), O_RDONLY);
	if(fd < 0) {
		print_msg(fmt::format("{}: error: Cannot open ini-file (errno={} ({}))", path, errno, strerror(errno)));
		return false;
	}

	struct stat sb;
	if(fstat(fd, &sb) < 0) {
		print_msg(fmt::format("{}: error: Cannot stat ini-file (errno={} ({}))", path, errno, strerror(errno)));
		return false;
	}

	content.resize(sb.st_size);	// Make sure we have room

	ssize_t err;
retry_read:
	err = ::read(fd, content.data(), sb.st_size);
	if(err < 0) {
		if(errno == EINTR)
			goto retry_read;
		::close(fd);
		print_msg(fmt::format("{}: error: Cannot read ini-file (errno={} ({}))", path, errno, strerror(errno)));
		return false;
	}
	::close(fd);
	if(err != (ssize_t)sb.st_size) {
		print_msg(fmt::format("{}: error: Cannot read complete ini-file. Data read ({}) != file size ({})",
				path, err, sb.st_size));
		return false; // Couldn't read it all
	}
	return true;
}

//
//*****************************************************************************
// IniFileCache: caching singleton
//*****************************************************************************
//
namespace linuxcnc {

class IniFileCache
{
private:
	IniFileCache() {};
	~IniFileCache();

	IniFileCache(const IniFileCache &) = delete;
	IniFileCache &operator=(const IniFileCache&) = delete;

	static IniFileCache &getInstance() {
		static IniFileCache inst;
		return inst;
	};

public:
	static const IniFileContent *getIniFile(const std::string &path);

private:
	// Note that the content are pointers because users can hold a pointer when
	// they instantiate IniFile(). That pointer must remain valid throughout
	// the lifetime of the IniFile instance and we cannot control that.
	// When another thread opens a new file, then the map internals can get
	// reallocated and that would invalidate pointers to map entries.
	std::map<std::string, IniFileContent *> cache;

	// Only one thread can load a file. Otherwise they both could load the same
	// one if we get unlucky.
	std::mutex locker;
};

} // namespace linuxcnc

IniFileCache::~IniFileCache()
{
	// Release all cached objects from the map
	for (auto &kv : cache) {
		delete kv.second;
		kv.second = nullptr;
	}
	// The map will subsequently deconstruct itself
}

const IniFileContent *IniFileCache::getIniFile(const std::string &path)
{
	IniFileCache &ifc = IniFileCache::getInstance();
	std::lock_guard<std::mutex> lck(ifc.locker);

	auto c = ifc.cache.find(path);

	if(c != ifc.cache.end()) {
		// We have a cached version
		return c->second;
	} else {
		// We don't have it in our cache, load it
		IniFileContent *x = new IniFileContent(path);
		if(!*x) {
			// Load resulted in an error. Get rid of it again.
			delete x;
			return nullptr;
		}
		// Got it loaded. Now add to the cache and return it
		ifc.cache[path] = x;
		return x;
	}
}

//
//*****************************************************************************
// IniFile class
//*****************************************************************************
//
IniFile::IniFile(const std::string &path)
	: _inifilecontent(nullptr),
	  _filepath{}
{
	Open(path);
}

bool IniFile::Open(const std::string &path)
{
	Close();
	if(auto f = IniFileCache::getIniFile(path)) {
		_inifilecontent = f;
		_filepath = path;
		return true;
	}
	return false;
}

bool IniFile::hasOpenError(const std::string &tag, const std::string &section) const
{
	if(!isOpen()) {
		print_msg(fmt::format("{}: error: Attempt to extract '[{}]{}' from invalid or not loaded ini-file.",
							_filepath, section, tag));
		return false;
	}
	return true;
}

//
// Find all the tags with name 'tag' in (optional) section 'section' and return
// references as a vector. The std::nullopt value is returned upon error.
//
std::optional<std::vector<const IniFileTag *>> IniFile::findTags(const std::string &tag, const std::string &section) const
{
	if(!hasOpenError(tag, section))
		return std::nullopt;

	std::vector<const IniFileTag *> tags;

	if(tag.empty() && section.empty()) {
		// This returns all tags from all sections in sequence
		for(size_t i = 0; true; i++) {
			if(auto t = _inifilecontent->getTagRef(i)) {
				tags.push_back(*t);
			} else {
				break;
			}
		}
		return tags;
	}

	if(tag.empty()) {
		// Return all the section's values
		if(auto sect = _inifilecontent->getSectionRef(section)) {
			// Within the section, pick all tags
			for(auto t : (*sect)->tags) {	// Linked by index in master _tags
				if(auto tp = _inifilecontent->getTagRef(t)) {
					tags.push_back(*tp);
				}
			}
		}
		return tags;
	}

	if(section.empty()) {
		// Return all values matching tag's name
		// Loop over all tags and find those with the proper name
		for(size_t i = 0; true; i++) {
			if(auto t = _inifilecontent->getTagRef(i)) {
				if((*t)->tagname == tag)
					tags.push_back(*t);
			} else {
				// We get a std::nullopt when the list is done
				// Return what we gathered
				return tags;
			}
		}
	}

	// Find the section
	if(auto sect = _inifilecontent->getSectionRef(section)) {
		// Within the section, pick the all tags of the requested name
		for(auto t : (*sect)->tags) {	// Linked by index in master _tags
			if(auto tp = _inifilecontent->getTagRef(t)) {
				if((*tp)->tagname == tag) {
					tags.push_back(*tp);
				}
			}
		}
	}
	return tags;
}

//
// Find the num'th 'tag' in (optional) section 'section' and return a reference
// to that tag. The std::nullopt is returned upon error or if the tag is not
// found.
//
std::optional<const IniFileTag *> IniFile::findTag(const std::string &tag, const std::string &section, int num) const
{
	if(!hasOpenError(tag, section))
		return std::nullopt;

	if(section.empty()) {
		// Must have a count if no section
		if(num < 1)
			return std::nullopt;
		// Loop over all tags and find those with the proper name
		// Empty tag means any (so take the num'th).
		for(size_t i = 0; true; i++) {
			if(auto t = _inifilecontent->getTagRef(i)) {
				if((tag.empty() || (*t)->tagname == tag) && !--num)
					return *t;
			} else {
				return std::nullopt;
			}
		}
	}

	if(num < 1)
		num = 1;
	// Find the section
	if(auto sect = _inifilecontent->getSectionRef(section)) {
		// Within the section, pick the num'th tag of the requested name
		// With an empty tag just take the num'th
		for(auto t : (*sect)->tags) {
			if(auto tp = _inifilecontent->getTagRef(t)) {
				if((tag.empty() || (*tp)->tagname == tag) && !--num) {
					return *tp;
				}
			}
		}
	}

	return std::nullopt;
}

//
// Find 'section' and return a reference to it. The std::nullopt is returned
// upon error or if the section is not found.
// Empty section names are *not* valid.
//
std::optional<const IniFileSection *> IniFile::findSection(const std::string &section) const
{
	if(section.empty() || !hasOpenError({}, section))
		return std::nullopt;

	return _inifilecontent->getSectionRef(section);
}

//
// Return the path and line number of a specified tag or empty/-1 if not found
//
std::pair<std::string,int> IniFile::lineOf(int num, const std::string &tag, const std::string &section) const
{
	if(auto t = findTag(tag, section, num))
		return {(*t)->path, (*t)->lineno};
	return {{}, -1};
}

//
// Helper to get the section name from a tag
//
std::string IniFile::sectionFromTag(const IniFileTag *val) const
{
		if(auto sect = _inifilecontent->getSectionRef(val->secidx))
			return (*sect)->secname;
		return "<?unknown?>";
}

//
// Standard conversion routines
// bool - TRUE/YES/1/ON and FALSE/NO/0/OFF (case insensitive)
// s64  - signed value with optional sign and radix prefix
// u64  - signed value with optional sign and radix prefix
// real - floating point value with optional sign and exponent
//
// These routines return std::nullopt when a conversion fails and an
// appropriate message is emitted.
//
std::optional<bool> IniFile::convertBool(const std::string &val)
{
	static const std::map<const std::string, const bool, IniFile::caseless> booleanMap = {
		{ "true",  true },
		{ "yes",   true },
		{ "1",     true },
		{ "on",    true },
		{ "false", false },
		{ "no",    false },
		{ "0",     false },
		{ "off",   false },
	};
	auto const b = booleanMap.find(val);	// Case-insensitive map search
	if(b != booleanMap.end())
		return b->second;
	return std::nullopt;
}

std::optional<bool> IniFile::convertBool(const IniFileTag *val) const
{
	if(auto b = IniFile::convertBool(val->tagvalue))
		return *b;

	print_msg(fmt::format("{}:{}: error: Invalid boolean value [{}]{}='{}'",
		val->path, val->lineno, sectionFromTag(val), val->tagname, val->tagvalue));

	return std::nullopt;
}

//
// Helper to get different radix numbers parsed properly
// Supported:
// * [+-]?[0-9]+             Decimal
// * [+-]?0[xX][0-9a-fA-F]+  Hexadecimal
// * [+-]?0[oO][0-7]+        Octal
// * [+-]?0[bB][0-1]+        Binary
//
static int radixAndPrefix(std::string &val)
{
	unsigned pm = 0;	// Default no +/-

	int base = 10;	// Default to decimal

	// String size must be 3 or more for alternate base values. Two for the
	// prefix and at least one digit. A leading +/- may also be present.
	// We know that the value from the tag has the leading whitespace removed.
	if(val.size() > 1 && ('-' == val[0] || '+' == val[0])) {
			pm = 1;
	}

	// Detect: [+-]?0[xXoObB].
	if(val.size() > pm+2 && '0' == val[pm]) {
		// Set the radix and remove the prefix
		switch(val[pm+1]) {
		case 'x': case 'X': base = 16; val.erase(pm, 2); break;
		case 'o': case 'O': base =  8; val.erase(pm, 2); break;
		case 'b': case 'B': base =  2; val.erase(pm, 2); break;
		}
	}
	return base;
}


std::optional<rtapi_s64> IniFile::convertSInt(const std::string &_val)
{
	std::string tval = _val;
	int base = radixAndPrefix(tval);
	char *eptr;

	// Make sure we always use the C locale for conversion (thread local)
	locale_t olc = uselocale(static_cast<locale_t>(0));
	locale_t nlc = newlocale(LC_NUMERIC_MASK, "C", static_cast<locale_t>(0));
	if(static_cast<locale_t>(0) == nlc) {
		print_msg("internal error: ConvertSInt(): Cannot set locale to \"C\" for strtoll");
		return std::nullopt;
	}
	uselocale(nlc);
	errno = 0;
	rtapi_s64 i = strtoll(tval.c_str(), &eptr, base);
	int errnosave = errno;
	uselocale(olc);
	freelocale(nlc);

	if(eptr == tval.c_str() || errnosave != 0) {
		return std::nullopt;
	}
	if(*eptr && !IniFile::isSpace(*eptr)) {
		print_msg(fmt::format("warning: Trailing character(s) in signed integer conversion of '{}'", tval));
	}
	return i;
}

std::optional<rtapi_s64> IniFile::convertSInt(const IniFileTag *val) const
{
	std::string tval = val->tagvalue;
	int base = radixAndPrefix(tval);
	char *eptr;

	// Make sure we always use the C locale for conversion (thread local)
	locale_t olc = uselocale(static_cast<locale_t>(0));
	locale_t nlc = newlocale(LC_NUMERIC_MASK, "C", static_cast<locale_t>(0));
	if(static_cast<locale_t>(0) == nlc) {
		print_msg(fmt::format("{}:{}: internal error: Cannot set locale to \"C\" for strtoll", val->path, val->lineno));
		return std::nullopt;
	}
	uselocale(nlc);
	errno = 0;
	rtapi_s64 i = strtoll(tval.c_str(), &eptr, base);
	int errnosave = errno;
	uselocale(olc);
	freelocale(nlc);

	if(eptr == tval.c_str() || errnosave != 0) {
		print_msg(fmt::format("{}:{}: error: Invalid signed integer [{}]{}='{}'",
			val->path, val->lineno, sectionFromTag(val), val->tagname, tval));
		return std::nullopt;
	}
	if(*eptr && !IniFile::isSpace(*eptr)) {
		print_msg(fmt::format("{}:{}: warning: Trailing character(s) in signed integer conversion ([{}]{}='{}')",
			val->path, val->lineno, sectionFromTag(val), val->tagname, tval));
	}
	return i;
}

std::optional<rtapi_u64> IniFile::convertUInt(const std::string &_val)
{
	std::string tval = IniFile::trimcpy(_val);
	int base = radixAndPrefix(tval);
	char *eptr;

	if(!tval.empty() && tval[0] == '-') {
		print_msg(fmt::format("warning: Unsigned integer conversion detected a leading minus sign (-)", tval));
	}

	// Make sure we always use the C locale for conversion (thread local)
	locale_t olc = uselocale(static_cast<locale_t>(0));
	locale_t nlc = newlocale(LC_NUMERIC_MASK, "C", static_cast<locale_t>(0));
	if(static_cast<locale_t>(0) == nlc) {
		print_msg("internal error: ConvertUInt(): Cannot set locale to \"C\" for strtoull");
		return std::nullopt;
	}
	uselocale(nlc);
	errno = 0;
	rtapi_u64 u = strtoull(tval.c_str(), &eptr, base);
	int errnosave = errno;
	uselocale(olc);
	freelocale(nlc);

	if(eptr == tval.c_str() || errnosave != 0) {
		return std::nullopt;
	}
	if(*eptr && !IniFile::isSpace(*eptr)) {
		print_msg(fmt::format("warning: Trailing character(s) in unsigned integer conversion of '{}')", tval));
	}
	return u;
}

std::optional<rtapi_u64> IniFile::convertUInt(const IniFileTag *val) const
{
	std::string tval = val->tagvalue;
	int base = radixAndPrefix(tval);
	char *eptr;

	if(!tval.empty() && tval[0] == '-') {
		print_msg(fmt::format("{}:{}: warning: Unsigned integer conversion detected a leading minus sign (-)",
				val->path, val->lineno, tval));
	}

	// Make sure we always use the C locale for conversion (thread local)
	locale_t olc = uselocale(static_cast<locale_t>(0));
	locale_t nlc = newlocale(LC_NUMERIC_MASK, "C", static_cast<locale_t>(0));
	if(static_cast<locale_t>(0) == nlc) {
		print_msg(fmt::format("{}:{}: internal error: Cannot set locale to \"C\" for strtoull", val->path, val->lineno));
		return std::nullopt;
	}
	uselocale(nlc);
	errno = 0;
	rtapi_u64 u = strtoull(tval.c_str(), &eptr, base);
	int errnosave = errno;
	uselocale(olc);
	freelocale(nlc);

	if(eptr == tval.c_str() || errnosave != 0) {
		print_msg(fmt::format("{}:{}: error: Invalid unsigned integer [{}]{}='{}'",
			val->path, val->lineno, sectionFromTag(val), val->tagname, tval));
		return std::nullopt;
	}
	if(*eptr && !IniFile::isSpace(*eptr)) {
		print_msg(fmt::format("{}:{}: warning: Trailing character(s) in unsigned integer conversion ([{}]{}='{}')",
			val->path, val->lineno, sectionFromTag(val), val->tagname, tval));
	}
	return u;
}

std::optional<double> IniFile::convertReal(const std::string &val)
{
	char *eptr;

	// Make sure we always use the C locale for conversion (thread local)
	locale_t olc = uselocale(static_cast<locale_t>(0));
	locale_t nlc = newlocale(LC_NUMERIC_MASK, "C", static_cast<locale_t>(0));
	if(static_cast<locale_t>(0) == nlc) {
		print_msg("internal error: ConvertReal(): Cannot set locale to \"C\" for strtod");
		return std::nullopt;
	}
	uselocale(nlc);
	errno = 0;
	double r = strtod(val.c_str(), &eptr);
	int errnosave = errno;
	uselocale(olc);
	freelocale(nlc);

	if(eptr == val.c_str() || errnosave != 0) {
		return std::nullopt;
	}
	if(*eptr && !IniFile::isSpace(*eptr)) {
		print_msg(fmt::format("warning: Trailing character(s) in floating point conversion of '{}')", val));
	}
	return r;
}

std::optional<double> IniFile::convertReal(const IniFileTag *val) const
{
	char *eptr;

	// Make sure we always use the C locale for conversion (thread local)
	locale_t olc = uselocale(static_cast<locale_t>(0));
	locale_t nlc = newlocale(LC_NUMERIC_MASK, "C", static_cast<locale_t>(0));
	if(static_cast<locale_t>(0) == nlc) {
		print_msg(fmt::format("{}:{}: internal error: Cannot set locale to \"C\" for strtod", val->path, val->lineno));
		return std::nullopt;
	}
	uselocale(nlc);
	errno = 0;
	double r = strtod(val->tagvalue.c_str(), &eptr);
	int errnosave = errno;
	uselocale(olc);
	freelocale(nlc);

	if(eptr == val->tagvalue.c_str() || errnosave != 0) {
		print_msg(fmt::format("{}:{}: error: Invalid floating point [{}]{}='{}'",
			val->path, val->lineno, sectionFromTag(val), val->tagname, val->tagvalue));
		return std::nullopt;
	}
	if(*eptr && !IniFile::isSpace(*eptr)) {
		print_msg(fmt::format("{}:{}: warning: Trailing character(s) in floating point conversion ([{}]{}='{}')",
			val->path, val->lineno, sectionFromTag(val), val->tagname, val->tagvalue));
	}
	return r;
}

//
// Find the num'th instance of '[section]tag' with optional 'section'
//
std::optional<std::string> IniFile::findString(int num, const std::string &tag, const std::string &section) const
{
	if(auto t = findTag(tag, section, num)) {
		return (*t)->tagvalue;
	}

	return std::nullopt;
}

std::optional<bool> IniFile::findBool(int num, const std::string &tag, const std::string &section) const
{
	if(auto t = findTag(tag, section, num)) {
		return convertBool(*t);
	}
	return std::nullopt;
}

//
// Find all instances of a '[section]name' with optional 'section'
//
std::vector<std::string> IniFile::findStringAll(const std::string &tag, const std::string &section) const
{
	std::vector<std::string> vals;

	// Find all matching tags
	if(auto t = findTags(tag, section)) {
		// Get all their values
		for(auto const c : (*t))
			vals.push_back(c->tagvalue);
	}
	return vals;
}

std::vector<bool> IniFile::findBoolAll(const std::string &tag, const std::string &section) const
{
	std::vector<bool> vals;

	// Find all matching tags
	if(auto t = findTags(tag, section)) {
		// Get all their values
		for(auto const c : (*t)) {
			if(auto b = convertBool(c))
				vals.push_back(*b);
		}
	}
	return vals;
}

std::vector<rtapi_s64> IniFile::findSIntAll(const std::string &tag, const std::string &section) const
{
	std::vector<rtapi_s64> vals;

	// Find all matching tags
	if(auto t = findTags(tag, section)) {
		// Get all their values
		for(auto const c : (*t)) {
			if(auto b = convertSInt(c))
				vals.push_back(*b);
		}
	}
	return vals;
}

std::vector<rtapi_u64> IniFile::findUIntAll(const std::string &tag, const std::string &section) const
{
	std::vector<rtapi_u64> vals;

	// Find all matching tags
	if(auto t = findTags(tag, section)) {
		// Get all their values
		for(auto const c : (*t)) {
			if(auto b = convertUInt(c))
				vals.push_back(*b);
		}
	}
	return vals;
}

std::vector<double> IniFile::findRealAll(const std::string &tag, const std::string &section) const
{
	std::vector<double> vals;

	// Find all matching tags
	if(auto t = findTags(tag, section)) {
		// Get all their values
		for(auto const c : (*t)) {
			if(auto b = convertReal(c))
				vals.push_back(*b);
		}
	}
	return vals;
}

//
// Find the num'th instance of '[section]name' with optional 'section' and
// convert to value.
// Optionally limited to range: mini <= value <= maxi
//
std::optional<rtapi_s64> IniFile::findSInt(int num, const std::string &tag, const std::string &section, rtapi_s64 mini, rtapi_s64 maxi) const
{
	if(auto t = findTag(tag, section, num)) {
		if(auto v = convertSInt(*t)) {
			if(*v >= mini && *v <= maxi) {
				return v;
			}
		}
	}
	return std::nullopt;
}

std::optional<rtapi_u64> IniFile::findUInt(int num, const std::string &tag, const std::string &section, rtapi_u64 mini, rtapi_u64 maxi) const
{
	if(auto t = findTag(tag, section, num)) {
		if(auto v = convertUInt(*t)) {
			if(*v >= mini && *v <= maxi) {
				return v;
			}
		}
	}
	return std::nullopt;
}

std::optional<double> IniFile::findReal(int num, const std::string &tag, const std::string &section, double mini, double maxi) const
{
	if(auto t = findTag(tag, section, num)) {
		if(auto v = convertReal(*t)) {
			if(*v >= mini && *v <= maxi) {
				return v;
			}
		}
	}
	return std::nullopt;
}

//
// Section support: Find all section and return to user
//
std::vector<std::string> IniFile::findSections() const
{
	std::vector<std::string> sects;

	for(size_t i = 0; true; i++) {
		if(auto const s = _inifilecontent->getSectionRef(i)) {
			sects.push_back((*s)->secname);
		} else {
			break;
		}
	}
	return sects;
}

//
// Variables support: Find all variables in an optional section and return to user
//
std::vector<std::pair<std::string,std::string>> IniFile::findVariables(const std::string &section) const
{
	std::vector<std::pair<std::string,std::string>> vars;

	if(section.empty()) {
		// Without section return all variable names from the master pool
		for(size_t i = 0; true; i++) {
			if(auto t = _inifilecontent->getTagRef(i)) {
				vars.push_back({(*t)->tagname, (*t)->tagvalue});
			} else {
				break;
			}
		}
	} else {
		// If a section is present, only return those variable names
		if(auto sect = _inifilecontent->getSectionRef(section)) {
			// Within the section, pick all tags
			for(auto t : (*sect)->tags) {	// Linked by index in master _tags
				if(auto tp = _inifilecontent->getTagRef(t)) {
					vars.push_back({(*tp)->tagname, (*tp)->tagvalue});
				}
			}
		}
	}
	return vars;
}

//
// Expand ~/filename to $HOME/filename
//
int IniFile::tildeExpand(const std::string &path, std::string &result)
{
	if(path.size() < 2 || '~' != path[0] || '/' != path[1]) {
		// Does not start with "~/", so we do not expand
		result = path; // Just copy
		return 0;
	}

	const char *home = getenv("HOME");
	if(!home)
		return -ENOENT;

	result = std::string(home) + path.substr(1);
	return 0;
}

//
//*****************************************************************************
// C-API interface routines
//*****************************************************************************
//
extern "C" {

int iniFindString(const char *inipath, const char *tag, const char *section, char *buf, size_t bufsize)
{
	if(!inipath || !tag || !buf || !bufsize)
		return -EINVAL;

	IniFile ini(inipath);
	if(!ini)
		return -EINVAL;

	if(!section) section = "";
	if(auto v = ini.findString(tag, section)) {
		if(v->size() >= bufsize)
			return -ENOSPC;	// buffer cannot hold string + nul char
		strcpy(buf, v->c_str());
		return 0;
	}
	return -ENOENT;
}

int iniFindBool(const char *inipath, const char *tag, const char *section, bool *result)
{
	if(!inipath || !tag || !result)
		return -EINVAL;

	IniFile ini(inipath);
	if(!ini)
		return -EINVAL;

	if(!section) section = "";
	if(auto v = ini.findBool(tag, section)) {
		*result = *v;
		return 0;
	}
	return -ENOENT;
}

int iniFindSInt(const char *inipath, const char *tag, const char *section, rtapi_s64 *result)
{
	if(!inipath || !tag || !result)
		return -EINVAL;

	IniFile ini(inipath);
	if(!ini)
		return -EINVAL;

	if(!section) section = "";
	if(auto v = ini.findSInt(1, tag, section)) {
		*result = *v;
		return 0;
	}
	return -ENOENT;
}

int iniFindUInt(const char *inipath, const char *tag, const char *section, rtapi_u64 *result)
{
	if(!inipath || !tag || !result)
		return -EINVAL;

	IniFile ini(inipath);
	if(!ini)
		return -EINVAL;

	if(!section) section = "";
	if(auto v = ini.findUInt(1, tag, section)) {
		*result = *v;
		return 0;
	}
	return -ENOENT;
}

int iniFindDouble(const char *inipath, const char *tag, const char *section, double *result)
{
	if(!inipath || !tag || !result)
		return -EINVAL;

	IniFile ini(inipath);
	if(!ini)
		return -EINVAL;

	if(!section) section = "";
	if(auto v = ini.findReal(1, tag, section)) {
		*result = *v;
		return 0;
	}
	return -ENOENT;
}

// Compatibility function
int iniFindInt(const char *inipath, const char *tag, const char *section, int *result)
{
	rtapi_s64 val;
	if(int err = iniFindSInt(inipath, tag, section, &val))
		return err;
	*result = (int)val;
	return 0;
}

int TildeExpansion(const char *path, char *buf, size_t bufsize)
{
	std::string p;
	if(int err = IniFile::tildeExpand(path, p))
		return err;

	if(p.size() >= bufsize)
		return -ENOSPC;	// buffer cannot hold string + nul char

	strcpy(buf, p.c_str());	
	return 0;
}

//
// Split string 'str' into tokens using 'delim' as delimiters
//
std::vector<std::string> IniFile::split(const std::string &delim, const std::string &str)
{
    std::vector<std::string> toks;
    size_t start = str.find_first_not_of(delim);    // Start-of-token pos (or npos if only delimters)
    size_t end   = str.find_first_of(delim, start); // End-of-token pos+1 (or npos if last token)

    // While start and end positions are available (meaning there is a token)
    while (!(std::string::npos == end && std::string::npos == start)) {
        toks.push_back(str.substr(start, end - start)); // Copy token into vector
        start = str.find_first_not_of(delim, end);      // Find new start-of-token from old end
        end   = str.find_first_of(delim, start);        // and end-of-token
    }
    return toks;
}

} // extern "C"

// vim: ts=4 sw=4
