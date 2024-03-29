#include <iostream>
#include <string>

#include <parser/nolife_parser.hpp>
#include <parser/astnode.hpp>

#include "parser/visitor.hpp"
#include "parser/visitorprinter.hpp"
#include "parser/visitortypechecker.hpp"

extern FILE* yyin;

extern ast::Base* gASTRoot;

int main(int argc, char *argv[]) {

    if (argc < 2) {
        std::cout << "Usage: ./nlc <.nl file>\n";
        return -1;
    }

    FILE* file = fopen(argv[1], "r");

    if (file) {
        yyin = file;
    } else {
        std::cout << "Error opening file " << argv[1] << "!\n";
    }

    // hack to disable cout temporarily
    std::cout.setstate(std::ios_base::failbit);

    yyparse(); // sets gASTRoot to the root of the ast.

    std::cout.clear();

    PrintVisitor p;
    TypeCheckVisitor t;

    gASTRoot->accept(p);
    gASTRoot->accept(t);
}
