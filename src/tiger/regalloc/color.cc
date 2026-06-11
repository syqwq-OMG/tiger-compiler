#include "tiger/regalloc/color.h"
#include <iostream>

extern frame::RegManager *reg_manager;

namespace col {

Color::Color(live::LiveGraph live_graph, temp::Map *initial)
    : live_graph_(live_graph), initial_(initial) {
  coloring_ = temp::Map::Empty();
  precolored = new live::INodeList();
  initial_nodes = new live::INodeList();
  simplifyWorklist = new live::INodeList();
  freezeWorklist = new live::INodeList();
  spillWorklist = new live::INodeList();
  spilledNodes = new live::INodeList();
  coalescedNodes = new live::INodeList();
  coloredNodes = new live::INodeList();
  selectStack = new live::INodeList();

  coalescedMoves = new live::MoveList();
  constrainedMoves = new live::MoveList();
  frozenMoves = new live::MoveList();
  worklistMoves = new live::MoveList();
  activeMoves = new live::MoveList();

  auto regs = reg_manager->Registers();
  K = regs->GetList().size();
  
  for (auto temp : regs->GetList()) {
    std::string* name = initial_->Look(temp);
    if (name) machine_colors.push_back(name);
  }

  for (auto node : live_graph_.interf_graph->Nodes()->GetList()) {
    auto temp = node->NodeInfo();
    std::string* color_name = initial_->Look(temp);
    if (color_name) {
      precolored->Append(node);
      color[node] = color_name;
      degree[node] = 1000000;
    } else {
      initial_nodes->Append(node);
      degree[node] = 0;
    }
    alias[node] = node;
    adjList[node] = new live::INodeList();
    moveList[node] = new live::MoveList();
  }
}

Color::~Color() {}

void Color::AddEdge(live::INodePtr u, live::INodePtr v) {
  if (u == v) return;
  if (adjSet[u].find(v) == adjSet[u].end()) {
    adjSet[u].insert(v);
    adjSet[v].insert(u);
    if (!precolored->Contain(u)) {
      adjList[u]->Append(v);
      degree[u]++;
    }
    if (!precolored->Contain(v)) {
      adjList[v]->Append(u);
      degree[v]++;
    }
  }
}

void Color::Build() {
  for (auto node : live_graph_.interf_graph->Nodes()->GetList()) {
    for (auto succ : node->Succ()->GetList()) {
      AddEdge(node, succ);
    }
  }

  for (auto move : live_graph_.moves->GetList()) {
    auto u = move.first;
    auto v = move.second;
    moveList[u]->Append(u, v);
    moveList[v]->Append(u, v);
    worklistMoves->Append(u, v);
  }
}

void Color::MakeWorklist() {
  for (auto n : initial_nodes->GetList()) {
    if (degree[n] >= K) {
      spillWorklist->Append(n);
    } else if (MoveRelated(n)) {
      freezeWorklist->Append(n);
    } else {
      simplifyWorklist->Append(n);
    }
  }
  initial_nodes->Clear();
}

live::INodeListPtr Color::Adjacent(live::INodePtr n) {
  auto res = new live::INodeList();
  for (auto m : adjList[n]->GetList()) {
    if (!selectStack->Contain(m) && !coalescedNodes->Contain(m)) {
      res->Append(m);
    }
  }
  return res;
}

live::MoveList* Color::NodeMoves(live::INodePtr n) {
  return moveList[n]->Intersect(activeMoves->Union(worklistMoves));
}

bool Color::MoveRelated(live::INodePtr n) {
  return !NodeMoves(n)->GetList().empty();
}

void Color::Simplify() {
  auto n = simplifyWorklist->GetList().front();
  simplifyWorklist->DeleteNode(n);
  selectStack->Prepend(n);
  for (auto m : Adjacent(n)->GetList()) {
    DecrementDegree(m);
  }
}

void Color::DecrementDegree(live::INodePtr m) {
  int d = degree[m];
  degree[m] = d - 1;
  if (d == K) {
    auto nodes = Adjacent(m);
    nodes->Append(m);
    EnableMoves(nodes);
    spillWorklist->DeleteNode(m);
    if (MoveRelated(m)) {
      freezeWorklist->Append(m);
    } else {
      simplifyWorklist->Append(m);
    }
  }
}

void Color::EnableMoves(live::INodeListPtr nodes) {
  for (auto n : nodes->GetList()) {
    for (auto m : NodeMoves(n)->GetList()) {
      if (activeMoves->Contain(m.first, m.second)) {
        activeMoves->Delete(m.first, m.second);
        worklistMoves->Append(m.first, m.second);
      }
    }
  }
}

void Color::Coalesce() {
  auto m = worklistMoves->GetList().front();
  auto x = GetAlias(m.first);
  auto y = GetAlias(m.second);
  live::INodePtr u, v;
  if (precolored->Contain(y)) {
    u = y; v = x;
  } else {
    u = x; v = y;
  }
  worklistMoves->Delete(m.first, m.second);

  if (u == v) {
    coalescedMoves->Append(m.first, m.second);
    AddWorkList(u);
  } else if (precolored->Contain(v) || adjSet[u].find(v) != adjSet[u].end()) {
    constrainedMoves->Append(m.first, m.second);
    AddWorkList(u);
    AddWorkList(v);
  } else {
    bool ok = true;
    if (precolored->Contain(u)) {
      for (auto t : Adjacent(v)->GetList()) {
        if (!OK(t, u)) {
          ok = false;
          break;
        }
      }
    } else {
      ok = Conservative(Adjacent(u)->Union(Adjacent(v)));
    }

    if (ok) {
      coalescedMoves->Append(m.first, m.second);
      Combine(u, v);
      AddWorkList(u);
    } else {
      activeMoves->Append(m.first, m.second);
    }
  }
}

void Color::AddWorkList(live::INodePtr u) {
  if (!precolored->Contain(u) && !MoveRelated(u) && degree[u] < K) {
    freezeWorklist->DeleteNode(u);
    simplifyWorklist->Append(u);
  }
}

bool Color::OK(live::INodePtr t, live::INodePtr r) {
  return degree[t] < K || precolored->Contain(t) || adjSet[t].find(r) != adjSet[t].end();
}

bool Color::Conservative(live::INodeListPtr nodes) {
  int k = 0;
  for (auto n : nodes->GetList()) {
    if (degree[n] >= K) k++;
  }
  return k < K;
}

live::INodePtr Color::GetAlias(live::INodePtr n) {
  if (coalescedNodes->Contain(n)) {
    return GetAlias(alias[n]);
  }
  return n;
}

void Color::Combine(live::INodePtr u, live::INodePtr v) {
  if (freezeWorklist->Contain(v)) {
    freezeWorklist->DeleteNode(v);
  } else {
    spillWorklist->DeleteNode(v);
  }
  coalescedNodes->Append(v);
  alias[v] = u;
  moveList[u] = moveList[u]->Union(moveList[v]);
  auto v_list = new live::INodeList();
  v_list->Append(v);
  EnableMoves(v_list);

  for (auto t : Adjacent(v)->GetList()) {
    AddEdge(t, u);
    DecrementDegree(t);
  }

  if (degree[u] >= K && freezeWorklist->Contain(u)) {
    freezeWorklist->DeleteNode(u);
    spillWorklist->Append(u);
  }
}

void Color::Freeze() {
  auto u = freezeWorklist->GetList().front();
  freezeWorklist->DeleteNode(u);
  simplifyWorklist->Append(u);
  FreezeMoves(u);
}

void Color::FreezeMoves(live::INodePtr u) {
  for (auto m : NodeMoves(u)->GetList()) {
    auto x = m.first;
    auto y = m.second;
    live::INodePtr v;
    if (GetAlias(y) == GetAlias(u)) {
      v = GetAlias(x);
    } else {
      v = GetAlias(y);
    }
    activeMoves->Delete(x, y);
    frozenMoves->Append(x, y);
    if (NodeMoves(v)->GetList().empty() && degree[v] < K) {
      freezeWorklist->DeleteNode(v);
      simplifyWorklist->Append(v);
    }
  }
}

void Color::SelectSpill() {
  int max_degree = -1;
  live::INodePtr m = nullptr;
  for (auto n : spillWorklist->GetList()) {
    if (degree[n] > max_degree) {
      max_degree = degree[n];
      m = n;
    }
  }
  spillWorklist->DeleteNode(m);
  simplifyWorklist->Append(m);
  FreezeMoves(m);
}

void Color::AssignColors() {
  for (auto n : selectStack->GetList()) {
    std::set<std::string*> okColors;
    for (auto c : machine_colors) okColors.insert(c);

    for (auto w : adjList[n]->GetList()) {
      auto aliasW = GetAlias(w);
      if (coloredNodes->Contain(aliasW) || precolored->Contain(aliasW)) {
        okColors.erase(color[aliasW]);
      }
    }

    if (okColors.empty()) {
      spilledNodes->Append(n);
    } else {
      coloredNodes->Append(n);
      color[n] = *okColors.begin();
    }
  }

  for (auto n : coalescedNodes->GetList()) {
    color[n] = color[GetAlias(n)];
  }
}

Result Color::Allocate() {
  Build();
  MakeWorklist();

  while (!simplifyWorklist->GetList().empty() || !worklistMoves->GetList().empty() ||
         !freezeWorklist->GetList().empty() || !spillWorklist->GetList().empty()) {
    if (!simplifyWorklist->GetList().empty()) {
      Simplify();
    } else if (!worklistMoves->GetList().empty()) {
      Coalesce();
    } else if (!freezeWorklist->GetList().empty()) {
      Freeze();
    } else if (!spillWorklist->GetList().empty()) {
      SelectSpill();
    }
  }

  AssignColors();

  if (!spilledNodes->GetList().empty()) {
    auto all_spills = new live::INodeList();
    for (auto n : spilledNodes->GetList()) {
      all_spills->Append(n);
    }
    for (auto n : coalescedNodes->GetList()) {
      if (spilledNodes->Contain(GetAlias(n))) {
        all_spills->Append(n);
      }
    }
    return Result(nullptr, all_spills);
  }

  auto final_coloring = temp::Map::Empty();
  for (auto node : live_graph_.interf_graph->Nodes()->GetList()) {
    if (color.find(node) != color.end()) {
      final_coloring->Enter(node->NodeInfo(), color[node]);
    }
  }

  return Result(final_coloring, nullptr);
}

} // namespace col
