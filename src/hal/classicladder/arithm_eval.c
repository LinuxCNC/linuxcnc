/* Classic Ladder Project */
/* Copyright (C) 2001-2007 Marc Le Douarain */
/* http://membres.lycos.fr/mavati/classicladder/ */
/* http://www.sourceforge.net/projects/classicladder */
/* October 2001 */
/* ------------------------------- */
/* Arithmetic expression evaluator */
/* ------------------------------- */
/* This library is free software; you can redistribute it and/or */
/* modify it under the terms of the GNU Lesser General Public */
/* License as published by the Free Software Foundation; either */
/* version 2.1 of the License, or (at your option) any later version. */

/* This library is distributed in the hope that it will be useful, */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU */
/* Lesser General Public License for more details. */

/* You should have received a copy of the GNU Lesser General Public */
/* License along with this library; if not, write to the Free Software */
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifdef MODULE
#include <linux/string.h>
#ifdef RTAI
#include <linux/kernel.h>
#include <linux/module.h>
#include "rtai.h"
#endif
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "classicladder.h"
#include "global.h"
#include "vars_access.h"
#ifdef GTK_INTERFACE
#include "vars_names.h"
#endif
#include "arithm_eval.h"


char * Expr;
char * ErrorDesc;
char * VerifyErrorDesc;
int UnderVerify;

/* for RTLinux module */
#if defined( MODULE )
int atoi(const char *p)
{
	int n=0;
	while(*p>='0' && *p<='9')
		n = n*10 + *p++-'0';
	return n;
}
#endif
int pow_int(int a,int b)
{
	int x;
	for (x=1;x<=b;x++)
		a = a*a;
	return a;
}


void SyntaxError(void)
{
	if (UnderVerify)
		VerifyErrorDesc = ErrorDesc;
	else
		debug_printf("Syntax error : '%s' , at %s !!!!!\n",ErrorDesc,Expr);
}

arithmtype Constant(void)
{
	arithmtype Res = 0;
	char cIsNeg = FALSE;
	/* negative constant ? */
	if ( *Expr=='-' )
	{
		cIsNeg = TRUE;
		Expr++;
	}
	if (*Expr=='$' )
	{
		Expr++;
		/* hexa number */
		while( (*Expr>='0' && *Expr<='9') || (*Expr>='A' && *Expr<='F')  || (*Expr>='a' && *Expr<='f') )
		{
			char Carac = *Expr;
			if ( Carac>='A' && Carac<='F' )
				Carac = Carac-'A'+10;
			else
			if ( Carac>='a' && Carac<='f' )
				Carac = Carac-'a'+10;
			else
				Carac = Carac-'0';
			Res = 16*Res + Carac;
			Expr++;
		}
	}
	else
	{
		/* decimal number */
		while(*Expr>='0' && *Expr<='9')
		{
			Res = 10*Res + (*Expr-'0');
			Expr++;
		}
	}
	if ( cIsNeg )
		Res = Res * -1;
	return Res;
}

/* return TRUE if okay: pointer of pointer on ONE var : "xxx/yyy@" or "xxx/yyy[" */
/* pointer of pointer is advanced to the first character after last y  : @ or [ */
int IdentifyVarContent( char **PtrStartExpr, int * ResType,int * ResOffset )
{
	int VarType,VarOffset;
	char * StartExpr = *PtrStartExpr;
	char * SearchSep = StartExpr;
//printf("IndentifyVar=%s\n", StartExpr);

	do
	{
		SearchSep++;
	}
	while( (*SearchSep!='/') && (*SearchSep!='\0') );

	if (*SearchSep=='/')
	{
		VarType = atoi(StartExpr);
		SearchSep++;
		StartExpr = SearchSep;
		do
		{
			StartExpr++;
		}
		while( (*StartExpr!='@') && (*StartExpr!='[') && (*StartExpr!='\0') );
		if (*StartExpr=='@' || *StartExpr=='[')
		{
			VarOffset = atoi(SearchSep);
			*ResType = VarType;
			*ResOffset = VarOffset;

			*PtrStartExpr = StartExpr;
//printf("IdentifyVarResult=%d/%d\n", VarType, VarOffset);
			return TRUE;
		}
		else
		{
			ErrorDesc = "Bad var coding (err=2), should be @xx/yy@";
			SyntaxError();
			return FALSE;
		}
	}
	else
	{
		ErrorDesc = "Bad var coding (err=1), should have a / for xx/yy form";
	}
	SyntaxError();
	return FALSE;
}

