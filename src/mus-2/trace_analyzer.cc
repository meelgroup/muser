//============================================================================
// Name        : trace_analyzer.cpp
// Author      : Alessandro Previtti
// Version     :
// Copyright   : Your copyright notice
// Description : Analysis of resolution proofs in TRACECHECK format
//============================================================================

#include <iostream>
#include <vector>
#include <queue>
#include <algorithm>
#include "trace_analyzer.hh"
#include <cstdio>
#include <limits>

using namespace std;
using namespace __gnu_cxx; // <-- for hash_map

//#define DBG(x) x
#define DBG(x) 

#define NONE 	 0
#define ARTPNT   1
#define FRSTSPRT 2
#define BOTH	 3 //when the node is an articulation point and at the same time a first supporter

struct nodeClause {
	vector<int> curClause;
	vector<nodeClause*> parents;
	vector<nodeClause*> children;
	int picosatClauseID;
	unsigned numOfChildrenVisitor;
	double numPath;
	/*articulation point data and DFS*/
	bool visited;
	//unsigned depth;
	unsigned dscTime;//discovery time
	unsigned back;
	unsigned short nodeType; //type: articulation point; first supporter; none
};



/*********************************************************************/

//typedef struct node nodeClause;
vector<nodeClause*> nodeList;
vector<nodeClause*> articulationPoint;//use a set -> otherwise repetition
hash_map<int, int> hm_pi2i;  // number of the clause in the trace(picosat id) -> number of clause index


/*********************************************************************/


void TraceAnalyzer::readTrace(FILE *fp){
	int num;
	int counter = 0;


	while (fscanf(fp,"%d",&num) != EOF){
          if (num == -1) // end of proof marker
            break;
		nodeClause *nclause = new nodeClause;
		hm_pi2i[num] = counter++; // associate picosat clause ID with index clause
		nclause->picosatClauseID = num; // record the clause ID as given by Picosat

		//init var for the following BFS
		nclause->numPath = 0;
		nclause->numOfChildrenVisitor = 0;
		nclause->nodeType = NONE;
		nclause->visited = false;

                DBG(printf("clid=%d, Lits: ", num););
		do {
			fscanf(fp,"%d",&num);
                        DBG(printf("%d ",num););
			if (num != 0){
				nclause->curClause.push_back(num);
			}
		} while (num != 0);


		nodeList.push_back(nclause);

                DBG(printf(", parents: "););
		do {
			fscanf(fp,"%d",&num);
                        DBG(printf("%d ", num););
			if (num != 0){
				int indexClause = hm_pi2i[num];
				nclause->parents.push_back(nodeList[indexClause]);
				(nodeList[indexClause])->children.push_back(nclause);
			}
		} while (num != 0);
	}
}

void TraceAnalyzer::reset(){
	//reset all data structure before the next call
	for (unsigned i = 0; i < nodeList.size(); i++){
		nodeClause *ndCls = nodeList[i];
		delete(ndCls);
	}
	nodeList.clear();
	articulationPoint.clear();//use a set -> otherwise repetition
	hm_pi2i.clear();  // number of the clause in the trace(picosat id) -> number of clause index
	pathCountEnded = false; //because before using 'compute_interesting_suppor' we have to be sure that the path counting has been done
	computedInterestingSupport = false;
	_iset.clear();
	_pmap.clear();
}

void TraceAnalyzer::pathCount(){
	nodeClause *root = nodeList[nodeList.size() - 1];
	root->numPath = 1;//needed for the following cycle
	for (int i = nodeList.size() - 2; i >= 0; i--){
		nodeClause *currClause = nodeList[i];
		for (unsigned j = 0; j < currClause->children.size(); j++){
			currClause->numPath = currClause->numPath + currClause->children[j]->numPath;
		}
	}
	pathCountEnded = true;
}


