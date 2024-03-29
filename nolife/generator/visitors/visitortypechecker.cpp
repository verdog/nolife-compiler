#include <iostream>

#include "visitortypechecker.hpp"

#include "../parser/astnode.hpp"
#include "../parser/astarrayaccessnode.hpp"
#include "../parser/astarraynode.hpp"
#include "../parser/astassignnode.hpp"
#include "../parser/astcallnode.hpp"
#include "../parser/astcaselabelsnode.hpp"
#include "../parser/astcasenode.hpp"
#include "../parser/astclausenode.hpp"
#include "../parser/astcompoundstmtnode.hpp"
#include "../parser/astconstantnode.hpp"
#include "../parser/astdeclnode.hpp"
#include "../parser/astexpressionnode.hpp"
#include "../parser/astifnode.hpp"
#include "../parser/astparamnode.hpp"
#include "../parser/astprocnode.hpp"
#include "../parser/astprognode.hpp"
#include "../parser/astreadnode.hpp"
#include "../parser/astreturnnode.hpp"
#include "../parser/aststmtnode.hpp"
#include "../parser/astsymnode.hpp"
#include "../parser/asttypenode.hpp"
#include "../parser/astvariablenode.hpp"
#include "../parser/astwhilenode.hpp"
#include "../parser/astwritenode.hpp"

TypeCheckVisitor::TypeCheckVisitor() {
    // special use case variables
    mCastType = ast::Type::Types::Undefined;
    mReturnedType = ast::Type::Types::Undefined;

    // flags
    mFlagCanUseArrayUnsubscripted = false;
    mFlagFoundReturnStatement = false;

    using TypePair = std::pair<ast::Type::Types, ast::Type::Types>;

    constexpr ast::Type::Types INT = ast::Type::Types::Integer;
    constexpr ast::Type::Types FLOAT = ast::Type::Types::Float;
    constexpr ast::Type::Types CHAR = ast::Type::Types::Character;
    constexpr ast::Type::Types ERROR = ast::Type::Types::Undefined;

    // define the type lookup tables.

    tTypeCompatibilityTable arithTable;
    arithTable[TypePair(INT,     INT)] = INT;
    arithTable[TypePair(INT,   FLOAT)] = FLOAT;
    arithTable[TypePair(FLOAT,   INT)] = FLOAT;
    arithTable[TypePair(FLOAT, FLOAT)] = FLOAT;

    tTypeCompatibilityTable logTable;
    logTable[TypePair(CHAR, CHAR)] = CHAR;
    logTable[TypePair(CHAR, INT)] = ERROR;
    logTable[TypePair(CHAR, FLOAT)] = ERROR;
    logTable[TypePair(INT, CHAR)] = ERROR;
    logTable[TypePair(INT, INT)] = INT;
    logTable[TypePair(INT, FLOAT)] = INT;
    logTable[TypePair(FLOAT, CHAR)] = ERROR;
    logTable[TypePair(FLOAT, INT)] = INT;
    logTable[TypePair(FLOAT, FLOAT)] = FLOAT;

    tTypeCompatibilityTable relTable;
    relTable[TypePair(CHAR, CHAR)] = INT;
    relTable[TypePair(CHAR, INT)] = ERROR;
    relTable[TypePair(CHAR, FLOAT)] = ERROR;
    relTable[TypePair(INT, CHAR)] = ERROR;
    relTable[TypePair(INT, INT)] = INT;
    relTable[TypePair(INT, FLOAT)] = INT;
    relTable[TypePair(FLOAT, CHAR)] = ERROR;
    relTable[TypePair(FLOAT, INT)] = INT;
    relTable[TypePair(FLOAT, FLOAT)] = INT;

    tTypeCompatibilityTable modTable;
    modTable[TypePair(INT, INT)] = INT;

    mOpToTableMap[ast::Expression::Operation::Plus] = arithTable;
    mOpToTableMap[ast::Expression::Operation::Minus] = arithTable;
    mOpToTableMap[ast::Expression::Operation::Multiply] = arithTable;
    
    mOpToTableMap[ast::Expression::Operation::Or] = logTable;
    mOpToTableMap[ast::Expression::Operation::And] = logTable;

    mOpToTableMap[ast::Expression::Operation::LessThan] = relTable;
    mOpToTableMap[ast::Expression::Operation::LessThanOrEqual] = relTable;
    mOpToTableMap[ast::Expression::Operation::GreaterThan] = relTable;
    mOpToTableMap[ast::Expression::Operation::GreaterThanOrEqual] = relTable;
    mOpToTableMap[ast::Expression::Operation::Equals] = relTable;
    mOpToTableMap[ast::Expression::Operation::NotEqual] = relTable;

    mOpToTableMap[ast::Expression::Operation::Modulo] = modTable;

    mAssignmentConversionTable[TypePair(INT, INT)] = INT;
    mAssignmentConversionTable[TypePair(INT, FLOAT)] = INT;
    mAssignmentConversionTable[TypePair(INT, CHAR)] = ERROR;
    mAssignmentConversionTable[TypePair(FLOAT, INT)] = FLOAT;
    mAssignmentConversionTable[TypePair(FLOAT, FLOAT)] = FLOAT;
    mAssignmentConversionTable[TypePair(FLOAT, CHAR)] = ERROR;
    mAssignmentConversionTable[TypePair(CHAR, INT)] = ERROR;
    mAssignmentConversionTable[TypePair(CHAR, FLOAT)] = ERROR;
    mAssignmentConversionTable[TypePair(CHAR, CHAR)] = CHAR;
}

