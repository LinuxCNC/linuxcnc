/* Classic Ladder Project */
/* Copyright (C) 2001-2006 Marc Le Douarain */
/* http://www.multimania.com/mavati/classicladder */
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

//#include <math.h>
#include "classicladder.h"
#include "global.h"
#include "vars_access.h"
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

/* return TRUE if okay */
int IdentifyVariable(char *StartExpr,int * ResType,int * ResOffset)
{
    int VarType,VarOffset;
    char * SearchSep = StartExpr;
    StartExpr++;
    do
    {
        SearchSep++;
    }
    while( (*SearchSep!='/') && (*SearchSep!='\0') );

    if (*SearchSep=='/')
    {
        VarType = atoi(StartExpr);
        SearchSep++;
        StartExpr = SearchSep;           /* ???????? */
        do
        {
            StartExpr++;
        }
        while( (*StartExpr!='@') && (*StartExpr!='\0') );
        if (*StartExpr=='@')
        {
            VarOffset = atoi(SearchSep);
            *ResType = VarType;
            *ResOffset = VarOffset;
            return TRUE;
        }
        else
        {
            ErrorDesc = "Bad var coding 1, should be @xx/yy@";
            SyntaxError();
        }
    }
    else
    {
        ErrorDesc = "Bad var coding 2, should be @xx/yy@";
        SyntaxError();
    }
    return FALSE;
}

arithmtype Variable(void)
{
    int VarType,VarOffset;
    if (IdentifyVariable(Expr,&VarType,&VarOffset))
    {
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

arithmtype Function()
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
	if ( !strcmp(tcFonc, "MOY") )
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
    if (*Expr=='(')
    {
        arithmtype Res;
        Expr++;
        Res = AddSub();
        if (*Expr!=')')
        {
            ErrorDesc = "Missing parenthesis";
            SyntaxError();
        }
        Expr++;
        return Res;
    }
    else
    if ( (*Expr>='0' && *Expr<='9') || (*Expr=='$') || (*Expr=='-') )
        return Constant();
    else
    if (*Expr>='A' && *Expr<='Z')
        return Function();
    else
    if (*Expr=='@')
    {
        return Variable();
    }
    else
    {
        if (*Expr=='!')
        {
            Expr++;
            return Term()?0:1;
        }
        else
        {
            ErrorDesc = "Unknown term";
            SyntaxError();
        }
    }
    return 0;
}

arithmtype Pow(void)
{
    arithmtype Q,Res = Term();
    while(*Expr=='^')
    {
        Expr++;
        Q = Pow();
        Res = pow_int(Res,Q);
    }
    return Res;
}

arithmtype MulDivMod(void)
{
    arithmtype Res = Pow();
    while(1)
    {
        if (*Expr=='*')
        {
            Expr++;
            Res = Res * Pow();
        }
        else
        if (*Expr=='/')
        {
            Expr++;
            Res = Res / Pow();
        }
        else
        if (*Expr=='%')
        {
            Expr++;
            Res = Res % Pow();
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
    if (*CompareString=='\0')
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
        ErrorDesc = "Missing < or > or = or ... to make comparison";
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
    int VarType,VarOffset;
    int  Found = FALSE;

    /* null expression ? */
    if (*CalcString=='\0')
        return;

    strcpy(StrCopy,CalcString);

    Expr = StrCopy;
    if (IdentifyVariable(Expr,&VarType,&VarOffset))
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
                WriteVar(VarType,VarOffset,(int)EvalExpr);
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

