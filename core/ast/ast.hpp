#pragma once
#include "../merkle/sha256.hpp"
#include "../types/types.hpp"
#include "json.hpp"
#include <memory>
#include <string>
#include <vector>

namespace fve {

enum class Op {
    Add, Sub, Mul, Div, Neg, Pow, Sqrt, Log, Exp, Sin, Cos, Tan
};

enum class Kind { Const, Var, Oper, Call, Hole, Disj };

struct Node;
using Nodep = std::shared_ptr<const Node>;

struct Node {
    Kind kind;
    Op op = Op::Add;
    double val = 0.0;
    std::string sym;
    std::string fn;
    std::string hole;
    char regime = 'A';
    int arity = 0;
    Dim dim;
    Ival phys = Ival::entire();
    std::vector<Nodep> args;
    std::vector<Nodep> cands;
    Digest hash;
};

Nodep konst(double v, Dim dim = Dim::none());
Nodep var(const std::string& s, Dim dim = Dim::none(), Ival phys = Ival::entire());
Nodep oper(Op o, std::vector<Nodep> args);
Nodep call(const std::string& fn, std::vector<Nodep> args);
Nodep hole(const std::string& id, int arity, Ival phys, char regime,
           std::vector<Nodep> args);
Nodep disj(std::vector<Nodep> cands);

std::string op_name(Op o);
bool op_from_name(const std::string& s, Op& out);
int op_arity(Op o);

JsonP to_json(const Nodep& n);
Nodep from_json(const JsonP& j);
std::string serialize(const Nodep& n);
Nodep deserialize(const std::string& s);

struct Slot {
    Kind kind;
    Op op = Op::Add;
    double val = 0.0;
    std::string sym;
    std::string fn;
    std::string hole;
    std::vector<int> args;
    std::vector<int> cands;
    Digest hash;
};

std::vector<Slot> linearize(const Nodep& n);

}
