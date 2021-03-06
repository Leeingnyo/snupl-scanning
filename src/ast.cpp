//------------------------------------------------------------------------------
/// @brief SnuPL abstract syntax tree
/// @author Bernhard Egger <bernhard@csap.snu.ac.kr>
/// @section changelog Change Log
/// 2012/09/14 Bernhard Egger created
/// 2013/05/22 Bernhard Egger reimplemented TAC generation
/// 2013/11/04 Bernhard Egger added typechecks for unary '+' operators
/// 2016/03/12 Bernhard Egger adapted to SnuPL/1
/// 2014/04/08 Bernhard Egger assignment 2: AST for SnuPL/-1
///
/// @section license_section License
/// Copyright (c) 2012-2016 Bernhard Egger
/// All rights reserved.
///
/// Redistribution and use in source and binary forms,  with or without modifi-
/// cation, are permitted provided that the following conditions are met:
///
/// - Redistributions of source code must retain the above copyright notice,
///   this list of conditions and the following disclaimer.
/// - Redistributions in binary form must reproduce the above copyright notice,
///   this list of conditions and the following disclaimer in the documentation
///   and/or other materials provided with the distribution.
///
/// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
/// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,  BUT NOT LIMITED TO,  THE
/// IMPLIED WARRANTIES OF MERCHANTABILITY  AND FITNESS FOR A PARTICULAR PURPOSE
/// ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER  OR CONTRIBUTORS BE
/// LIABLE FOR ANY DIRECT,  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSE-
/// QUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF  SUBSTITUTE
/// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
/// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT
/// LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE)  ARISING IN ANY WAY
/// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
/// DAMAGE.
//------------------------------------------------------------------------------

#include <iostream>
#include <cassert>
#include <cstring>

#include <typeinfo>

#include "ast.h"
using namespace std;


//------------------------------------------------------------------------------
// CAstNode
//
int CAstNode::_global_id = 0;

CAstNode::CAstNode(CToken token)
  : _token(token), _addr(NULL)
{
  _id = _global_id++;
}

CAstNode::~CAstNode(void)
{
  if (_addr != NULL) delete _addr;
}

int CAstNode::GetID(void) const
{
  return _id;
}

CToken CAstNode::GetToken(void) const
{
  return _token;
}

const CType* CAstNode::GetType(void) const
{
  return CTypeManager::Get()->GetNull();
}

string CAstNode::dotID(void) const
{
  ostringstream out;
  out << "node" << dec << _id;
  return out.str();
}

string CAstNode::dotAttr(void) const
{
  return " [label=\"" + dotID() + "\"]";
}

void CAstNode::toDot(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << dotID() << dotAttr() << ";" << endl;
}

CTacAddr* CAstNode::GetTacAddr(void) const
{
  return _addr;
}

ostream& operator<<(ostream &out, const CAstNode &t)
{
  return t.print(out);
}

ostream& operator<<(ostream &out, const CAstNode *t)
{
  return t->print(out);
}

//------------------------------------------------------------------------------
// CAstScope
//
CAstScope::CAstScope(CToken t, const string name, CAstScope *parent)
  : CAstNode(t), _name(name), _symtab(NULL), _parent(parent), _statseq(NULL),
    _cb(NULL)
{
  if (_parent != NULL) _parent->AddChild(this);
}

CAstScope::~CAstScope(void)
{
  delete _symtab;
  delete _statseq;
  delete _cb;
}

const string CAstScope::GetName(void) const
{
  return _name;
}

CAstScope* CAstScope::GetParent(void) const
{
  return _parent;
}

size_t CAstScope::GetNumChildren(void) const
{
  return _children.size();
}

CAstScope* CAstScope::GetChild(size_t i) const
{
  assert(i < _children.size());
  return _children[i];
}

CSymtab* CAstScope::GetSymbolTable(void) const
{
  assert(_symtab != NULL);
  return _symtab;
}

void CAstScope::SetStatementSequence(CAstStatement *statseq)
{
  _statseq = statseq;
}

CAstStatement* CAstScope::GetStatementSequence(void) const
{
  return _statseq;
}

bool CAstScope::TypeCheck(CToken *t, string *msg) const
{
  bool result = true;
  try {
    CAstStatement *s = _statseq;
    // check for all statements in the statement sequence
    while (result && (s != NULL)) {
      result = s->TypeCheck(t, msg);
      s = s->GetNext();
    }
    // check for all scopes in the children
    vector<CAstScope*>::const_iterator it = _children.begin();
    while (result && (it != _children.end())) {
      result = (*it)->TypeCheck(t, msg);
      it++;
    }
  } catch (...) {
    result = false;
  }
  return result;
}

ostream& CAstScope::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << "CAstScope: '" << _name << "'" << endl;
  out << ind << "  symbol table:" << endl;
  _symtab->print(out, indent+4);
  out << ind << "  statement list:" << endl;
  CAstStatement *s = GetStatementSequence();
  if (s != NULL) {
    do {
      s->print(out, indent+4);
      s = s->GetNext();
    } while (s != NULL);
  } else {
    out << ind << "    empty." << endl;
  }

  out << ind << "  nested scopes:" << endl;
  if (_children.size() > 0) {
    for (size_t i=0; i<_children.size(); i++) {
      _children[i]->print(out, indent+4);
    }
  } else {
    out << ind << "    empty." << endl;
  }
  out << ind << endl;

  return out;
}

void CAstScope::toDot(ostream &out, int indent) const
{
  string ind(indent, ' ');

  CAstNode::toDot(out, indent);

  CAstStatement *s = GetStatementSequence();
  if (s != NULL) {
    string prev = dotID();
    do {
      s->toDot(out, indent);
      out << ind << prev << " -> " << s->dotID() << " [style=dotted];" << endl;
      prev = s->dotID();
      s = s->GetNext();
    } while (s != NULL);
  }

  vector<CAstScope*>::const_iterator it = _children.begin();
  while (it != _children.end()) {
    CAstScope *s = *it++;
    s->toDot(out, indent);
    out << ind << dotID() << " -> " << s->dotID() << ";" << endl;
  }

}

CTacAddr* CAstScope::ToTac(CCodeBlock *cb)
{
  assert(cb != NULL);
  // add statement three address code until there is no statement
  CAstStatement *s = GetStatementSequence();
  while (s != NULL) {
    CTacLabel *next = cb->CreateLabel();
    s->ToTac(cb, next, NULL);
    cb->AddInstr(next);
    s = s->GetNext();
  }

  // clean up control flow
  cb->CleanupControlFlow();
  return NULL;
}

CCodeBlock* CAstScope::GetCodeBlock(void) const
{
  return _cb;
}

void CAstScope::SetSymbolTable(CSymtab *st)
{
  if (_symtab != NULL) delete _symtab;
  _symtab = st;
}

void CAstScope::AddChild(CAstScope *child)
{
  _children.push_back(child);
}


//------------------------------------------------------------------------------
// CAstModule
//
CAstModule::CAstModule(CToken t, const string name)
  : CAstScope(t, name, NULL)
{
  SetSymbolTable(new CSymtab());
}

CSymbol* CAstModule::CreateVar(const string ident, const CType *type)
{
  return new CSymGlobal(ident, type);
}

