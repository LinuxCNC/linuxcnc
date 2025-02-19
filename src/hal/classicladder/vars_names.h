//    Copyright 2005-2008, various authors
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


int GetSizeVarsForTypeVar( int iTypeVarToSearch );
char * CreateVarName(int Type, int Offset, char SymbolNameIfAvail);
char TextParserForAVar( char * TextToParse, int * VarTypeFound, int * VarOffsetFound, int * pNumberOfChars, char PartialNames );
char TestVarIsReadWrite( int TypeVarTested, int OffsetVarTested );