void TypeCheckVisitor::pushNewSymbolTable() {
    mSymbolTableStack.push_back(tSymbolTable());
    // std::cout << "Created a new symbol table. (number of tables remaining: " << mSymbolTableStack.size() << ")\n";
}

void TypeCheckVisitor::popSymbolTable() {
    auto table = mSymbolTableStack.back();

    for (auto pair : table) {
        auto info = pair.second;

        if (!info.isProcedure && info.referenceCount < 1) {
            std::cout << "!!!  Error: symbol \"" << info.name << "\" declared but never referenced.\n";
        }
    }

    mSymbolTableStack.pop_back();
    // std::cout << "Destroyed top symbol table. (number of tables remaining: " << mSymbolTableStack.size() << ")\n";
}

void TypeCheckVisitor::writeSymbol(std::string key, SymbolInfo value) {
    auto& topTable = mSymbolTableStack.back();
    topTable[key] = value;
    // std::cout << "  - inserted \"" << key << "\" into topmost table.\n";
}

bool TypeCheckVisitor::symbolExists(std::string key) {
    for (auto it = mSymbolTableStack.rbegin(); it != mSymbolTableStack.rend(); ++it) {
        auto& table = *it;
        if (table.count(key) != 0) {
            return true;
        }
    }

    return false;
}

SymbolInfo& TypeCheckVisitor::lookupSymbol(std::string key) {
    if (!symbolExists(key)) {
        throw "Symbol " + key + " does not exist in any symbol table.";
    }

    for (auto it = mSymbolTableStack.rbegin(); it != mSymbolTableStack.rend(); ++it) {
        auto& table = *it;
        if (table.count(key) != 0) {
            return table[key];
        }
    }
}

