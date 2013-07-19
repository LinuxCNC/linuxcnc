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
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#define MODE_MODIFY 0
#define MODE_ADD 1
#define MODE_INSERT 2

int TextToNumber(char * text,int ValMin,int ValMaxi,int *ValFound);
void SaveElementProperties(void);
void InitBufferRungEdited( StrRung * pRung );
int GetNbrRungsDefined(void);
int FindFreeRung(void);
void AddRung(void);
void InsertRung(void);
void ModifyCurrentRung(void);
void DeleteCurrentRung(void);
void CancelRungEdited(void);
void ApplyRungEdited(void);
void EditElementInRung(double x,double y);
void EditElementInThePage(double x,double y);
char * GetLadderElePropertiesForStatusBar(double x,double y);
char * ConvVarNameToHalSigName (char *);
char * FirstVariableInArithm(char *);
int SetDefaultVariableType(int NumElement);