string CAstModule::dotAttr(void) const
{
  return " [label=\"m " + GetName() + "\",shape=box]";
}



//------------------------------------------------------------------------------
// CAstProcedure
//
CAstProcedure::CAstProcedure(CToken t, const string name,
                             CAstScope *parent, CSymProc *symbol)
  : CAstScope(t, name, parent), _symbol(symbol)
{
  assert(GetParent() != NULL);
  SetSymbolTable(new CSymtab(GetParent()->GetSymbolTable()));
  assert(_symbol != NULL);
}

CSymProc* CAstProcedure::GetSymbol(void) const
{
  return _symbol;
}

CSymbol* CAstProcedure::CreateVar(const string ident, const CType *type)
{
  return new CSymLocal(ident, type);
}

const CType* CAstProcedure::GetType(void) const
{
  return GetSymbol()->GetDataType();
}

string CAstProcedure::dotAttr(void) const
{
  return " [label=\"p/f " + GetName() + "\",shape=box]";
}


//------------------------------------------------------------------------------
// CAstType
//
CAstType::CAstType(CToken t, const CType *type)
  : CAstNode(t), _type(type)
{
  assert(type != NULL);
}

const CType* CAstType::GetType(void) const
{
  return _type;
}

ostream& CAstType::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << "CAstType (" << _type << ")" << endl;
  return out;
}


//------------------------------------------------------------------------------
// CAstStatement
//
CAstStatement::CAstStatement(CToken token)
  : CAstNode(token), _next(NULL)
{
}

CAstStatement::~CAstStatement(void)
{
  delete _next;
}

void CAstStatement::SetNext(CAstStatement *next)
{
  _next = next;
}

CAstStatement* CAstStatement::GetNext(void) const
{
  return _next;
}

CTacAddr* CAstStatement::ToTac(CCodeBlock *cb, CTacLabel *next, CTacLabel* end)
{
  // nothing done
  cb->AddInstr(new CTacInstr(opGoto, next));
  return NULL;
}


//------------------------------------------------------------------------------
// CAstStatAssign
//
CAstStatAssign::CAstStatAssign(CToken t,
                               CAstDesignator *lhs, CAstExpression *rhs)
  : CAstStatement(t), _lhs(lhs), _rhs(rhs)
{
  assert(lhs != NULL);
  assert(rhs != NULL);
}

CAstDesignator* CAstStatAssign::GetLHS(void) const
{
  return _lhs;
}

CAstExpression* CAstStatAssign::GetRHS(void) const
{
  return _rhs;
}

bool CAstStatAssign::TypeCheck(CToken *t, string *msg) const
{
  bool chk = _lhs->TypeCheck(t, msg) && _rhs->TypeCheck(t, msg);
  if (!chk) return false;

  CTypeManager *tm = CTypeManager::Get();
  const CType* lhsType = _lhs->GetType();

  // Do not allow array type assignment
  if (!lhsType->IsScalar()) {
    if (t != NULL) *t = _rhs->GetToken();
    if (msg != NULL) *msg = "left handside designator must be scalar type";
    return false;
  }
  // check for same type of lhs and rhs
  if (!_rhs->GetType()->Match(lhsType)) {
    if (t != NULL) *t = _rhs->GetToken();
    if (msg != NULL) *msg = "right handside expression must be same type as left handside designator";
    return false;
  }
  return true;
}

const CType* CAstStatAssign::GetType(void) const
{
  return _lhs->GetType();
}

ostream& CAstStatAssign::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << ":=" << " ";

  const CType *t = GetType();
  if (t != NULL) out << t; else out << "<INVALID>";

  out << endl;

  _lhs->print(out, indent+2);
  _rhs->print(out, indent+2);

  return out;
}

string CAstStatAssign::dotAttr(void) const
{
  return " [label=\":=\",shape=box]";
}

void CAstStatAssign::toDot(ostream &out, int indent) const
{
  string ind(indent, ' ');

  CAstNode::toDot(out, indent);

  _lhs->toDot(out, indent);
  out << ind << dotID() << "->" << _lhs->dotID() << ";" << endl;
  _rhs->toDot(out, indent);
  out << ind << dotID() << "->" << _rhs->dotID() << ";" << endl;
}

CTacAddr* CAstStatAssign::ToTac(CCodeBlock *cb, CTacLabel *next, CTacLabel* end)
{
  // add three address code of left hand side, right hand side
  //     assignment instruction
  CTacAddr* dest = _lhs->ToTac(cb);
  CTacAddr* src = _rhs->ToTac(cb);
  cb->AddInstr(new CTacInstr(opAssign, dest, src));
  cb->AddInstr(new CTacInstr(opGoto, next));
  return NULL;
}


//------------------------------------------------------------------------------
// CAstStatCall
//
CAstStatCall::CAstStatCall(CToken t, CAstFunctionCall *call)
  : CAstStatement(t), _call(call)
{
  assert(call != NULL);
}

CAstFunctionCall* CAstStatCall::GetCall(void) const
{
  return _call;
}

bool CAstStatCall::TypeCheck(CToken *t, string *msg) const
{
  return GetCall()->TypeCheck(t, msg);
}

ostream& CAstStatCall::print(ostream &out, int indent) const
{
  _call->print(out, indent);

  return out;
}

string CAstStatCall::dotID(void) const
{
  return _call->dotID();
}

string CAstStatCall::dotAttr(void) const
{
  return _call->dotAttr();
}

void CAstStatCall::toDot(ostream &out, int indent) const
{
  _call->toDot(out, indent);
}

CTacAddr* CAstStatCall::ToTac(CCodeBlock *cb, CTacLabel *next, CTacLabel* end)
{
  // add three address code of call
  GetCall()->ToTac(cb);
  cb->AddInstr(new CTacInstr(opGoto, next));
  return NULL;
}


//------------------------------------------------------------------------------
// CAstStatReturn
//
CAstStatReturn::CAstStatReturn(CToken t, CAstScope *scope, CAstExpression *expr)
  : CAstStatement(t), _scope(scope), _expr(expr)
{
  assert(scope != NULL);
}

CAstScope* CAstStatReturn::GetScope(void) const
{
  return _scope;
}

CAstExpression* CAstStatReturn::GetExpression(void) const
{
  return _expr;
}

bool CAstStatReturn::TypeCheck(CToken *t, string *msg) const
{
  const CType *st = GetScope()->GetType();
  CAstExpression *e = GetExpression();
  if (st->Match(CTypeManager::Get()->GetNull())) {
    if (e != NULL) {
      if (t != NULL) *t = e->GetToken();
      if (msg != NULL) *msg = "superfluous expression after return.";
      return false;
    }
  } else {
    if (e == NULL) {
      if (t != NULL) *t = GetToken();
      if (msg != NULL) *msg = "expression expected after return.";
      return false;
    }
    if (!e->TypeCheck(t, msg)) return false;
    if (!st->Match(e->GetType())) {
      if (t != NULL) *t = e->GetToken();
      if (msg != NULL) *msg = "return type mismatch.";
      return false;
    }
  }
  return true;
}

