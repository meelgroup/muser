/*----------------------------------------------------------------------------*\
 * File:        mus_data_mt.hh
 *
 * Description: Class definition and implementation of MUSDataMT - a multi-thread
 *              safe version of MUSData container
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _MUS_DATA_MT_HH
#define _MUS_DATA_MT_HH 1

#include <tbb/reader_writer_lock.h>
#include "basic_group_set.hh"
#include "mus_data.hh"

/*----------------------------------------------------------------------------*\
 * Class: MUSDataMT
 *
 * Purpose: Multi-thread safe container for MUS extraction-related data
 *
 * Description: adds the read-write lock from TBB to MUSData
 *
 * Notes:
 *      1. TODO (major): switch to scoped-lock design -- mostly because of 
 *      potential disaster related to exception handling.
 *
\*----------------------------------------------------------------------------*/

class MUSDataMT : public MUSData {

public:

  MUSDataMT(BasicGroupSet& gset) : MUSData(gset) {}

  virtual ~MUSDataMT(void) {}

public:    // Lock functions (subclasses provide implementation)

  /* Get a read-lock on the object */
  virtual void lock_for_reading(void) const {
    _lock.lock_read();
  }

  /* Get a write-lock on the object: only through non constant reference */
  virtual void lock_for_writing(void) {
    _lock.lock();
  }

  /* Release lock */
  virtual void release_lock(void) const {
    _lock.unlock();
  }

protected:

  mutable tbb::reader_writer_lock _lock;    // lock -- TODO(major): switch to scoped_lock !

};

#endif /* _MUS_DATA_MT_HH */

/*----------------------------------------------------------------------------*/
