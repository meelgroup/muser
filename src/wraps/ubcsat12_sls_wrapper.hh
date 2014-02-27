//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        ubcsat12_sls_wrapper.hh
 *
 * Description: SAT solver wrapper for ubcsat 1.2
 *
 * Author:      antonb
 * 
 *                                     Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#ifndef _UBCSAT12_SLS_WRAPPER_H
#define _UBCSAT12_SLS_WRAPPER_H 1

#include <map>
#include <stdexcept>
#include <vector>
#include "basic_clause.hh"
#include "basic_clset.hh"
#include "globals.hh"
#include "id_manager.hh"
#include "solver_sls_wrapper.hh"
#include "solver_utils.hh"
#include "ubcsat.h"

class SATSolverSLSFactory;

//jpms:bc
/*----------------------------------------------------------------------------*\
 * Class: Ubcsat12SLSWrapper
 *
 * Purpose: provides interface to ubcsat 1.2 SLS SAT solver
\*----------------------------------------------------------------------------*/
//jpms:ec

class Ubcsat12SLSWrapper : public SATSolverSLSWrapper {

  friend class SATSolverSLSFactory;

public: // Lifecycle

  /** Initializes all internal data structs; call once per life-time */
  virtual void init_all(void);

  /** Inverse of init_all(), call before descruction */
  virtual void reset_all(void);

  /** Prepares for SAT run; call before each solve() */
  virtual void init_run(void);

  /** Inverse of init_run(), call after solve() */
  virtual void reset_run(void);

public: // Functionality

  /** Find an assignment or an approximation, starting from random */
  virtual SATRes solve(void) { 
    ensure_state(PREPARED); return _solve(0); }

  /** Find an assignment or an approximation, starting from init_assign.
   * Unassigned variables are initialized randomly. 
   */
  virtual SATRes solve(const IntVector& init_assign) { 
    ensure_state(PREPARED); return _solve(&init_assign); }

public: // Overriden configuration methods 

  /** Set the ceiling on the quality of the worstening step during SLS: if the
   * current solution quality is < max_break_value, but a selected flip would 
   * cause the solution quality to become >= max_break_value, the flip is 
   * aborted. The value of 0 disables this functionality.
   */
  virtual void set_max_break_value(XLINT max_break_value_) {
    if (!(algo == WALKSAT_SKC) && weighted)
      throw std::logic_error("set_max_break_value() in ubcsat12 wrapper is "
                             "implemented only for weighted walksat");
    SATSolverSLSWrapper::set_max_break_value(max_break_value_);
  }

public: // Manipulate local copy of clause set

  /** Returns the number of clauses in the solver */
  virtual LINT size(void) const { return _cl_lits.size(); }

  /** Returns the maximum variable id in the solver */
  virtual ULINT max_var(void) const;

  /** Adds a clause to the solver. If the solver is weighted the specified weight
   * is used, which must be > 0.
   */
  virtual void add_clause(const BasicClause* cl, XLINT weight) {
    ensure_state(INITIALIZED);
    _add_clause(cl->begin(), cl->end(), (weighted) ? weight : 1, cl->get_id());
  }

  /** Adds a clause to the solver. If the solver is weighted the clause weight
   * is used (cl->get_weight()), which must be > 0.
   */
  virtual void add_clause(const BasicClause* cl) { add_clause(cl, cl->get_weight()); }

  /** Same as above, but with a vector of literals, and weight.
   */
  virtual void add_clause(const IntVector& lits, XLINT weight) {
    ensure_state(INITIALIZED);
    _add_clause(lits.begin(), lits.end(), (weighted) ? weight : 1, 0);
  }

  /** Adds all clauses from the clause set. If the solver is weighted the weight
   * is taken from the clauses.
   */
  virtual void add_clauses(const BasicClauseSet& rclset) {
    ensure_state(INITIALIZED);
    BasicClauseSet* cls = const_cast<BasicClauseSet*>(&rclset); // argh
    _reserve_space(cls->size());
    for (cset_iterator pcl = cls->begin(); pcl != cls->end(); ++pcl)
      add_clause(*pcl);
  }

  /** Notifies the wrapper of a change in the weight of the specified clause; the
   * clause is identified by its ID (get_id()); returns true if weight is updated
   * successfully (this requires that the solver is weighted and the clause with
   * the ID is found).
   */
  virtual bool update_clause_weight(const BasicClause* cl);

protected: // Lifecycle

  Ubcsat12SLSWrapper(IDManager& _imgr) 
    : SATSolverSLSWrapper(_imgr), _argv(0), _argc(0), _ls_end(0) 
  {
    if (_pinstance != 0)
      throw std::logic_error("ubcsat 1.2 wrapper does not support multiple instances.");
    _pinstance = this;
  }

  virtual ~Ubcsat12SLSWrapper(void) { _pinstance = 0; }

private: 

  static Ubcsat12SLSWrapper* _pinstance; // this is a singleton (but no instance()) method

  // main solving routine; if init_assign is not 0, its used to as the initial 
  // assignment, otherwise the intial assignment is random.
  SATRes _solve(const IntVector* init_assign);
  
private: // Parameter handling 

  // extra params
  void set_ubcsat_param(const char* name, const char* value) {
    _params[string(name)] = string(value);
  }
  void remove_ubcsat_param(const char* name) { _params.erase(string(name)); }
  void clear_ubcsat_params(void) { _params.clear(); }

  typedef std::map<string, string> ParamMap;
  ParamMap _params;                     // ubcsat parameter map
  const char** _argv;                   // parameter string
  int _argc;                            // and count
  void _prepare_params(void);           // prepares param string
  void _print_params(std::ostream& out);// writes out the current settings
  void _cleanup_params(void);           // cleans up param string

private: // Clause storage and management
  
  std::vector<ubcsat::UINT32> _cl_lengths;   // clause lengths
  std::vector<ubcsat::LITTYPE*> _cl_lits;    // pointers to arrays of literals
  std::vector<ubcsat::UBIGINT> _cl_weights;  // clause weights

  void _add_clause(CLiterator begin, CLiterator end, XLINT weight, ULINT id);
  void _reserve_space(unsigned num_clauses);
  ubcsat::LITTYPE* _store_literals(CLiterator begin, CLiterator end);
  void _clear_storage(void);
  // this might change; use the methods above to manipulate storage
  std::vector<ubcsat::LITTYPE*> _lit_storage;// storage for literals (array of
                                             // pointers to chunks of literals)
  unsigned _ls_end;                          // where to store in the last chunk
  
  typedef std::map<ULINT, unsigned> ClidMap;
  std::vector<ULINT> _cl_ids;           // BOLT clause IDs (0 means no ID, i.e.
                                        // clause was given as array of lits)
  ClidMap _cl_map;                      // map from BOLT IDs to ubcsat indexes

private: // Misc

  void _check_invariants(XLINT test_quality); 

  // Dumps ubcsat clauses; status: 0 = false only, 1 = true only, 2 = both, 
  // 3 = don't look at truth values. If status < 3, uses ubcsat's current 
  // assignment to get truth values (so think before you call it)
  void _dump_clauses(ostream& out, int status);

};

#endif /* _UBCSAT12_SLS_WRAPPER_H */

/*----------------------------------------------------------------------------*/
