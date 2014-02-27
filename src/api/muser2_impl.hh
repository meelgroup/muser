/*----------------------------------------------------------------------------*\
 * File:        muser2_impl.hh
 *
 * Description: Declaration of the MUS/MES extraction API implementation class.
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *                                              Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/

#pragma once

#include <vector>
#include "basic_group_set.hh"
#include "id_manager.hh"
#include "mus_config.hh"
#include "mus_data.hh"
#include "mus_extractor.hh"
#include "muser2_api.hh"

/** This is the API for MUS/GMUS extraction.
 */
class muser2::muser2_impl
{

public:         // Lifecycle

  /** Constructor */
  muser2_impl(void);
  /** Destructor */
  ~muser2_impl(void);

  // copying and assignment are prohibited
  muser2_impl(const muser2& from) = delete;
  muser2_impl& operator=(const muser2_impl& from) = delete;

  /** Initializes all internal data-structures */
  void init_all(void);

  /** Resets all internal data-structures */
  void reset_all(void);

  /** Prepares extractor for the run */
  void init_run(void);

  /** Clears up all data-structures used for the run */
  void reset_run(void);

public:         // Functionality

  /** Tests the current group-set for satisfiability.
   * @return SAT competition code: 0 - UNKNOWN, 10 - SAT, 20 - UNSAT
   */
  unsigned test_sat(void);

  /** Compute a group-MUS of the current group-set.
   * @return 0 if GMUS approximation is computed, 20 if GMUS is precise, 
   * -1 on error
   */
  int compute_gmus(void);

  /** Returns a reference to the vector of group-IDs included in the
   * computed MUS. The content is valid until the next init_run() call.
   */
  std::vector<gid>& gmus_gids(void) { return _gmus_gids; }
  
public:         // Configuration

  /** Sets verbosity level and prefix for output messages; 0 means silent.
   * Defaults: 0, ""
   */
  void set_verbosity(unsigned verb, const char* prefix) {
    config.set_verbosity(_verb = verb);
    config.set_prefix(_pref = prefix);
  }

  /** Sets soft CPU time limit for extraction (seconds, 0 = no limit).
   */
  void set_cpu_time_limit(double limit) { _cpu_limit = limit; }

  /** Sets the limit on the number of iteration, where an "iteration" is
   * typically an iteration of the main loop of the algo. (0 = no limit).
   */
  void set_iter_limit(unsigned limit) { _iter_limit = limit; }

  /** Sets the minimization order (as per muser)
   */
  void set_order(unsigned order) { config.set_order_mode(_order = order); }

  /** When true, the groups deemed to be necessary (i.e. included in the
   * computed MUS) are added permanently to the group-set; i.e. they become
   * part of group 0. Default: true. */
  void set_finalize_necessary_groups(bool fng) { _fng = fng; }

  /** When true, the groups deemed to be unnecessary (i.e. outside of the
   * computed MUS) are removed permanently from the group-set. Default: true.
   */
  void set_delete_unnecessary_groups(bool dug) { _dug = dug; }

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
  gid add_clause(const int* first, const int* last, gid gid);

private:        // Main datastructures ...

  ToolConfig config;                    // configuration data

  IDManager _imgr;                      // ID manager

  BasicGroupSet* _pgset = 0;            // group-set

  MUSData* _pmd = 0;                    // MUSData

  //INIT ALEX
  std::vector<BasicClause*> cl_savec;
  //END ALEX

private:        // Configuration

  unsigned _verb = 0;                   // verbosity

  const char* _pref = "";               // prefix string

  double _cpu_limit = 0;                // CPU time limit (soft) for extraction

  unsigned _iter_limit = 0;             // iteration limit

  unsigned _order = 0;                  // minimization order

  bool _fng = true;                     // finalize necessary groups

  bool _dug = true;                     // delete unnecessary groups
    
private:        // Results

  std::vector<gid> _gmus_gids;          // group-IDs in the computed gmus

private:        // Misc

  unsigned _init_gsize = 0;             // initial number of groups

};

/*----------------------------------------------------------------------------*/
