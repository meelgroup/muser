/*----------------------------------------------------------------------------*\
 * File:        mus_extraction_alg.hh
 *
 * Description: Class definitions of the variouls MUS extraction algorithms. The 
 *              algorithms are packaged as classes that implement operator(). 
 *              This allows the algorithms to be run on multiple threads, if 
 *              needed.
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *                                            Copyright (c) 2011-12, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _MUS_EXTRACTION_ALG_HH
#define _MUS_EXTRACTION_ALG_HH 1

#include "group_scheduler.hh"
#include "model_rotator.hh"
#include "mus_config.hh"
#include "mus_data.hh"
#include "sat_checker.hh"


/** This is the ABC of MUS extraction algorithm implementations. In single 
 * threaded environment, just invoke the operator().
 */
class MUSExtractionAlg {
  
public:

  /* Common parameters consist of configuration, MUS data, scheduler and some 
   * workers. Important point: if you are planning to use multiple threads, then
   * anything that will be shared between threads has to support the sharing.
   * For example, 
   *  a. if MUSData instance is shared, then make sure that MUSData has the 
   *     locks; 
   *  b. make sure that group scheduler is multi-thread safe;
   *  c. make sure that workers are designed to be run in MT environment (note
   *     that they do not have to be MT safe if they are not shared).
   */ 
  MUSExtractionAlg(IDManager& imgr, ToolConfig& conf, SATChecker& sc, 
                   ModelRotator& mr, MUSData& md, GroupScheduler& s) :
    _id(sc.id()), _imgr(imgr), config(conf), _schecker(sc), _mrotter(mr),
    _md(md), _sched(s) {}

  virtual ~MUSExtractionAlg(void) {}
  
  /* The main extraction logic is implemented here. The method does not modify 
   * the group set, but rather computes the group ids of MUS groups in MUSData
   */
  virtual void operator()(void) = 0;

  // extra configuration

  /** Sets the soft limit on elapsed CPU time (seconds). 0 means no limit. */
  void set_cpu_time_limit(double limit) { _cpu_time_limit = limit; }

  /** Sets the limit on the number of iteration, where an "iteration" is typically
   * an iteration of the main loop of the algo. 0 means no limit. */
  void set_iter_limit(unsigned limit) { _iter_limit = limit; }
  
  // stats

  unsigned sat_calls(void) { return _sat_calls; }

  unsigned sat_outcomes(void) { return _sat_outcomes; }

  unsigned unsat_outcomes(void) { return _unsat_outcomes; }

  unsigned rot_groups(void) { return _rot_groups; }

  unsigned ref_groups(void) { return _ref_groups; }

  unsigned tainted_cores(void) { return _tainted_cores; }
  
  double sat_time(void) { return _sat_time; }

  double sat_time_sat(void) { return _sat_time_sat; }

  double sat_time_unsat(void) { return _sat_time_unsat; }

protected:
  
  unsigned _id;                 // logical thread ID - same as SAT checker's

  IDManager& _imgr;             // id manager
  
  ToolConfig& config;           // configuration (name is good for macros)

  SATChecker& _schecker;        // the SAT checker for this thread

  ModelRotator& _mrotter;       // the model rotator for this thread

  MUSData& _md;                 // MUS data to work on

  GroupScheduler& _sched;       // group scheduler

  // configuration

  double _cpu_time_limit = 0;   // soft limit on CPU time (0 = no limit)

  unsigned _iter_limit = 0;     // limit on the number of iterations (0 = no limit)

  // stats

  unsigned _sat_calls = 0;      // number of SAT calls

  unsigned _sat_outcomes = 0;   // SAT outcomes

  unsigned _unsat_outcomes = 0; // UNSAT outcomes

  unsigned _unknown_outcomes = 0;// UNKNOWN outcomes (approx mode)

  unsigned _rot_groups = 0;     // number of groups due to rotation

  unsigned _ref_groups = 0;     // number of groups removed with refinement

  unsigned _tainted_cores = 0;  // number of time rr got in a way

  double _sat_time = 0;         // time spent SAT solving

  double _sat_time_sat = 0;     // SAT solving time (SAT outcomes)

  double _sat_time_unsat = 0;   // SAT solving time (UNSAT outcomes)

  double _sat_timer = 0;        // used for timing

  void start_sat_timer(void) { _sat_timer = RUSAGE::read_cpu_time(); }

  void stop_sat_timer(SATRes outcome) {
    _sat_timer = RUSAGE::read_cpu_time() - _sat_timer;
    _sat_time += _sat_timer;
    if (outcome == SAT_True) { _sat_time_sat += _sat_timer; }
    if (outcome == SAT_False) { _sat_time_unsat += _sat_timer; }
  }
  
};

//
// concrete algorithms follow ...
//


/** This is the implementation of deletion-based MUS extraction algorithm.
 */
class MUSExtractionAlgDel : public MUSExtractionAlg {
  
public:
  
  MUSExtractionAlgDel(IDManager& imgr, ToolConfig& conf, SATChecker& sc, 
                      ModelRotator& mr, MUSData& md, GroupScheduler& s) 
    : MUSExtractionAlg(imgr, conf, sc, mr, md, s) {}

