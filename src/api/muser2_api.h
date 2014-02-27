/*----------------------------------------------------------------------------*\
 * File:        muser2_api.h
 *
 * Description: Declaration of the MUS/MES extraction API (C-style)
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *                                              Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _MUSER2_API_H
#define _MUSER2_API_H 1

#ifdef __cplusplus
extern "C" {
#endif

  /** This is the C-style API for MUS/GMUS extraction.
   */

  /** The handle */
  typedef void* MUSer2_t;

  /** Literals are just signed integers */
  typedef int MUSer2_Lit;

  /** Group ID are unsigned integers */
  typedef unsigned MUSer2_Gid;
  const static MUSer2_Gid MUSer2_Gid_Undef = (MUSer2_Gid)-1;

  // Lifecycle

  /** Constructor */
  MUSer2_t muser2_create(void);

  /** Destructor
   * @return 0 on success, -1 on error
   */
  int muser2_destroy(MUSer2_t h);

  /** Initializes all internal data-structures 
   * @return 0 on success, -1 on error
   */
  int muser2_init_all(MUSer2_t h);

  /** Resets all internal data-structures 
   * @return 0 on success, -1 on error
   */
  int muser2_reset_all(MUSer2_t h);

  /** Prepares extractor for the run 
   * @return 0 on success, -1 on error
   */
  int muser2_init_run(MUSer2_t h);

  /** Clears up all data-structures used for the run 
   * @return 0 on success, -1 on error
   */
  int muser2_reset_run(MUSer2_t h);

  // Configuration

  /** Sets verbosity level and prefix for output messages; 0 means silent.
   * Defaults: 0, ""
   */
  void muser2_set_verbosity(MUSer2_t h, unsigned verb, const char* prefix);

  /** Sets soft CPU time limit for extraction (seconds, 0 = no limit).
   */
  void muser2_set_cpu_time_limit(MUSer2_t h, double limit);

  /** Sets the limit on the number of iteration, where an "iteration" is
   * typically an iteration of the main loop of the algo. (0 = no limit).
   */
  void muser2_set_iter_limit(MUSer2_t h, unsigned limit);

  /** Sets group removal order:
   * 0 - default (max->min)
   * 3 - reverse (min->max)
   * 4 - random
   * Other values can be looked up in MUSer2 help.
   */
  void muser2_set_order(MUSer2_t h, unsigned order);

  /** When 1, the groups deemed to be necessary (i.e. included in the
   * computed MUS) are added permanently to the group-set; i.e. they become
   * part of group 0. Default: 1. */
  void muser2_set_finalize_necessary_groups(MUSer2_t h, int fng);

  /** When 1, the groups deemed to be unnecessary (i.e. outside of the
   * computed MUS) are removed permanently from the group-set. Default: true.
   */
  void muser2_set_delete_unnecessary_groups(MUSer2_t h, int dug);

  // Addition of clauses and groups

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
  MUSer2_Gid muser2_add_clause(MUSer2_t h, MUSer2_Lit* first, MUSer2_Lit* last, MUSer2_Gid gid);

  // Functionality

  /** Tests the current group-set for satisfiability.
   * @return SAT competition code: 0 - UNKNOWN, 10 - SAT, 20 - UNSAT or -1 in
   *         case of error.
   */
  int muser2_test_sat(MUSer2_t h);

  /** Compute a group-MUS of the current group-set.
   * @return 0 if GMUS approximation is computed, 20 if GMUS is precise, -1 on 
   * error
   */
  int muser2_compute_gmus(MUSer2_t h);

  /** Returns pointers to the first and the last elements of the array that
   * contains the the group-IDs included in the group MUS previously computed 
   * by muser2_compute_gmus(). first and/or last may be given NULL, in which
   * case the pointers are not returned obviously. The content is valid until 
   * the next muser2_init_run() call. Note that group 0 is not part of group MUS. 
   * Also, keep in mind that the memory is owned by the API, so do not do
   * anything to it.
   *
   * @return the number of groups in the vector (might be 0 if group 0 is unsat)
   */
  int muser2_gmus_gids(MUSer2_t h, MUSer2_Gid** first, MUSer2_Gid** last);

#ifdef __cplusplus
}
#endif

#endif /* _MUSER2_API_HH */

/*----------------------------------------------------------------------------*/
