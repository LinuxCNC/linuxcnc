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
#ifndef __LINUXCNC_INI_INIFILE_HH
#define __LINUXCNC_INI_INIFILE_HH

#ifndef __cplusplus
#error "inifile.hh cannot be used in C. Please use the C-API in inifile.h"
#endif

#include <string>
#include <optional>
#include <vector>
#include <map>
#include <utility>
#include <algorithm>
#include <climits>

#include <float.h>
#include <rtapi_stdint.h>

//
// IniFile public methods for extraction of values from the ini-file:
//
// Note that selecting the num'th value is the first argument to prevent
// accidents using wrong arguments for the bool/integer/real versions. This way
// you have to select the num'th as the first argument if you really mean to
// use it and cannot be mistaken as the default or min/max values. You can
// just omit the num if you just want the first value as a convenience method.
//
// All find_X_All() methods, except findStringAll(), will perform conversion to
// the requested type. That also means that mixed value content may result in
// errors and dropped results. Use the find_X_All() methods only when you know
// that the values are all supposed to be of the same type.
//
// Helper methods can be used to query the ini-file to get information about
// sections, variables, paths and line numbers:
//   bool hasSection(section)
//   bool hasVariable(variable, section)
//   bool hasVariable(num, variable, section)
// which all map to:
//   bool isSet(variable, section)
//   bool isSet(num, variable, section)
//
// Ini-file structural content methods:
//   std::vector<std::string> findSections()
//   std::vector<std::pair<std::string,std::string>> findVariables(section);
//   std::pair<std::string, int> lineOf(variable, section)
//   std::pair<std::string, int> lineOf(num, variable, section)
//
// General value extraction methods:
//   std::vector<std::string>   findStringAll(tag, section)
//   std::optional<std::string> findString(tag, section)
//   std::optional<std::string> findString(num, tag, section)
//   std::string                findStringV(tag, section, def)
//   std::string                findStringV(num, tag, section, def)
//
//   std::vector<bool>          findBoolAll(tag, section)
//   std::optional<bool>        findBool(tag, section)
//   std::optional<bool>        findBool(num, tag, section)
//   bool                       findBoolV(tag, section, def)
//   bool                       findBoolV(num, tag, section, def)
//                            
//   std::vector<rtapi_s64>     findSIntAll(tag, section)
//   std::optional<rtapi_s64>   findSInt(tag, section, mini = INT64_MIN, maxi = INT64_MAX)
//   std::optional<rtapi_s64>   findSInt(num, tag, section, mini = INT64_MIN, maxi = INT64_MAX)
//   rtapi_s64                  findSIntV(tag, section, def, mini = INT64_MIN, maxi = INT64_MAX)
//   rtapi_s64                  findSIntV(num, tag, section, def, mini = INT64_MIN, maxi = INT64_MAX)
//                            
//   std::vector<rtapi_u64>     findUIntAll(tag, section)
//   std::optional<rtapi_u64>   findUInt(tag, section, mini = 0, maxi = UINT64_MAX)
//   std::optional<rtapi_u64>   findUInt(num, tag, section, mini = 0, maxi = UINT64_MAX)
//   rtapi_u64                  findUIntV(tag, section, def, mini = 0, maxi = UINT64_MAX)
//   rtapi_u64                  findUIntV(num, tag, section, def, mini = 0, maxi = UINT64_MAX)
//                            
//   std::vector<double>        findRealAll(tag, section)
//   std::optional<double>      findReal(tag, section, mini = -DBL_MAX, maxi = +DBL_MAX)
//   std::optional<double>      findReal(num, tag, section, mini = -DBL_MAX, maxi = +DBL_MAX)
//   double                     findRealV(tag, section, def, mini = -DBL_MAX, maxi = +DBL_MAX)
//   double                     findRealV(num, tag, section, def, mini = -DBL_MAX, maxi = +DBL_MAX)
//
// Convenience methods using the (usually 32-bit) integer type are provided and
// map to findSInt:
//   std::optional<int>   findInt(tag, section, mini = INT_MIN, maxi = INT_MAX)
//   std::optional<int>   findInt(num, tag, section, mini = INT_MIN, maxi = INT_MAX)
//   int                  findIntV(tag, section, def, mini = INT_MIN, maxi = INT_MAX)
//   int                  findIntV(num, tag, section, def, mini = INT_MIN, maxi = INT_MAX)
//
// Implementing (case [in]sensitive) list type values can be done using the
// IniFile::findMap() template function for case sensitive and case insensitive
// compares. The map defined for type T mapping:
//   const std::map<const std::string, const T> = {...}
//   const std::map<const std::string, const T, IniFile::caseless> = {...}
// for function:
//   T findCustom(const IniFile &ini, const std::string &tag, const std::string &section, T def)
//
// Example:
//	double findUnits(const IniFile &ini, const std::string &tag, const std::string &section, double def)
//	{
//		static const std::map<const std::string, const double> unitsMap = {
//			{ "mm",         1.0 },
//			{ "metric",     1.0 },
//			{ "in",         1/25.4 },
//			{ "inch",       1/25.4 },
//			{ "imperial",   1/25.4 },
//		};
//    
//		if(auto c = ini.findMap(unitsMap, tag, section))
//			return *c;
//		return def;
//	}
//

