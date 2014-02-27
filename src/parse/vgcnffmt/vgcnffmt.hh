/*----------------------------------------------------------------------------*\
 * File:        vgcnffmt.hh
 *
 * Description: Class definitions for VGCNF parser -- this is the nickname for 
 *              CNF augmented with variable group information. The code is based
 *              on cnffmt.hh
 *
 * Author:      jpms, antonb
 * 
 * Notes:
 *              1. when linking, option -lz *must* be used
 *
 *                     Copyright (c) 2007-2012, Joao Marques-Silva, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _VGCNFFMT_H
#define _VGCNFFMT_H 1

#include <ctime>
#include <cmath>
#include <unistd.h>
#include <signal.h>
#include <zlib.h>
#include <vector>

#include "globals.hh"
#include "id_manager.hh"
#include "fmtutils.hh"
#include "basic_clause.hh"
#include "basic_clset.hh"
#include "cl_id_manager.hh"

using namespace std;

template<class B>
static void read_vgroup_vars(B& in, ULINT& vgid, vector<ULINT>& vars) {
  read_left_bracket(in);   // Parse left bracket
  vgid = FMTUtils::parseInt(in);    // Read clause group
  read_right_bracket(in);  // Parse right bracket
  LINT parsed_var;
  vars.clear();
  for (;;){
    parsed_var = FMTUtils::parseInt(in);
    if (parsed_var < 0) {
      cerr << "PARSE ERROR! Negative integer in a variable group {" 
          << vgid << "}: " << parsed_var << endl;
      exit(3); 
    }
    if (parsed_var == 0) break;
    vars.push_back((ULINT)parsed_var);
  }
}

/** Template parameters: B - input stream, CSet - the clause set to populate;
 * CSet is expected to have a method set_var_grp_id(ULINT var, GID vgid)
 */
template<class B, class CSet>
static void parse_vgcnf_file(B& in, IDManager& imgr, CSet& cldb) {
  ULINT mnid = 1;
  ULINT mxid = 1;
  ULINT clid = 0;       // clause's index in the input file
  ULINTSet vars, reg_vars; 
  for (;;){
    FMTUtils::skipWhitespace(in);
    if (*in == EOF)
      break;
    else if (*in == 'c')
      FMTUtils::skipLine(in);
    else if (*in == 'p') {
      ++in;
      FMTUtils::skipTabSpace(in);
      string fmt = FMTUtils::readString(in);
      if (fmt != "vgcnf") {
        cerr << "PARSE ERROR! Unexpected string: " << fmt << endl;
        exit(3); 
      }
      FMTUtils::skipTabSpace(in);
      LINT intcnt = 1;
      while (*in != '\n' && *in != '\r') {
        ULINT ival = (ULINT)FMTUtils::parseLongInt(in);
        if (intcnt == 1) { cldb.set_num_vars(ival); }
        else if (intcnt == 2) { cldb.set_num_cls(ival); }
        else if (intcnt == 3) { cldb.set_num_vgrp(ival); }
        else {
          LINT lval = ToLint(ival);
          cerr << "PARSE ERROR! Unexpected int: " << (long int)lval << endl;
          exit(3);
        }
        ++intcnt;
        FMTUtils::skipTabSpace(in);
      }
      ++in;
    } else if (*in == '{') {
      vector<ULINT> vars;
      ULINT vgid;
      read_vgroup_vars(in, vgid, vars);
      for (vector<ULINT>::iterator pvar = vars.begin(); pvar != vars.end(); ++pvar) {
        cldb.set_var_grp_id(*pvar, vgid);
        reg_vars.insert(*pvar);
      }
    } else {
      vector<LINT> lits;
      read_cnf_clause(in, mxid, lits);
      ++clid;
      // ANTON: for some applications its *essential* that clause id is the same
      // as the index of the clause in the input; if the input file contains 
      // dublicate clauses the automatic id will not be incremented, which in 
      // turn will mess up indexes of all clauses that follow; so catch up
      while (ClauseIdManager::Instance()->id() < clid) 
        ClauseIdManager::Instance()->new_id();
      BasicClause* ncl = cldb.create_clause(lits); // fheras: Automatic clause ID
      if (ncl != NULL)
        cldb.set_cl_grp_id(ncl, ncl->get_id());
      // remember the variables
      for (vector<LINT>::iterator plit = lits.begin(); plit != lits.end(); ++plit)
        vars.insert((ULINT)abs(*plit));
    }
  }
  imgr.new_ids(mxid, mnid, mxid);  // Register used IDs
  // check if any variables have not been given group IDs -- if yes, spit out a 
  // warning and add them to variable group 0
  unsigned count = 0;
  for (ULINTSet::iterator pvar = vars.begin(); pvar != vars.end(); ++pvar) {
    if (reg_vars.find(*pvar) == reg_vars.end()) {
      count++;
      cldb.set_var_grp_id(*pvar, 0);
    }
  }
  if (count) {
    cout << "PARSE WARNING: " << count 
        << " variables have not been given group ID, assuming group 0." << endl;
  }
}

/** Template parameters: CSet -- the clause set to populate
 */
template<class CSet>
class VarGroupCNFParserTmpl {
public:
  inline void load_vgcnf_file(gzFile input_stream,
      IDManager& imgr, CSet& cldb) {
    StreamBuffer in(input_stream);
    parse_vgcnf_file(in, imgr, cldb); }
};

#endif /* _VGCNFFMT_H */

/*----------------------------------------------------------------------------*/
