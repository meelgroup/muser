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
#include "length_vscheduler.hh"
#include "linear_scheduler.hh"
#include "linear_vscheduler.hh"
#include "mus_config.hh"
#include "mus_data.hh"
#include "mus_extractor.hh"
#include "mus_extraction_alg.hh"
#include "mus_extraction_alg_fbar.hh"
#include "mus_extraction_alg_prog.hh"
#include "random_scheduler.hh"
#include "rgraph_scheduler.hh"
#ifdef MULTI_THREADED
#include <tbb/tbb.h>
#include <tbb/tick_count.h>
#include <tbb/tbb_thread.h>
#include <tbb/compat/thread>
#include "linear_scheduler_mt.hh"
#endif

#ifndef MULTI_THREADED

/* Handles the ComputeMUS work item (single-threaded mode)
 */
bool MUSExtractor::process(ComputeMUS& cm)
{
  MUSData& md = cm.md();

  // workers
  bool own_schecker = false;
  if (_pschecker == NULL) {
    _pschecker = new SATChecker(_imgr, config);
    _pschecker->set_pre_mode(config.get_solpre_mode());
    own_schecker = true;
  }
  ModelRotator* pmrotter = 0;
  if (config.get_model_rotate_mode()) {
    if (!config.get_var_mode()) {
      if (config.get_rmr_mode())
        pmrotter = new RecursiveModelRotator();
      else if (config.get_emr_mode())
        pmrotter = new ExtendedModelRotator();
      else if (config.get_imr_mode())
        pmrotter = new IrrModelRotator();
      else if (config.get_intelmr_mode())
        pmrotter = new IntelModelRotator(config, _pschecker->solver());
      else if (config.get_smr_mode())
        pmrotter = new SiertModelRotator(config.get_smr_mode());
    } else {
      if (config.get_rmr_mode() || config.get_emr_mode())
        pmrotter = new VMUSModelRotator();
    }
  } else {
    pmrotter = new ModelRotator();  // dummy one
  }
  if (pmrotter == 0) // shouldn't happen
    throw std::logic_error("could not pick model rotator");
  ModelRotator& mrotter(*pmrotter);

  // scheduler
  GroupScheduler* psched = 0;
  if (!config.get_var_mode()) {
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
#ifdef XPMODE
    case 5: case 7:
      if (!md.has_rgraph()) md.build_rgraph(); 
      psched = new RGraphSchedulerMax(md); break;
    case 6: case 8:
      if (!md.has_rgraph()) md.build_rgraph(); 
      psched = new RGraphSchedulerMin(md); break;
    case 9:
      psched = new ImplRGraphSchedulerMax(md); break;
    case 10:
      psched = new ImplRGraphSchedulerMin(md); break;
    case 11:
      psched = new ImplCGraphSchedulerMax(md); break;
    case 12:
      psched = new ImplCGraphSchedulerMin(md); break;
#endif
    }
  } else {
    switch (config.get_order_mode()) {
    case 0:
      psched = new LinearVScheduler(md); break;
    case 1:
      psched = new LengthVScheduler(md, 1); break;      
    case 2:
      psched = new LengthVScheduler(md, 2); break;
    case 3:
      psched = new LinearVScheduler(md, true); break;
    }
  }
  if (psched == 0) // shouldn't happen
    throw std::logic_error("could not pick scheduler");
  GroupScheduler& sched(*psched);

  // extrator algo
  MUSExtractionAlg* pmus_thread = 0;
  if (!config.get_var_mode()) {
    if (config.get_del_mode())
      pmus_thread = new MUSExtractionAlgDel(_imgr, config, *_pschecker, mrotter, md, sched);
    else if (config.get_chunk_mode())
      pmus_thread = new MUSExtractionAlgChunk(_imgr, config, *_pschecker, mrotter, md, sched);
    else if (config.get_subset_mode() >= 10)
      pmus_thread = new MUSExtractionAlgSubset2(_imgr, config, *_pschecker, mrotter, md, sched);
    else if (config.get_subset_mode() >= 0)
      pmus_thread = new MUSExtractionAlgSubset(_imgr, config, *_pschecker, mrotter, md, sched);
    else if (config.get_ins_mode()) 
      pmus_thread = new MUSExtractionAlgIns(_imgr, config, *_pschecker, mrotter, md, sched);
    else if (config.get_dich_mode()) 
      pmus_thread = new MUSExtractionAlgDich(_imgr, config, *_pschecker, mrotter, md, sched);
    else if (config.get_fbar_mode())
      pmus_thread = new MUSExtractionAlgFBAR(_imgr, config, *_pschecker, mrotter, md, sched);
    else if (config.get_prog_mode())
      pmus_thread = new MUSExtractionAlgProg(_imgr, config, *_pschecker, mrotter, md, sched);
  } else {
    if (config.get_del_mode())
      pmus_thread = new VMUSExtractionAlgDel(_imgr, config, *_pschecker, mrotter, md, sched);
  }
  if (pmus_thread == 0) // shouldn't happen
    throw std::logic_error("could not pick MUS extration algorithm");
  MUSExtractionAlg& mus_thread(*pmus_thread);
  mus_thread.set_cpu_time_limit(_cpu_time_limit);
  mus_thread.set_iter_limit(_iter_limit);

  double t_start = RUSAGE::read_cpu_time();
  mus_thread();
  double t_end = RUSAGE::read_cpu_time();
  // stats
  _cpu_time = t_end - t_start;
  _sat_calls = mus_thread.sat_calls();
  _rot_groups = mus_thread.rot_groups();
  _ref_groups = mus_thread.ref_groups();

  delete &mus_thread;
  delete &sched;
  delete &mrotter;
  if (own_schecker) { delete _pschecker; }

  cm.set_completed();
  return true;  // all good
}