  /* The main extraction logic is implemented here.
   */
  void operator()(void);

};


/** This is the implementation of the basic insertion-based MUS extraction 
 * algorithm.
 */
class MUSExtractionAlgIns : public MUSExtractionAlg {
  
public:
  
  MUSExtractionAlgIns(IDManager& imgr, ToolConfig& conf, SATChecker& sc,        
                      ModelRotator& mr, MUSData& md, GroupScheduler& s) 
    : MUSExtractionAlg(imgr, conf, sc, mr, md, s) {}

  /* The main extraction logic is implemented here.
   */
  void operator()(void);

};


/** This is the implementation of dichotomic MUS extraction algorithm.
 */
class MUSExtractionAlgDich : public MUSExtractionAlg {
  
public:
  
  MUSExtractionAlgDich(IDManager& imgr, ToolConfig& conf, SATChecker& sc, 
                       ModelRotator& mr, MUSData& md, GroupScheduler& s) 
    : MUSExtractionAlg(imgr, conf, sc, mr, md, s) {}

  /* The main extraction logic is implemented here.
   */
  void operator()(void);

};


/** This is the implementation of chunked deletion-based MUS extraction 
 * algorithm (AAAI-12)
 */
class MUSExtractionAlgChunk : public MUSExtractionAlg {
  
public:
  
  MUSExtractionAlgChunk(IDManager& imgr, ToolConfig& conf, SATChecker& sc, 
                        ModelRotator& mr, MUSData& md, GroupScheduler& s) 
    : MUSExtractionAlg(imgr, conf, sc, mr, md, s) {}

  /* The main extraction logic is implemented here.
   */
  void operator()(void);

};


/** This is an implementation of deletion-based VMUS algorithm
 */
class VMUSExtractionAlgDel : public MUSExtractionAlg {
  
public:
  
  VMUSExtractionAlgDel(IDManager& imgr, ToolConfig& conf, SATChecker& sc, 
                       ModelRotator& mr, MUSData& md, GroupScheduler& s) 
    : MUSExtractionAlg(imgr, conf, sc, mr, md, s) {}

  /* The main extraction logic is implemented here.
   */
  void operator()(void);

};


/** This is the implementation of subset-based deletion-based MUS extraction 
 * algorithm (ECAI-12)
 */
class MUSExtractionAlgSubset : public MUSExtractionAlg {
  
public:
  
  MUSExtractionAlgSubset(IDManager& imgr, ToolConfig& conf, SATChecker& sc, 
                         ModelRotator& mr, MUSData& md, GroupScheduler& s) 
    : MUSExtractionAlg(imgr, conf, sc, mr, md, s), _subset_singleton(0), 
      _subset_nonsingleton(0), _analyzer_time(0), _sat_guesses(0), 
      _sat_correct_guesses(0), _unsat_guesses(0), _unsat_correct_guesses(0) {}

  /* The main extraction logic is implemented here.
   */
  void operator()(void);

protected:

  unsigned _subset_singleton;   // singleton outcomes in subset SAT outcomes
  
  unsigned _subset_nonsingleton;// non-singleton outcomes in subset SAT outcomes

  double _analyzer_time;        // time for analyzer itself

  unsigned _sat_guesses;        // times SAT outcome was expected based on heuristic
  
  unsigned _sat_correct_guesses;// times the SAT guess was correct

  unsigned _unsat_guesses;      // times UNSAT outcome was expected based on heuristic
  
  unsigned _unsat_correct_guesses;// times the UNSAT guess was correct

};


/** This is the implementation of an experimental resolution-graph based
 * subset-based deletion-based MUS extraction algorithm
 */
class MUSExtractionAlgSubset2 : public MUSExtractionAlg {
  
public:
  
  MUSExtractionAlgSubset2(IDManager& imgr, ToolConfig& conf, SATChecker& sc, 
                          ModelRotator& mr, MUSData& md, GroupScheduler& s) 
    : MUSExtractionAlg(imgr, conf, sc, mr, md, s) {}

  /* The main extraction logic is implemented here.
   */
  void operator()(void);

protected:      // implementation of various subsetting heurisics
                // note: the methods clear subset_gids

  /* Takes a number of groups in the order given by the scheduler */
  void make_order_subset(GIDSet& subset_gids);  

  /* Takes a group from scheduler, and builds its 1-neighbourhood in the
   * resolution graph.
   */
  void make_rgraph_1hood_subset(GIDSet& subset_gids);

protected:

  unsigned _subset_singleton = 0;   // singleton outcomes in subset SAT outcomes
  
  unsigned _subset_nonsingleton = 0;// non-singleton outcomes in subset SAT outcomes

  unsigned _sat_guesses = 0;        // times SAT outcome was expected based on heuristic
  
  unsigned _sat_correct_guesses = 0;// times the SAT guess was correct

  unsigned _unsat_guesses = 0;      // times UNSAT outcome was expected based on heuristic
  
  unsigned _unsat_correct_guesses = 0;// times the UNSAT guess was correct

};


/*----------------------------------------------------------------------------*/

#endif // _MUS_EXTRACTION_ALG_H
