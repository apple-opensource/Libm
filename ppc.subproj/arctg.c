/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*******************************************************************************
*                                                                              *
*     File atan.c,                                                             *
*     Function atan.                                                           *
*                                                                              *
*     Implementation of arctg based on the approximation provided by           *
*     Taligent, Inc. who based it on sources from IBM.                         *
*                                                                              *
*     Copyright � 1996-2001 Apple Computer, Inc.  All rights reserved.         *
*                                                                              *
*     Written by A. Sazegari, started on June 1996.                            *
*     Modified and ported by Robert A. Murley (ram) for Mac OS X.              *
*                                                                              *
*     A MathLib v4 file.                                                       *
*                                                                              *
*     The principal values of the inverse tangent function are:                *
*                                                                              *
*                 -�/2 � atan(x) � �/2,         -� � x � +�                    *
*                                                                              *
*     August    26 1996: produced a PowerMathLib� version.  this routine       *
*                        does not handle edge cases and does not set the       *
*                        ieee flags.  its computation is suseptible to the     *
*                        inherited rounding direction.  it is assumed that     *
*                        the rounding direction is set to the ieee default.    *
*     October   16 1996: fixed a grave mistake by replacing x2 with xSquared.  *
*     April     14 1997: added the environmental controls for mathlib v3.      *
*                        unlike mathlib v2, v3 will honor the inherited        *
*                        rounding mode since it is easier to set the flags     *
*                        computationally.                                      *
*     July      01 1997: this function no longer honors the rounding mode.     *
*                        corrected an error for nan arguments.                 *
*     July      20 2001: replaced __setflm with fegetenvd/fesetenvd.           *
*                        replaced DblInHex typedef with hexdouble.             *
*     September 07 2001: added #ifdef __ppc__.                                 *
*     September 09 2001: added more comments.                                  *
*     September 10 2001: added macros to detect PowerPC and correct compiler.  *
*     September 18 2001: added <CoreServices/CoreServices.h> to get to <fp.h>  *
*                        and <fenv.h>.                                         *
*     October   08 2001: removed <CoreServices/CoreServices.h>.                *
*                        changed compiler errors to warnings.                  *
*     November  06 2001: commented out warning about Intel architectures.      *
*                                                                              *
*     W A R N I N G:                                                           *
*     These routines require a 64-bit double precision IEEE-754 model.         *
*     They are written for PowerPC only and are expecting the compiler         *
*     to generate the correct sequence of multiply-add fused instructions.     *
*                                                                              *
*     These routines are not intended for 32-bit Intel architectures.          *
*                                                                              *
*     A version of gcc higher than 932 is required.                            *
*                                                                              *
*     GCC compiler options:                                                    *
*            optimization level 3 (-O3)                                        *
*            -fschedule-insns -finline-functions -funroll-all-loops            *
*                                                                              *
*******************************************************************************/

#ifdef      __APPLE_CC__
#if         __APPLE_CC__ > 930

#include    "fenv_private.h"
#include    "fp_private.h"

extern const unsigned short SqrtTable[];

static const double            c13     =  7.6018324085327799e-02;     /*  0x1.375efd7d7dcbep-4 */
static const double            c11     = -9.0904243427904791e-02;     /* -0x1.745802097294ep-4 */
static const double            c9      =  1.1111109821671356e-01;     /*  0x1.c71c6e5103cddp-4 */
static const double            c7      = -1.4285714283952728e-01;     /* -0x1.24924923f7566p-3 */
static const double            c5      =  1.9999999999998854e-01;     /*  0x1.99999999997fdp-3 */
static const double            c3      = -3.3333333333333330e-01;     /* -0x1.5555555555555p-2 */