const CType* CAstStatReturn::GetType(void) const
{
  const CType *t = NULL;

  if (GetExpression() != NULL) {
    t = GetExpression()->GetType();
  } else {
    t = CTypeManager::Get()->GetNull();
  }

  return t;
}

ostream& CAstStatReturn::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << "return" << " ";

  const CType *t = GetType();
  if (t != NULL) out << t; else out << "<INVALID>";

  out << endl;

  if (_expr != NULL) _expr->print(out, indent+2);

  return out;
}

string CAstStatReturn::dotAttr(void) const
{
  return " [label=\"return\",shape=box]";
}

void CAstStatReturn::toDot(ostream &out, int indent) const
{
  string ind(indent, ' ');

  CAstNode::toDot(out, indent);

  if (_expr != NULL) {
    _expr->toDot(out, indent);
    out << ind << dotID() << "->" << _expr->dotID() << ";" << endl;
  }
}

CTacAddr* CAstStatReturn::ToTac(CCodeBlock *cb, CTacLabel *next, CTacLabel* end)
{
  // check if expression exists
  // add retrun instruction
  if (_expr != NULL) {
    CTacAddr* src1 = _expr->ToTac(cb);
    cb->AddInstr(new CTacInstr(opReturn, NULL, src1));
  } else {
    cb->AddInstr(new CTacInstr(opReturn, NULL));
  }
  cb->AddInstr(new CTacInstr(opGoto, next));
  return NULL;
}


//------------------------------------------------------------------------------
// CAstStatIf
//
CAstStatIf::CAstStatIf(CToken t, CAstExpression *cond,
                       CAstStatement *ifBody, CAstStatement *elseBody)
  : CAstStatement(t), _cond(cond), _ifBody(ifBody), _elseBody(elseBody)
{
  assert(cond != NULL);
}

CAstExpression* CAstStatIf::GetCondition(void) const
{
  return _cond;
}

CAstStatement* CAstStatIf::GetIfBody(void) const
{
  return _ifBody;
}

CAstStatement* CAstStatIf::GetElseBody(void) const
{
  return _elseBody;
}

bool CAstStatIf::TypeCheck(CToken *t, string *msg) const
{
  // check recursively
  bool chk = _cond->TypeCheck(t, msg);
  CAstStatement *n = _ifBody;
  while (chk && n != NULL){
    chk = n->TypeCheck(t, msg);
    n = n->GetNext();
  }
  n = _elseBody;
  if (chk && n != NULL){
    chk = n->TypeCheck(t, msg);
    n = n->GetNext();
  }
  if (!chk) return false;
  CTypeManager *tm = CTypeManager::Get();
  if (!_cond->GetType()->Match(tm->GetBool())) { // condition must be boolean
    if (t != NULL) *t = _cond->GetToken();
    if (msg != NULL) *msg = "expected boolean type condition";
    return false;
  }
  return true;
}

ostream& CAstStatIf::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << "if cond" << endl;
  _cond->print(out, indent+2);
  out << ind << "if-body" << endl;
  if (_ifBody != NULL) {
    CAstStatement *s = _ifBody;
    do {
      s->print(out, indent+2);
      s = s->GetNext();
    } while (s != NULL);
  } else out << ind << "  empty." << endl;
  out << ind << "else-body" << endl;
  if (_elseBody != NULL) {
    CAstStatement *s = _elseBody;
    do {
      s->print(out, indent+2);
      s = s->GetNext();
    } while (s != NULL);
  } else out << ind << "  empty." << endl;

  return out;
}

string CAstStatIf::dotAttr(void) const
{
  return " [label=\"if\",shape=box]";
}

void CAstStatIf::toDot(ostream &out, int indent) const
{
  string ind(indent, ' ');

  CAstNode::toDot(out, indent);

  _cond->toDot(out, indent);
  out << ind << dotID() << "->" << _cond->dotID() << ";" << endl;

  if (_ifBody != NULL) {
    CAstStatement *s = _ifBody;
    if (s != NULL) {
      string prev = dotID();
      do {
        s->toDot(out, indent);
        out << ind << prev << " -> " << s->dotID() << " [style=dotted];"
            << endl;
        prev = s->dotID();
        s = s->GetNext();
      } while (s != NULL);
    }
  }

  if (_elseBody != NULL) {
    CAstStatement *s = _elseBody;
    if (s != NULL) {
      string prev = dotID();
      do {
        s->toDot(out, indent);
        out << ind << prev << " -> " << s->dotID() << " [style=dotted];"
            << endl;
        prev = s->dotID();
        s = s->GetNext();
      } while (s != NULL);
    }
  }
}

CTacAddr* CAstStatIf::ToTac(CCodeBlock *cb, CTacLabel *next, CTacLabel* end)
{
  // prepare labels
  CTacLabel* ifLabel = cb->CreateLabel();
  CTacLabel* elseLabel = cb->CreateLabel();
  CTacLabel* endLabel = cb->CreateLabel();
  CAstStatement *ifStats = GetIfBody();
  CAstStatement *elseStats = GetElseBody();

  // add three address code of condition with true, false labels
  _cond->ToTac(cb, ifLabel, elseLabel);
  cb->AddInstr(ifLabel);
  // add three address code of statements until
  while (ifStats != NULL) {
    CTacLabel *next = cb->CreateLabel();
    ifStats->ToTac(cb, next, end);
    cb->AddInstr(next);
    ifStats = ifStats->GetNext();
  }
  // skip else label after adding if statements, jump to end label
  cb->AddInstr(new CTacInstr(opGoto, endLabel));
  cb->AddInstr(elseLabel);
  // add three address code of statements until
  while (elseStats != NULL) {
    CTacLabel *next = cb->CreateLabel();
    elseStats->ToTac(cb, next, end);
    cb->AddInstr(next);
    elseStats = elseStats->GetNext();
  }
  cb->AddInstr(endLabel);
  cb->AddInstr(new CTacInstr(opGoto, next));
  return NULL;
}

//------------------------------------------------------------------------------
// CAstStatBreak
//
CAstStatBreak::CAstStatBreak(CToken t)
  : CAstStatement(t)
{
}

bool CAstStatBreak::TypeCheck(CToken *t, string *msg) const
{
  return true; // doesn't need any type check
}


ostream& CAstStatBreak::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << "break" << endl;

  return out;
}

string CAstStatBreak::dotAttr(void) const
{
  ostringstream out;
  out << " [label=\"" << "break" << "\",shape=ellipse]";
  return out.str();
}

CTacAddr* CAstStatBreak::ToTac(CCodeBlock *cb, CTacLabel *next, CTacLabel* end)
{
  assert(end != NULL);
  // go to the end of the loop
  cb->AddInstr(new CTacInstr(opGoto, end));
  return NULL;
}


//------------------------------------------------------------------------------
// CAstStatWhile
//
CAstStatWhile::CAstStatWhile(CToken t,
                             CAstExpression *cond, CAstStatement *body)
  : CAstStatement(t), _cond(cond), _body(body)
{
  assert(cond != NULL);
}

CAstExpression* CAstStatWhile::GetCondition(void) const
{
  return _cond;
}

CAstStatement* CAstStatWhile::GetBody(void) const
{
  return _body;
}