/*void TraceAnalyzer::pathCount(){//using a BFS approach
	nodeClause *root = nodeList[nodeList.size() - 1];
	root->numPath = 1;//needed for the following cycle
	root->numOfChildrenVisitor = -1; //just needed for the if below
	nodeClauseQueue.push(root);


	while(!nodeClauseQueue.empty()){
		nodeClause *ndCls = nodeClauseQueue.front();
		nodeClauseQueue.pop();

		if (ndCls->children.size() == ++(ndCls->numOfChildrenVisitor)){//we can count the number of the path for the current node as the sum of the paths of its children

			for (unsigned i=0; i < ndCls->children.size(); i++){
				ndCls->numPath = ndCls->numPath + ndCls->children[i]->numPath;
			}
			for (unsigned i=0; i < ndCls->parents.size(); i++){
				nodeClauseQueue.push(ndCls->parents[i]);
			}
		}
	}
	pathCountEnded = true;
}*/


//Root(in general) must be handle separately...in this case is just the empty clause.
void TraceAnalyzer::findArticulationPoint(nodeClause *ndCls){
	currTime++;
	ndCls->dscTime = currTime;
	ndCls->back = currTime;
	ndCls->visited = true;

	unsigned connectionSize = ndCls->parents.size() + ndCls->children.size();


	for (unsigned i=0; i < connectionSize; i++){

		nodeClause *nextCls;

		//check in all direction i.e parents and children(undirected graph)
		if (i < ndCls->parents.size()){
			nextCls = ndCls->parents[i];
		} else {
			unsigned j = i - ndCls->parents.size();
			nextCls = ndCls->children[j];
		}

		if (nextCls->visited != true){
			//printf("From %d to %d\n", ndCls->picosatClauseID, nextCls->picosatClauseID);
			findArticulationPoint(nextCls);
			//Backtracking
			if (nextCls->back < ndCls->dscTime){
				//printf("NOT Articulation point %d(%d) for %d(%d) found\n", ndCls->picosatClauseID,ndCls->back, nextCls->picosatClauseID, nextCls->back);
				ndCls->back = min(ndCls->back,nextCls->back);
			} else {//we found an articulation point -> add it just if it's not the root
				nodeClause *root = nodeList[nodeList.size() - 1];
				if (ndCls->picosatClauseID != root->picosatClauseID){
					//printf("*** Articulation point %d for %d found\n", ndCls->picosatClauseID, nextCls->picosatClauseID);
					if (ndCls->nodeType == FRSTSPRT){
						ndCls->nodeType = BOTH;
					} else {
						ndCls->nodeType = ARTPNT;
					}
					if (nextCls->nodeType == ARTPNT){
						nextCls->nodeType = BOTH;
					} else {
						nextCls->nodeType = FRSTSPRT;
					}
					articulationPoint.push_back(ndCls);
				}
			}
		} else {//Visited: back-edge
			//printf("BACK LINK From %d to %d(already visited)", ndCls->picosatClauseID, nextCls->picosatClauseID);
			ndCls->back = min(ndCls->back,nextCls->dscTime);
			//printf("curr back:%d nextBack:%d\n",ndCls->back,nextCls->back);
		}
	}
}


void TraceAnalyzer::printTrace(){
	for (unsigned i=0; i < nodeList.size(); i++){
		printf("%d Curr clause: ", nodeList[i]->picosatClauseID);
		for (unsigned j=0; j < nodeList[i]->curClause.size(); j++){
			printf(" %d",nodeList[i]->curClause[j]);
		}
		printf("/ Num of Paths: %f / Parents: ",nodeList[i]->numPath);
		nodeClause *nd = nodeList[i];
		for (unsigned j=0; j < nd->parents.size(); j++){
			printf(" %d",nd->parents[j]->picosatClauseID);
		}
		printf("/ Children: ");
		for (unsigned j=0; j < nd->children.size(); j++){
					printf(" %d",nd->children[j]->picosatClauseID);
		}
		printf("\n");
	}
}


TraceAnalyzer::TraceAnalyzer(){
	pathCountEnded = false;
}

