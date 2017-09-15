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
#ifndef S_LINE
//#define S_LINE "<!--"
//#define E_LINE "-->"
#define S_LINE ""
#define E_LINE ""

//#define FILE_PREFIX "cl_"
#define FILE_PREFIX ""

#endif



char *cl_fgets(char *s, int size, FILE *stream);
void LoadAllRungs_V1(char * BaseName,StrRung * Rungs,int * TheFirst,int * TheLast,int * TheCurrent);
void LoadAllRungs(char * BaseName,StrRung * Rungs);
void SaveAllRungs(char * BaseName);
char * ConvRawLineOfNumbers(char * RawLine,char NbrParams,int * ValuesFnd);
int ConvBaseInMilliSecsToId(int NbrMilliSecs);
#ifdef OLD_TIMERS_MONOS_SUPPORT
char LoadTimersParams(char * FileName,StrTimer * BufTimers);
char SaveTimersParams(char * FileName,StrTimer * BufTimers);
char LoadMonostablesParams(char * FileName,StrMonostable * BufMonostables);
char SaveMonostablesParams(char * FileName,StrMonostable * BufMonostables);
#endif
char LoadCountersParams(char * FileName);
char SaveCountersParams(char * FileName);
char LoadNewTimersParams(char * FileName);
char SaveNewTimersParams(char * FileName);
void DumpRung(StrRung * TheRung);
char LoadArithmeticExpr(char * FileName);
char SaveArithmeticExpr(char * FileName);
char LoadSectionsParams(char * FileName);
char SaveSectionsParams(char * FileName);
char LoadIOConfParams(char * FileName);
char SaveIOConfParams(char * FileName);
char LoadModbusIOConfParams(char * FileName);
char SaveModbusIOConfParams(char * FileName);
char LoadSymbols(char * FileName);
char SaveSymbols(char * FileName);
char LoadGeneralParameters(char * FileName);
char SaveGeneralParameters(char * FileName);

void LoadAllLadderDatas(char * DatasDirectory);
void SaveAllLadderDatas(char * DatasDirectory);

void CleanTmpLadderDirectory( char DestroyDir );