bool CAstStatWhile::TypeCheck(CToken *t, string *msg) const
{
  // check recursively
  bool chk = _cond->TypeCheck(t, msg);
  CAstStatement *n = _body;
  while (chk && n != NULL){
    chk = n->TypeCheck(t, msg);
    n = n->GetNext();
  }
  if (!chk) return false;

  CTypeManager *tm = CTypeManager::Get();
  if (!_cond->GetType()->Match(tm->GetBool())) { // condition must be boolean
    if (t != NULL) *t = _cond->GetToken();
    if (msg != NULL) *msg = "expected boolean type condition";
    return false;
  }
  return true;
}

ostream& CAstStatWhile::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << "while cond" << endl;
  _cond->print(out, indent+2);
  out << ind << "while-body" << endl;
  if (_body != NULL) {
    CAstStatement *s = _body;
    do {
      s->print(out, indent+2);
      s = s->GetNext();
    } while (s != NULL);
  }
  else out << ind << "  empty." << endl;

  return out;
}

string CAstStatWhile::dotAttr(void) const
{
  return " [label=\"while\",shape=box]";
}

void CAstStatWhile::toDot(ostream &out, int indent) const
{
  string ind(indent, ' ');

  CAstNode::toDot(out, indent);

  _cond->toDot(out, indent);
  out << ind << dotID() << "->" << _cond->dotID() << ";" << endl;

  if (_body != NULL) {
    CAstStatement *s = _body;
    if (s != NULL) {
      string prev = dotID();
      do {
        s->toDot(out, indent);
        out << ind << prev << " -> " << s->dotID() << " [style=dotted];"
            << endl;
        prev = s->dotID();
        s = s->GetNext();
      } while (s != NULL);
    }
  }
}

CTacAddr* CAstStatWhile::ToTac(CCodeBlock *cb, CTacLabel *next, CTacLabel* end)
{
  // prepare labels
  CTacLabel* re = cb->CreateLabel();
  CTacLabel* body = cb->CreateLabel();
  CAstStatement *s = GetBody();
  CTacLabel* loopEnd = cb->CreateLabel();

  // mark return label
  cb->AddInstr(re);
  // add three address code of condtion with true, false label
  _cond->ToTac(cb, body, loopEnd);
  cb->AddInstr(body);
  while (s != NULL) {
    CTacLabel *next = cb->CreateLabel();
    s->ToTac(cb, next, loopEnd);
    cb->AddInstr(next);
    s = s->GetNext();
  }
  // return to up after adding while statements
  cb->AddInstr(new CTacInstr(opGoto, re));
  cb->AddInstr(loopEnd);
  cb->AddInstr(new CTacInstr(opGoto, next));
  return NULL;
}


//------------------------------------------------------------------------------
// CAstExpression
//
CAstExpression::CAstExpression(CToken t)
  : CAstNode(t)
{
}

CTacAddr* CAstExpression::ToTac(CCodeBlock *cb)
{
  return NULL;
}

CTacAddr* CAstExpression::ToTac(CCodeBlock *cb,
                                CTacLabel *ltrue, CTacLabel *lfalse)
{
  return NULL;
}


//------------------------------------------------------------------------------
// CAstOperation
//
CAstOperation::CAstOperation(CToken t, EOperation oper)
  : CAstExpression(t), _oper(oper)
{
}

EOperation CAstOperation::GetOperation(void) const
{
  return _oper;
}


//------------------------------------------------------------------------------
// CAstBinaryOp
//
CAstBinaryOp::CAstBinaryOp(CToken t, EOperation oper,
                           CAstExpression *l,CAstExpression *r)
  : CAstOperation(t, oper), _left(l), _right(r)
{
  // these are the only binary operation we support for now
  assert((oper == opAdd)        || (oper == opSub)         ||
         (oper == opMul)        || (oper == opDiv)         ||
         (oper == opAnd)        || (oper == opOr)          ||
         (oper == opEqual)      || (oper == opNotEqual)    ||
         (oper == opLessThan)   || (oper == opLessEqual)   ||
         (oper == opBiggerThan) || (oper == opBiggerEqual)
        );
  assert(l != NULL);
  assert(r != NULL);
}

CAstExpression* CAstBinaryOp::GetLeft(void) const
{
  return _left;
}

CAstExpression* CAstBinaryOp::GetRight(void) const
{
  return _right;
}

bool CAstBinaryOp::TypeCheck(CToken *t, string *msg) const
{
  // check recursively
  bool ret = _left->TypeCheck(t,msg) && _right->TypeCheck(t,msg);
  CTypeManager* tm = CTypeManager::Get();
  if (!ret) return false;

  const CType* leftType = _left->GetType(), *rightType = _right->GetType();
  switch (GetOperation()) {
  case opAdd:
  case opMul:
  case opSub:
  case opDiv:
    // lhs : integer, rhs : integer
    if (!(leftType->Match(tm->GetInt()))) {
      if (t != NULL) *t = _left->GetToken();
      if (msg != NULL) *msg = "expected integer type expression in left operand";
      return false;
    } else if (!(rightType->Match(tm->GetInt()))) {
      if (t != NULL) *t = _right->GetToken();
      if (msg != NULL) *msg = "expected integer type expression in right operand";
      return false;
    }
    return true;
  case opAnd:
  case opOr:
    // lhs : boolean, rhs : boolean
    if (!(leftType->Match(tm->GetBool()))) {
      if (t != NULL) *t = _left->GetToken();
      if (msg != NULL) *msg = "expected boolean type expression in left operand";
      return false;
    } else if (!(rightType->Match(tm->GetBool()))) {
      if (t != NULL) *t = _right->GetToken();
      if (msg != NULL) *msg = "expected boolean type expression in right operand";
      return false;
    }
    return true;
  case opEqual:
  case opNotEqual:
    // lhs : boolean, character, integer
    // rhs : must be same as lhs
    if (!(leftType->Match(tm->GetBool()) || leftType->Match(tm->GetChar()) || leftType->Match(tm->GetInt()))) {
      if (t != NULL) *t = _left->GetToken();
      if (msg != NULL) *msg = "expected boolean or character or integer type expression in left operand";
      return false;
    } else if (!(rightType->Match(leftType))) {
      if (t != NULL) *t = _right->GetToken();
      if (msg != NULL) *msg = "different type between right and left operand";
      return false;
    }
    return true;
  case opBiggerEqual:
  case opBiggerThan:
  case opLessEqual:
  case opLessThan:
    // lhs : character, integer
    // rhs : must be same as lhs
    if (!(leftType->Match(tm->GetChar()) || leftType->Match(tm->GetInt()))) {
      if (t != NULL) *t = _left->GetToken();
      if (msg != NULL) *msg = "expected character or integer type expression in left operand";
      return false;
    } else if (!(rightType->Match(leftType))) {
      if (t != NULL) *t = _right->GetToken();
      if (msg != NULL) *msg = "different type between right and left operand";
      return false;
    }
    return true;
  default: // Never reached code
    return false;
  }
}