namespace linuxcnc {

// Forward declaration of internal classes
class IniFileContent;
class IniFileTag;
class IniFileSection;

//
// Public facing IniFile operations
//
class IniFile
{
public:
	IniFile(const std::string &filePath);

	operator bool() const { return isOpen(); }
	bool isOpen() const { return _inifilecontent != nullptr; }

	bool hasSection(const std::string &section) const {
		return isSet(1, "", section);
	}
	bool hasVariable(int num, const std::string &tag, const std::string &section) const {
		return isSet(num, tag, section);
	}
	bool hasVariable(const std::string &tag, const std::string &section) const {
		return hasVariable(1, tag, section);
	}

	// Returns true if the specified [section]tag is present
	bool isSet(int num, const std::string &tag, const std::string &section) const {
		if(tag.empty() && section.empty()) {
			// No, we don't have nothing
			return false;
		}
		if(tag.empty()) {
			// We can have a section that has no variables in it
			return (bool)findSection(section);
		}
		return (bool)findTag(tag, section, num);
	}
	bool isSet(const std::string &tag, const std::string &section) const {
		return isSet(1, tag, section);
	}

	// Returns the ini-file path and line number of the specified variable
	std::pair<std::string,int> lineOf(int num, const std::string &tag, const std::string &section) const;
	std::pair<std::string,int> lineOf(const std::string &tag, const std::string &section) const {
		return lineOf(1, tag, section);
	}

	// Get all variables named 'tag' from (optional) section in a vector.
	// Returns an empty vector if none found.
	std::vector<std::string> findStringAll(const std::string &tag, const std::string &section) const;
	std::vector<bool>        findBoolAll(const std::string &tag, const std::string &section) const;
	std::vector<rtapi_s64>   findSIntAll(const std::string &tag, const std::string &section) const;
	std::vector<rtapi_u64>   findUIntAll(const std::string &tag, const std::string &section) const;
	std::vector<double>      findRealAll(const std::string &tag, const std::string &section) const;

	// Get the num'th variable named 'tag' from (optional) section.
	// Returns std::nullopt if not found
	std::optional<std::string> findString(int num, const std::string &tag, const std::string &section) const;
	std::optional<bool>        findBool(int num, const std::string &tag, const std::string &section) const;

	std::optional<std::string> findString(const std::string &tag, const std::string &section) const {
		return findString(1, tag, section);
	}
	std::optional<bool>        findBool(const std::string &tag, const std::string &section) const {
		return findBool(1, tag, section);
	}

	// Get numerical values with options bounded ranges.
	// Returns std::nullopt if not found or out-of-range.
	std::optional<rtapi_s64>   findSInt(int num, const std::string &tag, const std::string &section,
										rtapi_s64 mini = INT64_MIN, rtapi_s64 maxi = INT64_MAX) const;
	std::optional<rtapi_u64>   findUInt(int num, const std::string &tag, const std::string &section,
										rtapi_u64 mini = 0, rtapi_u64 maxi = UINT64_MAX) const;
	std::optional<double>      findReal(int num, const std::string &tag, const std::string &section,
										double mini = -DBL_MAX, double maxi = +DBL_MAX) const;

	std::optional<rtapi_s64>   findSInt(const std::string &tag, const std::string &section,
										rtapi_s64 mini = INT64_MIN, rtapi_s64 maxi = INT64_MAX) const {
		return findSInt(1, tag, section, mini, maxi);
	}
	std::optional<rtapi_u64>   findUInt(const std::string &tag, const std::string &section,
										rtapi_u64 mini = 0, rtapi_u64 maxi = UINT64_MAX) const {
		return findUInt(1, tag, section, mini, maxi);
	}
	std::optional<double>      findReal(const std::string &tag, const std::string &section,
										double mini = -DBL_MAX, double maxi = +DBL_MAX) const {
		return findReal(1, tag, section, mini, maxi);
	}

