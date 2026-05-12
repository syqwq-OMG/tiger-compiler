import os

content = """#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/x64frame.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/frame.h"

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;

namespace {
frame::ProcFrag *ProcEntryExit(tr::Level *level, tr::Exp *body);
}

namespace tr {

Access *Access::AllocLocal(Level *level, bool escape) {
  return new Access(level, level->frame_->AllocLocal(escape));
}

class Cx {
public:
  PatchList trues_;
  PatchList falses_;
  tree::Stm *stm_;

  Cx(PatchList trues, PatchList falses, tree::Stm *stm)
      : trues_(trues), falses_(falses), stm_(stm) {}
};

class Exp {
public:
  [[nodiscard]] virtual tree::Exp *UnEx() const = 0;
  [[nodiscard]] virtual tree::Stm *UnNx() const = 0;
  [[nodiscard]] virtual Cx UnCx(err::ErrorMsg *errormsg) const = 0;
};

class ExpAndTy {
public:
  tr::Exp *exp_;
  type::Ty *ty_;

  ExpAndTy(tr::Exp *exp, type::Ty *ty) : exp_(exp), ty_(ty) {}
};

class ExExp : public Exp {
public:
  tree::Exp *exp_;

  explicit ExExp(tree::Exp *exp) : exp_(exp) {}

  [[nodiscard]] tree::Exp *UnEx() const override { 
    return exp_;
  }
  [[nodiscard]] tree::Stm *UnNx() const override {
    return new tree::ExpStm(exp_);
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    tree::Stm *stm = new tree::CjumpStm(tree::NE_OP, exp_, new tree::ConstExp(0), nullptr, nullptr);
    PatchList trues(std::list<temp::Label **>{&((static_cast<tree::CjumpStm *>(stm))->true_label_)});
    PatchList falses(std::list<temp::Label **>{&((static_cast<tree::CjumpStm *>(stm))->false_label_)});
    return Cx(trues, falses, stm);
  }
};

class NxExp : public Exp {
public:
  tree::Stm *stm_;

  explicit NxExp(tree::Stm *stm) : stm_(stm) {}

  [[nodiscard]] tree::Exp *UnEx() const override {
    return new tree::EseqExp(stm_, new tree::ConstExp(0));
  }
  [[nodiscard]] tree::Stm *UnNx() const override { 
    return stm_;
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    assert(0); // Should never happen
    return Cx(PatchList(), PatchList(), nullptr);
  }
};

class CxExp : public Exp {
public:
  Cx cx_;

  CxExp(PatchList trues, PatchList falses, tree::Stm *stm)
      : cx_(trues, falses, stm) {}
  
  [[nodiscard]] tree::Exp *UnEx() const override {
    temp::Temp *r = temp::TempFactory::NewTemp();
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();
    cx_.trues_.DoPatch(t);
    cx_.falses_.DoPatch(f);
    return new tree::EseqExp(
        new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(1)),
        new tree::EseqExp(
            cx_.stm_,
            new tree::EseqExp(
                new tree::LabelStm(f),
                new tree::EseqExp(
                    new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(0)),
                    new tree::EseqExp(new tree::LabelStm(t), new tree::TempExp(r))))));
  }
  [[nodiscard]] tree::Stm *UnNx() const override {
    temp::Label *label = temp::LabelFactory::NewLabel();
    cx_.trues_.DoPatch(label);
    cx_.falses_.DoPatch(label);
    return new tree::SeqStm(cx_.stm_, new tree::LabelStm(label));
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override { 
    return cx_;
  }
};

void ProgTr::Translate() {
  FillBaseTEnv();
  FillBaseVEnv();
  tr::ExpAndTy *exp_ty = absyn_tree_->Translate(
      venv_.get(), tenv_.get(), main_level_.get(),
      temp::LabelFactory::NamedLabel("tigermain"), errormsg_.get());
  
  if (exp_ty && exp_ty->exp_) {
    frags->PushBack(ProcEntryExit(main_level_.get(), exp_ty->exp_));
  }
}

} // namespace tr

namespace {

frame::ProcFrag *ProcEntryExit(tr::Level *level, tr::Exp *body) {
  tree::Stm *stm = new tree::MoveStm(
      new tree::TempExp(reg_manager->ReturnValue()), body->UnEx());
  stm = frame::ProcEntryExit1(level->frame_, stm);
  return new frame::ProcFrag(stm, level->frame_);
}
} // namespace

namespace absyn {

tr::ExpAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  return root_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  env::EnvEntry *entry = venv->Look(sym_);
  if (entry && typeid(*entry) == typeid(env::VarEntry)) {
    env::VarEntry *var_entry = static_cast<env::VarEntry *>(entry);
    tr::Access *access = var_entry->access_;
    tree::Exp *frame_ptr = new tree::TempExp(reg_manager->FramePointer());
    tr::Level *curr_level = level;
    while (curr_level != access->level_) {
      // Assuming static link is the first formal
      auto link_access = curr_level->frame_->Formals()->front();
      frame_ptr = link_access->access_->ToExp(frame_ptr);
      curr_level = curr_level->parent_;
    }
    return new tr::ExpAndTy(new tr::ExExp(access->access_->ToExp(frame_ptr)), var_entry->ty_->ActualTy());
  }
  errormsg->Error(pos_, "undefined variable %s", sym_->Name().data());
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::IntTy::Instance());
}

tr::ExpAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *exp_ty = var_->Translate(venv, tenv, level, label, errormsg);
  tr::Exp *exp = exp_ty->exp_;
  type::Ty *ty = exp_ty->ty_->ActualTy();

  if (typeid(*ty) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "not a record type");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  }

  auto record_ty = static_cast<type::RecordTy *>(ty);
  type::FieldList *field_list = record_ty->fields_;
  int order = 0;
  for (auto field : field_list->GetList()) {
    if (field->name_ == sym_) {
      tree::Exp *texp = new tree::MemExp(new tree::BinopExp(
          tree::PLUS_OP, exp->UnEx(),
          new tree::ConstExp(order * reg_manager->WordSize())));
      return new tr::ExpAndTy(new tr::ExExp(texp), field->ty_->ActualTy());
    }
    order++;
  }
  errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().data());
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::IntTy::Instance());
}

tr::ExpAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level, temp::Label *label,
                                      err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *var_ty = var_->Translate(venv, tenv, level, label, errormsg);
  type::Ty *actual_var_ty = var_ty->ty_->ActualTy();
  if (typeid(*actual_var_ty) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "array type required");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::IntTy::Instance());
  }
  tr::ExpAndTy *sub_ty = subscript_->Translate(venv, tenv, level, label, errormsg);
  if (typeid(*(sub_ty->ty_->ActualTy())) != typeid(type::IntTy)) {
    errormsg->Error(pos_, "integer required");
  }
  type::ArrayTy *array_ty = static_cast<type::ArrayTy *>(actual_var_ty);
  tree::Exp *addr = new tree::BinopExp(
      tree::PLUS_OP, var_ty->exp_->UnEx(),
      new tree::BinopExp(tree::TIMES_OP, sub_ty->exp_->UnEx(),
                         new tree::ConstExp(reg_manager->WordSize())));
  return new tr::ExpAndTy(new tr::ExExp(new tree::MemExp(addr)), array_ty->ty_->ActualTy());
}

tr::ExpAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  return var_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::NilTy::Instance());
}

tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(val_)), type::IntTy::Instance());
}

tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  temp::Label *str_label = temp::LabelFactory::NewLabel();
  frags->PushBack(new frame::StringFrag(str_label, str_));
  return new tr::ExpAndTy(new tr::ExExp(new tree::NameExp(str_label)), type::StringTy::Instance());
}

tr::ExpAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  env::EnvEntry *entry = venv->Look(func_);
  if (!entry || typeid(*entry) != typeid(env::FunEntry)) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::IntTy::Instance());
  }
  env::FunEntry *fun_entry = static_cast<env::FunEntry *>(entry);
  
  auto formal_it = fun_entry->formals_->GetList().begin();
  auto formal_end = fun_entry->formals_->GetList().end();
  auto arg_it = args_->GetList().begin();
  auto arg_end = args_->GetList().end();
  tree::ExpList *tree_args = new tree::ExpList();
  
  if (fun_entry->level_->parent_) { // If not an external function, pass static link
    tree::Exp *static_link = new tree::TempExp(reg_manager->FramePointer());
    tr::Level *curr_level = level;
    while (curr_level != fun_entry->level_->parent_) {
      auto link_access = curr_level->frame_->Formals()->front();
      static_link = link_access->access_->ToExp(static_link);
      curr_level = curr_level->parent_;
    }
    tree_args->Append(static_link);
  }

  while (formal_it != formal_end && arg_it != arg_end) {
    tr::ExpAndTy *arg_ty = (*arg_it)->Translate(venv, tenv, level, label, errormsg);
    if (!(*formal_it)->IsSameType(arg_ty->ty_)) {
      errormsg->Error((*arg_it)->pos_, "para type mismatch");
    }
    tree_args->Append(arg_ty->exp_->UnEx());
    ++formal_it;
    ++arg_it;
  }
  
  if (formal_it != formal_end) {
    errormsg->Error(pos_, "too few params in function %s", func_->Name().data());
  } else if (arg_it != arg_end) {
    errormsg->Error(pos_, "too many params in function %s", func_->Name().data());
  }

  tree::Exp *call_exp;
  if (fun_entry->level_->parent_) {
    call_exp = new tree::CallExp(new tree::NameExp(fun_entry->label_), tree_args);
  } else {
    // Note: externalCall expects to prepend staticLink magically, but we shouldn't append it manually if it's already done in frame::ExternalCall.
    // Actually frame::ExternalCall does `args->Insert(new tree::NameExp(...))`.
    call_exp = frame::ExternalCall(func_->Name(), tree_args);
  }
  
  return new tr::ExpAndTy(new tr::ExExp(call_exp), fun_entry->result_->ActualTy());
}

tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *left_ty = left_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *right_ty = right_->Translate(venv, tenv, level, label, errormsg);

  tree::Exp *left_exp = left_ty->exp_->UnEx();
  tree::Exp *right_exp = right_ty->exp_->UnEx();

  if (oper_ == PLUS_OP || oper_ == MINUS_OP || oper_ == TIMES_OP || oper_ == DIVIDE_OP) {
    tree::BinOp op;
    if (oper_ == PLUS_OP) op = tree::PLUS_OP;
    else if (oper_ == MINUS_OP) op = tree::MINUS_OP;
    else if (oper_ == TIMES_OP) op = tree::TIMES_OP;
    else op = tree::DIVIDE_OP;
    return new tr::ExpAndTy(new tr::ExExp(new tree::BinopExp(op, left_exp, right_exp)), type::IntTy::Instance());
  } else if (oper_ == EQ_OP || oper_ == NEQ_OP || oper_ == LT_OP || oper_ == LE_OP || oper_ == GT_OP || oper_ == GE_OP) {
    tree::CjumpStm *stm;
    if (typeid(*(left_ty->ty_->ActualTy())) == typeid(type::StringTy)) {
      tree::Exp *cmp_exp = frame::ExternalCall("string_equal", new tree::ExpList({left_exp, right_exp}));
      if (oper_ == EQ_OP) {
        stm = new tree::CjumpStm(tree::EQ_OP, cmp_exp, new tree::ConstExp(1), nullptr, nullptr);
      } else {
        stm = new tree::CjumpStm(tree::NE_OP, cmp_exp, new tree::ConstExp(1), nullptr, nullptr);
      }
    } else {
      tree::RelOp op;
      if (oper_ == EQ_OP) op = tree::EQ_OP;
      else if (oper_ == NEQ_OP) op = tree::NEQ_OP;
      else if (oper_ == LT_OP) op = tree::LT_OP;
      else if (oper_ == LE_OP) op = tree::LE_OP;
      else if (oper_ == GT_OP) op = tree::GT_OP;
      else op = tree::GE_OP;
      stm = new tree::CjumpStm(op, left_exp, right_exp, nullptr, nullptr);
    }
    tr::PatchList trues(std::list<temp::Label **>{&(stm->true_label_)});
    tr::PatchList falses(std::list<temp::Label **>{&(stm->false_label_)});
    return new tr::ExpAndTy(new tr::CxExp(trues, falses, stm), type::IntTy::Instance());
  }
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::IntTy::Instance());
}

tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,      
                                   err::ErrorMsg *errormsg) const {
  type::Ty *ty = tenv->Look(typ_);
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::IntTy::Instance());
  }
  type::Ty *actual = ty->ActualTy();
  if (typeid(*actual) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "not a record type");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::IntTy::Instance());
  }

  temp::Temp *r = temp::TempFactory::NewTemp();
  int num_fields = fields_->GetList().size();
  tree::Stm *alloc_stm = new tree::MoveStm(
      new tree::TempExp(r),
      frame::ExternalCall("alloc_record", new tree::ExpList({new tree::ConstExp(num_fields * reg_manager->WordSize())})));

  tree::Stm *seq_stm = alloc_stm;
  int i = 0;
  for (EField *efield : fields_->GetList()) {
    tr::ExpAndTy *f_ty = efield->exp_->Translate(venv, tenv, level, label, errormsg);
    tree::Stm *move_stm = new tree::MoveStm(
        new tree::MemExp(new tree::BinopExp(tree::PLUS_OP, new tree::TempExp(r), new tree::ConstExp(i * reg_manager->WordSize()))),
        f_ty->exp_->UnEx());
    seq_stm = new tree::SeqStm(seq_stm, move_stm);
    i++;
  }
  
  return new tr::ExpAndTy(new tr::ExExp(new tree::EseqExp(seq_stm, new tree::TempExp(r))), ty);
}

tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  if (seq_->GetList().empty()) {
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }
  tree::Stm *stm = nullptr;
  tr::ExpAndTy *last_exp_ty = nullptr;
  for (Exp *exp : seq_->GetList()) {
    last_exp_ty = exp->Translate(venv, tenv, level, label, errormsg);
    if (exp == seq_->GetList().back()) break;
    if (!stm) stm = last_exp_ty->exp_->UnNx();
    else stm = new tree::SeqStm(stm, last_exp_ty->exp_->UnNx());
  }
  
  if (!stm) {
    return new tr::ExpAndTy(new tr::ExExp(last_exp_ty->exp_->UnEx()), last_exp_ty->ty_);
  } else {
    return new tr::ExpAndTy(new tr::ExExp(new tree::EseqExp(stm, last_exp_ty->exp_->UnEx())), last_exp_ty->ty_);
  }
}

tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,                       
                                   err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *var_ty = var_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *exp_ty = exp_->Translate(venv, tenv, level, label, errormsg);
  
  if (typeid(*var_) == typeid(SimpleVar)) {
    SimpleVar *simple = static_cast<SimpleVar *>(var_);
    env::EnvEntry *entry = venv->Look(simple->sym_);
    if (entry && typeid(*entry) == typeid(env::VarEntry)) {
      if (static_cast<env::VarEntry *>(entry)->readonly_) {
        errormsg->Error(pos_, "loop variable can't be assigned");
      }
    }
  }

  if (!var_ty->ty_->IsSameType(exp_ty->ty_)) {
    errormsg->Error(pos_, "unmatched assign exp");
  }

  return new tr::ExpAndTy(new tr::NxExp(new tree::MoveStm(var_ty->exp_->UnEx(), exp_ty->exp_->UnEx())), type::VoidTy::Instance());
}

tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *test_ty = test_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *then_ty = then_->Translate(venv, tenv, level, label, errormsg);
  
  temp::Label *t = temp::LabelFactory::NewLabel();
  temp::Label *f = temp::LabelFactory::NewLabel();
  temp::Label *join = temp::LabelFactory::NewLabel();
  
  tr::Cx cx = test_ty->exp_->UnCx(errormsg);
  cx.trues_.DoPatch(t);
  cx.falses_.DoPatch(f);
  
  if (elsee_) {
    tr::ExpAndTy *else_ty = elsee_->Translate(venv, tenv, level, label, errormsg);
    if (!then_ty->ty_->IsSameType(else_ty->ty_)) {
      errormsg->Error(pos_, "then exp and else exp type mismatch");
    }
    temp::Temp *r = temp::TempFactory::NewTemp();
    tree::Stm *stm = new tree::SeqStm(
        cx.stm_,
        new tree::SeqStm(
            new tree::LabelStm(t),
            new tree::SeqStm(
                new tree::MoveStm(new tree::TempExp(r), then_ty->exp_->UnEx()),
                new tree::SeqStm(
                    new tree::JumpStm(new tree::NameExp(join), new std::vector<temp::Label *>{join}),
                    new tree::SeqStm(
                        new tree::LabelStm(f),
                        new tree::SeqStm(
                            new tree::MoveStm(new tree::TempExp(r), else_ty->exp_->UnEx()),
                            new tree::LabelStm(join)))))));
    return new tr::ExpAndTy(new tr::ExExp(new tree::EseqExp(stm, new tree::TempExp(r))), then_ty->ty_);
  } else {
    tree::Stm *stm = new tree::SeqStm(
        cx.stm_,
        new tree::SeqStm(
            new tree::LabelStm(t),
            new tree::SeqStm(
                then_ty->exp_->UnNx(),
                new tree::LabelStm(f))));
    return new tr::ExpAndTy(new tr::NxExp(stm), type::VoidTy::Instance());
  }
}

tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,            
                                  err::ErrorMsg *errormsg) const {
  temp::Label *test_lbl = temp::LabelFactory::NewLabel();
  temp::Label *body_lbl = temp::LabelFactory::NewLabel();
  temp::Label *done_lbl = temp::LabelFactory::NewLabel();
  
  tr::ExpAndTy *test_ty = test_->Translate(venv, tenv, level, label, errormsg);
  tr::Cx cx = test_ty->exp_->UnCx(errormsg);
  cx.trues_.DoPatch(body_lbl);
  cx.falses_.DoPatch(done_lbl);
  
  tr::ExpAndTy *body_ty = body_->Translate(venv, tenv, level, done_lbl, errormsg);
  
  tree::Stm *stm = new tree::SeqStm(
      new tree::LabelStm(test_lbl),
      new tree::SeqStm(
          cx.stm_,
          new tree::SeqStm(
              new tree::LabelStm(body_lbl),
              new tree::SeqStm(
                  body_ty->exp_->UnNx(),
                  new tree::SeqStm(
                      new tree::JumpStm(new tree::NameExp(test_lbl), new std::vector<temp::Label *>{test_lbl}),
                      new tree::LabelStm(done_lbl))))));
                      
  return new tr::ExpAndTy(new tr::NxExp(stm), type::VoidTy::Instance());
}

tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  venv->BeginScope();
  
  tr::ExpAndTy *lo_ty = lo_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *hi_ty = hi_->Translate(venv, tenv, level, label, errormsg);
  
  tr::Access *access = tr::Access::AllocLocal(level, escape_);
  venv->Enter(var_, new env::VarEntry(access, type::IntTy::Instance(), true));
  
  temp::Temp *limit = temp::TempFactory::NewTemp();
  
  temp::Label *body_lbl = temp::LabelFactory::NewLabel();
  temp::Label *inc_lbl = temp::LabelFactory::NewLabel();
  temp::Label *done_lbl = temp::LabelFactory::NewLabel();
  
  tr::ExpAndTy *body_ty = body_->Translate(venv, tenv, level, done_lbl, errormsg);
  
  tree::Exp *var_exp = access->access_->ToExp(new tree::TempExp(reg_manager->FramePointer()));
  
  tree::Stm *init_stm = new tree::SeqStm(
      new tree::MoveStm(var_exp, lo_ty->exp_->UnEx()),
      new tree::MoveStm(new tree::TempExp(limit), hi_ty->exp_->UnEx()));
      
  tree::Stm *cmp_stm = new tree::CjumpStm(tree::LE_OP, var_exp, new tree::TempExp(limit), body_lbl, done_lbl);
  
  tree::Stm *inc_stm = new tree::SeqStm(
      new tree::LabelStm(inc_lbl),
      new tree::SeqStm(
          new tree::CjumpStm(tree::LT_OP, var_exp, new tree::TempExp(limit), temp::LabelFactory::NewLabel(), done_lbl),
          // We need a proper inner jump for increment, but standard way is simpler: just increment and jump to cmp
          new tree::MoveStm(var_exp, new tree::BinopExp(tree::PLUS_OP, var_exp, new tree::ConstExp(1)))));
          
  temp::Label *true_inc = temp::LabelFactory::NewLabel();
  
  tree::Stm *for_stm = new tree::SeqStm(
      init_stm,
      new tree::SeqStm(
          cmp_stm,
          new tree::SeqStm(
              new tree::LabelStm(body_lbl),
              new tree::SeqStm(
                  body_ty->exp_->UnNx(),
                  new tree::SeqStm(
                      new tree::CjumpStm(tree::LT_OP, var_exp, new tree::TempExp(limit), true_inc, done_lbl),
                      new tree::SeqStm(
                          new tree::LabelStm(true_inc),
                          new tree::SeqStm(
                              new tree::MoveStm(var_exp, new tree::BinopExp(tree::PLUS_OP, var_exp, new tree::ConstExp(1))),
                              new tree::SeqStm(
                                  new tree::JumpStm(new tree::NameExp(body_lbl), new std::vector<temp::Label *>{body_lbl}),
                                  new tree::LabelStm(done_lbl)))))))));
                                  
  venv->EndScope();
  return new tr::ExpAndTy(new tr::NxExp(for_stm), type::VoidTy::Instance());
}

tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(new tr::NxExp(new tree::JumpStm(new tree::NameExp(label), new std::vector<temp::Label *>{label})), type::VoidTy::Instance());
}

tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  venv->BeginScope();
  tenv->BeginScope();
  tree::Stm *stm = nullptr;
  for (Dec *dec : decs_->GetList()) {
    tr::Exp *dec_exp = dec->Translate(venv, tenv, level, label, errormsg);
    if (dec_exp) {
      if (!stm) stm = dec_exp->UnNx();
      else stm = new tree::SeqStm(stm, dec_exp->UnNx());
    }
  }
  tr::ExpAndTy *result;
  if (body_) {
    result = body_->Translate(venv, tenv, level, label, errormsg);
    if (stm) {
      result->exp_ = new tr::ExExp(new tree::EseqExp(stm, result->exp_->UnEx()));
    }
  } else {
    result = new tr::ExpAndTy(new tr::NxExp(stm ? stm : new tree::ExpStm(new tree::ConstExp(0))), type::VoidTy::Instance());
  }
  tenv->EndScope();
  venv->EndScope();
  return result;
}

tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,                    
                                  err::ErrorMsg *errormsg) const {
  type::Ty *ty = tenv->Look(typ_);
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::IntTy::Instance());
  }
  type::Ty *actual = ty->ActualTy();
  if (typeid(*actual) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "array type required");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::IntTy::Instance());
  }
  tr::ExpAndTy *size_ty = size_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *init_ty = init_->Translate(venv, tenv, level, label, errormsg);
  
  tree::Exp *call_exp = frame::ExternalCall("init_array", new tree::ExpList({size_ty->exp_->UnEx(), init_ty->exp_->UnEx()}));
  return new tr::ExpAndTy(new tr::ExExp(call_exp), ty);
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(new tr::NxExp(new tree::ExpStm(new tree::ConstExp(0))), type::VoidTy::Instance());
}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  // Pass 1: add all signatures
  for (FunDec *func : functions_->GetList()) {
    type::TyList *formals = func->params_->MakeFormalTyList(tenv, errormsg);
    type::Ty *result_ty = type::VoidTy::Instance();
    if (func->result_) {
      type::Ty *ty = tenv->Look(func->result_);
      if (ty) result_ty = ty;
    }
    
    std::list<bool> escapes;
    for (Field *field : func->params_->GetList()) {
      escapes.push_back(field->escape_);
    }
    
    temp::Label *fun_label = temp::LabelFactory::NamedLabel(func->name_->Name());
    tr::Level *new_level = tr::Level::NewLevel(level, fun_label, escapes);
    venv->Enter(func->name_, new env::FunEntry(new_level, fun_label, formals, result_ty));
  }
  // Pass 2: translate bodies
  for (FunDec *func : functions_->GetList()) {
    venv->BeginScope();
    env::FunEntry *entry = static_cast<env::FunEntry *>(venv->Look(func->name_));
    auto formal_it = entry->formals_->GetList().begin();
    
    // First formal in Level is the static link, so we skip it to get parameters
    auto access_it = entry->level_->frame_->Formals()->begin();
    ++access_it; // Skip static link
    
    for (Field *field : func->params_->GetList()) {
      venv->Enter(field->name_, new env::VarEntry(new tr::Access(entry->level_, *access_it), *formal_it));
      ++formal_it;
      ++access_it;
    }
    
    tr::ExpAndTy *body_ty = func->body_->Translate(venv, tenv, entry->level_, entry->label_, errormsg);
    if (!entry->result_->IsSameType(body_ty->ty_)) {
      if (typeid(*(entry->result_->ActualTy())) == typeid(type::VoidTy)) {
        errormsg->Error(func->body_->pos_, "procedure returns value");
      } else {
        errormsg->Error(func->body_->pos_, "type mismatch"); 
      }
    }
    
    frags->PushBack(ProcEntryExit(entry->level_, body_ty->exp_));
    venv->EndScope();
  }
  return new tr::NxExp(new tree::ExpStm(new tree::ConstExp(0)));
}

tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *init_exp_ty = init_->Translate(venv, tenv, level, label, errormsg);
  type::Ty *init_ty = init_exp_ty->ty_;

  if (typ_) {
    type::Ty *ty = tenv->Look(typ_);
    if (!ty) {
      errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    } else if (!ty->IsSameType(init_ty)) {
      errormsg->Error(pos_, "type mismatch");
    }
    init_ty = ty;
  } else {
    auto actual_init_ty = init_ty->ActualTy();
    if (typeid(*actual_init_ty) == typeid(type::NilTy)) {
      errormsg->Error(pos_, "init should not be nil without type specified");
    }
  }

  tr::Access *access = tr::Access::AllocLocal(level, escape_);
  venv->Enter(var_, new env::VarEntry(access, init_ty));

  return new tr::NxExp(
      new tree::MoveStm(access->access_->ToExp(new tree::TempExp(
                            reg_manager->FramePointer())),
                        init_exp_ty->exp_->UnEx()));
}

tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  // Pass 1: Enter all names
  for (NameAndTy *nat : types_->GetList()) {
    tenv->Enter(nat->name_, new type::NameTy(nat->name_, nullptr));
  }
  
  // Pass 2: Resolve ty_
  for (NameAndTy *nat : types_->GetList()) {
    type::Ty *ty = tenv->Look(nat->name_);
    type::NameTy *name_ty = static_cast<type::NameTy *>(ty);
    name_ty->ty_ = nat->ty_->Translate(tenv, errormsg);
  }

  // Pass 3: Check cycles
  for (NameAndTy *nat : types_->GetList()) {
    type::Ty *ty = tenv->Look(nat->name_);
    type::Ty *curr = ty;
    while (typeid(*curr) == typeid(type::NameTy)) {
      type::NameTy *name_curr = static_cast<type::NameTy *>(curr);
      curr = name_curr->ty_;
      if (curr == ty) {
        errormsg->Error(pos_, "illegal type cycle");
        name_curr->ty_ = type::IntTy::Instance(); 
        break;
      }
    }
  }
  return new tr::NxExp(new tree::ExpStm(new tree::ConstExp(0)));
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  type::Ty *ty = tenv->Look(name_);
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", name_->Name().data());
    return type::IntTy::Instance();
  }
  return ty;
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  type::FieldList *ty_fields = new type::FieldList();
  for (Field *field : record_->GetList()) {
    type::Ty *ty = tenv->Look(field->typ_);
    if (!ty) {
      errormsg->Error(field->pos_, "undefined type %s", field->typ_->Name().data());
      ty = type::IntTy::Instance();
    }
    ty_fields->Append(new type::Field(field->name_, ty));
  }
  return new type::RecordTy(ty_fields);
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  type::Ty *ty = tenv->Look(array_);
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", array_->Name().data());
    return type::IntTy::Instance();
  }
  return new type::ArrayTy(ty);
}

} // namespace absyn
"""

with open("/home/syqwq/Workspace/tiger-compiler/src/tiger/translate/translate.cc", "w") as f:
    f.write(content)