/* return TRUE if okay: pointer on a simple var or indexed one : "@xxx/yyy@" or @xxx/yyy[xxx/yyy]@" */
int IdentifyVarIndexedOrNot(char *StartExpr,int * ResType,int * ResOffset, int * ResIndexType,int * ResIndexOffset)
{
	int VarType,VarOffset;
	char * ScanExpr = StartExpr;
//printf("START identify var at:%s...\n", ScanExpr);
	if (*ScanExpr!='@')
	{
		ErrorDesc = "Bad var coding (err=0), should start with @ for @xx/yy@ form";
		SyntaxError();
		return FALSE;
	}
	ScanExpr++;
	// no index per default!
	*ResIndexType = -1;
	*ResIndexOffset = -1;

	if ( IdentifyVarContent( &ScanExpr, &VarType, &VarOffset ) )
	{
		*ResType = VarType;
		*ResOffset = VarOffset;
		// there is an index present ?
		if ( *ScanExpr=='[')
		{
			ScanExpr++;
			if ( IdentifyVarContent( &ScanExpr, &VarType, &VarOffset ) )
			{
				*ResIndexType = VarType;
				*ResIndexOffset = VarOffset;
//printf("identified indexed var for %s => %d/%d[%d/%d]\n", StartExpr, *ResType, *ResOffset, *ResIndexType, *ResIndexOffset );
				return TRUE;
			}
		}
		else
		{
//printf("identified simple var for %s => %d/%d\n", StartExpr, *ResType, *ResOffset );
			return TRUE;
		}
	}
	ErrorDesc = "Bad var coding (unknown variable)";
	SyntaxError();
	return FALSE;
}

/* Give final variable (taking into acount the value of an index if present) */
int IdentifyFinalVar( char *StartExpr, int * ResType,int * ResOffset )
{
	int IndexVarType,IndexVarOffset;
	int SyntaxOk = IdentifyVarIndexedOrNot( StartExpr, ResType, ResOffset, &IndexVarType, &IndexVarOffset );
	if ( SyntaxOk )
	{
		if ( IndexVarType!=-1 && IndexVarOffset!=-1 )
		{
			// add index value from content of the index variable
			int IndexValue = ReadVar( IndexVarType, IndexVarOffset );
			*ResOffset = *ResOffset + IndexValue;
		}
//printf("Final var for %s => %d/%d\n", StartExpr, *ResType, *ResOffset );
	}
	return SyntaxOk;
}

arithmtype Variable(void)
{
	int VarType,VarOffset;
	if (IdentifyFinalVar(Expr, &VarType,&VarOffset))
	{
//printf("Variable:%d/%d\n", VarType, VarOffset);
		/* flush var found */
		Expr++;
		do
		{
			Expr++;
		}
		while( (*Expr!='@') && (*Expr!='\0') );
		Expr++;
		/* return var value */
		return (arithmtype)ReadVar(VarType,VarOffset);
	}
	else
	{
		return 0;
	}
}

arithmtype Function(void)
{
	char tcFonc[ 20 ], *pFonc;
	int Res = 0;

	/* which function ? */
	pFonc = tcFonc;
	while((unsigned int)(pFonc-tcFonc)<sizeof(tcFonc)-1 && *Expr>='A' && *Expr<='Z')
	{
		*pFonc++ = *Expr;
		Expr++;
	}
	*pFonc = '\0';

	/* functions with one parameter = variable */
	if ( !strcmp(tcFonc, "ABS") )
	{
		Expr++; /* ( */
		Res = Variable( );
		if ( Res<0 )
			Res = Res * -1;
		Expr++; /* ) */
		return Res;
	}

	/* functions with many parameters = many variables separated per ',' */
	if ( !strcmp(tcFonc, "MINI") )
	{
		Res = 0x7FFFFFFF;
		do
		{
			int iValVar;
			Expr++; /* ( -ou- , */
			iValVar = Variable( );
			if ( iValVar<Res )
				Res = iValVar;
		}
		while( *Expr!=')' );
		Expr++; /* ) */
		return Res;
	}
	if ( !strcmp(tcFonc, "MAXI") )
	{
		Res = 0x80000000;
		do
		{
			int iValVar;
			Expr++; /* ( -or- , */
			iValVar = Variable( );
			if ( iValVar>Res )
				Res = iValVar;
		}
		while( *Expr!=')' );
		Expr++; /* ) */
		return Res;
	}
	if ( !strcmp(tcFonc, "MOY") /*original french term!*/ || !strcmp(tcFonc, "AVG") /*added latter!!!*/ )
	{
		int NbrVars = 0;
		do
		{
			int ValVar;
			Expr++; /* ( -or- , */
			ValVar = Variable( );
			NbrVars++;
			Res = Res + ValVar;
		}
		while( *Expr!=')' );
		Expr++; /* ) */
		Res = Res/NbrVars;
		return Res;
	}

	/* functions with parameter = term */
//	int iValeurTerm = Term();
//	if ( !strcmp(tcFonc, "") )
//	{
//	}


	ErrorDesc = "Unknown function";
	SyntaxError();

	return 0;
}

