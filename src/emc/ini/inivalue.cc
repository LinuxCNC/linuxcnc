//
// inivalue - Ini-file query program
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
#include <iostream>
#include <fmt/format.h>

#include <getopt.h>
#include <locale.h>

#include "inifile.hh"

using namespace linuxcnc;

//
// Note: Short options chosen such to prevent overlap with the old inivar
// program's options (-var,-sec, -num, -tildeexpand and -ini). Therefore, no
// short options -v -s -n -t or -i.
//
static const struct option options[] = {
	{ "var",         required_argument, NULL, 'V' },
	{ "sec",         required_argument, NULL, 'S' },
	{ "num",         required_argument, NULL, 'N' },
	{ "type",        required_argument, NULL, 'y' },
	{ "minimum",     required_argument, NULL, 'm' },
	{ "maximum",     required_argument, NULL, 'M' },
	{ "all",         no_argument,       NULL, 'a' },
	{ "boolnum",     no_argument,       NULL, 'b' },
	{ "boolpy",      no_argument,       NULL, 'B' },
	{ "tildeexpand", no_argument,       NULL, 'T' },
	{ "quiet",       no_argument,       NULL, 'q' },
	{ "sections",    no_argument,       NULL, 'e' },
	{ "variables",   no_argument,       NULL, 'o' },
	{ "content",     no_argument,       NULL, 'c' },
	{ "prefix",      no_argument,       NULL, 'p' },
	{ "help",        no_argument,       NULL, 'h' },
	{ }
};
static const char options_str[] = "V:S:N:y:m:M:abBTqeocph";

static const char usage_str[] =
	"Usage:\n"
	"    inivalue --var=<tag> [--sec=<sect>] [more-options...] <ini-file>\n"
	"    inivalue --sections <ini-file>\n"
	"Options:\n"
	"  -V|--var=str      Locate 'str' named variable\n"
	"  -S|--sec=str      Only look in 'str' named section (default: <any section>)\n"
	"  -N|--num=n        Select the 'n'th occurrence of the variable (default: 1)\n"
	"  -a|--all          Return all available variable of specified name\n"
	"  -y|--type=x       Set var type to i[nteger], u[nsigned], r[eal], s[tring] or b[oolean] (default: s)\n"
	"  -m|--minimum=val  Set minimum value to test against (types i and u)\n"
	"  -M|--maximum=val  Set maximum value to test against (types i and u)\n"
	"  -b|--boolnum      Print booleans as 1/0 and not as true/false\n"
	"  -B|--boolpy       Print booleans as python compatible capitalized True/False\n"
	"  -T|--tildeexpand  Substitute ~/path with $(HOME)/path on string values\n"
	"  -q|--quiet        Don't print message if variable not found\n"
	"  -e|--sections     Return all section names from the ini-file\n"
	"  -o|--variables    Return all variables names from the ini-file (may be limited by section)\n"
	"  -c|--content      With -o, return the variable names with content (values)\n"
	"  -p|--prefix       With -o, return the variable names with section name prefixed\n"
	"  -h|--help         This message\n"
	;

static inline void print_err(const std::string &str)
{
	std::cerr << "error: " << str << std::endl;
}

// Query/conversion type
enum {
	TYPE_NONE,
	TYPE_STRING,
	TYPE_INTEGER,
	TYPE_UNSIGNED,
	TYPE_REAL,
	TYPE_BOOLEAN,
};

// Markers for --minimum and --maximum
#define HAVE_MINI	0x01
#define HAVE_MAXI	0x02

// Error exit values
#define EXIT_ENOENT	2	// No entry/variable found
#define EXIT_ERANGE	3	// Scalar out of range

//
// Helper to get different radix number parsed properly
//
static std::optional<rtapi_u64> convertValue(const std::string &optval)
{
	std::string val = optval;
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
		// Sets the radix and remove the prefix
		switch(val[pm+1]) {
		case 'x': case 'X': base = 16; val.erase(pm, 2); break;
		case 'o': case 'O': base =  8; val.erase(pm, 2); break;
		case 'b': case 'B': base =  2; val.erase(pm, 2); break;
		}
	}

	char *eptr;
	errno = 0;
	rtapi_u64 u = strtoull(val.c_str(), &eptr, base);

	if(eptr == val.c_str() || errno != 0) {
		print_err(fmt::format("Invalid number converting '{}'", val));
		return std::nullopt;
	}
	if(*eptr && !std::isspace(*eptr & 0xff)) {
		print_err(fmt::format("Trailing character(s) in numeric conversion of '{}')", val));
		return std::nullopt;
	}
	return u;
}

