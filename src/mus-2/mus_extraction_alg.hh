/*----------------------------------------------------------------------------*\
 * File:        mus_extraction_alg.hh
 *
 * Description: Class definitions of the variouls MUS extraction algorithms. The 
 *              algorithms are packaged as classes that implement operator(). 
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *                                            Copyright (c) 2011-12, Anton Belov
\*----------------------------------------------------------------------------*/

#include "group_scheduler.hh"
#include "model_rotator.hh"
#include "mus_config.hh"
#include "mus_data.hh"
#include "sat_checker.hh"

/** This is the ABC of MUS extraction algorithm implementations.
 */
class MUSExtractionAlg {
  
public:

  /* Common parameters consist of configuration, MUS data, scheduler and some 
   * workers.
   */ 
  MUSExtractionAlg(IDManager& imgr, ToolConfig& conf, SATChecker& sc, 
                   ModelRotator& mr, MUSData& md, GroupScheduler& s) :
    _id(sc.id()), _imgr(imgr), config(conf), _schecker(sc), _mrotter(mr),
    _md(md), _sched(s), _sat_calls(0), _rot_groups(0), _ref_groups(0),
    _sat_time(0), _sat_outcomes(0), _unsat_outcomes(0), _tainted_cores(0) {}

  virtual ~MUSExtractionAlg(void) {}
  
  /* The main extraction logic is implemented here. The method does not modify 
   * the group set, but rather computes the group ids of MUS groups in MUSData
   */
  virtual void operator()(void) = 0;

  // stats

  unsigned sat_calls(void) { return _sat_calls; }
  
  unsigned rot_groups(void) { return _rot_groups; }
  
  unsigned ref_groups(void) { return _ref_groups; }

  double sat_time(void) { return _sat_time; }

  unsigned sat_outcomes(void) { return _sat_outcomes; }

  unsigned unsat_outcomes(void) { return _unsat_outcomes; }

  unsigned tainted_cores(void) { return _tainted_cores; }
  
protected:
  
  unsigned _id;                 // logical ID - same as SAT checker's
  
  IDManager& _imgr;             // id manager
  
  ToolConfig& config;           // configuration (name is good for macros)

  SATChecker& _schecker;        // the SAT checker

  ModelRotator& _mrotter;       // the model rotator

  MUSData& _md;                 // MUS data to work on

  GroupScheduler& _sched;       // group scheduler

  // stats

  unsigned _sat_calls;          // actual SAT calls made by SATChecker

  unsigned _rot_groups;         // number of groups due to rotation

  unsigned _ref_groups;         // number of groups removed with refinement

  double _sat_time;             // time spent SAT solving

  unsigned _sat_outcomes;       // SAT outcomes

  unsigned _unsat_outcomes;     // UNSAT outcomes

  unsigned _tainted_cores;      // number of time rr got in a way
  
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


/*----------------------------------------------------------------------------*/
