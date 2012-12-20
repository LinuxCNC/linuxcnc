/*******************************************************************************
*                                                                              *
*      File fpP.h,                                                      *
*      All pack 4 dependencies for the MathLib elems plus some defines used    *
*      throughout MathLib.                                                     *
*                                                                              *
*      Copyright © 1991 Apple Computer, Inc.  All rights reserved.             *
*                                                                              *
*      Written by Ali Sazegari, started on October 1991,                       *
*                                                                              *
*      W A R N I N G:  This routine expects a 64 bit double model.             *
*                                                                              *
*******************************************************************************/

#define      NoException            0

/*******************************************************************************
*                              Values of constants.                            *
*******************************************************************************/

//#define    SgnMask            0x8000
#define      dSgnMask           0x80000000
#define      sSgnMask           0x7FFFFFFF

//#define    ExpMask            0x7FFF
#define      dExpMask           0x7FF00000
#define      sExpMask           0xFF000000

                                          /* according to rounding BIG & SMALL are:  */
#define      BIG               1.1e+300   /* used to deliver ±° or largest number,   */
#define      SMALL             1.1e-300   /* used to deliver ±0 or smallest number.  */
#define      InfExp            0x7FF
#define      dMaxExp           0x7FF00000

#define      MaxExpP1          1024
#define      MaxExp            1023

#define      DenormLimit       -52

//#define    ManMask           0x80000000
#define      dManMask          0x00080000

//#define    IsItDenorm         0x80000000
#define      dIsItDenorm        0x00080000

//#define    xIsItSNaN          0x40000000
#define      dIsItSNaN          0x00080000

#define      dHighMan           0x000FFFFF
#define      dFirstBitSet       0x00080000
#define      BIAS               0x3FF

//#define    GetSign            0x8000
#define      dGetSign           0x80000000
#define      sGetSign           0x80000000

//#define    Infinity(x)       ( x.hex.exponent & ExpMask ) == ExpMask
#define      dInfinity(x)      ( x.hex.high & dExpMask ) == dExpMask
#define      sInfinity(x)      ( ( x.hexsgl << 1 ) & sExpMask ) == sExpMask      

//#define    Exponent(x)       x.hex.exponent & ExpMask
#define      dExponent(x)      x.hex.high & dExpMask
#define      sExponent(x)      ( ( x.hexsgl << 1 ) & sExpMask )

#define      sZero(x)          ( x.hexsgl & sSgnMask ) == 0 
//#define    Sign(x)           ( x.hex.exponent & SgnMask ) == SgnMask

/*******************************************************************************
*                        Types used in the auxiliary functions.                *
*******************************************************************************/

typedef struct                   /*      Hex representation of a double.      */
      {
#if defined(__BIG_ENDIAN__)
      unsigned long int high;
      unsigned long int low;
#else
      unsigned long int low;
      unsigned long int high;
#endif
      } dHexParts;

typedef union
      {
      unsigned char byties[8];
      double dbl;
      } DblInHex;

//enum boolean { FALSE, TRUE };

/*******************************************************************************
*       Macros to access long subfields of a double value.                     *
*******************************************************************************/

#define highpartd(x) *((long *) &x)
#define lowpartd(x)  *((long *) &x + 1)

enum {
  FP_SNAN                       = 0,    /*      signaling NaN
      */
  FP_QNAN                       = 1,    /*      quiet NaN
      */
  FP_INFINITE                   = 2,    /*      + or - infinity
      */ 
  FP_ZERO                       = 3,    /*      + or - zero
      */
  FP_NORMAL                     = 4,    /*      all normal numbers 
      */
  FP_SUBNORMAL                  = 5     /*      denormal numbers 
      */
};     