const CType* CAstBinaryOp::GetType(void) const
{
  switch (GetOperation()) {
  case opAdd:
  case opMul:
  case opSub:
  case opDiv:
    return CTypeManager::Get()->GetInt();
  case opAnd:
  case opOr:
  case opEqual:
  case opNotEqual:
  case opLessThan:
  case opLessEqual:
  case opBiggerThan:
  case opBiggerEqual:
    return CTypeManager::Get()->GetBool();
  default: // Never reached code
    return NULL;
  }
}

ostream& CAstBinaryOp::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << GetOperation() << " ";

  const CType *t = GetType();
  if (t != NULL) out << t; else out << "<INVALID>";

  out << endl;

  _left->print(out, indent+2);
  _right->print(out, indent+2);

  return out;
}

string CAstBinaryOp::dotAttr(void) const
{
  ostringstream out;
  out << " [label=\"" << GetOperation() << "\",shape=box]";
  return out.str();
}

void CAstBinaryOp::toDot(ostream &out, int indent) const
{
  string ind(indent, ' ');

  CAstNode::toDot(out, indent);

  _left->toDot(out, indent);
  out << ind << dotID() << "->" << _left->dotID() << ";" << endl;
  _right->toDot(out, indent);
  out << ind << dotID() << "->" << _right->dotID() << ";" << endl;
}

CTacAddr* CAstBinaryOp::ToTac(CCodeBlock *cb)
{
  // check if a type is boolean type
  //     to use three address code with true, false label
  CTypeManager* tm = CTypeManager::Get();
  if (tm->GetBool()->Match(GetType())) {
    CTacAddr* ret = cb->CreateTemp(tm->GetBool());
    CTacLabel* trueLabel = cb->CreateLabel();
    CTacLabel* falseLabel = cb->CreateLabel();
    CTacLabel* nextLabel = cb->CreateLabel();
    ToTac(cb, trueLabel, falseLabel);
    cb->AddInstr(trueLabel);
    cb->AddInstr(new CTacInstr(opAssign, ret, new CTacConst(1)));
    cb->AddInstr(new CTacInstr(opGoto, nextLabel));
    cb->AddInstr(falseLabel);
    cb->AddInstr(new CTacInstr(opAssign, ret, new CTacConst(0)));
    cb->AddInstr(nextLabel);
    return ret;
  } else {
    CTacAddr* lhs = _left->ToTac(cb);
    CTacAddr* rhs = _right->ToTac(cb);
    CTacTemp* ret = cb->CreateTemp(GetType());
    cb->AddInstr(new CTacInstr(GetOperation(), ret, lhs, rhs));
    return ret;
  }
}

CTacAddr* CAstBinaryOp::ToTac(CCodeBlock *cb,
                              CTacLabel *ltrue, CTacLabel *lfalse)
{
  // this is the case of opAnd, opOr, opEqual, opNotEqual,
  // opLessThan, opLessEqual, opBiggerThan, opBiggerEqual
  // _right must have same type as _left
  CTypeManager* tm = CTypeManager::Get();
  EOperation oper = GetOperation();
  assert(tm->GetBool()->Match(GetType()));
  switch (oper) {
    case opAnd:
    case opOr:
    {
      CTacLabel* midLabel = cb->CreateLabel();
      if (oper == opAnd)
        _left->ToTac(cb, midLabel, lfalse);
      else // oper == opOr
        _left->ToTac(cb, ltrue, midLabel);
      cb->AddInstr(midLabel);
      _right->ToTac(cb, ltrue, lfalse);
      return NULL;
    }
    default: //relational operations
    {
      CTacAddr* lhs = _left->ToTac(cb);
      CTacAddr* rhs = _right->ToTac(cb);
      cb->AddInstr(new CTacInstr(oper, ltrue, lhs, rhs));
      cb->AddInstr(new CTacInstr(opGoto, lfalse));
      return NULL;
    }
  }
}


//------------------------------------------------------------------------------
// CAstUnaryOp
//
CAstUnaryOp::CAstUnaryOp(CToken t, EOperation oper, CAstExpression *e)
  : CAstOperation(t, oper), _operand(e)
{
  assert((oper == opNeg) || (oper == opPos) || (oper == opNot));
  assert(e != NULL);
}

CAstExpression* CAstUnaryOp::GetOperand(void) const
{
  return _operand;
}

bool CAstUnaryOp::TypeCheck(CToken *t, string *msg) const
{
  // check recursively
  bool ret = _operand->TypeCheck(t, msg);
  if (!ret) return false;

  const CType *eType = _operand->GetType();
  CTypeManager* tm = CTypeManager::Get();
  switch (GetOperation()) {
  case opNeg:
  case opPos:
    // operand : integer
    if(!(eType->Match(tm->GetInt()))) {
      if (t != NULL) *t = _operand->GetToken();
      if (msg != NULL) *msg = "expected integer type expression in the operand";
      return false;
    }
    return true;
  case opNot:
    // operand : boolean
    if(!(eType->Match(tm->GetBool()))) {
      if (t != NULL) *t = _operand->GetToken();
      if (msg != NULL) *msg = "expected boolean type expression in the operand";
      return false;
    }
    return true;
  default: // Never reached code
    return false;
  }
}

const CType* CAstUnaryOp::GetType(void) const
{
  switch (GetOperation()) {
    case opNeg:
    case opPos:
      return CTypeManager::Get()->GetInt();
    case opNot:
      return CTypeManager::Get()->GetBool();
    default: // Never reached code
      return NULL;
  }
}

ostream& CAstUnaryOp::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << GetOperation() << " ";

  const CType *t = GetType();
  if (t != NULL) out << t; else out << "<INVALID>";
  out << endl;

  _operand->print(out, indent+2);

  return out;
}

string CAstUnaryOp::dotAttr(void) const
{
  ostringstream out;
  out << " [label=\"" << GetOperation() << "\",shape=box]";
  return out.str();
}

void CAstUnaryOp::toDot(ostream &out, int indent) const
{
  string ind(indent, ' ');

  CAstNode::toDot(out, indent);

  _operand->toDot(out, indent);
  out << ind << dotID() << "->" << _operand->dotID() << ";" << endl;
}

CTacAddr* CAstUnaryOp::ToTac(CCodeBlock *cb)
{
  CTypeManager* tm = CTypeManager::Get();
  EOperation oper = GetOperation();
  switch (oper) {
    case opNeg:
    {
      CTacAddr* operand = _operand->ToTac(cb);
      CTacTemp* ret = cb->CreateTemp(GetType());
      cb->AddInstr(new CTacInstr(GetOperation(), ret, operand));
      return ret;
    }
    case opPos:
    {
      CTacAddr* operand = _operand->ToTac(cb);
      return operand;
    }
    default: // opNot
    // if an operator is opNot, add three address code with true, false label
    {
      CTacAddr* ret = cb->CreateTemp(tm->GetBool());
      CTacLabel* trueLabel = cb->CreateLabel();
      CTacLabel* falseLabel = cb->CreateLabel();
      CTacLabel* nextLabel = cb->CreateLabel();
      ToTac(cb, trueLabel, falseLabel);
      cb->AddInstr(trueLabel);
      cb->AddInstr(new CTacInstr(opAssign, ret, new CTacConst(1)));
      cb->AddInstr(new CTacInstr(opGoto, nextLabel));
      cb->AddInstr(falseLabel);
      cb->AddInstr(new CTacInstr(opAssign, ret, new CTacConst(0)));
      cb->AddInstr(nextLabel);
      return ret;
    }
  }
}