static std::optional<double> convertReal(const std::string &optval)
{
	char *eptr;
	errno = 0;
	// Make sure we always use the C locale for conversion
	std::string lcn = setlocale(LC_NUMERIC, NULL);
	setlocale(LC_NUMERIC, "C");
	double r = strtod(optval.c_str(), &eptr);
	setlocale(LC_NUMERIC, lcn.c_str());

	if(eptr == optval.c_str() || errno != 0) {
		print_err(fmt::format("Invalid floating point '{}'", optval));
		return std::nullopt;
	}
	if(*eptr && !std::isspace(*eptr & 0xff)) {
		print_err(fmt::format("Trailing character(s) in floating point of '{}')", optval));
		return std::nullopt;
	}
	return r;
}

// Escape all special characters in a double quoted string
static const std::string escapeString(const std::string &s)
{
	std::string escv{"\a\b\f\n\r\t\v"};
	std::string escc{"abfnrtv"}; // Letter order from above
	std::string res;
	for(size_t i = 0; i < s.size(); i++) {
		char c = s[i];
		if(0 == i && ' ' == c) {
			// First char is a space, must escape
			res += "\\x20";
		} else if(s.size()-1 == i && ' ' == c) {
			// Last char is a space, must escape
			res += "\\x20";
		} else if(c < ' ') {
			// It is a control character
			auto i = escv.find(c);
			if(std::string::npos != i) {
				// One of the standard escapes
				res += '\\';
				res += escc[i];
			} else {
				// Make it a hex escape
				res += fmt::format("\\x{:02x}", c & 0xff);
			}
		} else if('\\' == c) {
			// A backslash
			res += "\\\\";
		} else {
			// Nothing special, just add
			res += c;
		}
	}
	return res;
}

