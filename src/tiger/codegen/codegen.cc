#include "tiger/codegen/codegen.h"

#include <cassert>
#include <sstream>

extern frame::RegManager *reg_manager;

namespace cg {

static frame::Frame *current_frame = nullptr;

void CodeGen::Codegen() {
  current_frame = frame_;
  assem_instr_ = std::make_unique<AssemInstr>(new assem::InstrList());
  fs_ = frame_->name_->Name() + "_framesize";

  for (auto stm : traces_->GetStmList()->GetList()) {
    stm->Munch(*(assem_instr_->GetInstrList()), fs_);
  }
  
  frame::ProcEntryExit2(assem_instr_->GetInstrList());
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList())
    instr->Print(out, map);
  fprintf(out, "\n");
}
} // namespace cg

namespace tree {

temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list, std::string_view fs) {
  temp::TempList *arg_list = new temp::TempList();
  temp::TempList *arg_regs = reg_manager->ArgRegs();
  auto arg_regs_it = arg_regs->GetList().begin();
  int reg_idx = 0;
  
  if (cg::current_frame) {
    int args_count = exp_list_.size();
    if (args_count > 6) {
      cg::current_frame->AllocOutgoSpace((args_count - 6) * reg_manager->WordSize());
    }
  }

  for (auto arg : exp_list_) {
    if (typeid(*arg) == typeid(NameExp)) {
      NameExp *name_exp = static_cast<NameExp*>(arg);
      if (name_exp->name_->Name() == "staticLink") {
        continue;
      }
    }
    
    temp::Temp *arg_temp = arg->Munch(instr_list, fs);
    if (reg_idx < 6) {
      instr_list.Append(new assem::MoveInstr(
          "movq `s0, `d0", new temp::TempList(*arg_regs_it), new temp::TempList(arg_temp)));
      arg_list->Append(*arg_regs_it);
      arg_regs_it++;
    } else {
      int offset = (reg_idx - 6) * reg_manager->WordSize();
      instr_list.Append(new assem::OperInstr(
          "movq `s0, " + std::to_string(offset) + "(`s1)",
          new temp::TempList(), new temp::TempList({arg_temp, reg_manager->StackPointer()}), nullptr));
    }
    reg_idx++;
  }
  return arg_list;
}

void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  assert(0); // SeqStm should not exist in codegen phase
}

void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  instr_list.Append(new assem::LabelInstr(label_->Name(), label_));
}

void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  instr_list.Append(new assem::OperInstr(
      "jmp `j0", new temp::TempList(), new temp::TempList(), new assem::Targets(jumps_)));
}

void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *left_temp = left_->Munch(instr_list, fs);
  temp::Temp *right_temp = right_->Munch(instr_list, fs);
  instr_list.Append(new assem::OperInstr(
      "cmpq `s1, `s0", new temp::TempList(), new temp::TempList({left_temp, right_temp}), nullptr));

  std::string jmp_instr;
  switch (op_) {
    case EQ_OP: jmp_instr = "je"; break;
    case NE_OP: jmp_instr = "jne"; break;
    case LT_OP: jmp_instr = "jl"; break;
    case GT_OP: jmp_instr = "jg"; break;
    case LE_OP: jmp_instr = "jle"; break;
    case GE_OP: jmp_instr = "jge"; break;
    case ULT_OP: jmp_instr = "jb"; break;
    case ULE_OP: jmp_instr = "jbe"; break;
    case UGT_OP: jmp_instr = "ja"; break;
    case UGE_OP: jmp_instr = "jae"; break;
    default: assert(0);
  }
  
  std::vector<temp::Label *> *targets = new std::vector<temp::Label *>();
  targets->push_back(true_label_);
  targets->push_back(false_label_);
  
  instr_list.Append(new assem::OperInstr(
      jmp_instr + " `j0", new temp::TempList(), new temp::TempList(), new assem::Targets(targets)));
}

void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  if (typeid(*dst_) == typeid(MemExp)) {
    MemExp *mem_dst = static_cast<MemExp*>(dst_);
    
    // MEM(BINOP(PLUS, e1, CONST(c))) <- e2
    if (typeid(*(mem_dst->exp_)) == typeid(BinopExp)) {
      BinopExp *binop = static_cast<BinopExp*>(mem_dst->exp_);
      if (binop->op_ == PLUS_OP && typeid(*(binop->right_)) == typeid(ConstExp)) {
        ConstExp *c = static_cast<ConstExp*>(binop->right_);
        temp::Temp *e1 = binop->left_->Munch(instr_list, fs);
        temp::Temp *src = src_->Munch(instr_list, fs);
        instr_list.Append(new assem::OperInstr(
            "movq `s0, " + std::to_string(c->consti_) + "(`s1)",
            new temp::TempList(), new temp::TempList({src, e1}), nullptr));
        return;
      }
    }
    
    // MEM(e1) <- e2
    temp::Temp *e1 = mem_dst->exp_->Munch(instr_list, fs);
    temp::Temp *src = src_->Munch(instr_list, fs);
    instr_list.Append(new assem::OperInstr(
        "movq `s0, (`s1)",
        new temp::TempList(), new temp::TempList({src, e1}), nullptr));
    return;
  }
  
  if (typeid(*dst_) == typeid(TempExp)) {
    TempExp *temp_dst = static_cast<TempExp*>(dst_);
    temp::Temp *src = src_->Munch(instr_list, fs);
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0",
        new temp::TempList(temp_dst->temp_), new temp::TempList(src)));
    return;
  }
  
  assert(0);
}

