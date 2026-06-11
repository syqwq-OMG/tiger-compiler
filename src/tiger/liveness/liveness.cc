#include "tiger/liveness/liveness.h"

extern frame::RegManager *reg_manager;

namespace live {

bool MoveList::Contain(INodePtr src, INodePtr dst) {
  return std::any_of(move_list_.cbegin(), move_list_.cend(),
                     [src, dst](std::pair<INodePtr, INodePtr> move) {
                       return move.first == src && move.second == dst;
                     });
}

void MoveList::Delete(INodePtr src, INodePtr dst) {
  assert(src && dst);
  auto move_it = move_list_.begin();
  for (; move_it != move_list_.end(); move_it++) {
    if (move_it->first == src && move_it->second == dst) {
      break;
    }
  }
  move_list_.erase(move_it);
}

MoveList *MoveList::Union(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : move_list_) {
    res->move_list_.push_back(move);
  }
  for (auto move : list->GetList()) {
    if (!res->Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

MoveList *MoveList::Intersect(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : list->GetList()) {
    if (Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

void LiveGraphFactory::LiveMap() {
  for (auto node : flowgraph_->Nodes()->GetList()) {
    in_->Enter(node, new temp::TempList());
    out_->Enter(node, new temp::TempList());
  }

  bool changed = true;
  while (changed) {
    changed = false;
    auto node_list = flowgraph_->Nodes()->GetList();
    for (auto it = node_list.rbegin(); it != node_list.rend(); ++it) {
      auto node = *it;
      auto old_in = in_->Look(node);
      auto old_out = out_->Look(node);

      auto instr = node->NodeInfo();
      auto use = instr->Use();
      auto def = instr->Def();

      auto new_in = new temp::TempList();
      if (use) {
        for (auto u : use->GetList()) new_in->Append(u);
      }
      if (old_out) {
        for (auto o : old_out->GetList()) {
          bool found = false;
          if (def) {
            for (auto d : def->GetList()) {
              if (o == d) { found = true; break; }
            }
          }
          if (!found) {
            bool in_new_in = false;
            for (auto n : new_in->GetList()) {
              if (n == o) { in_new_in = true; break; }
            }
            if (!in_new_in) new_in->Append(o);
          }
        }
      }

      auto new_out = new temp::TempList();
      for (auto s : node->Succ()->GetList()) {
        auto s_in = in_->Look(s);
        if (s_in) {
          for (auto si : s_in->GetList()) {
            bool in_new_out = false;
            for (auto no : new_out->GetList()) {
              if (no == si) { in_new_out = true; break; }
            }
            if (!in_new_out) new_out->Append(si);
          }
        }
      }

      auto temp_list_eq = [](temp::TempList* a, temp::TempList* b) {
        if (!a && !b) return true;
        if (!a || !b) return false;
        if (a->GetList().size() != b->GetList().size()) return false;
        for (auto ta : a->GetList()) {
          bool found = false;
          for (auto tb : b->GetList()) {
            if (ta == tb) { found = true; break; }
          }
          if (!found) return false;
        }
        return true;
      };

      if (!temp_list_eq(old_in, new_in)) {
        in_->Set(node, new_in);
        changed = true;
      } else {
        delete new_in;
      }

      if (!temp_list_eq(old_out, new_out)) {
        out_->Set(node, new_out);
        changed = true;
      } else {
        delete new_out;
      }
    }
  }
}

void LiveGraphFactory::InterfGraph() {
  auto get_or_create_node = [this](temp::Temp* t) -> INode* {
    INode* n = temp_node_map_->Look(t);
    if (!n) {
      n = live_graph_.interf_graph->NewNode(t);
      temp_node_map_->Enter(t, n);
    }
    return n;
  };

  std::vector<temp::Temp*> all_machine_regs;
  if (reg_manager->Registers()) {
    for (auto r : reg_manager->Registers()->GetList()) all_machine_regs.push_back(r);
  }
  all_machine_regs.push_back(reg_manager->StackPointer());

  for (size_t i = 0; i < all_machine_regs.size(); i++) {
    INode* ni = get_or_create_node(all_machine_regs[i]);
    for (size_t j = i + 1; j < all_machine_regs.size(); j++) {
      INode* nj = get_or_create_node(all_machine_regs[j]);
      live_graph_.interf_graph->AddEdge(ni, nj);
      live_graph_.interf_graph->AddEdge(nj, ni);
    }
  }

  for (auto node : flowgraph_->Nodes()->GetList()) {
    auto instr = node->NodeInfo();
    auto def = instr->Def();
    auto out = out_->Look(node);
    bool is_move = typeid(*instr) == typeid(assem::MoveInstr);

    if (is_move) {
      auto use = instr->Use();
      if (def && use) {
        for (auto d : def->GetList()) {
          INode* dn = get_or_create_node(d);
          for (auto u : use->GetList()) {
            INode* un = get_or_create_node(u);
            live_graph_.moves->Append(un, dn); // src to dst
          }
          if (out) {
            for (auto o : out->GetList()) {
              bool is_src = false;
              for (auto u : use->GetList()) {
                if (o == u) { is_src = true; break; }
              }
              if (!is_src && d != o) {
                INode* on = get_or_create_node(o);
                live_graph_.interf_graph->AddEdge(dn, on);
                live_graph_.interf_graph->AddEdge(on, dn);
              }
            }
          }
        }
      }
    } else {
      if (def && out) {
        for (auto d : def->GetList()) {
          INode* dn = get_or_create_node(d);
          for (auto o : out->GetList()) {
            if (d != o) {
              INode* on = get_or_create_node(o);
              live_graph_.interf_graph->AddEdge(dn, on);
              live_graph_.interf_graph->AddEdge(on, dn);
            }
          }
        }
      }
    }
  }
}

void LiveGraphFactory::Liveness() {
  LiveMap();
  InterfGraph();
}

} // namespace live
