
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C
   
      Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.4.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Copy the first part of user declarations.  */

/* Line 189 of yacc.c  */
#line 21 "dtc-parser.y"

#include <stdio.h>
#include <inttypes.h>

#include "dtc.h"
#include "srcpos.h"

YYLTYPE yylloc;

extern int yylex(void);
extern void print_error(char const *fmt, ...);
extern void yyerror(char const *s);

extern struct boot_info *the_boot_info;
extern int treesource_error;

static unsigned long long eval_literal(const char *s, int base, int bits);
static unsigned char eval_char_literal(const char *s);


/* Line 189 of yacc.c  */
#line 94 "dtc-parser.tab.c"

/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     DT_V1 = 258,
     DT_PLUGIN = 259,
     DT_MEMRESERVE = 260,
     DT_LSHIFT = 261,
     DT_RSHIFT = 262,
     DT_LE = 263,
     DT_GE = 264,
     DT_EQ = 265,
     DT_NE = 266,
     DT_AND = 267,
     DT_OR = 268,
     DT_BITS = 269,
     DT_DEL_PROP = 270,
     DT_DEL_NODE = 271,
     DT_PROPNODENAME = 272,
     DT_LITERAL = 273,
     DT_CHAR_LITERAL = 274,
     DT_BASE = 275,
     DT_BYTE = 276,
     DT_STRING = 277,
     DT_LABEL = 278,
     DT_REF = 279,
     DT_INCBIN = 280
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 214 of yacc.c  */
#line 41 "dtc-parser.y"

	char *propnodename;
	char *literal;
	char *labelref;
	unsigned int cbase;
	uint8_t byte;
	struct data data;

	struct {
		struct data	data;
		int		bits;
	} array;

	struct property *prop;
	struct property *proplist;
	struct node *node;
	struct node *nodelist;
	struct reserve_info *re;
	uint64_t integer;
	int is_plugin;



