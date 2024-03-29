#pragma once

#include "aststmtnode.hpp"

namespace ast {

    class Binary : public Statement {
        public:
            Binary(Base* l, Base* r);

            Base* getLeft();
            Base* getRight();

            virtual void accept(Visitor &v);
        private:
    };

} // ast