int main(int argc, char * const argv[])
{
	int lose = 0;
	int num = -1;	// Detect 'not set on commandline'
	std::string variable;
	std::string section;
	std::string inipath;
	bool tildeexp = false;
	bool allvars  = false;
	bool sections = false;
	bool variables= false;
	bool content  = false;
	bool prefix   = false;
	bool quiet    = false;
	bool boolnum  = false;
	bool boolpy   = false;
	int vtype     = TYPE_NONE;
	int havemm    = 0;
	rtapi_u64 mini = 0;
	rtapi_u64 maxi = UINT64_MAX;
	double rmini = -DBL_MAX;
	double rmaxi = +DBL_MAX;

	char *eptr;
	int optc;
	while(-1 != (optc = getopt_long(argc, argv, options_str, options, NULL))) {
		switch(optc) {
		case 'V': variable = optarg; break;
		case 'S': section  = optarg; break;
		case 'a': allvars  = true;   break;
		case 'q': quiet    = true;   break;
		case 'T': tildeexp = true;   break;
		case 'e': sections = true;   break;
		case 'o': variables= true;   break;
		case 'c': content  = true;   break;
		case 'p': prefix   = true;   break;
		case 'b': boolnum  = true;   break;
		case 'B': boolpy   = true;   break;
		case 'y':
			switch(optarg[0]) {
			case 'I': case 'i': vtype = TYPE_INTEGER; break;
			case 'U': case 'u': vtype = TYPE_UNSIGNED; break;
			case 'B': case 'b': vtype = TYPE_BOOLEAN; break;
			case 'S': case 's': vtype = TYPE_STRING; break;
			case 'R': case 'r': vtype = TYPE_REAL; break;
			default:
				print_err(fmt::format("Invalid type identifier '{}' accepted={{i,u,b,s,r}}", optarg[0]));
				lose++;
				break;
			}
			break;
		case 'm':
			if(vtype == TYPE_NONE) {
				print_err("Must first specify --type before minimum");
				lose++;
			} else {
				havemm |= HAVE_MINI;
				if(vtype == TYPE_INTEGER || vtype == TYPE_UNSIGNED) {
					if(auto m = convertValue(optarg)) {
						mini = *m;
					} else {
						lose++;
					}
				} else if(vtype == TYPE_REAL) {
					if(auto m = convertReal(optarg)) {
						rmini = *m;
					} else {
						lose++;
					}
				} // else ignore string or boolean
			}
			break;
		case 'M':
			if(vtype == TYPE_NONE) {
				print_err("Must first specify --type before maximum");
				lose++;
			} else {
				havemm |= HAVE_MAXI;
				if(vtype == TYPE_INTEGER || vtype == TYPE_UNSIGNED) {
					if(auto m = convertValue(optarg)) {
						maxi = *m;
					} else {
						lose++;
					}
				} else if(vtype == TYPE_REAL) {
					if(auto m = convertReal(optarg)) {
						rmaxi = *m;
					} else {
						lose++;
					}
				} // else ignore string or boolean
			}
			break;
		case 'N':
			errno = 0;
			num = strtol(optarg, &eptr, 0);
			if(eptr == optarg || errno != 0 || (*eptr && !isspace(*eptr & 0xff)) || num < 1) {
				print_err(fmt::format("Invalid number '{}' in --num argument", optarg));
				lose++;
			}
			break;
		case 'h':
			std::cout << usage_str;
			return EXIT_FAILURE;
		default:
			lose++;
			break;
		}
	}

	if(optind < argc) {
		inipath = argv[optind];
	} else if(!lose) {	// Don't bother if we're already doomed
		print_err("Missing ini-file");
		lose++;
	}

	if(lose)
		return EXIT_FAILURE;

	IniFile ini(inipath);
	if(!ini) {
		// Instantiating should have printed any error message
		// print_err(fmt::format("Failed to open '{}'", inipath));
		return EXIT_FAILURE;
	}

	//
	// Precedence for command-line option actions:
	// 1. -e - list all sections
	// 2. -o - list all variable names (optional by section)
	// 3. -a - list all values for a variable name (optional by section)
	// 4. -V - list the value of a variable (with optional range)
	//
	if(sections) {
		// Return all section names
		for(auto const &sect : ini.findSections())
			std::cout << sect << std::endl;
	} else if(variables) {
		// Return all variables
		if(section.empty()) {
			// Do it for each section
			for(auto const &sect : ini.findSections()) {
				for(auto const &var : ini.findVariables(sect)) {
					if(prefix)
						std::cout << '[' << sect << ']';
					std::cout << var.first;
					if(content)
						std::cout << '=' << escapeString(var.second);
					std::cout << std::endl;
				}
			}
		} else {
			// Or just the section we were asked to handle
			for(auto const &var : ini.findVariables(section)) {
				if(prefix)
					std::cout << '[' << section << ']';
				std::cout << var.first;
				if(content)
					std::cout << '=' << escapeString(var.second);
				std::cout << std::endl;
			}
		}
	} else if(allvars) {
		// Return all variables of certain name (global or from optional section)
		std::vector<std::string> strs;
		switch(vtype) {
		case TYPE_NONE:	// If no --type on cmd-line we default to string
		case TYPE_STRING:
			strs = ini.findStringAll(variable, section);
			break;
		case TYPE_INTEGER: {
				auto iv = ini.findSIntAll(variable, section);
				for(auto const &i : iv)
					strs.push_back(fmt::format("{}", i));
			} break;
		case TYPE_UNSIGNED: {
				auto iv = ini.findUIntAll(variable, section);
				for(auto const &i : iv)
					strs.push_back(fmt::format("{}", i));
			} break;
		case TYPE_REAL: {
				auto iv = ini.findRealAll(variable, section);
				for(auto const &i : iv)
					strs.push_back(fmt::format("{:.15g}", i));
			} break;
		case TYPE_BOOLEAN: {
				auto iv = ini.findBoolAll(variable, section);
				for(auto const &i : iv)
					strs.push_back(fmt::format("{}", i ? "true" : "false"));
			} break;
		default:
			print_err(fmt::format("internal: Invalid type case (vtype={})", (int)vtype));
			break;
		}
		// Only test if num is actually set
		if(num > 0 && num > (ssize_t)strs.size()) {
			if(!quiet) {
				if(strs.empty())
					print_err(fmt::format("Cannot find any instance of '[{}]{}'", section, variable));
				else
					print_err(fmt::format("No instance {} or more of '[{}]{}'", num, section, variable));
			}
			return EXIT_ENOENT;
		} else if(num < 1) {
			num = 1;	// Was not set, start from the beginning
		}
		if(tildeexp && (TYPE_NONE == vtype || TYPE_STRING == vtype)) {
			for(size_t i = num-1; i < strs.size(); i++) {
				std::string ex;
				if(IniFile::tildeExpand(strs[i], ex)) {
					print_err(fmt::format("Tilde expansion failed on '{}'", strs[i]));
					return EXIT_FAILURE;
				}
				std::cout << ex << std::endl;
			}
		} else {
			for(size_t i = num-1; i < strs.size(); i++)
				std::cout << strs[i] << std::endl;
		}
	} else {
		// Print the num'th named variable (global or in optional section)
		if(num <= 0)
			num = 1;
		if(auto str = ini.findString(num, variable, section)) {
			// We get here and we know '[section]variable' exists, regardless content
			switch(vtype) {
			case TYPE_NONE:	// If no --type on cmd-line we default to string
			case TYPE_STRING:
				if (tildeexp) {
					std::string ex;
					if(IniFile::tildeExpand(*str, ex)) {
						print_err(fmt::format("Tilde expansion failed on '{}'", *str));
						return EXIT_FAILURE;
					}
					std::cout << ex << std::endl;
				} else {
					std::cout << *str << std::endl;
				}
				break;
			case TYPE_INTEGER: {
					rtapi_s64 smini = (havemm & HAVE_MINI) ? (rtapi_s64)mini : INT64_MIN;
					rtapi_s64 smaxi = (havemm & HAVE_MAXI) ? (rtapi_s64)maxi : INT64_MAX;
					if(smini > smaxi) {
						print_err(fmt::format("Invalid integer range min '{}' > max '{}'", smini, smaxi));
						return EXIT_ERANGE;
					}
					if(auto i = ini.findSInt(num, variable, section, smini, smaxi)) {
						std::cout << *i << std::endl;
					} else {
						print_err(fmt::format("Invalid integer [{}]{}='{}' or out of range [{},{}]",
									section, variable, *str, smini, smaxi));
						return EXIT_ERANGE;
					}
				} break;
			case TYPE_UNSIGNED:
				if(!(havemm & HAVE_MINI))
					mini = 0;
				if(!(havemm & HAVE_MAXI))
					maxi = UINT64_MAX;
				if(mini > maxi) {
					print_err(fmt::format("Invalid unsigned range min '{}' > max '{}'", mini, maxi));
					return EXIT_ERANGE;
				}
				if(auto i = ini.findUInt(num, variable, section, mini, maxi)) {
					std::cout << *i << std::endl;
				} else {
					print_err(fmt::format("Invalid unsigned [{}]{}='{}' or out of range [{},{}]",
									section, variable, *str, mini, maxi));
					return EXIT_ERANGE;
				}
				break;
			case TYPE_REAL:
				if(rmini > rmaxi) {
					print_err(fmt::format("Invalid real range min '{}' > max '{}'", rmini, rmaxi));
					return EXIT_ERANGE;
				}
				if(auto i = ini.findReal(num, variable, section, rmini, rmaxi)) {
					std::cout << fmt::format("{:.15e}", *i) << std::endl;
				} else {
					print_err(fmt::format("Invalid real [{}]{}='{}' or out of range [{:g},{:g}]",
									section, variable, *str, rmini, rmaxi));
					return EXIT_ERANGE;
				}
				break;
			case TYPE_BOOLEAN:
				if(auto i = ini.findBool(num, variable, section)) {
					if(boolnum)
						std::cout << fmt::format("{}", *i ? "1" : "0") << std::endl;
					else if(boolpy)
						std::cout << fmt::format("{}", *i ? "True" : "False") << std::endl;
					else
						std::cout << fmt::format("{}", *i ? "true" : "false") << std::endl;
				} else {
					// Message was printed if failed conversion
					// print_err(fmt::format("Invalid boolean '{}'", *str));
					return EXIT_ERANGE;
				}
				break;
			default:
				print_err(fmt::format("internal: Invalid type case (vtype={})", (int)vtype));
				break;
			}
		} else {
			if(!quiet) {
				print_err(fmt::format("Cannot find instance {} of '[{}]{}'", num, section, variable));
			}
			return EXIT_ENOENT;
		}
	}

	return EXIT_SUCCESS;
}
// vim: ts=4 sw=4