CTacAddr* CAstUnaryOp::ToTac(CCodeBlock *cb,
                             CTacLabel *ltrue, CTacLabel *lfalse)
{
  //opNot
  assert(CTypeManager::Get()->GetBool()->Match(GetType()));
  _operand->ToTac(cb, lfalse, ltrue); // change the order of label for not operation
  return NULL;
}


//------------------------------------------------------------------------------
// CAstSpecialOp
//
CAstSpecialOp::CAstSpecialOp(CToken t, EOperation oper, CAstExpression *e,
                             const CType *type)
  : CAstOperation(t, oper), _operand(e), _type(type)
{
  assert((oper == opAddress) || (oper == opDeref) || (oper = opCast));
  assert(e != NULL);
  assert(((oper != opCast) && (type == NULL)) ||
         ((oper == opCast) && (type != NULL)));
}

CAstExpression* CAstSpecialOp::GetOperand(void) const
{
  return _operand;
}

bool CAstSpecialOp::TypeCheck(CToken *t, string *msg) const
{
  if(!_operand->TypeCheck(t, msg)) return false;
  switch (GetOperation()) {
  case opAddress:
    // check if the type is array
    // : we only use opAddress for implicit change on array argument
    if(!_operand->GetType()->IsArray()) {
      if (t != NULL) *t = GetToken();
      if (msg != NULL) *msg = "opAddress is only used on array type";
      return false;
    }
    return true;
  case opDeref:
    // check if the type is pointer type
    if (!_operand->GetType()->IsPointer()) {
      if (t != NULL) *t = GetToken();
      if (msg != NULL) *msg = "opDeref should be used on pointer type";
      return false;
    }
    return true;
  case opCast:
    // opCast is never used
    // so return false for the type checking
    if (t != NULL) *t = GetToken();
    if (msg != NULL) *msg = "opCast is never used";
    return false;
  default: // Never reached code
    return false;
  }
}

const CType* CAstSpecialOp::GetType(void) const
{
  CTypeManager* tm = CTypeManager::Get();
  switch (GetOperation()) {
  case opAddress:
    return tm->GetPointer(_operand->GetType());
  case opDeref:
    const CPointerType* pt;
    pt = dynamic_cast<const CPointerType*>(_operand->GetType());
    if (pt == NULL) return NULL;
    return pt->GetBaseType(); // dereferencing makes type to the basetype
  case opCast:
    return _type; // change the type to specific type
  default:
    return NULL;
  }
}

ostream& CAstSpecialOp::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << GetOperation() << " ";

  const CType *t = GetType();
  if (t != NULL) out << t; else out << "<INVALID>";
  out << endl;

  _operand->print(out, indent+2);

  return out;
}

string CAstSpecialOp::dotAttr(void) const
{
  ostringstream out;
  out << " [label=\"" << GetOperation() << "\",shape=box]";
  return out.str();
}

void CAstSpecialOp::toDot(ostream &out, int indent) const
{
  string ind(indent, ' ');

  CAstNode::toDot(out, indent);

  _operand->toDot(out, indent);
  out << ind << dotID() << "->" << _operand->dotID() << ";" << endl;
}

CTacAddr* CAstSpecialOp::ToTac(CCodeBlock *cb)
{
  // make an operation instruction and return temp value
  CTacAddr* operand = _operand->ToTac(cb);
  CTacTemp* temp = cb->CreateTemp(GetType());
  cb->AddInstr(new CTacInstr(GetOperation(), temp, operand));
  return temp;
}


//------------------------------------------------------------------------------
// CAstFunctionCall
//
CAstFunctionCall::CAstFunctionCall(CToken t, const CSymProc *symbol)
  : CAstExpression(t), _symbol(symbol)
{
  assert(symbol != NULL);
}

const CSymProc* CAstFunctionCall::GetSymbol(void) const
{
  return _symbol;
}

void CAstFunctionCall::AddArg(CAstExpression *arg)
{
  _arg.push_back(arg);
}

int CAstFunctionCall::GetNArgs(void) const
{
  return (int)_arg.size();
}

CAstExpression* CAstFunctionCall::GetArg(int index) const
{
  assert((index >= 0) && (index < _arg.size()));
  return _arg[index];
}

bool CAstFunctionCall::TypeCheck(CToken *t, string *msg) const
{
  const CSymProc* symProc = GetSymbol();
  // check the number of arguments
  if (symProc->GetNParams() != GetNArgs()) {
    if (t != NULL) *t = GetToken();
    if (msg != NULL) *msg = "number of arguments does not match the number of parameters";
    return false;
  }
  // check recursively
  for (CAstExpression* it : _arg) {
    if (!it->TypeCheck(t, msg)) {
      return false;
    }
  }
  // check whether arguments' type matches parameters' type
  int n = symProc->GetNParams();
  for (int i = 0; i < n; i++) {
    const CSymParam* param = symProc->GetParam(i);
    CAstExpression* argument = GetArg(i);
    if (param->GetDataType() == NULL) {
      if (t != NULL) *t = argument->GetToken();
      if (msg != NULL) *msg = "argument's type is invalid";
      return false;
    }
    if (!param->GetDataType()->Match(argument->GetType())) {
      if (t != NULL) *t = GetToken();
      if (msg != NULL) *msg = "argument's type does not match with the parameter";
      return false;
    }
  }
  return true;
}

const CType* CAstFunctionCall::GetType(void) const
{
  return GetSymbol()->GetDataType();
}

ostream& CAstFunctionCall::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << "call " << _symbol << " ";
  const CType *t = GetType();
  if (t != NULL) out << t; else out << "<INVALID>";
  out << endl;

  for (size_t i=0; i<_arg.size(); i++) {
    _arg[i]->print(out, indent+2);
  }

  return out;
}

string CAstFunctionCall::dotAttr(void) const
{
  ostringstream out;
  out << " [label=\"call " << _symbol->GetName() << "\",shape=box]";
  return out.str();
}

void CAstFunctionCall::toDot(ostream &out, int indent) const
{
  string ind(indent, ' ');

  CAstNode::toDot(out, indent);

  for (size_t i=0; i<_arg.size(); i++) {
    _arg[i]->toDot(out, indent);
    out << ind << dotID() << "->" << _arg[i]->dotID() << ";" << endl;
  }
}

CTacAddr* CAstFunctionCall::ToTac(CCodeBlock *cb)
{
  // make return type temp variable if the return type is not null
  CTacAddr* dst;
  CTypeManager* tm = CTypeManager::Get();
  if (tm->GetNull()->Match(GetType())){
    dst = NULL;
  } else {
    dst = cb->CreateTemp(GetType());
  }

  // add params
  for (int i = GetNArgs() - 1; i >= 0; i--)
    cb->AddInstr(new CTacInstr(opParam, new CTacConst(i), GetArg(i)->ToTac(cb)));

  // add an call instruction
  cb->AddInstr(new CTacInstr(opCall, dst, new CTacName(GetSymbol())));
  return dst;
}