static const double Rint        = 6.755399441055744e15;        /* 0x1.8p52              */
static const double PiOver2     = 1.570796326794896619231322;  /* head of �/2           */
static const double PiOver2Tail = 6.1232339957367660e-17;      /* tail of �/2           */
static const hexdouble Pi       = HEXDOUBLE(0x400921fb, 0x54442d18);
static const double kMinNormal = 2.2250738585072014e-308;  // 0x1.0p-1022

/*******************************************************************************
********************************************************************************
*                              A   T   A   N                                   *
********************************************************************************
*******************************************************************************/

struct tableEntry                             /* tanatantable entry structure */
      {
      double p;
      double f5;
      double f4;
      double f3;
      double f2;
      double f1;
      double f0;
      };

extern const unsigned long tanatantable[];

double atan ( double x )
      {
      hexdouble argument, reducedX, OldEnvironment;
      register double fabsOfx, xSquared, xFourth, xThird, temp1, temp2, result, z;
      struct tableEntry *tablePointer;
      register unsigned long int xHead;

      fabsOfx = __fabs ( x );
      argument.d = x;
      xHead = argument.i.hi & 0x7fffffff;

/*******************************************************************************
*     initialization of table pointer.                                         *
*******************************************************************************/

      tablePointer = ( struct tableEntry * ) ( tanatantable - ( 16 * 14 ) );

/*******************************************************************************
*     |x| > 1.0 or NaN.                                                        *
*******************************************************************************/
      
      fegetenvd( OldEnvironment.d );
      if ( xHead > 0x3ff00000) 
            {

/*******************************************************************************
*     |x| is nontrivial.                                                       *
*******************************************************************************/

            if ( xHead < 0x434d0297UL ) 
                  {
                  register double y, yTail, z, resultHead, resultTail;
                  y = 1.0 / fabsOfx;                                       /* parameter y = 1.0/|x|      */
                  reducedX.d = 256.0 * y + Rint;
                  xSquared = y * y;
                  OldEnvironment.i.lo |= FE_INEXACT;

/*******************************************************************************
*     |x| > 16.0                                                               *
*******************************************************************************/

                  if ( xHead > 0x40300000UL ) 
                        {
                        xFourth = xSquared * xSquared;
                        xThird  = xSquared * y;
                        temp1   = c9 + xFourth * c13;
                        temp2   = c7 + xFourth * c11;
                        temp1   = c5 + xFourth * temp1;
                        temp2   = c3 + xFourth * temp2;
                        resultHead   = PiOver2 - y;                        /* zeroth order term          */
                        temp1   = temp2 + xSquared * temp1;
                        yTail   = y * ( 1.0 - fabsOfx * y );               /* tail of 1.0/|x|            */
                        resultTail   = ( resultHead - PiOver2 ) + y;       /* correction to zeroth order */
                        temp1   = PiOver2Tail - xThird * temp1;            /* correction for �/2         */
                        
                        if ( x > 0.0 )                                     /* adjust sign of result      */
                            result = ( ( ( temp1 - yTail ) - resultTail ) + resultHead );
                        else
                            result = ( ( ( yTail - temp1 ) + resultTail ) - resultHead );
                        fesetenvd( OldEnvironment.d );                     /* restore the environment    */
                        return result;
                        }
            
/*******************************************************************************
*     1 <= |x| <= 16  use table-driven approximation for arctg(y = 1/|x|).     *
*******************************************************************************/
                  
                  yTail = y * ( 1.0 - fabsOfx * y );                       /* tail of 1.0/|x|            */
                  z = y - tablePointer[reducedX.i.lo].p + yTail;           /*y delta                     */
                  resultHead = PiOver2 - tablePointer[reducedX.i.lo].f0;   /* zeroth order term          */
                  temp1 = ( ( ( ( tablePointer[reducedX.i.lo].f5 * z       /* table-driven approximation */ 
                                + tablePointer[reducedX.i.lo].f4 ) * z
                                + tablePointer[reducedX.i.lo].f3 ) * z 
                                + tablePointer[reducedX.i.lo].f2 ) * z
                                + tablePointer[reducedX.i.lo].f1 );
                  
                  if ( x > 0.0 )                                            /* adjust for sign of x       */
                        result = ( ( PiOver2Tail - z * temp1 ) + resultHead );
                  else
                        result = ( ( z * temp1 - PiOver2Tail ) - resultHead );      
                  fesetenvd( OldEnvironment.d );                            /* restore the environment    */
                  return result;
                  }

/*******************************************************************************
*     |x| is huge, INF, or NaN.                                                *
*     For x = INF, then the expression �/2 � �tail would return the round up   *
*     or down version of atan if rounding is taken into consideration.         *
*     otherwise, just ��/2 would be delivered.                                 *
*******************************************************************************/
            else 
                  {                                                         /* |x| is huge, INF, or NaN   */
                  if ( x != x )                                             /* NaN argument is returned   */
                        result = x;
                  else 
                    {
                    OldEnvironment.i.lo |= FE_INEXACT;
                    if ( x > 0.0 )                                       /* positive x returns �/2     */
                            result =  ( Pi.d * 0.5 + PiOver2Tail );
                    else                                                      /* negative x returns -�/2    */
                            result =  ( - Pi.d * 0.5 - PiOver2Tail );
                    }
                  }
            fesetenvd( OldEnvironment.d );                                  /* restore the environment    */
            return result;
            }
      

/*******************************************************************************
*     |x| <= 1.0.                                                              *
*******************************************************************************/

      reducedX.d = 256.0 * fabsOfx + Rint;
      xSquared = x * x;
    
/*******************************************************************************
*     1.0/16 < |x| < 1  use table-driven approximation for arctg(x).           *
*******************************************************************************/

      if ( xHead > 0x3fb00000UL ) 
            {
            z = fabsOfx - tablePointer[reducedX.i.lo].p;                    /* x delta                    */
            temp1 = ( ( ( ( tablePointer[reducedX.i.lo].f5 * z 
                          + tablePointer[reducedX.i.lo].f4 ) * z
                          + tablePointer[reducedX.i.lo].f3 ) * z 
                          + tablePointer[reducedX.i.lo].f2 ) * z
                          + tablePointer[reducedX.i.lo].f1 );               /* table-driven approximation */ 
            if ( x > 0.0 )                                                  /* adjust for sign of x       */
                  result = ( tablePointer[reducedX.i.lo].f0 + z * temp1 );
            else 
                  result = - z * temp1 - tablePointer[reducedX.i.lo].f0;
            OldEnvironment.i.lo |= FE_INEXACT;
            fesetenvd( OldEnvironment.d );                                  /* restore the environment    */
            return result;
            }

/*******************************************************************************
*     |x| <= 1.0/16 fast, simple polynomial approximation.                     *
*******************************************************************************/

      if ( fabsOfx == 0.0 )
            {
            fesetenvd( OldEnvironment.d );                                  /* restore the environment    */
            return x; /* +0 or -0 preserved */
            }
            
      xFourth = xSquared * xSquared;
      temp1 = c9 + xFourth * c13;
      temp2 = c7 + xFourth * c11;
      temp1 = c5 + xFourth * temp1;
      temp2 = c3 + xFourth * temp2;
      xThird = x * xSquared;
      temp1 = temp2 + xSquared * temp1;
      result = x + xThird * temp1;
      
      if ( fabs ( result ) < kMinNormal )
            OldEnvironment.i.lo |= FE_UNDERFLOW;

      OldEnvironment.i.lo |= FE_INEXACT;
      fesetenvd( OldEnvironment.d );                                         /* restore the environment    */
      return result;
      }

#ifdef notdef
float atanf( float x )
{
    return (float)atan( x );
}
#endif

#else       /* __APPLE_CC__ version */
#warning A higher version than gcc-932 is required.
#endif      /* __APPLE_CC__ version */
#endif      /* __APPLE_CC__ */