/* Line 214 of yacc.c  */
#line 179 "dtc-parser.tab.c"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 264 of yacc.c  */
#line 191 "dtc-parser.tab.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  4
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   134

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  49
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  29
/* YYNRULES -- Number of rules.  */
#define YYNRULES  81
/* YYNRULES -- Number of states.  */
#define YYNSTATES  144

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   280

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    48,     2,     2,     2,    46,    42,     2,
      34,    36,    45,    43,    35,    44,     2,    27,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    39,    26,
      37,    30,    31,    38,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    32,     2,    33,    41,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    28,    40,    29,    47,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     9,    10,    13,    14,    17,    22,    25,
      28,    32,    36,    41,    47,    48,    51,    56,    59,    63,
      66,    69,    73,    78,    81,    91,    97,   100,   101,   104,
     107,   111,   113,   116,   119,   122,   124,   126,   130,   132,
     134,   140,   142,   146,   148,   152,   154,   158,   160,   164,
     166,   170,   172,   176,   180,   182,   186,   190,   194,   198,
     202,   206,   208,   212,   216,   218,   222,   226,   230,   232,
     234,   237,   240,   243,   244,   247,   250,   251,   254,   257,
     260,   264
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      50,     0,    -1,     3,    26,    51,    52,    54,    -1,    -1,
       4,    26,    -1,    -1,    53,    52,    -1,     5,    61,    61,
      26,    -1,    23,    53,    -1,    27,    55,    -1,    54,    27,
      55,    -1,    54,    24,    55,    -1,    54,    16,    24,    26,
      -1,    28,    56,    76,    29,    26,    -1,    -1,    56,    57,
      -1,    17,    30,    58,    26,    -1,    17,    26,    -1,    15,
      17,    26,    -1,    23,    57,    -1,    59,    22,    -1,    59,
      60,    31,    -1,    59,    32,    75,    33,    -1,    59,    24,
      -1,    59,    25,    34,    22,    35,    61,    35,    61,    36,
      -1,    59,    25,    34,    22,    36,    -1,    58,    23,    -1,
      -1,    58,    35,    -1,    59,    23,    -1,    14,    18,    37,
      -1,    37,    -1,    60,    61,    -1,    60,    24,    -1,    60,
      23,    -1,    18,    -1,    19,    -1,    34,    62,    36,    -1,
      63,    -1,    64,    -1,    64,    38,    62,    39,    63,    -1,
      65,    -1,    64,    13,    65,    -1,    66,    -1,    65,    12,
      66,    -1,    67,    -1,    66,    40,    67,    -1,    68,    -1,
      67,    41,    68,    -1,    69,    -1,    68,    42,    69,    -1,
      70,    -1,    69,    10,    70,    -1,    69,    11,    70,    -1,
      71,    -1,    70,    37,    71,    -1,    70,    31,    71,    -1,
      70,     8,    71,    -1,    70,     9,    71,    -1,    71,     6,
      72,    -1,    71,     7,    72,    -1,    72,    -1,    72,    43,
      73,    -1,    72,    44,    73,    -1,    73,    -1,    73,    45,
      74,    -1,    73,    27,    74,    -1,    73,    46,    74,    -1,
      74,    -1,    61,    -1,    44,    74,    -1,    47,    74,    -1,
      48,    74,    -1,    -1,    75,    21,    -1,    75,    23,    -1,
      -1,    77,    76,    -1,    77,    57,    -1,    17,    55,    -1,
      16,    17,    26,    -1,    23,    77,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   113,   113,   124,   127,   135,   138,   145,   149,   157,
     161,   165,   175,   189,   197,   200,   207,   211,   215,   219,
     227,   231,   235,   239,   243,   260,   270,   278,   281,   285,
     292,   307,   312,   332,   346,   353,   357,   361,   368,   372,
     373,   377,   378,   382,   383,   387,   388,   392,   393,   397,
     398,   402,   403,   404,   408,   409,   410,   411,   412,   416,
     417,   418,   422,   423,   424,   428,   429,   430,   431,   435,
     436,   437,   438,   443,   446,   450,   458,   461,   465,   473,
     477,   481
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "DT_V1", "DT_PLUGIN", "DT_MEMRESERVE",
  "DT_LSHIFT", "DT_RSHIFT", "DT_LE", "DT_GE", "DT_EQ", "DT_NE", "DT_AND",
  "DT_OR", "DT_BITS", "DT_DEL_PROP", "DT_DEL_NODE", "DT_PROPNODENAME",
  "DT_LITERAL", "DT_CHAR_LITERAL", "DT_BASE", "DT_BYTE", "DT_STRING",
  "DT_LABEL", "DT_REF", "DT_INCBIN", "';'", "'/'", "'{'", "'}'", "'='",
  "'>'", "'['", "']'", "'('", "','", "')'", "'<'", "'?'", "':'", "'|'",
  "'^'", "'&'", "'+'", "'-'", "'*'", "'%'", "'~'", "'!'", "$accept",
  "sourcefile", "plugindecl", "memreserves", "memreserve", "devicetree",
  "nodedef", "proplist", "propdef", "propdata", "propdataprefix",
  "arrayprefix", "integer_prim", "integer_expr", "integer_trinary",
  "integer_or", "integer_and", "integer_bitor", "integer_bitxor",
  "integer_bitand", "integer_eq", "integer_rela", "integer_shift",
  "integer_add", "integer_mul", "integer_unary", "bytestring", "subnodes",
  "subnode", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,    59,    47,   123,   125,
      61,    62,    91,    93,    40,    44,    41,    60,    63,    58,
     124,    94,    38,    43,    45,    42,    37,   126,    33
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    49,    50,    51,    51,    52,    52,    53,    53,    54,
      54,    54,    54,    55,    56,    56,    57,    57,    57,    57,
      58,    58,    58,    58,    58,    58,    58,    59,    59,    59,
      60,    60,    60,    60,    60,    61,    61,    61,    62,    63,
      63,    64,    64,    65,    65,    66,    66,    67,    67,    68,
      68,    69,    69,    69,    70,    70,    70,    70,    70,    71,
      71,    71,    72,    72,    72,    73,    73,    73,    73,    74,
      74,    74,    74,    75,    75,    75,    76,    76,    76,    77,
      77,    77
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     5,     0,     2,     0,     2,     4,     2,     2,
       3,     3,     4,     5,     0,     2,     4,     2,     3,     2,
       2,     3,     4,     2,     9,     5,     2,     0,     2,     2,
       3,     1,     2,     2,     2,     1,     1,     3,     1,     1,
       5,     1,     3,     1,     3,     1,     3,     1,     3,     1,
       3,     1,     3,     3,     1,     3,     3,     3,     3,     3,
       3,     1,     3,     3,     1,     3,     3,     3,     1,     1,
       2,     2,     2,     0,     2,     2,     0,     2,     2,     2,
       3,     2
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     0,     0,     3,     1,     0,     5,     4,     0,     0,
       0,     5,    35,    36,     0,     0,     8,     0,     2,     6,
       0,     0,     0,    69,     0,    38,    39,    41,    43,    45,
      47,    49,    51,    54,    61,    64,    68,     0,    14,     9,
       0,     0,     0,    70,    71,    72,    37,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     7,    76,     0,    11,
      10,    42,     0,    44,    46,    48,    50,    52,    53,    57,
      58,    56,    55,    59,    60,    62,    63,    66,    65,    67,
       0,     0,     0,     0,    15,     0,    76,    12,     0,     0,
       0,    17,    27,    79,    19,    81,     0,    78,    77,    40,
      18,    80,     0,     0,    13,    26,    16,    28,     0,    20,
      29,    23,     0,    73,    31,     0,     0,     0,     0,    34,
      33,    21,    32,    30,     0,    74,    75,    22,     0,    25,
       0,     0,     0,    24
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     2,     6,    10,    11,    18,    39,    67,    94,   112,
     113,   125,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,   128,    95,    96
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -81
static const yytype_int8 yypact[] =
{
      33,    44,    63,    47,   -81,    46,     9,   -81,    22,     9,
      55,     9,   -81,   -81,   -10,    22,   -81,    -3,    37,   -81,
     -10,   -10,   -10,   -81,    49,   -81,    -7,    76,    50,    48,
      52,     8,     2,    36,   -15,    -1,   -81,    65,   -81,   -81,
      68,    -3,    -3,   -81,   -81,   -81,   -81,   -10,   -10,   -10,
     -10,   -10,   -10,   -10,   -10,   -10,   -10,   -10,   -10,   -10,
     -10,   -10,   -10,   -10,   -10,   -10,   -81,    51,    67,   -81,
     -81,    76,    56,    50,    48,    52,     8,     2,     2,    36,
      36,    36,    36,   -15,   -15,    -1,    -1,   -81,   -81,   -81,
      79,    80,    45,    51,   -81,    69,    51,   -81,   -10,    73,
      74,   -81,   -81,   -81,   -81,   -81,    75,   -81,   -81,   -81,
     -81,   -81,    34,    -2,   -81,   -81,   -81,   -81,    84,   -81,
     -81,   -81,    70,   -81,   -81,    31,    66,    83,    -6,   -81,
     -81,   -81,   -81,   -81,    23,   -81,   -81,   -81,    22,   -81,
      71,    22,    72,   -81
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -81,   -81,   -81,    96,   100,   -81,   -40,   -81,   -80,   -81,
     -81,   -81,    -8,    62,    13,   -81,    77,    64,    78,    61,
      82,    27,    21,    24,    25,   -17,   -81,    18,    26
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const yytype_uint8 yytable[] =
{
      15,    69,    70,    43,    44,    45,    47,    37,    12,    13,
      55,    56,   118,   104,     8,   135,   107,   136,    53,    54,
     119,   120,   121,   122,    14,    38,    63,   137,    61,    62,
     123,    48,     9,    57,    20,   124,     1,    21,    22,    58,
      12,    13,    59,    60,    64,    65,    87,    88,    89,    12,
      13,     5,   103,    40,   129,   130,    14,   115,   138,   139,
     116,    41,   131,     4,    42,    14,    90,    91,    92,   117,
       3,   101,     7,    38,    93,   102,    79,    80,    81,    82,
      77,    78,    17,    83,    84,    46,    85,    86,    49,    51,
      50,    66,    68,    97,    52,    98,    99,   100,   106,   110,
     111,   114,   126,   133,   127,   134,   141,    19,   143,    16,
      72,   109,    75,    73,   108,     0,     0,   132,     0,   105,
       0,     0,     0,     0,    71,     0,     0,     0,    74,     0,
     140,     0,     0,   142,    76
};

static const yytype_int16 yycheck[] =
{
       8,    41,    42,    20,    21,    22,    13,    15,    18,    19,
       8,     9,    14,    93,     5,    21,    96,    23,    10,    11,
      22,    23,    24,    25,    34,    28,    27,    33,    43,    44,
      32,    38,    23,    31,    44,    37,     3,    47,    48,    37,
      18,    19,     6,     7,    45,    46,    63,    64,    65,    18,
      19,     4,    92,    16,    23,    24,    34,    23,    35,    36,
      26,    24,    31,     0,    27,    34,    15,    16,    17,    35,
      26,    26,    26,    28,    23,    30,    55,    56,    57,    58,
      53,    54,    27,    59,    60,    36,    61,    62,    12,    41,
      40,    26,    24,    26,    42,    39,    17,    17,    29,    26,
      26,    26,    18,    37,    34,    22,    35,    11,    36,     9,
      48,    98,    51,    49,    96,    -1,    -1,   125,    -1,    93,
      -1,    -1,    -1,    -1,    47,    -1,    -1,    -1,    50,    -1,
     138,    -1,    -1,   141,    52
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     3,    50,    26,     0,     4,    51,    26,     5,    23,
      52,    53,    18,    19,    34,    61,    53,    27,    54,    52,
      44,    47,    48,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    61,    28,    55,
      16,    24,    27,    74,    74,    74,    36,    13,    38,    12,
      40,    41,    42,    10,    11,     8,     9,    31,    37,     6,
       7,    43,    44,    27,    45,    46,    26,    56,    24,    55,
      55,    65,    62,    66,    67,    68,    69,    70,    70,    71,
      71,    71,    71,    72,    72,    73,    73,    74,    74,    74,
      15,    16,    17,    23,    57,    76,    77,    26,    39,    17,
      17,    26,    30,    55,    57,    77,    29,    57,    76,    63,
      26,    26,    58,    59,    26,    23,    26,    35,    14,    22,
      23,    24,    25,    32,    37,    60,    18,    34,    75,    23,
      24,    31,    61,    37,    22,    21,    23,    33,    35,    36,
      61,    35,    61,    36
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yyrule)
    YYSTYPE *yyvsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  YYUSE (yyvaluep);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}