CTacAddr* CAstFunctionCall::ToTac(CCodeBlock *cb,
                                  CTacLabel *ltrue, CTacLabel *lfalse)
{
  // the return type should be the Boolean type
  assert(CTypeManager::Get()->GetBool()->Match(GetType()));
  CTacAddr* dst = ToTac(cb);
  cb->AddInstr(new CTacInstr(opEqual, ltrue, dst, new CTacConst(1)));
  cb->AddInstr(new CTacInstr(opGoto, lfalse));
  return NULL;
}



//------------------------------------------------------------------------------
// CAstOperand
//
CAstOperand::CAstOperand(CToken t)
  : CAstExpression(t)
{
}


//------------------------------------------------------------------------------
// CAstDesignator
//
CAstDesignator::CAstDesignator(CToken t, const CSymbol *symbol)
  : CAstOperand(t), _symbol(symbol)
{
  assert(symbol != NULL);
}

const CSymbol* CAstDesignator::GetSymbol(void) const
{
  return _symbol;
}

bool CAstDesignator::TypeCheck(CToken *t, string *msg) const
{
  if (GetType() == NULL) {
    if (t != NULL) *t = GetToken();
    if (msg != NULL) *msg = "Invalid Type for the symbol";
    return false;
  }
  return true;
}

const CType* CAstDesignator::GetType(void) const
{
  return GetSymbol()->GetDataType();
}

ostream& CAstDesignator::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << _symbol << " ";

  const CType *t = GetType();
  if (t != NULL) out << t; else out << "<INVALID>";

  out << endl;

  return out;
}

string CAstDesignator::dotAttr(void) const
{
  ostringstream out;
  out << " [label=\"" << _symbol->GetName();
  out << "\",shape=ellipse]";
  return out.str();
}

void CAstDesignator::toDot(ostream &out, int indent) const
{
  string ind(indent, ' ');

  CAstNode::toDot(out, indent);
}

CTacAddr* CAstDesignator::ToTac(CCodeBlock *cb)
{
  // make symbol name
  return new CTacName(GetSymbol());
}

CTacAddr* CAstDesignator::ToTac(CCodeBlock *cb,
                                CTacLabel *ltrue, CTacLabel *lfalse)
{
  assert(CTypeManager::Get()->GetBool()->Match(GetType()));
  cb->AddInstr(new CTacInstr(opEqual, ltrue, ToTac(cb), new CTacConst(1)));
  cb->AddInstr(new CTacInstr(opGoto, lfalse));
  return NULL;
}


//------------------------------------------------------------------------------
// CAstArrayDesignator
//
CAstArrayDesignator::CAstArrayDesignator(CToken t, const CSymbol *symbol)
  : CAstDesignator(t, symbol), _done(false), _offset(NULL)
{
}

void CAstArrayDesignator::AddIndex(CAstExpression *idx)
{
  assert(!_done);
  _idx.push_back(idx);
}

void CAstArrayDesignator::IndicesComplete(void)
{
  assert(!_done);
  _done = true;
}

int CAstArrayDesignator::GetNIndices(void) const
{
  return (int)_idx.size();
}

CAstExpression* CAstArrayDesignator::GetIndex(int index) const
{
  assert((index >= 0) && (index < _idx.size()));
  return _idx[index];
}

bool CAstArrayDesignator::TypeCheck(CToken *t, string *msg) const
{
  bool result = true;

  assert(_done);
  // check if the symbol is array or pointer of array
  const CType* ret = _symbol->GetDataType();
  if (ret->IsPointer()) ret = dynamic_cast<const CPointerType*>(ret)->GetBaseType();
  if (!ret->IsArray()) {
    if (t != NULL) *t = GetToken();
    if (msg != NULL) *msg = "symbol's type should be array or pointer of array";
    return false;
  }

  //check for indices's type
  for (int i = 0; i < _idx.size(); i++) {
    CAstExpression* it = _idx[i];
    assert(it != NULL);
    result = it->TypeCheck(t, msg);
    if (!result) return false;
    if (!it->GetType()->Match(CTypeManager::Get()->GetInt())) {
      if (t != NULL) *t = it->GetToken();
      if (msg != NULL) *msg = "index in array designator must be integer type";
      return false;
    }
  }

  // GetType is checking with the loop, so if it returns NULL, it is invalid because of too many indices
  // if it returns array type, this means indices are not full
  const CType* type = GetType();
  if (type == NULL) {
    if (t != NULL) *t = GetToken();
    if (msg != NULL) *msg = "Too many indices";
    return false;
  } else if (type->IsArray()) {
    if (t != NULL) *t = GetToken();
    if (msg != NULL) *msg = "Not enough indices";
    return false;
  }

  return result;
}

const CType* CAstArrayDesignator::GetType(void) const
{
  const CType* ret = _symbol->GetDataType();
  try {
    if (ret->IsPointer()) ret = dynamic_cast<const CPointerType*>(ret)->GetBaseType();
    for (int i = 0; i < _idx.size(); i++) {
      if (!ret->IsArray()) return NULL; // if it access the non array, type is INVALID
      const CArrayType* arrType = dynamic_cast<const CArrayType*>(ret);
      assert(arrType);
      ret = arrType->GetInnerType(); // get the inner type for index count
    }
    return ret;
  } catch (...) {
    return NULL;
  }
}

ostream& CAstArrayDesignator::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << _symbol << " ";

  const CType *t = GetType();
  if (t != NULL) out << t; else out << "<INVALID>";

  out << endl;

  for (size_t i=0; i<_idx.size(); i++) {
    _idx[i]->print(out, indent+2);
  }

  return out;
}

string CAstArrayDesignator::dotAttr(void) const
{
  ostringstream out;
  out << " [label=\"" << _symbol->GetName() << "[]\",shape=ellipse]";
  return out.str();
}

void CAstArrayDesignator::toDot(ostream &out, int indent) const
{
  string ind(indent, ' ');

  CAstNode::toDot(out, indent);

  for (size_t i=0; i<_idx.size(); i++) {
    _idx[i]->toDot(out, indent);
    out << ind << dotID() << "-> " << _idx[i]->dotID() << ";" << endl;
  }
}