void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  exp_->Munch(instr_list, fs);
}

temp::Temp *BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *left_temp = left_->Munch(instr_list, fs);
  temp::Temp *right_temp = right_->Munch(instr_list, fs);
  temp::Temp *res = temp::TempFactory::NewTemp();

  if (op_ == PLUS_OP || op_ == MINUS_OP) {
    std::string op_str = (op_ == PLUS_OP) ? "addq" : "subq";
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList(res), new temp::TempList(left_temp)));
    instr_list.Append(new assem::OperInstr(
        op_str + " `s0, `d0",
        new temp::TempList(res), new temp::TempList({right_temp, res}), nullptr));
    return res;
  }
  
  if (op_ == MUL_OP || op_ == DIV_OP) {
    temp::Temp *rax = reg_manager->GetRegister(frame::X64RegManager::RAX);
    temp::Temp *rdx = reg_manager->GetRegister(frame::X64RegManager::RDX);
    
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList(rax), new temp::TempList(left_temp)));
        
    if (op_ == DIV_OP) {
      instr_list.Append(new assem::OperInstr(
          "cqto", new temp::TempList({rax, rdx}), new temp::TempList(rax), nullptr));
      instr_list.Append(new assem::OperInstr(
          "idivq `s0", new temp::TempList({rax, rdx}), new temp::TempList({right_temp, rax, rdx}), nullptr));
    } else {
      instr_list.Append(new assem::OperInstr(
          "imulq `s0", new temp::TempList({rax, rdx}), new temp::TempList({right_temp, rax}), nullptr));
    }
    
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList(res), new temp::TempList(rax)));
    return res;
  }
  if (op_ == AND_OP || op_ == OR_OP || op_ == XOR_OP) {
    std::string op_str;
    if (op_ == AND_OP) op_str = "andq";
    else if (op_ == OR_OP) op_str = "orq";
    else op_str = "xorq";
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList(res), new temp::TempList(left_temp)));
    instr_list.Append(new assem::OperInstr(
        op_str + " `s0, `d0",
        new temp::TempList(res), new temp::TempList({right_temp, res}), nullptr));
    return res;
  }
  
  assert(0);
  return res;
}

temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  if (typeid(*exp_) == typeid(BinopExp)) {
    BinopExp *binop = static_cast<BinopExp*>(exp_);
    if (binop->op_ == PLUS_OP && typeid(*(binop->right_)) == typeid(ConstExp)) {
      ConstExp *c = static_cast<ConstExp*>(binop->right_);
      temp::Temp *e1 = binop->left_->Munch(instr_list, fs);
      temp::Temp *r = temp::TempFactory::NewTemp();
      instr_list.Append(new assem::OperInstr(
          "movq " + std::to_string(c->consti_) + "(`s0), `d0",
          new temp::TempList(r), new temp::TempList(e1), nullptr));
      return r;
    }
  }

  temp::Temp *e1 = exp_->Munch(instr_list, fs);
  temp::Temp *r = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::OperInstr(
      "movq (`s0), `d0",
      new temp::TempList(r), new temp::TempList(e1), nullptr));
  return r;
}

temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  if (temp_ == reg_manager->FramePointer()) {
    temp::Temp *r = temp::TempFactory::NewTemp();
    instr_list.Append(new assem::OperInstr(
        "leaq " + std::string(fs) + "(`s0), `d0",
        new temp::TempList(r), new temp::TempList(reg_manager->StackPointer()), nullptr));
    return r;
  }
  return temp_;
}

temp::Temp *EseqExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  assert(0); // Should not exist
  return nullptr;
}

temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *r = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::OperInstr(
      "leaq " + name_->Name() + "(%rip), `d0",
      new temp::TempList(r), new temp::TempList(), nullptr));
  return r;
}

temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *r = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::OperInstr(
      "movq $" + std::to_string(consti_) + ", `d0",
      new temp::TempList(r), new temp::TempList(), nullptr));
  return r;
}

temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::TempList *args = args_->MunchArgs(instr_list, fs);
  
  if (typeid(*fun_) == typeid(NameExp)) {
    NameExp *name = static_cast<NameExp*>(fun_);
    instr_list.Append(new assem::OperInstr(
        "callq " + name->name_->Name(),
        reg_manager->CallerSaves(), args, nullptr));
  } else {
    temp::Temp *fun_temp = fun_->Munch(instr_list, fs);
    temp::TempList *src_list = new temp::TempList(fun_temp);
    for (auto arg : args->GetList()) src_list->Append(arg);
    
    instr_list.Append(new assem::OperInstr(
        "callq *`s0",
        reg_manager->CallerSaves(), src_list, nullptr));
  }
      
  temp::Temp *res = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::MoveInstr(
      "movq `s0, `d0",
      new temp::TempList(res), new temp::TempList(reg_manager->ReturnValue())));
  return res;
}

} // namespace tree
