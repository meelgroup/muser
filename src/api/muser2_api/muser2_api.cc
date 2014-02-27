/*----------------------------------------------------------------------------*\
 * File:        muser2_api.cc
 *
 * Description: Implementation of MUSer2 API class and C-style interface.
 *
 * Author:      antonb
 * 
 * Notes:       All methods simply forward to MUSer2_Impl (see muser2_impl.hh)
 *
 *                                              Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/

#include <vector>
#include "muser2_api.hh"
#include "muser2_api.h"
#include "muser2_impl.hh"

using namespace std;

/** Constructor */
MUSer2::MUSer2(void) { _pimpl = new MUSer2::MUSer2_Impl(); }

/** Destructor */
MUSer2::~MUSer2(void) { delete _pimpl; }

/** Initializes all internal data-structures */
void MUSer2::init_all(void) { _pimpl->init_all(); }

/** Resets all internal data-structures */
void MUSer2::reset_all(void) { _pimpl->reset_all(); }

/** Prepares extractor for the run */
void MUSer2::init_run(void) { _pimpl->init_run(); }

/** Clears up all data-structures used for the run */
void MUSer2::reset_run(void) { _pimpl->reset_run(); }

/** Sets verbosity level and prefix for output messages; 0 means silent. */
void MUSer2::set_verbosity(unsigned verb, const char* prefix) 
{
  _pimpl->set_verbosity(verb, prefix);
}

/** Sets soft CPU time limit for extraction (seconds, 0 = no limit).
 */
void MUSer2::set_cpu_time_limit(double limit)
{
  _pimpl->set_cpu_time_limit(limit);
}

/** Sets the limit on the number of iteration, where an "iteration" is
 * typically an iteration of the main loop of the algo. (0 = no limit).
 */
void MUSer2::set_iter_limit(unsigned limit)
{
  _pimpl->set_iter_limit(limit);
}

/** Sets group removal order */
void MUSer2::set_order(unsigned order)
{
  _pimpl->set_order(order);
}

/** When true, the groups deemed to be necessary (i.e. included in the
 * computed MUS) are added permanently to the group-set; i.e. they become
 * part of group 0. Default: true. */
void MUSer2::set_finalize_necessary_groups(bool fng) 
{
  _pimpl->set_finalize_necessary_groups(fng);
}

/** When true, the groups deemed to be unnecessary (i.e. outside of the
 * computed MUS) are removed permanently from the group-set. Default: true.
 */
void MUSer2::set_delete_unnecessary_groups(bool dug)
{
  _pimpl->set_delete_unnecessary_groups(dug);
}

/** Add a clause to the group-set. Forwards to MUSer2_Impl. */
MUSer2::Gid MUSer2::add_clause(const MUSer2::Lit* first, 
                               const MUSer2::Lit* last, MUSer2::Gid gid)
{
  return (MUSer2::Gid)_pimpl->add_clause(first, last, gid);
}

/** Tests the current group-set for satisfiability. */
int MUSer2::test_sat(void) { return _pimpl->test_sat(); }

/** Compute a group-MUS of the current group-set. Forwards to MUSer2_Impl. */
int MUSer2::compute_gmus(void) { return _pimpl->compute_gmus(); }

/** Returns a reference to the vector of group-IDs included in the group MUS */
const vector<MUSer2::Gid>& MUSer2::gmus_gids(void) const { return _pimpl->gmus_gids(); }

//
// C-style interface
//

namespace { // helpers

  // cast handle to class pointer
  inline MUSer2* pm(MUSer2_t h) { return reinterpret_cast<MUSer2*>(h); }

}

/** Constructor */
MUSer2_t muser2_create(void) 
{ 
  try { return reinterpret_cast<MUSer2_t>(new MUSer2()); } catch(...) { return NULL; }
}

/** Destructor */
int muser2_destroy(MUSer2_t h) 
{
  try { delete pm(h); } catch (...) { return -1; }
  return 0;
}

/** Initializes all internal data-structures */
int muser2_init_all(MUSer2_t h) 
{ 
  try { pm(h)->init_all(); } catch (...) { return -1; }  
  return 0;
}

/** Resets all internal data-structures */
int muser2_reset_all(MUSer2_t h) 
{
  try { pm(h)->reset_all(); } catch (...) { return -1; }  
  return 0;
}

/** Prepares extractor for the run */
int muser2_init_run(MUSer2_t h)
{
  try { pm(h)->init_run(); } catch (...) { return -1; }  
  return 0;
}

/** Clears up all data-structures used for the run */
int muser2_reset_run(MUSer2_t h)
{
  try { pm(h)->reset_run(); } catch (...) { return -1; }  
  return 0;
}

/** Sets verbosity level and prefix for output messages; 0 means silent. */
void muser2_set_verbosity(MUSer2_t h, unsigned verb, const char* prefix)
{
  pm(h)->set_verbosity(verb, prefix);
}

/** Sets soft CPU time limit for extraction (seconds, 0 = no limit). */
void muser2_set_cpu_time_limit(MUSer2_t h, double limit)
{
  pm(h)->set_cpu_time_limit(limit);
}

/** Sets the limit on the number of iteration, where an "iteration" is
 * typically an iteration of the main loop of the algo. (0 = no limit).
 */
void muser2_set_iter_limit(MUSer2_t h, unsigned limit)
{
  pm(h)->set_iter_limit(limit);
}

/** Sets group removal order */
void muser2_set_order(MUSer2_t h, unsigned order)
{
  pm(h)->set_order(order);
}

/** When 1, the groups deemed to be necessary (i.e. included in the
 * computed MUS) are added permanently to the group-set; i.e. they become
 * part of group 0. Default: 1. */
void muser2_set_finalize_necessary_groups(MUSer2_t h, int fng)
{
  pm(h)->set_finalize_necessary_groups(fng);
}

/** When 1, the groups deemed to be unnecessary (i.e. outside of the
 * computed MUS) are removed permanently from the group-set. Default: true.
 */
void muser2_set_delete_unnecessary_groups(MUSer2_t h, int dug)
{
  pm(h)->set_delete_unnecessary_groups(dug);
}

/** Add a clause to the group-set. */
MUSer2_Gid muser2_add_clause(MUSer2_t h, MUSer2_Lit* first, MUSer2_Lit* last, MUSer2_Gid gid)
{
  try {
    return (MUSer2_Gid)pm(h)->add_clause(first, last, gid);
  } catch (...) {
    return MUSer2_Gid_Undef;
  }
}

/** Tests the current group-set for satisfiability. */
int muser2_test_sat(MUSer2_t h)
{
  try { return pm(h)->test_sat(); } catch (...) { return -1; }
}

/** Compute a group-MUS of the current group-set. */
int muser2_compute_gmus(MUSer2_t h)
{
  try { return pm(h)->compute_gmus(); } catch (...) { return -1; }
}


/** Returns pointers to the first and the last elements of the array that contains 
 * the the group-IDs included in the group MUS previously computed by 
 * muser2_compute_gmus(). 
 */
int muser2_gmus_gids(MUSer2_t h, MUSer2_Gid** first, MUSer2_Gid** last)
{
  const vector<MUSer2::Gid>& gmus = pm(h)->gmus_gids();
  unsigned size = gmus.size();
  if (first) { *first = const_cast<MUSer2_Gid*>(gmus.data()); }
  if (last) { *last = const_cast<MUSer2_Gid*>(gmus.data() + size - 1); }
  return size;
}

/*----------------------------------------------------------------------------*/
