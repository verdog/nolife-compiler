#pragma once

#include <string>

#include "visitor.hpp"

class PrintVisitor : public Visitor {
    public:
        PrintVisitor();
        
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
        void visit(ast::Write* w);
        void visit(ast::Read* r);
    private:
        void visit_universal(ast::Base* b);

        std::string mIndentString;
        std::string mIndentUnit;
};