#include "tiger/liveness/flowgraph.h"

namespace fg {

void FlowGraphFactory::AssemFlowGraph() {
  FNode *prev = nullptr;
  for (assem::Instr *instr : instr_list_->GetList()) {
    FNode *curr = flowgraph_->NewNode(instr);
    if (prev != nullptr) {
      bool is_uncond_jmp = false;
      if (auto oper = dynamic_cast<assem::OperInstr *>(prev->NodeInfo())) {
        if (oper->assem_.find("jmp") == 0) {
          is_uncond_jmp = true;
        }
      }
      if (!is_uncond_jmp) {
        flowgraph_->AddEdge(prev, curr);
      }
    }
    
    if (auto label_instr = dynamic_cast<assem::LabelInstr *>(instr)) {
      label_map_->Enter(label_instr->label_, curr);
    }
    prev = curr;
  }

  for (FNode *node : flowgraph_->Nodes()->GetList()) {
    if (auto oper = dynamic_cast<assem::OperInstr *>(node->NodeInfo())) {
      if (oper->jumps_ != nullptr) {
        for (temp::Label *label : *(oper->jumps_->labels_)) {
          FNode *target = label_map_->Look(label);
          if (target != nullptr) {
            flowgraph_->AddEdge(node, target);
          }
        }
      }
    }
  }
}

} // namespace fg

namespace assem {

temp::TempList *LabelInstr::Def() const {
  return new temp::TempList();
}

temp::TempList *MoveInstr::Def() const {
  return dst_ ? dst_ : new temp::TempList();
}

temp::TempList *OperInstr::Def() const {
  return dst_ ? dst_ : new temp::TempList();
}

temp::TempList *LabelInstr::Use() const {
  return new temp::TempList();
}

temp::TempList *MoveInstr::Use() const {
  return src_ ? src_ : new temp::TempList();
}

temp::TempList *OperInstr::Use() const {
  return src_ ? src_ : new temp::TempList();
}
} // namespace assem
