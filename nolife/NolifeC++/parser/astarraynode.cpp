#include <iostream>

#include "astarraynode.hpp"
#include "astconstantnode.hpp"
#include "astsymnode.hpp"
#include "visitor.hpp"

namespace ast {

    Array::Array() 
    : Type::Type()
    {
        mKind = "array";

        // std::cout << "  Array created.\n";

        mChildren.resize(3);
    }

    Array* Array::clone() {
        auto clone = new Array();

        auto symbol = dynamic_cast<Symbol*>(mChildren[0]);
        auto min = dynamic_cast<Constant*>(mChildren[1]);
        auto max = dynamic_cast<Constant*>(mChildren[2]);

        if (symbol == nullptr) {
            // clone.setChild(nullptr)
        } else {
            clone->setSymbol(new Symbol(*symbol));
        }

        if (min == nullptr || max == nullptr) {
            std::cout << "Something is afoot in Array::clone()!\n";
        } else {
            clone->setBounds(new Constant(*min), new Constant(*max));
        }

        return clone;
    }

    Type::Types Array::getType() {
        return Type::Types::Undefined;
    }

    void Array::setSymbol(Symbol* s) {
        mChildren[0] = s;
    }
    
    Symbol* Array::getSymbol() {
        if (mChildren.size() > 0) {
            return dynamic_cast<Symbol*>(mChildren[0]);
        } else {
            return nullptr;
        }
    }

    void Array::setBounds(Constant* min, Constant* max) {
        mChildren[1] = min;
        mChildren[2] = max;
    }

    Symbol* Array::getLowBound() {
        if (mChildren.size() >= 2) {
            return dynamic_cast<Symbol*>(mChildren[1]);
        }
    }

    Symbol* Array::getHighBound() {
        if (mChildren.size() >= 2) {
            return dynamic_cast<Symbol*>(mChildren[2]);
        }
    }

    void Array::accept(Visitor &v) {
        v.visit(this);
    }

}