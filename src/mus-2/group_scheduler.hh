/*----------------------------------------------------------------------------*\
 * File:        group_scheduler.hh
 *
 * Description: abstract base class for group schedulers used in MUS extraction
 *
 * Author:      antonb
 * 
 *                                               Copyright (c) 2011, Anton Belov
 \*----------------------------------------------------------------------------*/

#ifndef _GROUP_SCHEDULER_HH
#define _GROUP_SCHEDULER_HH

#include "basic_group_set.hh"
#include "mus_data.hh"

/*----------------------------------------------------------------------------*\
 * Class:  GroupScheduler
 *
 * Purpose: An interface implemented by anyone who knows to schedule groups for 
 * execution for a given worker id. Subclasses of GroupScheduler designed for 
 * multi-threaded environments will be called by multiple threads simultaneously 
 * (except constructor/descructor), and so are assumed to be thread-safe.
 *
 * Notes:
 *
\*----------------------------------------------------------------------------*/

class GroupScheduler {

public:

  /** In principle an instance of MUSData and total number of workers is
   * enough */
  GroupScheduler(MUSData& md, unsigned num_workers = 1)
    : _md(md), _num_workers(num_workers) {}

  virtual ~GroupScheduler(void) {}

public:

  /** Returns true and sets the next group id for a given worker ID
   * [0,num_workers) if there's more groups; otherwise false
   */
  virtual bool next_group(GID& next_gid, unsigned worker_id = 0) = 0;

  /** This allows users to re-schedule a group ID check - the invariant is that
   * after this call next_group will give out the gid at some point
   */
  virtual void reschedule(GID gid) = 0;

  /** This allows to push some gids to the front (if this makes sense)
   */
  virtual void fasttrack(GID gid) {}

  /** Some schedulers are dynamic; use this to indicate that a group got
   * removed or became necessary, or that its order may have been affected
   */
  virtual void update_removed(GID gid) {}
  virtual void update_necessary(GID gid) {}
  virtual void update(GID gid) {}

protected:

  MUSData& _md;         // keeps a reference to MUSData (e.g. for altering schedules)

  unsigned _num_workers; // number of workers in the system

};

#endif // _GROUP_SCHEDULER_HH

/*----------------------------------------------------------------------------*/