#else 

/* Handles the ComputeMUS work item (multi-threaded mode)
 */
bool MUSExtractor::process(ComputeMUS& cm)
{
  MUSData& md = cm.md();

  // figure out the actual number of threads
  unsigned num_threads = config.get_num_threads();
  if ((num_threads == 0) || (num_threads > thread::hardware_concurrency()))
    num_threads = thread::hardware_concurrency();
  
  cout_pref << "Number of threads (used/avail): " << num_threads << "/"
            << thread::hardware_concurrency() << endl;
  assert(num_threads);

  // clones of ID manager
  vector<IDManager> imgrs(num_threads, _imgr);

  // sat checkers -- if one is given, reuse it
  vector<SATChecker*> scheckers(num_threads, _pschecker);
  for (unsigned id = 0; id < num_threads; id++)
    if (id > 0 || _pschecker == NULL)
      scheckers[id] = new SATChecker(imgrs[id], config, id);

  // scheduler (TODO: pick one based on configuration)
  LinearSchedulerMT sched(md, num_threads);
  //BlockSchedulerMT sched(md, num_threads);
  
  // rotator(s) TEMP: will not be used
  ModelRotator& mrotter(*new ModelRotator());
  assert(!config.get_model_rotate_mode());

  // off we go ...
  double t_start = RUSAGE::read_cpu_time();
  tbb::tick_count wt_start = tbb::tick_count::now();
  vector<thread*> mus_threads(num_threads);
  for (unsigned id = 0; id < num_threads; id++)
    mus_threads[id] = 
      new thread(MUSExtractionAlgDel(_imgr, config, *scheckers[id], mrotter, md, sched));
  // wait for them to finish ...
  for (unsigned id = 0; id < num_threads; id++)
      mus_threads[id]->join();      
  // done
  tbb::tick_count wt_end = tbb::tick_count::now();
  double t_end = RUSAGE::read_cpu_time();    

  // stats and cleanup
  _cpu_time = t_end - t_start;
  _wc_time = (wt_end - wt_start).seconds();
  _sat_calls = _rot_groups = 0; // TEMP - figure out how to do this 
  delete &mrotter;
  for (unsigned id = 0; id < num_threads; id++) {
    delete mus_threads[id];
    if (id > 0 || _pschecker == NULL)
      delete scheckers[id];
  }

  // done
  cm.set_completed();
  return true;
}

#endif // MULTI_THREADED


/*----------------------------------------------------------------------------*/
