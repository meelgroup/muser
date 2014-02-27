
/*----------------------------------------------------------------------------*\
 * File:        gcnffmt.hh
 *
 * Description: Class definitions for the Group-oriented CNF parser,
 *              based extensively on MiniSAT parser, but using the STL.
 *              NOTE: When linking, option -lz *must* be used
 *
 * Author:      jpms
 * 
 *                                     Copyright (c) 2010, Joao Marques-Silva
\*----------------------------------------------------------------------------*/

#ifndef _GCNFFMT_H
#define _GCNFFMT_H 1

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


//jpms:bc
/*----------------------------------------------------------------------------*\
 * SoftCNF Parser: (This borrows **extensively** from the MiniSAT parser)
\*----------------------------------------------------------------------------*/
//jpms:ec

template<class B>
static
void read_gcnf_clause(B& in, ULINT& clgrp, ULINT& mxid, vector<LINT>& lits) {
  read_left_bracket(in);   // Parse left bracket
  clgrp = FMTUtils::parseInt(in);    // Read clause group
  read_right_bracket(in);  // Parse right bracket

  LINT parsed_lit;
  lits.clear();
  for (;;){
    parsed_lit = FMTUtils::parseInt(in);
    if (parsed_lit == 0) break;
    if (fabs(parsed_lit) > mxid) { mxid = fabs(parsed_lit); }
    lits.push_back(parsed_lit);
  }
}

/** Template parameters: B - input stream, CSet - the clause set to populate
 */
template<class B, class CSet>
static void parse_gcnf_file(B& in, IDManager& imgr, CSet& cldb) {
  ULINT mnid = 1;
  ULINT mxid = 1;
  ULINT clid = 0;   // index of the clause in the input
  vector<LINT> lits;
  for (;;){
	FMTUtils::skipWhitespace(in);
    if (*in == EOF)
      break;
    else if (*in == 'c')
       FMTUtils::skipLine(in);
    else if (*in == 'p') {
      ++in;
      // Either read 2 or 3 integers, depending on new line
      FMTUtils::skipTabSpace(in);
      string fmt = FMTUtils::readString(in);
      if (fmt != "gcnf") {
	cerr << "PARSE ERROR! Unexpected int: " << fmt << endl; exit(3); }
      FMTUtils::skipTabSpace(in);
      LINT intcnt = 1;
      while (*in != '\n' && *in != '\r') {
	XLINT ival = FMTUtils::parseLongInt(in);
	if (intcnt == 1) { cldb.set_num_vars(ToLint(ival)); }
	else if (intcnt == 2) { cldb.set_num_cls(ToLint(ival)); }
	else if (intcnt == 3) { cldb.set_num_grp(ival); }
	else {
	  LINT lval = ToLint(ival);
	  fprintf(stderr, "PARSE ERROR! Unexpected int: %ld\n",
		  (long int)lval); exit(3);
	} ++intcnt; FMTUtils::skipTabSpace(in); }
      ++in;
    } else {
      ULINT clgrp;
      read_gcnf_clause(in, clgrp, mxid, lits);
      ++clid;
      // catch up (see cnffmt.hh for explanations)
      while (ClauseIdManager::Instance()->id() < clid) 
        ClauseIdManager::Instance()->new_id();
      BasicClause* ncl = cldb.create_clause(lits); // fheras: Automatic clause ID
      assert(ncl != NULL);
      if (ncl != NULL)
        cldb.set_cl_grp_id(ncl, clgrp);
    }
  }
  imgr.new_ids(mxid, mnid, mxid);  // Register used IDs
}

template<class B>
static
void read_left_bracket(B& in)
{
  if (*in != '{') {
    cerr << "PARSE ERROR! Expecting { instead of " << *in << endl; exit(3);
  }
  ++in;
}

template<class B>
static
void read_right_bracket(B& in)
{
  if (*in != '}') {
    cerr << "PARSE ERROR! Expecting } instead of " << *in << endl; exit(3);
  }
  ++in;
}

/** Template parameters: CSet -- the clause set to populate
 */
template<class CSet>
class GroupCNFParserTmpl {
public:
  inline void load_gcnf_file(gzFile input_stream, 
			     IDManager& imgr, CSet& cldb) {
    StreamBuffer in(input_stream);
    parse_gcnf_file(in, imgr, cldb); }
};
// definition of GroupCNFParser, for backward compatibility
typedef GroupCNFParserTmpl<BasicClauseSet> GroupCNFParser;


#endif /* _GCNFFMT_H */

/*----------------------------------------------------------------------------*/
