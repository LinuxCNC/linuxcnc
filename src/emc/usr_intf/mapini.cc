//
// Ini-value mapping for ini-file entries
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
#include "mapini.hh"

#include "libnml/rcs/rcs_print.hh"
#include "unitenum.hh"

using namespace linuxcnc;

std::optional<RCS_PRINT_DESTINATION_TYPE> mapRcsDestination(const IniFile &ini, const std::string &var, const std::string &sec)
{
	static const std::map<const std::string, const RCS_PRINT_DESTINATION_TYPE, IniFile::caseless> rcsDestinationMap = {
		{ "STDOUT", RCS_PRINT_TO_STDOUT },
		{ "STDERR", RCS_PRINT_TO_STDERR },
		{ "FILE",   RCS_PRINT_TO_FILE },
		{ "LOGGER", RCS_PRINT_TO_LOGGER },
		{ "MSGBOX", RCS_PRINT_TO_MESSAGE_BOX },
		{ "NULL",   RCS_PRINT_TO_NULL },
	};
	if (auto val = ini.findMap(rcsDestinationMap, var, sec))
		return *val;
	return std::nullopt;
}

std::optional<LINEAR_UNIT_CONVERSION> mapLinearUnits(const IniFile &ini, const std::string &var, const std::string &sec)
{
	static const std::map<const std::string, const LINEAR_UNIT_CONVERSION, IniFile::caseless> linUnitMap = {
		{ "AUTO", LINEAR_UNITS_AUTO },
		{ "INCH", LINEAR_UNITS_INCH },
		{ "MM",   LINEAR_UNITS_MM },
		{ "CM",   LINEAR_UNITS_CM },
	};
	if (auto inival = ini.findMap(linUnitMap, var, sec))
		return *inival;
	return std::nullopt;
}

std::optional<ANGULAR_UNIT_CONVERSION> mapAngularUnits(const IniFile &ini, const std::string &var, const std::string &sec)
{
	static const std::map<const std::string, const ANGULAR_UNIT_CONVERSION, IniFile::caseless> angUnitMap = {
		{ "AUTO", ANGULAR_UNITS_AUTO },
		{ "DEG",  ANGULAR_UNITS_DEG },
		{ "RAD",  ANGULAR_UNITS_RAD },
		{ "GRAD", ANGULAR_UNITS_GRAD },
	};
	if (auto inival = ini.findMap(angUnitMap, var, sec))
		return *inival;
	return std::nullopt;
}

// vim: ts=4 sw=4
