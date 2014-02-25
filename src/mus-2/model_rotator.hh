/*----------------------------------------------------------------------------*\
 * File:        model_rotator.hh
 *
 * Description: Class definitions of workers that know to do model rotation.
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _MODEL_ROTATOR_HH
#define _MODEL_ROTATOR_HH 1

#include <ext/hash_map>
#include "basic_clset.hh"
#include "basic_group_set.hh"
#include "mus_data.hh"
#include "rotate_model.hh"
#include "types.hh"
#include "worker.hh"

/*----------------------------------------------------------------------------*\
 * Class:  ModelRotator
 *
 * Purpose: An BC for workers that knows to process RotateModel item.
 *
 * Notes:
 *
\*----------------------------------------------------------------------------*/

class ModelRotator : public Worker {

public:

  // lifecycle

  ModelRotator(unsigned id = 0) : Worker(id), _num_points(0) {}

  virtual ~ModelRotator(void) {}

  // functionality

  using Worker::process;

  /* Handles the RotateModel work item
   */
  virtual bool process(RotateModel& rm) { return false; }
  
  // stats

  /* Returns the number of points visited by the rotator */
  ULINT num_points(void) const { return _num_points; }

protected:

  ULINT _num_points;
  
};

/*----------------------------------------------------------------------------*\
 * Class:  RecursiveModelRotator
 *
 * Purpose: A model rotator that implements the RMR algorithm (FMCAD-2011)
 *
 * Notes:
 *
 *  1. Currently supported work items: RotateModel
 *  2. IMPORTANT: the current implementation is not designed for MT environments
 *     (some data is global).
 *
\*----------------------------------------------------------------------------*/

class RecursiveModelRotator : public ModelRotator {

public:

  // lifecycle

  RecursiveModelRotator(unsigned id = 0) : ModelRotator(id) {}

  virtual ~RecursiveModelRotator(void) {}

  // functionality

  using Worker::process;

  /* Handles the RotateModel work item
   */
  virtual bool process(RotateModel& rm);
  
};

#endif /* _MODEL_ROTATOR_HH */

/*----------------------------------------------------------------------------*/
