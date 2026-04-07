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
#ifndef __LINUXCNC_USR_INTF_MAPINI_HH
#define __LINUXCNC_USR_INTF_MAPINI_HH

#include <inifile.hh>

// Forward declarations
enum RCS_PRINT_DESTINATION_TYPE : int;
enum LINEAR_UNIT_CONVERSION : int;
enum ANGULAR_UNIT_CONVERSION : int;

std::optional<RCS_PRINT_DESTINATION_TYPE> mapRcsDestination(const linuxcnc::IniFile &ini,
	const std::string &var, const std::string &sec);
std::optional<LINEAR_UNIT_CONVERSION> mapLinearUnits(const linuxcnc::IniFile &ini,
	const std::string &var, const std::string &sec);
std::optional<ANGULAR_UNIT_CONVERSION> mapAngularUnits(const linuxcnc::IniFile &ini,
	const std::string &var, const std::string &sec);

#endif
