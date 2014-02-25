/*----------------------------------------------------------------------------*\
 * File:        compute_mus.hh
 *
 * Description: Class declaration and implementation of work item for computing
 *              an MUS of a groupset.
 *
 * Author:      antonb
 * 
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _COMPUTE_MUS_HH
#define _COMPUTE_MUS_HH 1

#include "basic_group_set.hh"
#include "mus_data.hh"
#include "work_item.hh"

/*----------------------------------------------------------------------------*\
 * Class:  ComputeMUS
 *
 * Purpose: A work item for computing an MUS of a group set.
 *
 * Notes:
 *
\*----------------------------------------------------------------------------*/

class ComputeMUS : public WorkItem {

public:     // Lifecycle

  ComputeMUS(MUSData& md)       // note that the reference is not constant
    : _md(md) {}

  virtual ~ComputeMUS(void) {};

public:     // Parameter and result 

  MUSData& md(void) const { return _md; }

protected:

  MUSData& _md;                  // MUS data
  
};

#endif /* _COMPUTE_MUS_HH */

/*----------------------------------------------------------------------------*/
