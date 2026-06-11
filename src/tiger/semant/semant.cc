#include "tiger/absyn/absyn.h"
#include "tiger/semant/semant.h"
#include <set>

namespace absyn {

void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  root_->SemAnalyze(venv, tenv, 0, errormsg);
}

type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  env::EnvEntry *entry = venv->Look(sym_);
  if (entry && typeid(*entry) == typeid(env::VarEntry)) {
    return static_cast<env::VarEntry *>(entry)->ty_->ActualTy();
  }
  errormsg->Error(pos_, "undefined variable %s", sym_->Name().data());
  return type::IntTy::Instance();
}

type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *var_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*var_ty) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "not a record type");
    return type::IntTy::Instance();
  }
  type::RecordTy *record_ty = static_cast<type::RecordTy *>(var_ty);
  for (type::Field *field : record_ty->fields_->GetList()) {
    if (field->name_ == sym_) {
      return field->ty_->ActualTy();
    }
  }
  errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().data());
  return type::IntTy::Instance();
}

type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   int labelcount,
                                   err::ErrorMsg *errormsg) const {
  type::Ty *var_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*var_ty) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "array type required");
    return type::IntTy::Instance();
  }
  type::Ty *sub_ty = subscript_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*sub_ty) != typeid(type::IntTy)) {
    errormsg->Error(pos_, "integer required");
  }
  type::ArrayTy *array_ty = static_cast<type::ArrayTy *>(var_ty);
  return array_ty->ty_->ActualTy();
}

type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  return var_->SemAnalyze(venv, tenv, labelcount, errormsg);
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  return type::NilTy::Instance();
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  return type::IntTy::Instance();
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  return type::StringTy::Instance();
}

type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  env::EnvEntry *entry = venv->Look(func_);
  if (!entry || typeid(*entry) != typeid(env::FunEntry)) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().data());
    return type::IntTy::Instance();
  }
  env::FunEntry *fun_entry = static_cast<env::FunEntry *>(entry);
  int formal_size = fun_entry->formals_->GetList().size();
  int arg_size = args_->GetList().size();
  if (formal_size > arg_size) {
    errormsg->Error(pos_, "too few params in function %s", func_->Name().data());
  } else if (formal_size < arg_size) {
    errormsg->Error(pos_, "too many params in function %s", func_->Name().data());
  }

  auto formal_it = fun_entry->formals_->GetList().begin();
  auto formal_end = fun_entry->formals_->GetList().end();
  auto arg_it = args_->GetList().begin();
  auto arg_end = args_->GetList().end();

  while (formal_it != formal_end && arg_it != arg_end) {
    type::Ty *arg_ty = (*arg_it)->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!(*formal_it)->IsSameType(arg_ty)) {
      errormsg->Error((*arg_it)->pos_, "para type mismatch");
    }
    ++formal_it;
    ++arg_it;
  }
  
  while (arg_it != arg_end) {
    (*arg_it)->SemAnalyze(venv, tenv, labelcount, errormsg);
    ++arg_it;
  }
  return fun_entry->result_->ActualTy();
}

type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *left_ty = left_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *right_ty = right_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

  if (oper_ == PLUS_OP || oper_ == MINUS_OP || oper_ == TIMES_OP || oper_ == DIVIDE_OP) {
    if (typeid(*left_ty) != typeid(type::IntTy)) {
      errormsg->Error(left_->pos_, "integer required");
    }
    if (typeid(*right_ty) != typeid(type::IntTy)) {
      errormsg->Error(right_->pos_, "integer required");
    }
    return type::IntTy::Instance();
  } else if (oper_ == EQ_OP || oper_ == NEQ_OP) {
    if (!left_ty->IsSameType(right_ty)) {
      errormsg->Error(pos_, "same type required");
    }
    return type::IntTy::Instance();
  } else {
    // LT_OP, LE_OP, GT_OP, GE_OP
    if (!left_ty->IsSameType(right_ty)) {
      errormsg->Error(pos_, "same type required");
    } else if (typeid(*left_ty) != typeid(type::IntTy) && typeid(*left_ty) != typeid(type::StringTy)) {
      errormsg->Error(pos_, "same type required");
    }
    return type::IntTy::Instance();
  }
}

