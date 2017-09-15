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

void InitRungs(void);
void PrepareRungs(void);
void InitTimers(void);
void PrepareTimers(void);
void InitMonostables(void);
void PrepareMonostables(void);
void InitCounters(void);
void PrepareCounters(void);
void InitTimersIEC(void);
void PrepareTimersIEC(void);
void PrepareAllDatasBeforeRun(void);
void InitArithmExpr(void);
void InitIOConf( void );
void RefreshASection( StrSection * pSection );
void ClassicLadder_RefreshAllSections(void);
void CopyRungToRung(StrRung * RungSrc,StrRung * RungDest);
