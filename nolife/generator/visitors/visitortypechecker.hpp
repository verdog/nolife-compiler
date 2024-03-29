#pragma once

#include <map>
#include <string>
#include <iostream>

#include "../parser/asttypenode.hpp"
#include "../parser/astexpressionnode.hpp"

#include "visitor.hpp"

class SymbolInfo {
    public:
        SymbolInfo() {
            name = "";
            type = ast::Type::Types::Undefined;
            isArray = false;
            isProcedure = false;
            arrayLowBound = "";
            arrayHighBound = "";
            parameters = nullptr;
            referenceCount = 0;
        }

        SymbolInfo(std::string id) 
        : SymbolInfo()
        {
            name = id;
        }

        void dumpInfo();

        std::string name;
        ast::Type::Types type;
        bool isArray;
        bool isProcedure;
        std::string arrayLowBound;
        std::string arrayHighBound;
        bool isReal;
        ast::Parameters* parameters;
        int referenceCount;
};

class TypeCheckVisitor : public Visitor {
    friend class CodeGeneratorVisitor;
    public:
        TypeCheckVisitor();

        void visit(ast::Base* b);
        void visit(ast::Program* p);
        void visit(ast::Declaration* d);
        void visit(ast::CompoundStatement* cs);
        void visit(ast::Parameters* p);
        void visit(ast::Symbol* s);
        void visit(ast::Type* t);
        void visit(ast::Integer* i);
        void visit(ast::Float* f);
        void visit(ast::Character* c);
        void visit(ast::Void* v);
        void visit(ast::ArrayAccess* aa);
        void visit(ast::Array* a);
        void visit(ast::Assignment* a);
        void visit(ast::Call* c);
        void visit(ast::CaseLabels* cl);
        void visit(ast::Case* c);
        void visit(ast::Clause* c);
        void visit(ast::Constant* c);
        void visit(ast::Expression* e);
        void visit(ast::If* i);
        void visit(ast::Procedure* p);
        void visit(ast::Return* r);
        void visit(ast::Statement* s);
        void visit(ast::Variable* v);
        void visit(ast::While* w);
        void visit(ast::Read* r);
        void visit(ast::Write* w);

        void visit_type(ast::Type* t);

        void pushNewSymbolTable();
        void popSymbolTable();
        void dumpTable();

        void registerProcedures(ast::Declaration* d);
        
        ast::Type::Types getCombinedType(ast::Type::Types t1, ast::Type::Types t2, ast::Expression::Operation op);
    private:
        using tSymbolTable = std::map<std::string, SymbolInfo>;
        std::vector<tSymbolTable> mSymbolTableStack;

        using tTypeCompatibilityTable = std::map<std::pair<ast::Type::Types, ast::Type::Types>, ast::Type::Types>;

        tTypeCompatibilityTable mAssignmentConversionTable;
        
        std::map<ast::Expression::Operation, tTypeCompatibilityTable> mOpToTableMap;

        void writeSymbol(std::string key, SymbolInfo value);
        SymbolInfo& lookupSymbol(std::string key);
        bool symbolExists(std::string key);

        // special usage cases
        ast::Type::Types mCastType;
        ast::Type::Types mReturnedType;

        // flags
        bool mFlagCanUseArrayUnsubscripted;
        bool mFlagFoundReturnStatement;
};