arithmtype Term(void)
{
//if (UnderVerify)
//printf("Term_Expr=%s (%c)\n",Expr, *Expr);
	if (*Expr=='(')
	{
		arithmtype Res;
		Expr++;
//		Res = AddSub();
		Res = Or();
		if (*Expr!=')')
		{
			ErrorDesc = "Missing parenthesis";
			SyntaxError();
		}
		Expr++;
		return Res;
	}
	else if ( (*Expr>='0' && *Expr<='9') || (*Expr=='$') || (*Expr=='-') )
		return Constant();
	else if (*Expr>='A' && *Expr<='Z')
		return Function();
	else if (*Expr=='@')
	{
		return Variable();
	}
	else if (*Expr=='!')
	{
		Expr++;
		return Term()?0:1;
	}
	else
	{
if (UnderVerify)
rtapi_print("TermERROR!_ExprHere=%s\n",Expr);
		ErrorDesc = "Unknown term";
		SyntaxError();
		return 0;
	}
	return 0;
}

arithmtype Pow(void)
{
	arithmtype Q,Res = Term();
	while(*Expr=='^')
	{
		if ( ErrorDesc )
			break;
		Expr++;
		Q = Pow();
		Res = pow_int(Res,Q);
	}
	return Res;
}

arithmtype MulDivMod(void)
{
	int Val=0;
	arithmtype Res = Pow();
	while(1)
	{
		if ( ErrorDesc )
			break;
//if (UnderVerify)
//printf("MulDivMod_Expr=%s\n",Expr);
		if (*Expr=='*')
		{
			Expr++;
			Res = Res * Pow();
		}
		else
		if (*Expr=='/')
		{
			Expr++;
			Val = Pow();
			if ( ErrorDesc==NULL )
				Res = Res / Val;
		}
		else
		if (*Expr=='%')
		{
			Expr++;
			Val = Pow();
			if ( ErrorDesc==NULL )
				Res = Res % Val;
		}
		else
		{
			break;
		}
	}
	return Res;
}

arithmtype AddSub(void)
{
	arithmtype Res = MulDivMod();
	while(1)
	{
		if ( ErrorDesc )
			break;
//if (UnderVerify)
//printf("AddSub_Expr=%s\n",Expr);
		if (*Expr=='+')
		{
			Expr++;
			Res = Res + MulDivMod();
		}
		else
		if (*Expr=='-')
		{
			Expr++;
			Res = Res - MulDivMod();
		}
		else
		{
			break;
		}
	}
	return Res;
}

arithmtype And(void)
{
	arithmtype Res = AddSub();
	while(1)
	{
		if ( ErrorDesc )
			break;
		if (*Expr=='&')
		{
			Expr++;
			Res = Res & AddSub();
		}
		else
		{
			break;
		}
	}
	return Res;
}
arithmtype Xor(void)
{
	arithmtype Res = And();
	while(1)
	{
		if ( ErrorDesc )
			break;
		if (*Expr=='^')
		{
			Expr++;
			Res = Res ^ And();
		}
		else
		{
			break;
		}
	}
	return Res;
}
arithmtype Or(void)
{
	arithmtype Res = Xor();
	while(1)
	{
		if ( ErrorDesc )
			break;
		if (*Expr=='|')
		{
			Expr++;
			Res = Res | Xor();
		}
		else
		{
			break;
		}
	}
	return Res;
}

arithmtype EvalExpression(char * ExprString)
{
	arithmtype Res;
	Expr = ExprString;
//    Res = AddSub();
	ErrorDesc = NULL;
	Res = Or();

	return Res;
}



