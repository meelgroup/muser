/*----------------------------------------------------------------------------*\
 * File:        mus_extractor_del.cc
 *
 * Description: Implementation of deletion-based MUS extractor.
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *                                          Copyright (c) 2011-2012, Anton Belov
\*----------------------------------------------------------------------------*/

#include "basic_group_set.hh"
#include "group_scheduler.hh"
#include "id_manager.hh"
#include "length_scheduler.hh"
#include "linear_scheduler.hh"
#include "mus_config.hh"
#include "mus_data.hh"
#include "mus_extractor.hh"
#include "mus_extraction_alg.hh"
#include "random_scheduler.hh"


/* Handles the ComputeMUS work item
 */
bool MUSExtractor::process(ComputeMUS& cm)
{
  // and make a copy of config that forces "group mode" so that the
  // group-based solvers are created; TODO: fix this
  ToolConfig sol_config = config;
  sol_config.set_grp_mode();

  MUSData& md = cm.md();

  // workers
  SATChecker& schecker((_pschecker == NULL) 
                       ? *(new SATChecker(_imgr, sol_config)) : *_pschecker);
  ModelRotator* pmrotter = 0;
  if (config.get_model_rotate_mode()) {
    if (config.get_rmr_mode())
      pmrotter = new RecursiveModelRotator();
  } else {
    pmrotter = new ModelRotator();  // dummy one
  }     
  if (pmrotter == 0) // shouldn't happen
    throw std::logic_error("could not pick model rotator");
  ModelRotator& mrotter(*pmrotter);

  // scheduler - depends on the mode
  GroupScheduler* psched = 0;
  switch (config.get_order_mode()) {
  case 0:
    psched = new LinearScheduler(md); break;
  case 1:
    psched = new LengthScheduler(md, 1); break;      
  case 2:
    psched = new LengthScheduler(md, 2); break;
  case 3:
    psched = new LinearScheduler(md, true); break;
  case 4:
    psched = new RandomScheduler(md); break;
  }
  if (psched == 0) // shouldn't happen
    throw std::logic_error("could not pick scheduler");
  GroupScheduler& sched(*psched);

  // extrator algo
  MUSExtractionAlg* pmus_alg = 0;
  if (config.get_del_mode())
    pmus_alg = new MUSExtractionAlgDel(_imgr, config, schecker, mrotter, md, sched);
  else if (config.get_ins_mode()) 
    pmus_alg = new MUSExtractionAlgIns(_imgr, config, schecker, mrotter, md, sched);
  else if (config.get_dich_mode()) 
    pmus_alg = new MUSExtractionAlgDich(_imgr, config, schecker, mrotter, md, sched);
  if (pmus_alg == 0) // shouldn't happen
    throw std::logic_error("could not pick MUS extration algorithm");
  MUSExtractionAlg& mus_alg(*pmus_alg);

  double t_start = RUSAGE::read_cpu_time();
  mus_alg();
  double t_end = RUSAGE::read_cpu_time();
  // stats
  _cpu_time = t_end - t_start;
  _sat_calls = mus_alg.sat_calls();
  _rot_groups = mus_alg.rot_groups();
  _ref_groups = mus_alg.ref_groups();

  delete &mus_alg;
  delete &sched;
  delete &mrotter;
  if (_pschecker == NULL)     
    delete &schecker;

  cm.set_completed();
  return true;  // all good
}

/*----------------------------------------------------------------------------*/
