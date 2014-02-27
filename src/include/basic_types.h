//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        basic_types.h
 *
 * Description: Basic types used by BOLT.
 *
 * Author:      jpms
 * 
 * Revision:    $Id$.
 *
 *                        Copyright (c) 2009-13, Joao Marques-Silva, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#ifndef _BASIC_TYPES_H
#define _BASIC_TYPES_H 1

#include <cstdint>

/*----------------------------------------------------------------------------*\
 * Values besides 0 and 1
\*----------------------------------------------------------------------------*/

// 32-bit integers
typedef uint32_t ULINT;
typedef int32_t LINT;
#define MAXLINT INT32_MAX
#define MINLINT INT32_MIN
#define MAXULINT UINT32_MAX

#ifdef GMPDEF
#include <gmpxx.h>
typedef mpz_class XLINT;
#define ToLint(x) x.get_si()
#else
typedef int64_t XLINT;
#define ToLint(x) (LINT)x
#endif

#endif /* _BASIC_TYPES_H */

/*----------------------------------------------------------------------------*/
