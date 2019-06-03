#ifndef MINI_CGP_H
#define MINI_CGP_H

#include "kgd/settings/configfile.h"
#include "kgd/utils/functions.h"

namespace cgp {
using FuncID = std::array<unsigned char, 4>;
} // end of namespace cgp

namespace config {

struct CONFIG_FILE(CGP) {
  using FunctionSet = std::set<cgp::FuncID>;
  DECLARE_PARAMETER(FunctionSet, functionSet)
};

} // end of namespace config

namespace cgp {

using NodeID = uint;

template <uint S>
using FuncInputs_t = std::array<double, S>;

template <uint S>
using UpstreamNodes_t = std::array<NodeID, S>;

template <uint I, uint N, uint O, uint A>
struct Chromosome_t {
  using UpstreamNodes = UpstreamNodes_t<A>;
  
  /// \return The slice corresponding to node i's connection ids
  UpstreamNodes nodeConnections (uint i) const {
    return NodesIn (connections.begin()+i*A,
                    connections.begin()+(i+1)*A-1);
  }
  
  NodeID outputConnection (uint i) const {
    return connections[N*A+i];
  }
  
  FuncID function (uint i) const {
    return functions[i];
  }
  
private:
  std::array<NodeID, N*A+O> connections;
  std::array<FuncID, N> functions;
};

template <uint A>
struct Node_t {
  using UpstreamNodes = UpstreamNodes_t<A>;
  using FuncInputs = FuncInputs_t<A>;
  using F = double (*) (const FuncInputs &);
  
  F function;
  UpstreamNodes connections;
  
  Node_t (const UpstreamNodes &c, const F f) : connections(c), function(f) {}
};

template <uint I, uint N, uint O, uint A = 3>
struct CGP {
  using Chromosome = Chromosome_t<I,N,O,A>;
  Chromosome chromosome;
  
  using Node = Node_t<A>;
  std::array<Node, N> nodes;
  std::array<NodeID, O> outputConnections;

  std::vector<Node&> usedNodes;

  std::array<double, I+N> data;

  static std::map<FuncID, typename Node::F> functionsMap;
  
  using Inputs = std::array<double, I>;
  using Outputs = std::array<double, O>;
  
  void clear (void) {
    data.fill(0);
  }

  void decode (void) {
    usedNodes.clear();
    
    std::array<bool, N> toEvaluate {false};
    
    for (uint i=0; i<O; i++)
      toEvaluate[outputConnections[i]] = true;
    
    for (uint i=N-1; i>=0; i--) {
      if (toEvaluate[i]) {
        Node &n = nodes[i];
        for (NodeID c: n.connections) toEvaluate[c] = true;
        usedNodes.push_back(n);
      }
    }
  }
  
  void evaluate (const Inputs &inputs, const Outputs &outputs) {
    std::copy(inputs.begin(), inputs.end(), data.begin());

    std::array<double, A> connections;
    for (uint i=0; i<nodes.size(); i++) {
      const Node &n = nodes[i];
      
      for (int j=0; j<A; j++) connections[j] = data[n.connection[j]];
      data[i] = n.function(connections);
    }
    
    for (int i=0; i<O; i++)
      outputs[i] = data[chromosome.outputConnection(i)];
  }
};


namespace functions {

namespace oary {
template <uint S>
double rand (const FuncInputs_t<S> &);

template <uint S>
double one (const FuncInputs_t<S> &) {  return 1;     }

template <uint S>
double zero (const FuncInputs_t<S> &) { return 0;     }

template <uint S>
double pi (const FuncInputs_t<S> &) {   return M_PI;  }
} // end of namespace oary

namespace unary {
template <uint S>
double id (const FuncInputs_t<S> &inputs) {
  return inputs[0];
}

template <uint S>
double abs (const FuncInputs_t<S> &inputs) {
  return std::fabs(inputs[0]);
}

template <uint S>
double sq (const FuncInputs_t<S> &inputs) {
  return inputs[0] * inputs[0];
}

template <uint S>
double sqrt (const FuncInputs_t<S> &inputs) {
  return std::sqrt(inputs[0]);
}

template <uint S>
double exp (const FuncInputs_t<S> &inputs) {
  return std::exp(inputs[0]);
}

template <uint S>
double sin (const FuncInputs_t<S> &inputs) {
  return std::sin(inputs[0]);
}

template <uint S>
double cos (const FuncInputs_t<S> &inputs) {
  return std::cos(inputs[0]);
}

template <uint S>
double tan (const FuncInputs_t<S> &inputs) {
  return std::tan(inputs[0]);
}

template <uint S>
double tanh (const FuncInputs_t<S> &inputs) {
  return std::tanh(inputs[0]);
}

template <uint S>
double step (const FuncInputs_t<S> &inputs) {
  return utils::sgn(inputs[0]);
}
} // end of namespace unary

namespace binary {
template <uint S>
double del (const FuncInputs_t<S> &inputs) {
  return inputs[0] - inputs[1];
}

template <uint S>
double div (const FuncInputs_t<S> &inputs) {
  if (inputs[1] != 0)
    return inputs[0] / inputs[1];
  return 0;
}
} // end of namespace binary

namespace ternay {
template <uint S>
double gauss (const FuncInputs_t<S> &inputs) {
  return utils::gauss(inputs[0], inputs[1], inputs[2]);
}
} // end of namespace ternary

namespace nary {
template <uint S>
double add (const FuncInputs_t<S> &inputs) {
  double sum = 0;
  for (auto i: inputs)  sum += i;
  return sum;
}

template <uint S>
double mult (const FuncInputs_t<S> &inputs) {
  double prod = 0;
  for (auto i: inputs)  prod *= i;
  return prod;
}
} // end of namespace nary

} // end of namespace functions

template <uint I, uint N, uint O, uint A>
std::map<FuncID, typename Node_t<A>::F> CGP<I,N,O,A>::functionsMap {

};

} // end of namespace cgp

#endif
