/*    This is a component of LinuxCNC
 *    Copyright 2011 Michael Haberler <git@mah.priv.at>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

typedef pp::array_1_t< int, ACTIVE_G_CODES> active_g_codes_array, (*active_g_codes_w)( Interp & );
typedef pp::array_1_t< int, ACTIVE_M_CODES> active_m_codes_array, (*active_m_codes_w)( Interp & );
typedef pp::array_1_t< double, ACTIVE_SETTINGS> active_settings_array, (*active_settings_w)( Interp & );
typedef pp::array_1_t< block, MAX_NESTED_REMAPS> blocks_array, (*blocks_w)( Interp & );

typedef pp::array_1_t< double, RS274NGC_MAX_PARAMETERS > parameters_array, (*parameters_w)( Interp & );
typedef pp::array_1_t< CANON_TOOL_TABLE, CANON_POCKETS_MAX> tool_table_array, (*tool_table_w)( Interp & );
typedef pp::array_1_t< context, INTERP_SUB_ROUTINE_LEVELS> sub_context_array, (*sub_context_w)( Interp & );
typedef pp::array_1_t< int, 16> g_modes_array, (*g_modes_w)( block & );
typedef pp::array_1_t< int, 11> m_modes_array, (*m_modes_w)( block & );
typedef pp::array_1_t< double, INTERP_SUB_PARAMS> params_array,
    (*params_w)( block &),
    (*saved_params_w) (context &);