void TraceAnalyzer::findSupport(nodeClause *ndCls){

	if (ndCls->visited == true){
		return;
	}

	ndCls->visited = true;
	if (ndCls->parents.size() == 0){// ORIGINAL CLAUSE
		_iset.insert(ndCls->picosatClauseID);
		//printf("Found clause support %d....\n",ndCls->picosatClauseID);
		return;
	}
	for (unsigned i=0; i < ndCls->parents.size(); i++){
		//printf("from %d to %d....\n",ndCls->picosatClauseID, ndCls->parents[i]->picosatClauseID);
		findSupport(ndCls->parents[i]);
	}
}

const TraceAnalyzer::ClauseSet& TraceAnalyzer::compute_interesting_support(bool trueSupport,bool maxArtPoint){

	if (computedInterestingSupport == false){
		currTime = 0;

		if (pathCountEnded == false){
			pathCount();
		}

		//printTrace();

		nodeClause *root = nodeList[nodeList.size() - 1];
		findArticulationPoint(root);



		//printf("FINISHED ART POINT CALCULATION......\n");


		if (articulationPoint.size() != 0){
			unsigned index=0;

			if (maxArtPoint == false){
				double minNumPath = numeric_limits<double>::max();
				//Trova l'articulation point con il numero minore di paths
				for (unsigned i = 0; i < articulationPoint.size(); i++){
					if (articulationPoint[i]->numPath <= minNumPath){
						minNumPath = articulationPoint[i]->numPath;
						index = i;// controlla che non sia la radice....e cmq meglio sarebbe aggiungere livello a ogni nodo
					}
				}
			} else {
				unsigned long int maxNumPath = 0;
				//Trova l'articulation point con il numero maggiore di paths
				for (unsigned i = 0; i < articulationPoint.size(); i++){
					if (articulationPoint[i]->numPath >= maxNumPath){
						maxNumPath = articulationPoint[i]->numPath;
						index = i;// controlla che non sia la radice....e cmq meglio sarebbe aggiungere livello a ogni nodo
					}
				}
			}
			//printf("Trovata min articulation point\n");

			/*printf("Articulation point:");
			for (unsigned i = 0; i < articulationPoint.size(); i++){
				printf(" %d",articulationPoint[i]->picosatClauseID);
			}
			printf("...done\n");*/

			//needed because we use the visited var also for the findSupport method
			for (unsigned i=0; i < nodeList.size(); i++){
				nodeList[i]->visited = false;
			}

			//find the support of the choosen articulationPoint
			nodeClause *artPnt = articulationPoint[index];
			if (trueSupport == true){
				for (unsigned i=0; i < artPnt->parents.size(); i++){
					if (artPnt->parents[i]->nodeType == FRSTSPRT || artPnt->parents[i]->nodeType == BOTH){
						findSupport(artPnt->parents[i]);// if an articulation point exist
					}
				}
			} else {
				findSupport(artPnt);
			}

		} else {
                  //printf("No articulation point");//HERE
		}
		computedInterestingSupport = true;
	}


	//printf("Support size:%d \n",_iset.size());

	return _iset;
}

const TraceAnalyzer::PathCountMap& TraceAnalyzer::compute_path_count_map(){

	//don't compute more then once the num of path on the same instance
	if (pathCountEnded == false){
		pathCount();

		for (unsigned i = 0; i < nodeList.size(); i++){
			nodeClause *ndCls = nodeList[i];
			int picosatClauseID = ndCls->picosatClauseID;
			this->_pmap[picosatClauseID] = ndCls->numPath;
		}
	}

	return _pmap;
}

void TraceAnalyzer::set_trace_stream(FILE* t_stream){
	reset();

	/*FILE* tf = fopen("hello6.txt", "r");
    if (!tf) {
      cerr << "[tat] error: unable to open " << 'hello6.txt' << " for read/writing." << endl;
      exit(-1);
    }
	this->readTrace(tf);*/

	this->readTrace(t_stream);
}


/*int main() {
	cout << "Start reading formula" << endl;
	readTrace("hello2.txt");
	pathCount();
	printTrace();
}*/
