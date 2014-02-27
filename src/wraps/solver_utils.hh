//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        solver_utils.hh
 *
 * Description: Utilities used in interfacing SAT solvers.
 *
 * Author:      jpms
 * 
 *                                     Copyright (c) 2010, Joao Marques-Silva
\*----------------------------------------------------------------------------*/
//jpms:ec

#ifndef _SOLVER_UTILS_H
#define _SOLVER_UTILS_H 1

//jpms:bc
/*----------------------------------------------------------------------------*\
 * Basic defs
\*----------------------------------------------------------------------------*/
//jpms:ec

typedef enum SAT_Result {
  SAT_NoRes = 0x0000,    // Default value: no result
  SAT_True  = 0x2000,    // Instance is satisfiable
  SAT_False = 0x2001,    // Instance is unsatisfiable
  SAT_Abort = 0x2002,    // Resources exceeded (results are invalid)
  SAT_Unknown = 0x2004   // Unknown (but still have an approximation, cf. SLS)
} SATRes;


namespace SolverUtils {

  template <typename S, typename T>
  void add_clauses(S& solver,
		   typename T::iterator ccpos, typename T::iterator ccend) {
    for(; ccpos != ccend; ++ccpos) { solver.add_clause(*ccpos); }
  }

  template <typename S, typename T>
  void del_clauses(S& solver, typename T::iterator ccpos,
		   typename T::iterator ccend) {
    for(; ccpos != ccend; ++ccpos) { solver.del_clause(*ccpos); }
  }

  template <typename S, typename T>
  void activate_clauses(S& solver, typename T::iterator ccpos,
			typename T::iterator ccend) {
    for(; ccpos != ccend; ++ccpos) { solver.activate_clause(*ccpos); }
  }

  template <typename S, typename T>
  void deactivate_clauses(S& solver, typename T::iterator ccpos,
			  typename T::iterator ccend) {
    for(; ccpos != ccend; ++ccpos) { solver.deactivate_clause(*ccpos); }
  }

}

#endif /* _SOLVER_UTILS_H */

/*----------------------------------------------------------------------------*/