ast::Type::Types TypeCheckVisitor::getCombinedType(ast::Type::Types t1, ast::Type::Types t2, ast::Expression::Operation op) {
    using TypePair = std::pair<ast::Type::Types, ast::Type::Types>;

    if (op == ast::Expression::Operation::Not) {
        std::cout << "Since NOT is a unary operation, check it before calling getCombinedType.\n";
        return ast::Type::Types::Undefined;
    } else if (t1 == ast::Type::Types::Undefined && t2 == ast::Type::Types::Undefined) {
        // any + any = any
        return ast::Type::Types::Undefined;
    } else if (t1 == ast::Type::Types::Undefined) {
        // any + something = something
        return t2;
    } else if (t2 == ast::Type::Types::Undefined) {
        // something + any = something
        return t1;
    } else {
        if (mOpToTableMap.count(op)) {
            auto& table = mOpToTableMap[op];
            return table[TypePair(t1, t2)];
        } else {
            std::cout << "No known table for operation " << op << "!\n";
            return ast::Type::Types::Undefined;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

void TypeCheckVisitor::visit(ast::Base* b) {
    // std::cout << "Visited a base node.\n";
}

void TypeCheckVisitor::visit(ast::Program* p) {
    // std::cout << "Visited a program node.\n";

    // create new symbol table
    pushNewSymbolTable();

    // store info about the program
    std::string programName = p->getSymbol()->getImage();

    SymbolInfo programInfo = SymbolInfo(programName);
    programInfo.isProcedure = true;

    writeSymbol(programName, programInfo);

    // visit the symbol
    p->getSymbol()->accept(*this);

    // process declarations
    if (p->getDecl()) { // exists
        p->getDecl()->accept(*this);
    } else {
        // std::cout << programName << " has no decls.\n";
    }

    mFlagFoundReturnStatement = false;

    // process compound statement
    if (p->getCompoundStatement()) { // exists
        p->getCompoundStatement()->accept(*this);
    } else {
        // std::cout << programName << " has no compound statement.\n";
    }

    if (mFlagFoundReturnStatement == true) {
        std::cout << "!!!  Error: return statement in main program.\n";
    }

    mFlagFoundReturnStatement = false;

    // dump tables
    // std::cout << "Tables after processing the entire program:\n";
    // dumpTable();

    // remove symbol table
    popSymbolTable();

    mDone = true;
}

void TypeCheckVisitor::visit(ast::Declaration* d) {
    // std::cout << "visited a decl node.\n";

    registerProcedures(d);

    auto children = d->getChildren();

    for (auto node : children) {
        if (node != nullptr) {
            // delegate the bookkeeping to the type nodes.
            node->accept(*this);
        }
    }
}

void TypeCheckVisitor::registerProcedures(ast::Declaration* d) {
    for (auto child : d->getChildren()) {
        auto type = dynamic_cast<ast::Type*>(child);
        if (type != nullptr) {
            if (auto proc = type->childAsProcedure()) {
                // register procedure
                // std::cout << "  Detected procedure \"" << proc->getSymbol()->getImage() << "\".\n";

                if (mSymbolTableStack.back().count(proc->getSymbol()->getImage()) == 0) {
                    auto symInfo = SymbolInfo(proc->getSymbol()->getImage());
                    symInfo.type = type->getType();
                    symInfo.isProcedure = true;
                    symInfo.parameters = proc->getParameters();

                    writeSymbol(proc->getSymbol()->getImage(), symInfo);
                } else {
                    // variable already declared in this scope
                    std::cout << "!!!  Error: Redeclaration of symbol \"" << proc->getSymbol()->getImage() << "\".\n";
                }
            }
        }
    }
}

void TypeCheckVisitor::visit(ast::CompoundStatement* cs) {
    // std::cout << "Visited a compound statement node.\n";

    auto children = cs->getChildren();

    for (auto node : children) {
        if (node != nullptr) {
            // check for a function call used as a statement
            if (auto func = dynamic_cast<ast::Call*>(node)) {
                if (symbolExists(func->getSymbol()->getImage())) {
                    auto info = lookupSymbol(func->getSymbol()->getImage());
                    if (info.type != ast::Type::Types::Void) {
                        // since it returns something, it's a function.
                        std::cout << "!!!  Error: symbol \"" << func->getSymbol()->getImage() << "\" is a function, but was used as a procedure.\n";
                    }
                } else {
                    // symbol does not exist.
                    // this gets caught lower in the tree.
                }
            }

            // visit each statement
            node->accept(*this);
        }
    }
}

void TypeCheckVisitor::visit(ast::Parameters* p) {
    // std::cout << "visited a parameters node.\n";

    auto children = p->getChildren();

    mFlagCanUseArrayUnsubscripted = true;
    for (auto node : children) {
        if (node != nullptr) {
            // delegate the bookkeeping to the type nodes.
            node->accept(*this);
        }
    }
    mFlagCanUseArrayUnsubscripted = false;
}

void TypeCheckVisitor::visit(ast::Symbol* s) {
    // std::cout << "Visited symbol node \"" << s->getImage() << "\".\n";

    // if this node is visited, it means the symbol was access without subscripting.
    // if this symbol is an array, this is an error

    if (symbolExists(s->getImage())) {
        auto info = lookupSymbol(s->getImage());

        if (info.isArray && !mFlagCanUseArrayUnsubscripted) {
            std::cout << "!!!  Error: Illegal use of unsubscripted array type " << s->getImage() << ".\n";
        }
    } else {
        std::cout << "!!!  Error: Symbol \"" << s->getImage() << "\" used but never declared.\n";
    }
}

void TypeCheckVisitor::visit(ast::Type* t) {
    // std::cout << "Visited a type node. This probably shouldn't happen.\n";
}

void TypeCheckVisitor::visit_type(ast::Type* t) {
    if (auto sym = dynamic_cast<ast::Symbol*>(t->getChild())) {
        // std::cout << "  Detected a symbol.\n";

        if (mSymbolTableStack.back().count(sym->getImage()) == 0) {
            auto symInfo = SymbolInfo(sym->getImage());
            symInfo.type = t->getType();

            writeSymbol(sym->getImage(), symInfo);
        } else {
            // variable already declared in this scope
            std::cout << "!!!  Error: Redeclaration of symbol \"" << sym->getImage() << "\".\n";
        }

    } else if (auto arr = dynamic_cast<ast::Array*>(t->getChild())) {
        // std::cout << "  Detected an array.\n";

        if (mSymbolTableStack.back().count(arr->getSymbol()->getImage()) == 0) {
            auto symInfo = SymbolInfo(arr->getSymbol()->getImage());
            symInfo.type = t->getType();
            symInfo.isArray = true;
            symInfo.arrayLowBound = arr->getLowBound()->getImage();
            symInfo.arrayHighBound = arr->getHighBound()->getImage();

            writeSymbol(arr->getSymbol()->getImage(), symInfo);

            mFlagCanUseArrayUnsubscripted = true;
        } else {
            // variable already declared in this scope
            std::cout << "!!!  Error: Redeclaration of symbol \"" << arr->getSymbol()->getImage() << "\".\n";
        }

    } else if (auto proc = dynamic_cast<ast::Procedure*>(t->getChild())) {
        // procedures should have already been visited
    }

    t->getChild()->accept(*this);
    mFlagCanUseArrayUnsubscripted = false;
}

void TypeCheckVisitor::visit(ast::Integer* i) {
    // std::cout << "Visited an integer node.\n";
    visit_type(i);
}

void TypeCheckVisitor::visit(ast::Float* f) {
    // std::cout << "Visited a float node.\n";
    visit_type(f);
}

void TypeCheckVisitor::visit(ast::Character* c) {
    // std::cout << "Visited a character node.\n";
    visit_type(c);
}

void TypeCheckVisitor::visit(ast::Void* v) {
    // std::cout << "Visited a void node.\n";
    visit_type(v);
}

void TypeCheckVisitor::visit(ast::Array* a) {
    // std::cout << "Visited an array node.\n";
    for (auto node : a->getChildren()) {
        node->accept(*this);
    }
}

void TypeCheckVisitor::visit(ast::Assignment* a) {
    using TypePair = std::pair<ast::Type::Types, ast::Type::Types>;

    // std::cout << "visited assignment node.\n";

    // visit children to deterimine their type
    auto children = a->getChildren();
    auto left = children[0];
    auto right = dynamic_cast<ast::Expression*>(children[1]);

    left->accept(*this);
    right->accept(*this);

    std::string leftImage = a->getVariable()->getSymbol()->getImage();

    if (symbolExists(leftImage)) {
        auto leftType = lookupSymbol(leftImage).type;
        lookupSymbol(leftImage).referenceCount++;

        auto combineType = mAssignmentConversionTable[TypePair(leftType, right->getType())];

        if (combineType == ast::Type::Types::Undefined) {
            std::cout << "!!!  Error: Invalid types used in an assignment " << leftImage << " := " << "(...)\n";
        }
    } else {
        // left is undeclared
        // caught when the node is visited
    }

    // set converted type
    right->setConvertedType(lookupSymbol(leftImage).type);
}

void TypeCheckVisitor::visit(ast::Call* c) {
    // std::cout << "visited call node\n";

    auto funcName = c->getSymbol()->getImage();
    int paramsNumber = c->getChildren().size() - 1; // subtract the symbol node

    // visit children to derive types

    mFlagCanUseArrayUnsubscripted = true;
    for (auto node : c->getChildren()) {
        node->accept(*this);
    }
    mFlagCanUseArrayUnsubscripted = false;

    if (symbolExists(funcName)) {
        auto info = lookupSymbol(funcName);

        if (info.isProcedure) {
            // check if number of arguments/types are correct
            auto params = info.parameters;

            if (params != nullptr) {
                if (params->getChildren().size() != paramsNumber) {
                    std::cout << "!!!  Error: Incorrect number of arguments when calling " << funcName << ".\n";
                } else {
                    // number of arguments is correct. check their type.
                    bool typeError = false;
                    bool arrayError = false;
                    bool paramIsArray;
                    ast::Type::Types properType;
                    ast::Type::Types compareType; 
                    ast::Type* paramNode;
                    ast::Variable* compareVariable;
                    int i;

                    for (i = 0; i < params->getChildren().size(); i++) {
                        paramNode = dynamic_cast<ast::Type*>(params->getChildren()[i]);
                        paramIsArray = (dynamic_cast<ast::Array*>(paramNode->getChildren()[0]));
                        properType = paramNode->getType();
                        compareType = dynamic_cast<ast::Expression*>(c->getChildren()[i+1])->getType();
                        compareVariable = dynamic_cast<ast::Expression*>(c->getChildren()[i+1])->childAsVariable();

                        if (properType != compareType) {
                            typeError = true;
                            break;
                        } else if (compareVariable != nullptr) {
                            // the argument is simply a symbol.
                            if (symbolExists(compareVariable->getSymbol()->getImage())) {
                                if (lookupSymbol(compareVariable->getSymbol()->getImage()).isArray != paramIsArray) {
                                    arrayError = true;
                                }
                            } else {
                                // symbol does not exist. this will be caught when it is visited.
                            }
                        }
                    }

                    if (typeError) {
                        std::cout << "!!!  Error: Incorrect type of arguments when calling " << funcName 
                            << " (" << ast::typeToString(properType) << " != " << ast::typeToString(compareType) 
                            << ", argument " << i+1 << ").\n";
                    } else if (arrayError) {
                        if (paramIsArray) {
                            std::cout << "!!!  Error: invalid use of an array: Parameter number " << i 
                                << " in function/procedure " << c->getSymbol()->getImage() 
                                << " is an array, but was passed a non-array type.\n";
                        } else {
                            std::cout << "!!!  Error: invalid use of an array: Parameter number " << i 
                                << " in function/procedure " << c->getSymbol()->getImage() 
                                << " is not an array, but was passed an array type.\n";
                        }
                    }
                }
            } else {
                // params = nullptr. this means the procedure accepts no arguments.
                if (paramsNumber != 0) {
                    std::cout << "!!!  Error: Incorrect type of arguments when calling " << funcName << "!\n";
                }
            }
        } else {
            std::cout << "!!!  Error: Symbol \"" << funcName << "\" is not callable!\n";
        }
    } else {
        std::cout << "!!!  Error: Tried to call a function/procedure named \"" << funcName << "\", which was never declared.\n";
    }    
}

void TypeCheckVisitor::visit(ast::CaseLabels* cl) {
    // std::cout << "visited case labels node.\n";
    for (auto node : cl->getChildren()) {
        node->accept(*this);

        auto constant = dynamic_cast<ast::Constant*>(node);
        if (constant->getType() != mCastType) {
            std::cout << "!!!  Error: Constant \"" << constant->getImage() 
            << "\" is incorrect type in regards to what is being evaluated by the case statement.\n";
        }
    }
}

void TypeCheckVisitor::visit(ast::Case* c) {
    // std::cout << "visited a case node.\n";

    auto expr = dynamic_cast<ast::Expression*>(c->getChildren()[0]);
    // visit to derive type
    expr->accept(*this);
    mCastType = expr->getType();

    for (int i = 1; i < c->getChildren().size(); i++) {
        auto node = c->getChildren()[i];
        node->accept(*this);
    }

    mCastType = ast::Type::Types::Undefined;
}

void TypeCheckVisitor::visit(ast::Clause* c) {
    // std::cout << "visited a clause node.\n";
    for (auto node : c->getChildren()) {
        node->accept(*this);
    }
}

void TypeCheckVisitor::visit(ast::Constant* c) {
    // std::cout << "visited constant \"" << c->getImage() << "\" (" << ast::typeToString(c->getType()) << ").\n";
}

void TypeCheckVisitor::visit(ast::Expression* e) {
    // std::cout << "Visited expression node.\n";

    auto children = e->getChildren();

    if (e->getOperation() == ast::Expression::Operation::Not) {
        // special behavior for not
        // not is only valid on integers and produces an integer
        auto childExp = dynamic_cast<ast::Expression*>(e->getChildren()[0]);

        // visit to determine type
        childExp->accept(*this);

        if (childExp->getType() == ast::Type::Types::Integer) {
            e->setType(ast::Type::Types::Integer);
        } else if (childExp->getType() == ast::Type::Types::Undefined) {
            e->setType(ast::Type::Types::Undefined);
        } else {
            e->setType(ast::Type::Types::Undefined);
            std::cout << "!!!  Error: NOT operation only allowed in integers.\n";
        }

    } else if (e->getOperation() != ast::Expression::Operation::Noop) {
        // the expression node is some kind of binary expression.
        
        auto left = dynamic_cast<ast::Expression*>(e->getChildren()[0]);
        auto right = dynamic_cast<ast::Expression*>(e->getChildren()[1]);

        left->accept(*this);
        right->accept(*this);

        auto leftType = left->getType();
        auto rightType = right->getType();

        auto myType = getCombinedType(leftType, rightType, e->getOperation());
        e->setType(myType);

        using OPER = ast::Expression::Operation;
        auto myOp = e->getOperation();
        if (
            myOp != OPER::LessThan &&
            myOp != OPER::LessThanOrEqual &&
            myOp != OPER::GreaterThan &&
            myOp != OPER::GreaterThanOrEqual &&
            myOp != OPER::Equals &&
            myOp != OPER::NotEqual &&
            myOp != OPER::And &&
            myOp != OPER::Or
        ) {
            left->setConvertedType(myType);
            right->setConvertedType(myType);
        } else {
            // operations don't require coverted types
            left->setConvertedType(left->getType());
            right->setConvertedType(right->getType());
        }


        if (myType == ast::Type::Types::Undefined) {
            std::cout << "!!!  Error: Incompatable types in expression.\n";
        }

    } else {
        // the expression has a single child which will determine its type.

        if (auto constant = dynamic_cast<ast::Constant*>(e->getChildren()[0])) {
            // std::cout << "  Constant detected.\n";
            e->setType(constant->getType());
            // std::cout << "  Set type as: " << ast::typeToString(e->getType()) << "\n";
        } else if (auto var = dynamic_cast<ast::Variable*>(e->getChildren()[0])) {
            // std::cout << "  Variable detected.\n";
            std::string symImg = var->getSymbol()->getImage();

            if (symbolExists(symImg)) {
                ast::Type::Types t;
                lookupSymbol(symImg).referenceCount++;
                t = lookupSymbol(symImg).type;
                e->setType(t);
            } else {
                // variable never declared error
                // will be caught when the variable node gets visited
                e->setType(ast::Type::Types::Undefined);
            }

        } else if (auto call = dynamic_cast<ast::Call*>(e->getChildren()[0])) {
            // std::cout << "  Call detected.\n";
            std::string callImg = call->getSymbol()->getImage();

            if (symbolExists(callImg)) {
                ast::Type::Types t;
                t = lookupSymbol(callImg).type;

                if (t == ast::Type::Types::Void) {
                    std::cout << "!!!  Error: symbol \"" << call->getSymbol()->getImage() << "\" is a procedure, but was used as a function.\n";
                    e->setType(ast::Type::Types::Undefined);
                } else {
                    e->setType(t);
                }

            } else {
                // function not declared error
                // will be caught when the call node gets visited
                e->setType(ast::Type::Types::Undefined);
            }
        }

        // visit the child
        e->getChildren()[0]->accept(*this);
    }
}

void TypeCheckVisitor::visit(ast::If* i) {
    // std::cout << "visited an if node.\n";
    for (auto node : i->getChildren()) {
        if (node != nullptr) {
            node->accept(*this);
        }
    }
}

void TypeCheckVisitor::visit(ast::Procedure* p) {
    // std::cout << "Visited a procedure node.\n";

    // create new symbol table
    pushNewSymbolTable();

    // visit the symbol
    p->getSymbol()->accept(*this);

    // process parameters
    if (p->getParameters()) { // exists
        p->getParameters()->accept(*this);
    } else {
        // std::cout << "procedure \"" << p->getSymbol()->getImage() << "\" has no parameters.\n";
    }

    // process declarations
    if (p->getDecl()) { // exists
        p->getDecl()->accept(*this);
    } else {
        // std::cout << "procedure \"" << p->getSymbol()->getImage() << "\" has no decls.\n";
    }

    mFlagFoundReturnStatement = false;
    mReturnedType = ast::Type::Types::Void;
    // process compound statement
    if (p->getCompoundStatement()) { // exists
        p->getCompoundStatement()->accept(*this);
    } else {
        // std::cout << "procedure \"" << p->getSymbol()->getImage() << "\" has no compound statement.\n";
    }

    auto funcInfo = lookupSymbol(p->getSymbol()->getImage());
    if (funcInfo.type == ast::Type::Types::Void && mFlagFoundReturnStatement == true) {
        std::cout << "!!!  Error: return statement in procedure " << funcInfo.name << ".\n";
    } else if (funcInfo.type != ast::Type::Types::Void && mFlagFoundReturnStatement == false) {
        std::cout << "!!!  Error: no return statement in function " << funcInfo.name << ".\n";
    }

    if (funcInfo.type != mReturnedType) {
        std::cout << "!!!  Error: function " << funcInfo.name << " returns incorrect type.\n";
    }

    mFlagFoundReturnStatement = false;
    mReturnedType = ast::Type::Types::Void;

    // dump tables
    // std::cout << "Tables after processing procedure " << p->getSymbol()->getImage() << ":\n";
    // dumpTable();

    // remove symbol table
    popSymbolTable();
}

void TypeCheckVisitor::visit(ast::Return* r) {
    // std::cout << "vistied return node.\n";

    mFlagFoundReturnStatement = true;
    r->getChildren()[0]->accept(*this);

    auto expChild = dynamic_cast<ast::Expression*>(r->getChildren()[0]);
    mReturnedType = expChild->getType();
}

void TypeCheckVisitor::visit(ast::Statement* s) {
    // std::cout << "visted a statement node.\n";
}

void TypeCheckVisitor::visit(ast::Variable* v) {
    // std::cout << "visited a variable node.\n";

    for (auto node : v->getChildren()) {
        node->accept(*this);
    }
}

void TypeCheckVisitor::visit(ast::ArrayAccess* aa) {
    // std::cout << "visited an array access node.\n";

    auto symbolStr = aa->getSymbol()->getImage();

    if (symbolExists(symbolStr)) {
        auto& info = lookupSymbol(symbolStr);

        // check if symbol is subscriptable in the first place
        if (info.isArray == true) {
            // check if index is within declared bounds, if we can
            auto expr = aa->getExpression();
            if (auto constant = dynamic_cast<ast::Constant*>(expr->getChildren()[0])) {
                auto idx = constant->getImage();

                try {
                    // check if bounds is a string or a number
                    int low = std::stoi(info.arrayLowBound);
                    int high = std::stoi(info.arrayHighBound);
                    int iidx = std::stoi(constant->getImage());

                    if (iidx < low || iidx > high) {
                        std::cout << "!!!  Error: Array index \"" << idx << "\" is out of range of array \"" << symbolStr << "\".\n";
                    }
                } catch (std::invalid_argument) {
                    // it's characters
                    if (idx < info.arrayLowBound || idx > info.arrayHighBound) {
                        std::cout << "!!!  Error: Array index \"" << idx << "\" is out of range of array \"" << symbolStr << "\".\n";
                    }
                }

                // no errors. increase ref count
                info.referenceCount++;
            }
        } else {
            std::cout << "!!!  Error: Symbol \"" << symbolStr << "\" is not subscriptable.\n";
        }
    } else {
        // symbol does not exist, this is caught when we visit the symbol node
    }

    // set array access flag
    mFlagCanUseArrayUnsubscripted = true;

    for (auto node : aa->getChildren()) {
        node->accept(*this);
    }

    mFlagCanUseArrayUnsubscripted = false;
}

void TypeCheckVisitor::visit(ast::While* w) {
    // std::cout << "visited a while node.\n";
    for (auto node : w->getChildren()) {
        node->accept(*this);
    }
}

void TypeCheckVisitor::visit(ast::Read* r) {
    for (auto node : r->getChildren()) {
        node->accept(*this);
    }
}

void TypeCheckVisitor::visit(ast::Write* w) {
    // std::cout << "visited a write node.\n";
    for (auto node : w->getChildren()) {
        node->accept(*this);
    }
}

////////////////////////////////////////////////////////////////////////////////

void TypeCheckVisitor::dumpTable() {
    std::cout << "Symbol table dump (topmost table writen first):\n";
    
    for (auto it = mSymbolTableStack.rbegin(); it != mSymbolTableStack.rend(); ++it) {
        std::cout << "--------- NEW TABLE --------\n";
        auto& table = *it;
        for (auto pair : table) {
            pair.second.dumpInfo();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

void SymbolInfo::dumpInfo() {
    std::cout 
        << name << std::endl
        << "  Type: " << ast::typeToString(type) << std::endl;
    ;

    if (isProcedure) {
        std::string paramString = "(";

        if (parameters != nullptr) {
            for (int i = 0; i < parameters->getChildren().size(); i++) {
                auto type = dynamic_cast<ast::Type*>(parameters->getChildren()[i]);
                if (i == 0) {
                    paramString += ast::typeToString(type->getType());
                } else {
                    paramString += ", " + ast::typeToString(type->getType());
                }
            }
        }

        paramString += ")";

        std::cout << "  Procedure " << paramString << std::endl;
    }

    if (isArray) {
        std::cout << "  Array: " << arrayLowBound << " .. " << arrayHighBound << std::endl;
    }
    
    std::cout << "  Referenced " << referenceCount << " times.\n";
}