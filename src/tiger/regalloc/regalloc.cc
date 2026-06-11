#include "tiger/regalloc/regalloc.h"
#include "tiger/output/logger.h"
#include <map>

extern frame::RegManager *reg_manager;

namespace ra {

Result::~Result() {
  // delete coloring_;
  // delete il_;
}

RegAllocator::RegAllocator(frame::Frame *frame, std::unique_ptr<cg::AssemInstr> assem_instr)
    : frame_(frame), assem_instr_(std::move(assem_instr)), result_(std::make_unique<ra::Result>()) {}

void RegAllocator::RegAlloc() {
  bool done = false;
  auto il = assem_instr_->GetInstrList();

  while (!done) {
    fg::FlowGraphFactory fg_factory(il);
    fg_factory.AssemFlowGraph();
    auto flowgraph = fg_factory.GetFlowGraph();

    live::LiveGraphFactory lg_factory(flowgraph);
    lg_factory.Liveness();
    auto live_graph = lg_factory.GetLiveGraph();

    col::Color color(live_graph, reg_manager->temp_map_);
    col::Result col_res = color.Allocate();

    if (col_res.spills != nullptr && !col_res.spills->GetList().empty()) {
      auto new_il = new assem::InstrList();
      std::map<temp::Temp*, int> spills_map;
      
      for (auto node : col_res.spills->GetList()) {
        auto t = node->NodeInfo();
        frame_->AllocLocal(true);
        spills_map[t] = frame_->offset_;
      }

      for (auto instr : il->GetList()) {
        auto use = instr->Use();
        auto def = instr->Def();
        std::map<temp::Temp*, temp::Temp*> replace_map;

        auto get_or_create = [&](temp::Temp* t) {
          if (replace_map.find(t) == replace_map.end()) {
            replace_map[t] = temp::TempFactory::NewTemp();
          }
          return replace_map[t];
        };

        if (use) {
          for (auto &u : const_cast<std::list<temp::Temp*>&>(use->GetList())) {
            if (spills_map.find(u) != spills_map.end()) {
              auto new_t = get_or_create(u);
              int offset = spills_map[u];
              std::string fs = frame_->GetLabel() + "_framesize";
              std::string assem = "movq " + fs + std::to_string(offset) + "(`s0), `d0";
              new_il->Append(new assem::OperInstr(assem, new temp::TempList(new_t), new temp::TempList(reg_manager->StackPointer()), nullptr));
              u = new_t;
            }
          }
        }

        new_il->Append(instr);

        if (def) {
          for (auto &d : const_cast<std::list<temp::Temp*>&>(def->GetList())) {
            if (spills_map.find(d) != spills_map.end()) {
              auto new_t = get_or_create(d);
              int offset = spills_map[d];
              std::string fs = frame_->GetLabel() + "_framesize";
              std::string assem = "movq `s0, " + fs + std::to_string(offset) + "(`s1)";
              new_il->Append(new assem::OperInstr(assem, new temp::TempList(), new temp::TempList({new_t, reg_manager->StackPointer()}), nullptr));
              d = new_t;
            }
          }
        }
      }
      il = new_il;
    } else {
      done = true;
      result_->coloring_ = col_res.coloring;
      auto final_il = new assem::InstrList();
      for (auto instr : il->GetList()) {
        if (typeid(*instr) == typeid(assem::MoveInstr)) {
          auto move = static_cast<assem::MoveInstr*>(instr);
          auto dst = move->dst_->GetList().front();
          auto src = move->src_->GetList().front();
          auto c_dst = col_res.coloring->Look(dst);
          auto c_src = col_res.coloring->Look(src);
          if (c_dst == nullptr || c_src == nullptr || *c_dst != *c_src) {
            final_il->Append(instr);
          }
        } else {
          final_il->Append(instr);
        }
      }
      result_->il_ = final_il;
    }
  }
}

} // namespace ra