type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *ty = tenv->Look(typ_);
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return type::IntTy::Instance();
  }
  type::Ty *actual = ty->ActualTy();
  if (typeid(*actual) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "not a record type");
    return type::IntTy::Instance();
  }
  
  type::RecordTy *rec_ty = static_cast<type::RecordTy *>(actual);
  auto expected_fields = rec_ty->fields_->GetList();
  auto actual_fields = fields_->GetList();
  auto exp_it = expected_fields.begin();
  auto act_it = actual_fields.begin();
  
  while (exp_it != expected_fields.end() && act_it != actual_fields.end()) {
    if ((*exp_it)->name_ != (*act_it)->name_) {
       errormsg->Error((*act_it)->exp_->pos_, "type mismatch"); 
    }
    type::Ty *act_ty = (*act_it)->exp_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!(*exp_it)->ty_->IsSameType(act_ty)) {
       errormsg->Error((*act_it)->exp_->pos_, "type mismatch");
    }
    ++exp_it; ++act_it;
  }
  return ty;
}

type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *last_ty = type::VoidTy::Instance();
  for (Exp *exp : seq_->GetList()) {
    last_ty = exp->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  return last_ty;
}

type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *test_ty = test_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*test_ty) != typeid(type::IntTy)) {
    errormsg->Error(test_->pos_, "integer required");
  }
  type::Ty *then_ty = then_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (elsee_) {
    type::Ty *else_ty = elsee_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!then_ty->IsSameType(else_ty)) {
      errormsg->Error(pos_, "then exp and else exp type mismatch");
    }
    return then_ty;
  } else {
    if (typeid(*(then_ty->ActualTy())) != typeid(type::VoidTy)) {
      errormsg->Error(pos_, "if-then exp's body must produce no value");
    }
    return type::VoidTy::Instance();
  }
}

type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *test_ty = test_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*test_ty) != typeid(type::IntTy)) {
    errormsg->Error(test_->pos_, "integer required");
  }
  type::Ty *body_ty = body_->SemAnalyze(venv, tenv, 1, errormsg)->ActualTy(); 
  if (typeid(*body_ty) != typeid(type::VoidTy)) {
    errormsg->Error(body_->pos_, "while body must produce no value");
  }
  return type::VoidTy::Instance();
}

type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *lo_ty = lo_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *hi_ty = hi_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*lo_ty) != typeid(type::IntTy) || typeid(*hi_ty) != typeid(type::IntTy)) {
    errormsg->Error(lo_->pos_, "for exp's range type is not integer");
  }
  venv->BeginScope();
  venv->Enter(var_, new env::VarEntry(type::IntTy::Instance(), true)); 
  type::Ty *body_ty = body_->SemAnalyze(venv, tenv, 1, errormsg)->ActualTy();
  if (typeid(*body_ty) != typeid(type::VoidTy)) {
    // Intentionally omitted error message unless required
  }
  venv->EndScope();
  return type::VoidTy::Instance();
}

type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  return type::VoidTy::Instance();
}

type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  venv->BeginScope();
  tenv->BeginScope();
  for (Dec *dec : decs_->GetList()) {
    dec->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  type::Ty *result = type::VoidTy::Instance();
  if (body_) {
    result = body_->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  tenv->EndScope();
  venv->EndScope();
  return result;
}

type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *ty = tenv->Look(typ_);
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return type::IntTy::Instance();
  }
  type::Ty *actual = ty->ActualTy();
  if (typeid(*actual) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "array type required");
    return type::IntTy::Instance();
  }
  type::Ty *size_ty = size_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*size_ty) != typeid(type::IntTy)) {
    errormsg->Error(size_->pos_, "integer required");
  }
  type::Ty *init_ty = init_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::ArrayTy *arr_ty = static_cast<type::ArrayTy *>(actual);
  if (!arr_ty->ty_->IsSameType(init_ty)) {
    errormsg->Error(init_->pos_, "type mismatch");
  }
  return ty; 
}

type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  return type::VoidTy::Instance();
}