	// Get the num'th value with defaults if not found.
	std::string findStringV(int num, const std::string &tag, const std::string &section, const std::string &def) const {
		if(auto v = findString(num, tag, section))
			return *v;
		return def;
	}
	bool        findBoolV(int num, const std::string &tag, const std::string &section, bool def) const {
		if(auto v = findBool(num, tag, section))
			return *v;
		return def;
	}

	std::string findStringV(const std::string &tag, const std::string &section, const std::string &def) const {
		return findStringV(1, tag, section, def);
	}
	bool        findBoolV(const std::string &tag, const std::string &section, bool def) const {
		return findBoolV(1, tag, section, def);
	}

	// Find the num'th value within min/max range.
	// Returns default value if not found or out-of-range.
	// These have a suffix 'V' to counter the overloading ambiguity.
	rtapi_s64   findSIntV(int num, const std::string &tag, const std::string &section, rtapi_s64 def,
							rtapi_s64 mini = INT64_MIN, rtapi_s64 maxi = INT64_MAX) const {
		if(auto v = findSInt(num, tag, section, mini, maxi))
			return *v;
		return def;
	}
	rtapi_u64   findUIntV(int num, const std::string &tag, const std::string &section, rtapi_u64 def,
							rtapi_u64 mini = 0, rtapi_u64 maxi = UINT64_MAX) const {
		if(auto v = findUInt(num, tag, section, mini, maxi))
			return *v;
		return def;
	}
	double      findRealV(int num, const std::string &tag, const std::string &section, double def,
							double mini = -DBL_MAX, double maxi = +DBL_MAX) const {
		if(auto v = findReal(num, tag, section, mini, maxi))
			return *v;
		return def;
	}

	rtapi_s64   findSIntV(const std::string &tag, const std::string &section, rtapi_s64 def,
							rtapi_s64 mini = INT64_MIN, rtapi_s64 maxi = INT64_MAX) const {
		return findSIntV(1, tag, section, def, mini, maxi);
	}
	rtapi_u64   findUIntV(const std::string &tag, const std::string &section, rtapi_u64 def,
							rtapi_u64 mini = 0, rtapi_u64 maxi = UINT64_MAX) const {
		return findUIntV(1, tag, section, def, mini, maxi);
	}
	double      findRealV(const std::string &tag, const std::string &section, double def,
							double mini = -DBL_MAX, double maxi = +DBL_MAX) const {
		return findRealV(1, tag, section, def, mini, maxi);
	}

	// Convenience methods
	std::optional<int> findInt(int num, const std::string &tag, const std::string &section,
							int mini = INT_MIN, int maxi = INT_MAX) const {
		if(auto v = findSInt(num, tag, section, mini, maxi))
			return (int)*v;
		return std::nullopt;
	}
	std::optional<int> findInt(const std::string &tag, const std::string &section,
							int mini = INT_MIN, int maxi = INT_MAX) const {
		return findInt(1, tag, section, mini, maxi);
	}
	int findIntV(int num, const std::string &tag, const std::string &section, int def,
							int mini = INT_MIN, int maxi = INT_MAX) const {
		return (int)findSIntV(num, tag, section, def, mini, maxi);
	}
	int findIntV(const std::string &tag, const std::string &section, int def,
							int mini = INT_MIN, int maxi = INT_MAX) const {
		return findIntV(1, tag, section, def, mini, maxi);
	}

	// Map-search matching of values returning the mapped value.
	// Search is case-sensitive.
	template<typename T>
		std::optional<T> findMap(int num, const std::map<const std::string, const T> &map,
								 const std::string &tag, const std::string &section = "") const {
		if(auto s = findString(num, tag, section)) {
			auto const m = map.find(*s);
			if(m != map.end()) {
				return m->second;
			}
		}
		return std::nullopt;
	}

