#include "tiger/frame/x64frame.h"
#include "tiger/codegen/assem.h"

extern frame::RegManager *reg_manager;

namespace frame {

X64RegManager::X64RegManager() : RegManager() {
  for (int i = 0; i < REG_COUNT; i++)
    regs_.push_back(temp::TempFactory::NewTemp());

  // Note: no frame pointer in tiger compiler
  std::array<std::string_view, REG_COUNT> reg_name{
      "%rax", "%rbx", "%rcx", "%rdx", "%rsi", "%rdi", "%rbp", "%rsp",
      "%r8",  "%r9",  "%r10", "%r11", "%r12", "%r13", "%r14", "%r15"};
  int reg = RAX;
  for (auto &name : reg_name) {
    temp_map_->Enter(regs_[reg], new std::string(name));
    reg++;
  }
}

temp::TempList *X64RegManager::Registers() {
  const std::array reg_array{
      RAX, RBX, RCX, RDX, RSI, RDI, RBP, R8, R9, R10, R11, R12, R13, R14, R15,
  };
  auto *temp_list = new temp::TempList();
  for (auto &reg : reg_array)
    temp_list->Append(regs_[reg]);
  return temp_list;
}

temp::TempList *X64RegManager::ArgRegs() {
  const std::array reg_array{RDI, RSI, RDX, RCX, R8, R9};
  auto *temp_list = new temp::TempList();
  ;
  for (auto &reg : reg_array)
    temp_list->Append(regs_[reg]);
  return temp_list;
}

temp::TempList *X64RegManager::CallerSaves() {
  std::array reg_array{RAX, RDI, RSI, RDX, RCX, R8, R9, R10, R11};
  auto *temp_list = new temp::TempList();
  ;
  for (auto &reg : reg_array)
    temp_list->Append(regs_[reg]);
  return temp_list;
}

temp::TempList *X64RegManager::CalleeSaves() {
  std::array reg_array{RBP, RBX, R12, R13, R14, R15};
  auto *temp_list = new temp::TempList();
  ;
  for (auto &reg : reg_array)
    temp_list->Append(regs_[reg]);
  return temp_list;
}

temp::TempList *X64RegManager::ReturnSink() {
  temp::TempList *temp_list = CalleeSaves();
  temp_list->Append(regs_[SP]);
  temp_list->Append(regs_[RV]);
  return temp_list;
}

int X64RegManager::WordSize() { return 8; }

temp::Temp *X64RegManager::FramePointer() { return regs_[FP]; }

temp::Temp *X64RegManager::StackPointer() { return regs_[SP]; }

temp::Temp *X64RegManager::ReturnValue() { return regs_[RV]; }

class InFrameAccess : public Access {
public:
  int offset;

  explicit InFrameAccess(int offset) : offset(offset) {}
  /* TODO: Put your lab5 code here */
  tree::Exp *ToExp(tree::Exp *frame_ptr) const override {
    return new tree::MemExp(new tree::BinopExp(tree::PLUS_OP, frame_ptr, new tree::ConstExp(offset)));
  }
  /* End for lab5 code */
};


class InRegAccess : public Access {
public:
  temp::Temp *reg;

  explicit InRegAccess(temp::Temp *reg) : reg(reg) {}
  /* TODO: Put your lab5 code here */
  tree::Exp *ToExp(tree::Exp *framePtr) const override {
    return new tree::TempExp(reg);
  }
  /* End for lab5 code */
};

class X64Frame : public Frame {
  /* TODO: Put your lab5 code here */
public:
  tree::Stm *view_shift;
  int outgo_size_;

  X64Frame(temp::Label *name, std::list<frame::Access *> *formals)
      : Frame(8, 0, name, formals), view_shift(nullptr), outgo_size_(0) {}

