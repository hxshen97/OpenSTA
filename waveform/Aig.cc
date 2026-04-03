// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
//
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
//
// This notice may not be removed or altered from any source distribution.

#include "Aig.hh"
#include <algorithm>

namespace sta {

Aig::Aig() : root_edge(0) {
  // Node 0 is constant false.
  nodes_.push_back({AigNodeType::CONST, false, nullptr, 0, 0});
}

size_t
Aig::makeConst(bool value) {
  if (!value)
    return 0; // Node 0 is const 0.
  return aigMakeEdge(0, true); // Const 1 = NOT(const 0).
}

size_t
Aig::makeInput(const LibertyPort *port) {
  size_t idx = nodes_.size();
  nodes_.push_back({AigNodeType::INPUT, false, port, 0, 0});
  return idx;
}

size_t
Aig::andHashKey(size_t left, size_t right) const {
  if (left > right)
    std::swap(left, right);
  return left ^ (right * 2654435761ULL);
}

size_t
Aig::makeAnd(size_t left_edge, size_t right_edge) {
  // Same signal: AND(a,a)=a, AND(a,!a)=0
  if (aigEdgeIndex(left_edge) == aigEdgeIndex(right_edge)) {
    if (aigEdgeIsComplement(left_edge) == aigEdgeIsComplement(right_edge))
      return left_edge;
    else
      return makeConst(false);
  }

  // Constant propagation: AND(0,x)=0, AND(1,x)=x
  size_t l = aigEdgeIndex(left_edge);
  size_t r = aigEdgeIndex(right_edge);
  if (l == 0 && !aigEdgeIsComplement(left_edge))
    return makeConst(false);
  if (r == 0 && !aigEdgeIsComplement(right_edge))
    return makeConst(false);
  if (l == 0 && aigEdgeIsComplement(left_edge))
    return right_edge;
  if (r == 0 && aigEdgeIsComplement(right_edge))
    return left_edge;

  // Structural hash dedup.
  size_t key = andHashKey(left_edge, right_edge);
  auto it = and_hash_.find(key);
  if (it != and_hash_.end())
    return it->second;

  size_t idx = nodes_.size();
  nodes_.push_back({AigNodeType::AND, false, nullptr, left_edge, right_edge});
  and_hash_[key] = idx;
  return idx;
}

Aig
buildAig(const FuncExpr *expr) {
  Aig aig;
  if (!expr) {
    aig.root_edge = aig.makeConst(false);
    return aig;
  }

  std::function<size_t(const FuncExpr*)> build = [&](const FuncExpr *e) -> size_t {
    switch (e->op()) {
    case FuncExpr::op_zero:
      return aig.makeConst(false);
    case FuncExpr::op_one:
      return aig.makeConst(true);
    case FuncExpr::op_port:
      return aig.makeInput(e->port());
    case FuncExpr::op_not: {
      size_t child = build(e->left());
      return aigMakeEdge(aigEdgeIndex(child), !aigEdgeIsComplement(child));
    }
    case FuncExpr::op_and:
      return aig.makeAnd(build(e->left()), build(e->right()));
    case FuncExpr::op_or: {
      // OR(a,b) = NOT(AND(NOT(a), NOT(b)))
      size_t l = build(e->left());
      size_t r = build(e->right());
      size_t nl = aigMakeEdge(aigEdgeIndex(l), !aigEdgeIsComplement(l));
      size_t nr = aigMakeEdge(aigEdgeIndex(r), !aigEdgeIsComplement(r));
      size_t and_edge = aig.makeAnd(nl, nr);
      return aigMakeEdge(aigEdgeIndex(and_edge), !aigEdgeIsComplement(and_edge));
    }
    case FuncExpr::op_xor: {
      // XOR(a,b) = OR(AND(a,!b), AND(!a,b))
      size_t l = build(e->left());
      size_t r = build(e->right());
      size_t nl = aigMakeEdge(aigEdgeIndex(l), !aigEdgeIsComplement(l));
      size_t nr = aigMakeEdge(aigEdgeIndex(r), !aigEdgeIsComplement(r));
      size_t a_and_nb = aig.makeAnd(l, nr);
      size_t na_and_b = aig.makeAnd(nl, r);
      // OR = NOT(AND(NOT, NOT))
      size_t na1 = aigMakeEdge(aigEdgeIndex(a_and_nb), !aigEdgeIsComplement(a_and_nb));
      size_t na2 = aigMakeEdge(aigEdgeIndex(na_and_b), !aigEdgeIsComplement(na_and_b));
      size_t final_and = aig.makeAnd(na1, na2);
      return aigMakeEdge(aigEdgeIndex(final_and), !aigEdgeIsComplement(final_and));
    }
    }
    return aig.makeConst(false);
  };

  aig.root_edge = build(expr);
  return aig;
}

char
Aig::invert(char value) const {
  switch (value) {
  case '0': return '1';
  case '1': return '0';
  default:  return 'X';
  }
}

char
Aig::evaluateAnd(char left, char right) const {
  if (left == '0' || right == '0')
    return '0';
  if (left == '1' && right == '1')
    return '1';
  return 'X';
}

char
Aig::evaluateNode(size_t node_idx, bool complement,
                  const std::unordered_map<const LibertyPort*, char> &values) const {
  const AigNode &node = nodes_[node_idx];
  char result;
  switch (node.type) {
  case AigNodeType::CONST:
    result = node.const_value ? '1' : '0';
    break;
  case AigNodeType::INPUT: {
    auto it = values.find(node.port);
    result = (it != values.end()) ? it->second : 'X';
    break;
  }
  case AigNodeType::AND: {
    char lv = evaluateNode(aigEdgeIndex(node.left), aigEdgeIsComplement(node.left), values);
    char rv = evaluateNode(aigEdgeIndex(node.right), aigEdgeIsComplement(node.right), values);
    result = evaluateAnd(lv, rv);
    break;
  }
  default:
    result = 'X';
    break;
  }
  return complement ? invert(result) : result;
}

char
Aig::evaluate(const std::unordered_map<const LibertyPort*, char> &values) const {
  return evaluateNode(aigEdgeIndex(root_edge), aigEdgeIsComplement(root_edge), values);
}

} // namespace sta