/* Result of the comparison of 2 arithmetics expressions : */
/* Expr1 ... Expr2 where ... can be : < , > , = , <= , >= , <> */
int EvalCompare(char * CompareString)
{
	char * FirstExpr,* SecondExpr = NULL;
	char StrCopy[ARITHM_EXPR_SIZE+1]; /* used for putting null char after first expr */
	char * SearchSep;
	char * CutFirst;
	int Found = FALSE;
	int BoolRes = 0;

	/* null expression ? */
	if (*CompareString=='\0' || *CompareString=='#')
		return BoolRes;

	strcpy(StrCopy,CompareString);

	/* search for '>' or '<' or '=' or '>=' or '<=' */
	CutFirst = FirstExpr = StrCopy;
	SearchSep = CompareString;
	do
	{
		if ( (*SearchSep=='>') || (*SearchSep=='<') || (*SearchSep=='=') )
		{
			Found = TRUE;
			*CutFirst = '\0';
			CutFirst++;
			SecondExpr = CutFirst;
			/* 2 chars if '>=' or '<=' or '<>' */
			if ( *CutFirst=='=' || *CutFirst=='>')
			{
				CutFirst++;
				SecondExpr = CutFirst;
			}
		}
		else
		{
			SearchSep++;
			CutFirst++;
		}
	}
	while (*SearchSep!='\0' && !Found);
	if (Found)
	{
		arithmtype EvalFirst,EvalSecond;
//printf("EvalCompare FirstString=%s , SecondString=%s\n",FirstExpr,SecondExpr);
		EvalFirst = EvalExpression(FirstExpr);
		EvalSecond = EvalExpression(SecondExpr);
//printf("EvalCompare ResultFirst=%d , ResultSecond=%d\n",EvalFirst,EvalSecond);
		/* verify if compare is true */
		if ( *SearchSep=='>' && EvalFirst>EvalSecond )
			BoolRes = 1;
		if ( *SearchSep=='<' && *(SearchSep+1)!='>' && EvalFirst<EvalSecond )
			BoolRes = 1;
		if ( *SearchSep=='<' && *(SearchSep+1)=='>' && EvalFirst!=EvalSecond )
			BoolRes = 1;
		if ( (*SearchSep=='=' || *(SearchSep+1)=='=') && EvalFirst==EvalSecond )
			BoolRes = 1;
	}
	else
	{
		ErrorDesc = "Missing < or > or = or ... to make compare";
		SyntaxError();
	}
//printf("Eval FinalBoolResult = %d\n",BoolRes);
	return BoolRes;
}

/* Calc the new value of a variable from an arithmetic expression : */
/* VarDest := ArithmExpr */
void MakeCalc(char * CalcString,int VerifyMode)
{
	char StrCopy[ARITHM_EXPR_SIZE+1]; /* used for putting null char after first expr */
	int TargetVarType,TargetVarOffset;
	int  Found = FALSE;

	/* null expression ? */
	if (*CalcString=='\0' || *CalcString=='#')
		return;

	strcpy(StrCopy,CalcString);

	Expr = StrCopy;
	if (IdentifyFinalVar(Expr,&TargetVarType,&TargetVarOffset))
	{
		/* flush var found */
		Expr++;
		do
		{
			Expr++;
		}
		while( (*Expr!='@') && (*Expr!='\0') );
		Expr++;
		/* verify if there is the '=' or ':=' */
		do
		{
			if (*Expr==':')
				Expr++;
			if (*Expr=='=')
			{
				Found = TRUE;
				Expr++;
			}
			if (*Expr==' ')
				Expr++;
		}
		while( !Found && *Expr!='\0' );
		while( *Expr==' ')
			Expr++;
		if (Found)
		{
			arithmtype EvalExpr;
//printf("Calc - Eval String=%s\n",Expr);
			EvalExpr = EvalExpression(Expr);
//printf("Calc - Result=%d\n",EvalExpr);
			if (!VerifyMode)
			{
				WriteVar(TargetVarType,TargetVarOffset,(int)EvalExpr);
			}
#ifdef GTK_INTERFACE
			else
			{
				if ( !TestVarIsReadWrite( TargetVarType, TargetVarOffset ) )
				{
					ErrorDesc = "Target variable must be read/write !";
					SyntaxError();
				}
			}
#endif
		}
		else
		{
			ErrorDesc = "Missing := to make operate";
			SyntaxError();
		}
	}
}

/* Used one time after user input to verify syntax only */
/* return NULL if ok, else pointer on error description */
char * VerifySyntaxForEvalCompare(char * StringToVerify)
{
	UnderVerify = TRUE;
	VerifyErrorDesc = NULL;
	EvalCompare(StringToVerify);
	UnderVerify = FALSE;
	return VerifyErrorDesc;
}
/* Used one time after user input to verify syntax only */
/* return NULL if ok, else pointer on error description */
char * VerifySyntaxForMakeCalc(char * StringToVerify)
{
	UnderVerify = TRUE;
	VerifyErrorDesc = NULL;
	MakeCalc(StringToVerify,TRUE /* verify mode */);
	UnderVerify = FALSE;
	return VerifyErrorDesc;
}