CTacAddr* CAstArrayDesignator::ToTac(CCodeBlock *cb)
{
  //dereference if the symbol is pointer type
  const CArrayType* arrayType;
  CAstExpression* arrayPointer;
  CToken emptyToken = new CToken();

  if (_symbol->GetDataType()->IsPointer()) {
    const CPointerType* pointerType = dynamic_cast<const CPointerType*>(_symbol->GetDataType());
    arrayType = dynamic_cast<const CArrayType*>(pointerType->GetBaseType());
    arrayPointer = new CAstDesignator(emptyToken, _symbol);
  } else {
    arrayType = dynamic_cast<const CArrayType*>(_symbol->GetDataType());
    arrayPointer = new CAstSpecialOp(emptyToken, opAddress, new CAstDesignator(emptyToken, _symbol));
  }
  assert(arrayType != NULL);

  //fill the empty indices
  CTypeManager* tm = CTypeManager::Get();
  const CType* t = arrayType;
  int cnt = 0;
  while(t->IsArray()) {
    if (cnt >= _idx.size()) {
      _idx.push_back(new CAstConstant(emptyToken, tm->GetInt(), 0));
    }
    cnt++;
    t = dynamic_cast<const CArrayType*>(t)->GetInnerType();
  }

  //get size from type, t is already unwrapped
  int size = t->GetSize();

  //use function to find out the offset
  CAstExpression* offset;
  const CSymProc* dimSym = dynamic_cast<const CSymProc*>(cb->GetOwner()->GetSymbolTable()->FindSymbol("DIM"));
  const CSymProc* dofsSym = dynamic_cast<const CSymProc*>(cb->GetOwner()->GetSymbolTable()->FindSymbol("DOFS"));
  for (int i = 0; i < _idx.size(); i++) {
    if (i == 0) {
      offset = _idx[i];
    } else {
      offset = new CAstBinaryOp(emptyToken, opAdd, offset, _idx[i]);
    }
    if (i == _idx.size() - 1) {
      offset = new CAstBinaryOp(emptyToken, opMul, offset, new CAstConstant(emptyToken, tm->GetInt(), size));
    } else {
      CAstFunctionCall* dimCall = new CAstFunctionCall(emptyToken, dimSym);
      dimCall->AddArg(arrayPointer);
      dimCall->AddArg(new CAstConstant(emptyToken, tm->GetInt(), i+2));
      offset = new CAstBinaryOp(emptyToken, opMul, offset, dimCall);
    }
  }

  //add the offset to the symbol's address
  CAstFunctionCall* dofsCall = new CAstFunctionCall(emptyToken, dofsSym);
  dofsCall->AddArg(arrayPointer);
  CAstExpression* address = new CAstBinaryOp(emptyToken, opAdd, offset, dofsCall);
  address = new CAstBinaryOp(emptyToken, opAdd, arrayPointer, address);

  //return the referencing variable
  CTacAddr* result = dynamic_cast<CTacAddr*>(address->ToTac(cb));
  CTacName* resultName = dynamic_cast<CTacName*>(result);
  if (resultName == NULL) {
    CTacTemp* temp = cb->CreateTemp(tm->GetInt());
    cb->AddInstr(new CTacInstr(opAssign, temp, result));
    resultName = temp;
  }
  return new CTacReference(resultName->GetSymbol(), _symbol);
}

CTacAddr* CAstArrayDesignator::ToTac(CCodeBlock *cb,
                                     CTacLabel *ltrue, CTacLabel *lfalse)
{
  CTacAddr* ret = ToTac(cb);
  cb->AddInstr(new CTacInstr(opEqual, ltrue, ret, new CTacConst(1)));
  cb->AddInstr(new CTacInstr(opGoto, lfalse));
  return ret;
}


//------------------------------------------------------------------------------
// CAstConstant
//
CAstConstant::CAstConstant(CToken t, const CType *type, long long value)
  : CAstOperand(t), _type(type), _value(value)
{
}

void CAstConstant::SetValue(long long value)
{
  _value = value;
}

long long CAstConstant::GetValue(void) const
{
  return _value;
}

string CAstConstant::GetValueStr(void) const
{
  ostringstream out;

  if (GetType() == CTypeManager::Get()->GetBool()) {
    out << (_value == 0 ? "false" : "true");
  } else {
    out << dec << _value;
  }

  return out.str();
}

bool CAstConstant::TypeCheck(CToken *t, string *msg) const
{
  CTypeManager* tm = CTypeManager::Get();
  if (_type->Match(tm->GetInt())) {
    if (_value < -2147483648 || _value > 2147483647) {
      if (t != NULL) *t = GetToken();
      if (msg != NULL) *msg = "invalid value for integer type constant";
      return false;
    }
  } else if (_type->Match(tm->GetChar())) {
    if (_value < 0 || _value > 255) {
      if (t != NULL) *t = GetToken();
      if (msg != NULL) *msg = "invalid value for character type constant";
      return false;
    }
  } else if (_type->Match(tm->GetBool())) {
    if (_value != 0 && _value != 1) {
      if (t != NULL) *t = GetToken();
      if (msg != NULL) *msg = "invalid value for boolean type constant";
      return false;
    }
  } else {
    if (t != NULL) *t = GetToken();
    if (msg != NULL) *msg = "invalid type for constant";
    return false;
  }
  return true;
}

const CType* CAstConstant::GetType(void) const
{
  return _type;
}

ostream& CAstConstant::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << GetValueStr() << " ";

  const CType *t = GetType();
  if (t != NULL) out << t; else out << "<INVALID>";

  out << endl;

  return out;
}

string CAstConstant::dotAttr(void) const
{
  ostringstream out;
  out << " [label=\"" << GetValueStr() << "\",shape=ellipse]";
  return out.str();
}

CTacAddr* CAstConstant::ToTac(CCodeBlock *cb)
{
  // return constant instance with value
  return new CTacConst((int)GetValue());
}

CTacAddr* CAstConstant::ToTac(CCodeBlock *cb,
                                CTacLabel *ltrue, CTacLabel *lfalse)
{
  assert(CTypeManager::Get()->GetBool()->Match(GetType()));
  cb->AddInstr(new CTacInstr(opEqual, ltrue, ToTac(cb), new CTacConst(1)));
  cb->AddInstr(new CTacInstr(opGoto, lfalse));
  return NULL;
}


//------------------------------------------------------------------------------
// CAstStringConstant
//
int CAstStringConstant::_idx = 0;

CAstStringConstant::CAstStringConstant(CToken t, const string value,
                                       CAstScope *s)
  : CAstOperand(t)
{
  CTypeManager *tm = CTypeManager::Get();

  _type = tm->GetArray(strlen(CToken::unescape(value).c_str())+1,
                       tm->GetChar());
  _value = new CDataInitString(value);

  ostringstream o;
  o << "_str_" << ++_idx;

  _sym = new CSymGlobal(o.str(), _type);
  _sym->SetData(_value);
  s->GetSymbolTable()->AddSymbol(_sym);
}

const string CAstStringConstant::GetValue(void) const
{
  return _value->GetData();
}

const string CAstStringConstant::GetValueStr(void) const
{
  return GetValue();
}

bool CAstStringConstant::TypeCheck(CToken *t, string *msg) const
{
  return true;
}

const CType* CAstStringConstant::GetType(void) const
{
  return _type;
}

ostream& CAstStringConstant::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << '"' << GetValueStr() << '"' << " ";

  const CType *t = GetType();
  if (t != NULL) out << t; else out << "<INVALID>";

  out << endl;

  return out;
}

string CAstStringConstant::dotAttr(void) const
{
  ostringstream out;
  // the string is already escaped, but dot requires double escaping
  out << " [label=\"\\\"" << CToken::escape(GetValueStr())
      << "\\\"\",shape=ellipse]";
  return out.str();
}

CTacAddr* CAstStringConstant::ToTac(CCodeBlock *cb)
{
  // return name of string
  return new CTacName(_sym);
}

CTacAddr* CAstStringConstant::ToTac(CCodeBlock *cb,
                                CTacLabel *ltrue, CTacLabel *lfalse)
{
  // never reached code
  return NULL;
}


