//
// trace_analyzer.hh -- the interface for resolution proof analysis functionality
//

#include <ext/hash_map>
#include <ext/hash_set>

struct nodeClause;

class TraceAnalyzer {

public:

	TraceAnalyzer();

	/* The type for map from picosat clause IDs to path counts */
	typedef __gnu_cxx::hash_map<int, double> PathCountMap;
	/* The type for set of picosat clause IDs */
	typedef __gnu_cxx::hash_set<int> ClauseSet;

	/* Set the stream from which the trace is to be read */
	void set_trace_stream(FILE* t_stream);

	/* Computes and returns the path count map: index = clause ID,
	 * value = number of paths in the proof.
	 */
	const PathCountMap& compute_path_count_map(void);

	/* Computes and returns the set of clauses in the support of
	 * some articulation point
	 */
	//trueSupport : true for 'true support' false otherwise; maxArtPoint: true if you want the support of the articulation point with more paths
	const ClauseSet& compute_interesting_support(bool trueSupport,bool maxArtPoint);

private:

	void readTrace(FILE *fp);

	void pathCount();

	void findArticulationPoint(nodeClause *ndCls);

	void printTrace();

	void reset();

	void findSupport(nodeClause *ndCls);

	PathCountMap _pmap;           // path count map

	ClauseSet _iset;              // interesting set

	unsigned currTime;			// used by articolation point algorithm

	bool pathCountEnded;		// if true the count of the paths has already been done

	bool computedInterestingSupport;	// if true the count of interesting support has already been done

};
