#pragma once

#include "astnode.hpp"
#include "astsymnode.hpp"

namespace ast {

    class Variable : public Base {
        public:
            Variable(Symbol *sym);

            virtual void accept(Visitor &v);
            Symbol* getSymbol();
        private:
    };

} // ast