type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *var_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*var_) == typeid(SimpleVar)) {
    SimpleVar *simple = static_cast<SimpleVar *>(var_);
    env::EnvEntry *entry = venv->Look(simple->sym_);
    if (entry && typeid(*entry) == typeid(env::VarEntry)) {
      if (static_cast<env::VarEntry *>(entry)->readonly_) {
        errormsg->Error(pos_, "loop variable can't be assigned");
      }
    }
  }
  type::Ty *exp_ty = exp_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!var_ty->IsSameType(exp_ty)) {
    errormsg->Error(pos_, "unmatched assign exp");
  }
  return type::VoidTy::Instance();
}

void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  std::set<std::string> names;
  // Pass 1: add all signatures
  for (FunDec *func : functions_->GetList()) {
    if (names.count(func->name_->Name())) {
      errormsg->Error(func->pos_, "two functions have the same name");
    }
    names.insert(func->name_->Name());
    
    type::TyList *formals = func->params_->MakeFormalTyList(tenv, errormsg);
    type::Ty *result_ty = type::VoidTy::Instance();
    if (func->result_) {
      type::Ty *ty = tenv->Look(func->result_);
      if (ty) result_ty = ty;
    }
    venv->Enter(func->name_, new env::FunEntry(formals, result_ty));
  }
  // Pass 2: check bodies
  for (FunDec *func : functions_->GetList()) {
    venv->BeginScope();
    env::FunEntry *entry = static_cast<env::FunEntry *>(venv->Look(func->name_));
    auto formal_it = entry->formals_->GetList().begin();
    for (Field *field : func->params_->GetList()) {
      venv->Enter(field->name_, new env::VarEntry(*formal_it));
      ++formal_it;
    }
    type::Ty *body_ty = func->body_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!entry->result_->IsSameType(body_ty)) {
      if (typeid(*(entry->result_->ActualTy())) == typeid(type::VoidTy)) {
        errormsg->Error(func->body_->pos_, "procedure returns value");
      } else {
        errormsg->Error(func->body_->pos_, "type mismatch"); 
      }
    }
    venv->EndScope();
  }
}

void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                        err::ErrorMsg *errormsg) const {
  type::Ty *init_ty = init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typ_) {
    type::Ty *declared_ty = tenv->Look(typ_);
    if (!declared_ty) {
       errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
       venv->Enter(var_, new env::VarEntry(type::IntTy::Instance()));
       return;
    } else {
       if (!declared_ty->IsSameType(init_ty)) {
         errormsg->Error(init_->pos_, "type mismatch");
       }
       venv->Enter(var_, new env::VarEntry(declared_ty));
       return;
    }
  } else {
    if (typeid(*(init_ty->ActualTy())) == typeid(type::NilTy)) {
      errormsg->Error(pos_, "init should not be nil without type specified");
      venv->Enter(var_, new env::VarEntry(type::IntTy::Instance()));
      return;
    }
  }
  venv->Enter(var_, new env::VarEntry(init_ty));
}

void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                         err::ErrorMsg *errormsg) const {
  std::set<std::string> names;
  // Pass 1: Enter all names into tenv as NameTy with nullptr ty_
  for (NameAndTy *nat : types_->GetList()) {
    if (names.count(nat->name_->Name())) {
      errormsg->Error(pos_, "two types have the same name");
    }
    names.insert(nat->name_->Name());
    tenv->Enter(nat->name_, new type::NameTy(nat->name_, nullptr));
  }
  
  // Pass 2: Resolve ty_ for NameTy
  for (NameAndTy *nat : types_->GetList()) {
    type::Ty *ty = tenv->Look(nat->name_);
    type::NameTy *name_ty = static_cast<type::NameTy *>(ty);
    name_ty->ty_ = nat->ty_->SemAnalyze(tenv, errormsg);
  }

  // Pass 3: Check for illegal type cycles
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
}

type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  type::Ty *ty = tenv->Look(name_);
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", name_->Name().data());
    return type::IntTy::Instance();
  }
  return ty;
}

type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
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

type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  type::Ty *ty = tenv->Look(array_);
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", array_->Name().data());
    return type::IntTy::Instance();
  }
  return new type::ArrayTy(ty);
}

} // namespace absyn

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}

} // namespace sem
