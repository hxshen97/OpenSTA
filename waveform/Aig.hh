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

#pragma once

#include <vector>
#include <unordered_map>
#include <functional>
#include "sta/FuncExpr.hh"
#include "sta/Liberty.hh"

namespace sta {

// Complement bit: MSB of size_t indicates inversion on an AIG edge.
const size_t AIG_COMPLEMENT = size_t(1) << (sizeof(size_t) * 8 - 1);

inline size_t aigEdgeIndex(size_t edge) { return edge & ~AIG_COMPLEMENT; }
inline bool aigEdgeIsComplement(size_t edge) { return (edge & AIG_COMPLEMENT) != 0; }
inline size_t aigMakeEdge(size_t index, bool complement) {
  return complement ? (index | AIG_COMPLEMENT) : index;
}
inline size_t aigEdgeComplement(size_t edge) {
  return aigMakeEdge(aigEdgeIndex(edge), !aigEdgeIsComplement(edge));
}

enum class AigNodeType { CONST, INPUT, AND };

struct AigNode {
  AigNodeType type;
  bool const_value;           // CONST only
  const LibertyPort *port;    // INPUT only
  size_t left;                // AND only: edge index into node vector
  size_t right;               // AND only: edge index into node vector
};

class Aig {
public:
  Aig();
  size_t root_edge;

  const std::vector<AigNode> &nodes() const { return nodes_; }
  size_t makeConst(bool value);
  size_t makeInput(const LibertyPort *port);
  size_t makeAnd(size_t left_edge, size_t right_edge);
  char evaluate(const std::unordered_map<const LibertyPort*, char> &values) const;

private:
  char evaluateNode(size_t node_idx, bool complement,
                    const std::unordered_map<const LibertyPort*, char> &values) const;
  char evaluateAnd(char left, char right) const;
  char invert(char value) const;

  std::vector<AigNode> nodes_;
  std::unordered_map<size_t, size_t> and_hash_;
  size_t andHashKey(size_t left, size_t right) const;
};

Aig buildAig(const FuncExpr *expr);

} // namespace sta