/* Prevent warnings from -Wmissing-prototypes.  */
#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */


/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*-------------------------.
| yyparse or yypush_parse.  |
`-------------------------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{


    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.

       Refer to the stacks thru separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */
  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss_alloc, yyss);
	YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:

/* Line 1455 of yacc.c  */
#line 114 "dtc-parser.y"
    {
			(yyvsp[(5) - (5)].node)->is_plugin = (yyvsp[(3) - (5)].is_plugin);
			(yyvsp[(5) - (5)].node)->is_root = 1;
			the_boot_info = build_boot_info((yyvsp[(4) - (5)].re), (yyvsp[(5) - (5)].node),
							guess_boot_cpuid((yyvsp[(5) - (5)].node)));
		;}
    break;

  case 3:

/* Line 1455 of yacc.c  */
#line 124 "dtc-parser.y"
    {
			(yyval.is_plugin) = 0;
		;}
    break;

  case 4:

/* Line 1455 of yacc.c  */
#line 128 "dtc-parser.y"
    {
			(yyval.is_plugin) = 1;
		;}
    break;

  case 5:

/* Line 1455 of yacc.c  */
#line 135 "dtc-parser.y"
    {
			(yyval.re) = NULL;
		;}
    break;

  case 6:

/* Line 1455 of yacc.c  */
#line 139 "dtc-parser.y"
    {
			(yyval.re) = chain_reserve_entry((yyvsp[(1) - (2)].re), (yyvsp[(2) - (2)].re));
		;}
    break;

  case 7:

/* Line 1455 of yacc.c  */
#line 146 "dtc-parser.y"
    {
			(yyval.re) = build_reserve_entry((yyvsp[(2) - (4)].integer), (yyvsp[(3) - (4)].integer));
		;}
    break;

  case 8:

/* Line 1455 of yacc.c  */
#line 150 "dtc-parser.y"
    {
			add_label(&(yyvsp[(2) - (2)].re)->labels, (yyvsp[(1) - (2)].labelref));
			(yyval.re) = (yyvsp[(2) - (2)].re);
		;}
    break;

  case 9:

/* Line 1455 of yacc.c  */
#line 158 "dtc-parser.y"
    {
			(yyval.node) = name_node((yyvsp[(2) - (2)].node), "");
		;}
    break;

  case 10:

/* Line 1455 of yacc.c  */
#line 162 "dtc-parser.y"
    {
			(yyval.node) = merge_nodes((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		;}
    break;

  case 11:

/* Line 1455 of yacc.c  */
#line 166 "dtc-parser.y"
    {
			struct node *target = get_node_by_ref((yyvsp[(1) - (3)].node), (yyvsp[(2) - (3)].labelref));

			if (target)
				merge_nodes(target, (yyvsp[(3) - (3)].node));
			else
				print_error("label or path, '%s', not found", (yyvsp[(2) - (3)].labelref));
			(yyval.node) = (yyvsp[(1) - (3)].node);
		;}
    break;

  case 12:

/* Line 1455 of yacc.c  */
#line 176 "dtc-parser.y"
    {
			struct node *target = get_node_by_ref((yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].labelref));

			if (!target)
				print_error("label or path, '%s', not found", (yyvsp[(3) - (4)].labelref));
			else
				delete_node(target);

			(yyval.node) = (yyvsp[(1) - (4)].node);
		;}
    break;

  case 13:

/* Line 1455 of yacc.c  */
#line 190 "dtc-parser.y"
    {
			(yyval.node) = build_node((yyvsp[(2) - (5)].proplist), (yyvsp[(3) - (5)].nodelist));
		;}
    break;

  case 14:

/* Line 1455 of yacc.c  */
#line 197 "dtc-parser.y"
    {
			(yyval.proplist) = NULL;
		;}
    break;

  case 15:

/* Line 1455 of yacc.c  */
#line 201 "dtc-parser.y"
    {
			(yyval.proplist) = chain_property((yyvsp[(2) - (2)].prop), (yyvsp[(1) - (2)].proplist));
		;}
    break;

  case 16:

/* Line 1455 of yacc.c  */
#line 208 "dtc-parser.y"
    {
			(yyval.prop) = build_property((yyvsp[(1) - (4)].propnodename), (yyvsp[(3) - (4)].data));
		;}
    break;

  case 17:

/* Line 1455 of yacc.c  */
#line 212 "dtc-parser.y"
    {
			(yyval.prop) = build_property((yyvsp[(1) - (2)].propnodename), empty_data);
		;}
    break;

  case 18:

/* Line 1455 of yacc.c  */
#line 216 "dtc-parser.y"
    {
			(yyval.prop) = build_property_delete((yyvsp[(2) - (3)].propnodename));
		;}
    break;

  case 19:

/* Line 1455 of yacc.c  */
#line 220 "dtc-parser.y"
    {
			add_label(&(yyvsp[(2) - (2)].prop)->labels, (yyvsp[(1) - (2)].labelref));
			(yyval.prop) = (yyvsp[(2) - (2)].prop);
		;}
    break;

  case 20:

/* Line 1455 of yacc.c  */
#line 228 "dtc-parser.y"
    {
			(yyval.data) = data_merge((yyvsp[(1) - (2)].data), (yyvsp[(2) - (2)].data));
		;}
    break;

  case 21:

/* Line 1455 of yacc.c  */
#line 232 "dtc-parser.y"
    {
			(yyval.data) = data_merge((yyvsp[(1) - (3)].data), (yyvsp[(2) - (3)].array).data);
		;}
    break;

  case 22:

/* Line 1455 of yacc.c  */
#line 236 "dtc-parser.y"
    {
			(yyval.data) = data_merge((yyvsp[(1) - (4)].data), (yyvsp[(3) - (4)].data));
		;}
    break;

  case 23:

/* Line 1455 of yacc.c  */
#line 240 "dtc-parser.y"
    {
			(yyval.data) = data_add_marker((yyvsp[(1) - (2)].data), REF_PATH, (yyvsp[(2) - (2)].labelref));
		;}
    break;

  case 24:

/* Line 1455 of yacc.c  */
#line 244 "dtc-parser.y"
    {
			FILE *f = srcfile_relative_open((yyvsp[(4) - (9)].data).val, NULL);
			struct data d;

			if ((yyvsp[(6) - (9)].integer) != 0)
				if (fseek(f, (yyvsp[(6) - (9)].integer), SEEK_SET) != 0)
					print_error("Couldn't seek to offset %llu in \"%s\": %s",
						     (unsigned long long)(yyvsp[(6) - (9)].integer),
						     (yyvsp[(4) - (9)].data).val,
						     strerror(errno));

			d = data_copy_file(f, (yyvsp[(8) - (9)].integer));

			(yyval.data) = data_merge((yyvsp[(1) - (9)].data), d);
			fclose(f);
		;}
    break;

  case 25:

/* Line 1455 of yacc.c  */
#line 261 "dtc-parser.y"
    {
			FILE *f = srcfile_relative_open((yyvsp[(4) - (5)].data).val, NULL);
			struct data d = empty_data;

			d = data_copy_file(f, -1);

			(yyval.data) = data_merge((yyvsp[(1) - (5)].data), d);
			fclose(f);
		;}
    break;

  case 26:

/* Line 1455 of yacc.c  */
#line 271 "dtc-parser.y"
    {
			(yyval.data) = data_add_marker((yyvsp[(1) - (2)].data), LABEL, (yyvsp[(2) - (2)].labelref));
		;}
    break;

  case 27:

/* Line 1455 of yacc.c  */
#line 278 "dtc-parser.y"
    {
			(yyval.data) = empty_data;
		;}
    break;

  case 28:

/* Line 1455 of yacc.c  */
#line 282 "dtc-parser.y"
    {
			(yyval.data) = (yyvsp[(1) - (2)].data);
		;}
    break;

  case 29:

/* Line 1455 of yacc.c  */
#line 286 "dtc-parser.y"
    {
			(yyval.data) = data_add_marker((yyvsp[(1) - (2)].data), LABEL, (yyvsp[(2) - (2)].labelref));
		;}
    break;

  case 30:

/* Line 1455 of yacc.c  */
#line 293 "dtc-parser.y"
    {
			(yyval.array).data = empty_data;
			(yyval.array).bits = eval_literal((yyvsp[(2) - (3)].literal), 0, 7);

			if (((yyval.array).bits !=  8) &&
			    ((yyval.array).bits != 16) &&
			    ((yyval.array).bits != 32) &&
			    ((yyval.array).bits != 64))
			{
				print_error("Only 8, 16, 32 and 64-bit elements"
					    " are currently supported");
				(yyval.array).bits = 32;
			}
		;}
    break;

  case 31:

/* Line 1455 of yacc.c  */
#line 308 "dtc-parser.y"
    {
			(yyval.array).data = empty_data;
			(yyval.array).bits = 32;
		;}
    break;

  case 32:

/* Line 1455 of yacc.c  */
#line 313 "dtc-parser.y"
    {
			if ((yyvsp[(1) - (2)].array).bits < 64) {
				uint64_t mask = (1ULL << (yyvsp[(1) - (2)].array).bits) - 1;
				/*
				 * Bits above mask must either be all zero
				 * (positive within range of mask) or all one
				 * (negative and sign-extended). The second
				 * condition is true if when we set all bits
				 * within the mask to one (i.e. | in the
				 * mask), all bits are one.
				 */
				if (((yyvsp[(2) - (2)].integer) > mask) && (((yyvsp[(2) - (2)].integer) | mask) != -1ULL))
					print_error(
						"integer value out of range "
						"%016lx (%d bits)", (yyvsp[(1) - (2)].array).bits);
			}

			(yyval.array).data = data_append_integer((yyvsp[(1) - (2)].array).data, (yyvsp[(2) - (2)].integer), (yyvsp[(1) - (2)].array).bits);
		;}
    break;

  case 33:

/* Line 1455 of yacc.c  */
#line 333 "dtc-parser.y"
    {
			uint64_t val = ~0ULL >> (64 - (yyvsp[(1) - (2)].array).bits);

			if ((yyvsp[(1) - (2)].array).bits == 32)
				(yyvsp[(1) - (2)].array).data = data_add_marker((yyvsp[(1) - (2)].array).data,
							  REF_PHANDLE,
							  (yyvsp[(2) - (2)].labelref));
			else
				print_error("References are only allowed in "
					    "arrays with 32-bit elements.");

			(yyval.array).data = data_append_integer((yyvsp[(1) - (2)].array).data, val, (yyvsp[(1) - (2)].array).bits);
		;}
    break;

  case 34:

/* Line 1455 of yacc.c  */
#line 347 "dtc-parser.y"
    {
			(yyval.array).data = data_add_marker((yyvsp[(1) - (2)].array).data, LABEL, (yyvsp[(2) - (2)].labelref));
		;}
    break;

  case 35:

/* Line 1455 of yacc.c  */
#line 354 "dtc-parser.y"
    {
			(yyval.integer) = eval_literal((yyvsp[(1) - (1)].literal), 0, 64);
		;}
    break;

  case 36:

/* Line 1455 of yacc.c  */
#line 358 "dtc-parser.y"
    {
			(yyval.integer) = eval_char_literal((yyvsp[(1) - (1)].literal));
		;}
    break;

  case 37:

/* Line 1455 of yacc.c  */
#line 362 "dtc-parser.y"
    {
			(yyval.integer) = (yyvsp[(2) - (3)].integer);
		;}
    break;

  case 40:

/* Line 1455 of yacc.c  */
#line 373 "dtc-parser.y"
    { (yyval.integer) = (yyvsp[(1) - (5)].integer) ? (yyvsp[(3) - (5)].integer) : (yyvsp[(5) - (5)].integer); ;}
    break;

  case 42:

/* Line 1455 of yacc.c  */
#line 378 "dtc-parser.y"
    { (yyval.integer) = (yyvsp[(1) - (3)].integer) || (yyvsp[(3) - (3)].integer); ;}
    break;

  case 44:

/* Line 1455 of yacc.c  */
#line 383 "dtc-parser.y"
    { (yyval.integer) = (yyvsp[(1) - (3)].integer) && (yyvsp[(3) - (3)].integer); ;}
    break;

  case 46:

/* Line 1455 of yacc.c  */
#line 388 "dtc-parser.y"
    { (yyval.integer) = (yyvsp[(1) - (3)].integer) | (yyvsp[(3) - (3)].integer); ;}
    break;

  case 48:

/* Line 1455 of yacc.c  */
#line 393 "dtc-parser.y"
    { (yyval.integer) = (yyvsp[(1) - (3)].integer) ^ (yyvsp[(3) - (3)].integer); ;}
    break;

  case 50:

/* Line 1455 of yacc.c  */
#line 398 "dtc-parser.y"
    { (yyval.integer) = (yyvsp[(1) - (3)].integer) & (yyvsp[(3) - (3)].integer); ;}
    break;

  case 52:

/* Line 1455 of yacc.c  */
#line 403 "dtc-parser.y"
    { (yyval.integer) = (yyvsp[(1) - (3)].integer) == (yyvsp[(3) - (3)].integer); ;}
    break;

  case 53:

/* Line 1455 of yacc.c  */
#line 404 "dtc-parser.y"
    { (yyval.integer) = (yyvsp[(1) - (3)].integer) != (yyvsp[(3) - (3)].integer); ;}
    break;

  case 55:

/* Line 1455 of yacc.c  */
#line 409 "dtc-parser.y"
    { (yyval.integer) = (yyvsp[(1) - (3)].integer) < (yyvsp[(3) - (3)].integer); ;}
    break;

  case 56:

/* Line 1455 of yacc.c  */
#line 410 "dtc-parser.y"
    { (yyval.integer) = (yyvsp[(1) - (3)].integer) > (yyvsp[(3) - (3)].integer); ;}
    break;

  case 57:

/* Line 1455 of yacc.c  */
#line 411 "dtc-parser.y"
    { (yyval.integer) = (yyvsp[(1) - (3)].integer) <= (yyvsp[(3) - (3)].integer); ;}
    break;

  case 58:

/* Line 1455 of yacc.c  */
#line 412 "dtc-parser.y"
    { (yyval.integer) = (yyvsp[(1) - (3)].integer) >= (yyvsp[(3) - (3)].integer); ;}
    break;

  case 59:

/* Line 1455 of yacc.c  */
#line 416 "dtc-parser.y"
    { (yyval.integer) = (yyvsp[(1) - (3)].integer) << (yyvsp[(3) - (3)].integer); ;}
    break;

  case 60:

/* Line 1455 of yacc.c  */
#line 417 "dtc-parser.y"
    { (yyval.integer) = (yyvsp[(1) - (3)].integer) >> (yyvsp[(3) - (3)].integer); ;}
    break;

  case 62:

/* Line 1455 of yacc.c  */
#line 422 "dtc-parser.y"
    { (yyval.integer) = (yyvsp[(1) - (3)].integer) + (yyvsp[(3) - (3)].integer); ;}
    break;

  case 63:

/* Line 1455 of yacc.c  */
#line 423 "dtc-parser.y"
    { (yyval.integer) = (yyvsp[(1) - (3)].integer) - (yyvsp[(3) - (3)].integer); ;}
    break;

  case 65:

/* Line 1455 of yacc.c  */
#line 428 "dtc-parser.y"
    { (yyval.integer) = (yyvsp[(1) - (3)].integer) * (yyvsp[(3) - (3)].integer); ;}
    break;

  case 66:

/* Line 1455 of yacc.c  */
#line 429 "dtc-parser.y"
    { (yyval.integer) = (yyvsp[(1) - (3)].integer) / (yyvsp[(3) - (3)].integer); ;}
    break;

  case 67:

/* Line 1455 of yacc.c  */
#line 430 "dtc-parser.y"
    { (yyval.integer) = (yyvsp[(1) - (3)].integer) % (yyvsp[(3) - (3)].integer); ;}
    break;

  case 70:

/* Line 1455 of yacc.c  */
#line 436 "dtc-parser.y"
    { (yyval.integer) = -(yyvsp[(2) - (2)].integer); ;}
    break;

  case 71:

/* Line 1455 of yacc.c  */
#line 437 "dtc-parser.y"
    { (yyval.integer) = ~(yyvsp[(2) - (2)].integer); ;}
    break;

  case 72:

/* Line 1455 of yacc.c  */
#line 438 "dtc-parser.y"
    { (yyval.integer) = !(yyvsp[(2) - (2)].integer); ;}
    break;

  case 73:

/* Line 1455 of yacc.c  */
#line 443 "dtc-parser.y"
    {
			(yyval.data) = empty_data;
		;}
    break;

  case 74:

/* Line 1455 of yacc.c  */
#line 447 "dtc-parser.y"
    {
			(yyval.data) = data_append_byte((yyvsp[(1) - (2)].data), (yyvsp[(2) - (2)].byte));
		;}
    break;

  case 75:

/* Line 1455 of yacc.c  */
#line 451 "dtc-parser.y"
    {
			(yyval.data) = data_add_marker((yyvsp[(1) - (2)].data), LABEL, (yyvsp[(2) - (2)].labelref));
		;}
    break;

  case 76:

/* Line 1455 of yacc.c  */
#line 458 "dtc-parser.y"
    {
			(yyval.nodelist) = NULL;
		;}
    break;

  case 77:

/* Line 1455 of yacc.c  */
#line 462 "dtc-parser.y"
    {
			(yyval.nodelist) = chain_node((yyvsp[(1) - (2)].node), (yyvsp[(2) - (2)].nodelist));
		;}
    break;

  case 78:

/* Line 1455 of yacc.c  */
#line 466 "dtc-parser.y"
    {
			print_error("syntax error: properties must precede subnodes");
			YYERROR;
		;}
    break;

  case 79:

/* Line 1455 of yacc.c  */
#line 474 "dtc-parser.y"
    {
			(yyval.node) = name_node((yyvsp[(2) - (2)].node), (yyvsp[(1) - (2)].propnodename));
		;}
    break;

  case 80:

/* Line 1455 of yacc.c  */
#line 478 "dtc-parser.y"
    {
			(yyval.node) = name_node(build_node_delete(), (yyvsp[(2) - (3)].propnodename));
		;}
    break;

  case 81:

/* Line 1455 of yacc.c  */
#line 482 "dtc-parser.y"
    {
			add_label(&(yyvsp[(2) - (2)].node)->labels, (yyvsp[(1) - (2)].labelref));
			(yyval.node) = (yyvsp[(2) - (2)].node);
		;}
    break;



/* Line 1455 of yacc.c  */
#line 2153 "dtc-parser.tab.c"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (yymsg);
	  }
	else
	  {
	    yyerror (YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined(yyoverflow) || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}



/* Line 1675 of yacc.c  */
#line 488 "dtc-parser.y"


void print_error(char const *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	srcpos_verror(&yylloc, fmt, va);
	va_end(va);

	treesource_error = 1;
}

void yyerror(char const *s) {
	print_error("%s", s);
}

static unsigned long long eval_literal(const char *s, int base, int bits)
{
	unsigned long long val;
	char *e;

	errno = 0;
	val = strtoull(s, &e, base);
	if (*e) {
		size_t uls = strspn(e, "UL");
		if (e[uls])
			print_error("bad characters in literal");
	}
	if ((errno == ERANGE)
		 || ((bits < 64) && (val >= (1ULL << bits))))
		print_error("literal out of range");
	else if (errno != 0)
		print_error("bad literal");
	return val;
}

static unsigned char eval_char_literal(const char *s)
{
	int i = 1;
	char c = s[0];

	if (c == '\0')
	{
		print_error("empty character literal");
		return 0;
	}

	/*
	 * If the first character in the character literal is a \ then process
	 * the remaining characters as an escape encoding. If the first
	 * character is neither an escape or a terminator it should be the only
	 * character in the literal and will be returned.
	 */
	if (c == '\\')
		c = get_escape_char(s, &i);

	if (s[i] != '\0')
		print_error("malformed character literal");

	return c;
}

