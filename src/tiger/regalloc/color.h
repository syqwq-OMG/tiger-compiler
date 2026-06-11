#ifndef TIGER_COMPILER_COLOR_H
#define TIGER_COMPILER_COLOR_H

#include "tiger/codegen/assem.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/util/graph.h"
#include <map>
#include <set>
#include <vector>

namespace col {
struct Result {
  Result() : coloring(nullptr), spills(nullptr) {}
  Result(temp::Map *coloring, live::INodeListPtr spills)
      : coloring(coloring), spills(spills) {}
  temp::Map *coloring;
  live::INodeListPtr spills;
};

class Color {
public:
  Color(live::LiveGraph live_graph, temp::Map *initial);
  ~Color();

  Result Allocate();

private:
  live::LiveGraph live_graph_;
  temp::Map *initial_;
  temp::Map *coloring_;

  live::INodeListPtr precolored;
  live::INodeListPtr initial_nodes;
  live::INodeListPtr simplifyWorklist;
  live::INodeListPtr freezeWorklist;
  live::INodeListPtr spillWorklist;
  live::INodeListPtr spilledNodes;
  live::INodeListPtr coalescedNodes;
  live::INodeListPtr coloredNodes;
  live::INodeListPtr selectStack;

  std::map<live::INodePtr, std::set<live::INodePtr>> adjSet;
  std::map<live::INodePtr, live::INodeListPtr> adjList;
  std::map<live::INodePtr, int> degree;
  std::map<live::INodePtr, live::MoveList*> moveList;
  std::map<live::INodePtr, live::INodePtr> alias;
  std::map<live::INodePtr, std::string*> color;

  live::MoveList* coalescedMoves;
  live::MoveList* constrainedMoves;
  live::MoveList* frozenMoves;
  live::MoveList* worklistMoves;
  live::MoveList* activeMoves;

  int K;
  std::vector<std::string*> machine_colors;

  void Build();
  void AddEdge(live::INodePtr u, live::INodePtr v);
  void MakeWorklist();
  live::INodeListPtr Adjacent(live::INodePtr n);
  live::MoveList* NodeMoves(live::INodePtr n);
  bool MoveRelated(live::INodePtr n);
  void Simplify();
  void DecrementDegree(live::INodePtr m);
  void EnableMoves(live::INodeListPtr nodes);
  void Coalesce();
  void AddWorkList(live::INodePtr u);
  bool OK(live::INodePtr t, live::INodePtr r);
  bool Conservative(live::INodeListPtr nodes);
  live::INodePtr GetAlias(live::INodePtr n);
  void Combine(live::INodePtr u, live::INodePtr v);
  void Freeze();
  void FreezeMoves(live::INodePtr u);
  void SelectSpill();
  void AssignColors();
  
  void init();
};
} // namespace col

#endif // TIGER_COMPILER_COLOR_H
