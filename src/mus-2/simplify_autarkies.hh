/*----------------------------------------------------------------------------*\
 * File:        simplify_autarikies.hh
 *
 * Description: Class definition and implementation of a work item for 
 *              autarky-based simplification.
 *
 * Author:      antonb
 *
 * Notes:
 *
 *   1. IMPORTANT: current implementation is tested during pre-processing, 
 *      however must be modified to support inter-processing. Treat it as
 *      experimental.
 * 
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _SIMPLIFY_AUTARKIES_H
#define _SIMPLIFY_AUTARKIES_H 1

#include "mus_data.hh"
#include "work_item.hh"

/*----------------------------------------------------------------------------*\
 * Class:  SimplifyAut
 *
 * Purpose: A work item for autarky-based simplification of group set
 *
 * Notes:
 *
\*----------------------------------------------------------------------------*/

class SimplifyAut : public WorkItem {

public:     // Lifecycle

  SimplifyAut(const MUSData& md)
    : _md(md) {}

  virtual ~SimplifyAut(void) {}

public:     // Parameters

  const MUSData& md(void) const { return _md; }

public:     // Results

  /* Returns the version of MUSData the results are for */
  const unsigned& version(void) const { return _version; }
  void set_version(unsigned version) { _version = version; }

public:     // Reset/recycle

  virtual void reset(void) {
  }

protected:

  // parameters

  const MUSData& _md;                        // MUS data

  // results

  unsigned _version;                         // the version of MUSData this result is for

};

#endif /* _SIMPLIFY_AUTARKIES_H */

/*----------------------------------------------------------------------------*/
