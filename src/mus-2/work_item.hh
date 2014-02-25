/*----------------------------------------------------------------------------*\
 * File:        work_item.hh
 *
 * Description: Abstract base class definition of work items.
 *
 * Author:      antonb
 * 
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _WORK_ITEM_H
#define _WORK_ITEM_H 1

/*----------------------------------------------------------------------------*\
 * Class:  WorkItem
 *
 * Purpose: Abstract base for all work items.
 *
 * Notes:
 *
 *  1. TODO: make items recyclable (add virtual recycle() method)
 *
\*----------------------------------------------------------------------------*/

class WorkItem {

public:

  // lifecycle

  WorkItem(void) : _completed(false) {}

  virtual ~WorkItem(void) {}

  // completion status

  void set_completed(void) { _completed = true; }

  bool completed(void) const { return _completed; }

  // reset to initial state

  virtual void reset(void) { _completed = false; }

private:

  bool _completed;                           // true when completed

};

#endif /* _WORK_ITEM_H */

/*----------------------------------------------------------------------------*/