  [[nodiscard]] std::string GetLabel() const override { return name_->Name(); }
  [[nodiscard]] temp::Label *Name() const override { return name_; }
  [[nodiscard]] std::list<frame::Access *> *Formals() const override {
    return formals_;
  }
  frame::Access *AllocLocal(bool escape) override {
    /* TODO: Put your lab5 code here */
    frame::Access *access;
    if (escape) {
      offset_ -= reg_manager->WordSize();
      access = new InFrameAccess(offset_);
    } else {
      access = new InRegAccess(temp::TempFactory::NewTemp());
    }
    return access;
  }
  void AllocOutgoSpace(int size) override {
    if (size > outgo_size_) {
      outgo_size_ = size;
    }
  }
};

frame::Frame *NewFrame(temp::Label *name, std::list<bool> formals) {
  /* TODO: Put your lab5 code here */
  auto access_list = new std::list<frame::Access *>();
  X64Frame *frame = new X64Frame(name, access_list);
  
  tree::Stm *view_shift = nullptr;
  temp::TempList *arg_regs = reg_manager->ArgRegs();
  auto reg_it = arg_regs->GetList().begin();
  int reg_count = 0;
  int formal_offset = 0; // arguments starting offset relative to fp

  for (bool escape : formals) {
    frame::Access *access = frame->AllocLocal(escape);
    access_list->push_back(access);

    tree::Exp *dst = access->ToExp(new tree::TempExp(reg_manager->FramePointer()));
    tree::Exp *src;

    if (reg_count < 6) {
      src = new tree::TempExp(*reg_it);
      ++reg_it;
      ++reg_count;
    } else {
      // Starting from 7th parameter (index 6), it is passed on the stack.
      // offset is 8 for 7th argument, 16 for 8th, etc.
      src = new tree::MemExp(new tree::BinopExp(
          tree::PLUS_OP, new tree::TempExp(reg_manager->FramePointer()),
          new tree::ConstExp(formal_offset + 8)));
      formal_offset += reg_manager->WordSize();
    }

    if (view_shift) {
      view_shift = new tree::SeqStm(view_shift, new tree::MoveStm(dst, src));
    } else {
      view_shift = new tree::MoveStm(dst, src);
    }
  }

  frame->view_shift = view_shift;
  return frame;
}

tree::Exp *ExternalCall(std::string_view s, tree::ExpList *args) {
  // Prepend a magic exp at first arg, indicating do not pass static link on
  // stack
  args->Insert(new tree::NameExp(temp::LabelFactory::NamedLabel("staticLink")));
  return new tree::CallExp(new tree::NameExp(temp::LabelFactory::NamedLabel(s)),
                           args);
}

/**
 * Moving incoming formal parameters, the saving and restoring of callee-save
 * Registers
 * @param frame curruent frame
 * @param stm statements
 * @return statements with saving, restoring and view shift
 */
tree::Stm *ProcEntryExit1(frame::Frame *frame, tree::Stm *stm) {
  auto x64_frame = dynamic_cast<frame::X64Frame *>(frame);
  assert(x64_frame);

  auto callee_list = new tree::ExpList();

  // Save callee-saved register
  tree::Stm *save_stm = nullptr;
  temp::TempList *callees = reg_manager->CalleeSaves();
  for (auto callee : callees->GetList()) {
    temp::Temp *r = temp::TempFactory::NewTemp();
    if (!save_stm)
      save_stm =
          new tree::MoveStm(new tree::TempExp(r), new tree::TempExp(callee));
    else
      save_stm = new tree::SeqStm(
          save_stm,
          new tree::MoveStm(new tree::TempExp(r), new tree::TempExp(callee)));
    callee_list->Append(new tree::TempExp(r));
  }

  // Restore callee-saved register
  tree::Stm *restore_stm = nullptr;
  callees = reg_manager->CalleeSaves();
  auto callee_it = callee_list->GetList().begin();
  for (auto callee : callees->GetList()) {
    assert(callee_it != callee_list->GetList().end());
    if (!restore_stm)
      restore_stm = new tree::MoveStm(new tree::TempExp(callee), *callee_it++);
    else
      restore_stm = new tree::SeqStm(
          restore_stm,
          new tree::MoveStm(new tree::TempExp(callee), *callee_it++));
  }

  // Add view shift for arguments
  tree::Stm *exit_stm;
  if (x64_frame->view_shift == nullptr) {
    // Outermost frame and functions with no formals do not have formal access_
    // list and view shift
    exit_stm = new tree::SeqStm(save_stm, new tree::SeqStm(stm, restore_stm));
  } else
    exit_stm = new tree::SeqStm(
        save_stm, new tree::SeqStm(x64_frame->view_shift,
                                   new tree::SeqStm(stm, restore_stm)));
  return exit_stm;
}

assem::InstrList *ProcEntryExit2(assem::InstrList *body) {
  body->Append(new assem::OperInstr(
      "", new temp::TempList(), reg_manager->ReturnSink(), nullptr));
  return body;
}

assem::Proc *ProcEntryExit3(frame::Frame *frame, assem::InstrList *body) {
  char buf[256];
  std::string prolog;
  std::string epilog;
  std::string fn_name = frame->name_->Name();
  
  // Calculate framesize including outgo space
  int frame_size = -frame->offset_;
  auto x64_frame = dynamic_cast<X64Frame *>(frame);
  if (x64_frame) {
    frame_size += x64_frame->outgo_size_;
  }
  
  // Align stack pointer to 16 bytes for x64 ABI calling convention
  // Upon entry, return address is pushed (8 bytes), so we need frame_size % 16 == 8
  frame_size = ((frame_size + 7) / 16) * 16 + 8;

  prolog.append(fn_name + ":\n");
  sprintf(buf, ".set %s_framesize, %d\n", fn_name.data(), frame_size);
  prolog.append(buf);
  sprintf(buf, "subq $%d, %%rsp\n", frame_size);
  prolog.append(buf);

  sprintf(buf, "addq $%d, %%rsp\n", frame_size);
  epilog.append(buf);
  epilog.append("retq\n");

  return new assem::Proc(prolog, body, epilog);
}
/* End for lab5 code */

} // namespace frame