	template<typename T>
		std::optional<T> findMap(const std::map<const std::string, const T> &map,
								 const std::string &tag, const std::string &section = "") const {
		return findMap(1, map, tag, section);
	}
	// Map compare function without case
	struct caseless {
		struct caseless_cmp {
			bool operator() (const char &a, const char &b) const {
				return std::tolower(a & 0xff) < std::tolower(b & 0xff);
			}
		};
		bool operator() (const std::string &a, const std::string &b) const {
			return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), caseless_cmp());
		}
	};

	// Map-search matching of values returning the mapped value.
	// Search is case-insensitive.
	template<typename T>
		std::optional<T> findMap(int num, const std::map<const std::string, const T, caseless> &map,
								 const std::string &tag, const std::string &section = "") const {
		if(auto s = findString(num, tag, section)) {
			auto const m = map.find(*s);
			if(m != map.end()) {
				return m->second;
			}
		}
		return std::nullopt;
	}

	template<typename T>
		std::optional<T> findMap(const std::map<const std::string, const T, caseless> &map,
								 const std::string &tag, const std::string &section = "") const {
		return findMap(1, map, tag, section);
	}

	// Return a list of section names from the ini-file
	std::vector<std::string> findSections() const;
	// Return a list of variable name/value pairs from an optional section in the ini-file
	std::vector<std::pair<std::string,std::string>> findVariables(const std::string &section) const;

	// The fact that this is here is because of compatibility
	// Perform tilde expansion using HOME environment variable.
	// Returns the filePath "~/path" as "$HOME/path"
	// Zero is returned on success or a negative value (-errno) on failure.
	static int tildeExpand(const std::string &filePath, std::string &res);

	// Compatibility method
	static int TildeExpansion(const std::string &filePath, std::string &res) {
		return IniFile::tildeExpand(filePath, res);
	}

	static std::optional<bool>      convertBool(const std::string &val);
	static std::optional<rtapi_s64> convertSInt(const std::string &val);
	static std::optional<rtapi_u64> convertUInt(const std::string &val);
	static std::optional<double>    convertReal(const std::string &val);

	// split() Tokenize 'str' based on 'delim'
	static std::vector<std::string> split(const std::string &delim, const std::string &str);

	// Trim leading/trailing or both
	static void rtrim(std::string &str) {
		size_t n = str.find_last_not_of(IniFile::STR_WS);
		if(std::string::npos != n)
			str.erase(n+1);
	}
	static void ltrim(std::string &str) {
		if(str.empty())
			return;
		size_t n = str.find_first_not_of(IniFile::STR_WS);
		if(std::string::npos == n)
			str.clear();	// Only whitespace
		else
			str.erase(0, n);
	}

	static void trim(std::string &str) {
		rtrim(str);
		ltrim(str);
	}

	// Trim on a copy and return the trimmed copy
	static std::string rtrimcpy(const std::string &str) {
		std::string cpy = str;
		rtrim(cpy);
		return cpy;
	}

	static std::string ltrimcpy(const std::string &str) {
		std::string cpy = str;
		ltrim(cpy);
		return cpy;
	}

	static std::string trimcpy(const std::string &str) {
		std::string cpy = str;
		rtrim(cpy);
		ltrim(cpy);
		return cpy;
	}

	// White-space characters for argument to find_first_of() and the like.
	// Using static constexpr std::string does not seem to work on Debian 11
	// with clang-19 and older than that. Gcc on debian 11 and before doesn't
	// support enough C++20 to build LinuxCNC at all.
	static constexpr char STR_WS[] =" \t\v\f\r\n";

	// This isSpace is guaranteed not to depend on locale
	static bool isSpace(char c) {
		return std::string::npos != (std::string{STR_WS}).find(c);
	}
private:
	bool Open(const std::string &filePath);
	bool Close() { _inifilecontent = nullptr; _filepath.clear(); return true; }

	bool hasOpenError(const std::string &tag, const std::string &section) const;
	std::string sectionFromTag(const IniFileTag *val) const;

	std::optional<const IniFileSection *> findSection(const std::string &section) const;
	std::optional<const IniFileTag *> findTag(const std::string &tag, const std::string &section, int num) const;
	std::optional<std::vector<const IniFileTag *>> findTags(const std::string &tag, const std::string &section) const;

	std::optional<bool>      convertBool(const IniFileTag *val) const;
	std::optional<rtapi_s64> convertSInt(const IniFileTag *val) const;
	std::optional<rtapi_u64> convertUInt(const IniFileTag *val) const;
	std::optional<double>    convertReal(const IniFileTag *val) const;

	const IniFileContent *_inifilecontent;
	std::string _filepath;
};

}	// namespace linuxcnc

#endif
// vim: ts=4 sw=4
