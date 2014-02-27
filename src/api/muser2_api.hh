/*----------------------------------------------------------------------------*\
 * File:        muser2_api.hh
 *
 * Description: Declaration of the MUS/MES extraction API (C++)
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *                                              Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/

#pragma once

/** This is the API for MUS/GMUS extraction.
 */
class muser2 
{
public:         // Types

  /** literals are signed integers (non 0); clauses are arrays of literals given 
   * by the start/end pointers. */
  typedef int lit;
  /** Group IDs are unsigned integers; gid_Undef is the special, undefined group 
   * ID. */
  typedef unsigned gid;
  const static gid gid_Undef = (gid)-1;

public:         // Lifecycle

  /** Constructor */
  muser2(void);
  /** Destructor */
  ~muser2(void);

  // copying and assignment are prohibited
  private: muser2(const muser2& from); muser2& operator=(const muser2& from); 
  public:

  /** Initializes all internal data-structures */
  void init_all(void);

  /** Resets all internal data-structures */
  void reset_all(void);

  /** Prepares extractor for the run */
  void init_run(void);

  /** Clears up all data-structures used for the run */
  void reset_run(void);

public:         // Configuration

  /** Sets verbosity level and prefix for output messages; 0 means silent.
   * Defaults: 0, ""
   */
  void set_verbosity(unsigned verb, const char* prefix = "");

  /** Sets soft CPU time limit for extraction (seconds, 0 = no limit).
   */
  void set_cpu_time_limit(double limit);

  /** Sets the limit on the number of iteration, where an "iteration" is
   * typically an iteration of the main loop of the algo. (0 = no limit).
   */
  void set_iter_limit(unsigned limit);

  /** Sets group removal order:
   * 0 - default (max->min)
   * 3 - reverse (min->max)
   * 4 - random
   * Other values can be looked up in muser2 help.
   */
  void set_order(unsigned order);

  /** When true, the groups deemed to be necessary (i.e. included in the
   * computed MUS) are added permanently to the group-set; i.e. they become
   * part of group 0. Default: true. */
  void set_finalize_necessary_groups(bool fng);

  /** When true, the groups deemed to be unnecessary (i.e. outside of the
   * computed MUS) are removed permanently from the group-set. Default: true.
   */
  void set_delete_unnecessary_groups(bool dug);

public:         // Addition of clauses and groups

  /** Add a clause to the group-set.
   * 
   * Clauses added with gid == 0 are added permanently, and will never be a part 
   * of computed MUS. Clauses with gid == gid_Undef will be assigned a unique 
   * group-ID which will be returned by the method. This group-ID will be used to 
   * identify the clause in the computed MUS. Dublicate clauses are not allowed,
   * and so if a clause with the same set of literals is already present in the
   * underlying group-set its group-ID will be returned, instead of the requested
   * one.
   *
   * @return group-ID of the added (or existing) clause.
   */
  gid add_clause(const lit* first, const lit* last, gid gid);

  // TODO: add "normal" C++ versions of add_clause()

public:         // Functionality

  /** Tests the current group-set for satisfiability.
   * @return SAT competition code: 0 - UNKNOWN, 10 - SAT, 20 - UNSAT
   */
  int test_sat(void);

  /** Compute a group-MUS of the current group-set.
   * @return 0 if GMUS approximation is computed, 20 if GMUS is precise, -1 on 
   * error
   */
  int compute_gmus(void);

  /** Returns a reference to the vector of group-IDs included in the
   * group MUS previously computed by compute_gmus(). The content is valid 
   * until the next init_run() call. The vector might be empty if group 0
   * is unsat (note that group 0 is not part of group MUS).
   */
  const std::vector<gid>& gmus_gids(void) const;

private:

  class muser2_impl;
  muser2_impl* _pimpl;    // PIMPL idiom

};

/*----------------------------------------------------------------------------*/
