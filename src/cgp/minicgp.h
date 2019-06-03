#ifndef MINI_CGP_H
#define MINI_CGP_H

namespace cgp {

using NodeID = uint;
using FuncID = std::array<unsigned char, 4>; 

template <uint I, uint N, uint O, uint A>
struct Chromosome {
  using NodesIn = std::array<NodeID, A>;
  
  /// \return The slice corresponding to node i's connection ids
  NodesIn nodeConnections (uint i) const {
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
}

template <typename A>
struct Node {
  using F = double (*) (const std::array<double, A> &);
  
  F function;
  NodesIn connection;
  
  Node (const NodesIn &c, const F f) : connections(c), function(f) {}
};

template <uint I, uint N, uint O, uint A = 2>
struct CGP {
  Chromosome<I,N,O,A> chromosome;
  
  std::vector<Node<A>> nodes;
  std::array<double, I+N> data;
  
  using Inputs = std::array<double, I>;
  using Outputs = std::array<double, O>;
  
  void decode (void) {
    nodes.clear();
    
    std::array<bool, N> toEvaluate {false};
    
    for (uint i=0; i<O; i++)
      toEvaluate[chromosome.outputConnection(i)] = true;
    
    for (uint i=N-1; i>=0; i--) {
      if (toEvaluate[i]) {
        auto connections = chromosome.nodeConnections(i);
        for (NodeID c: connections) toEvaluate[c] = true;
        nodes.emplace_back(connections, functionsMap[i]);
      }
    }
  }
  
  void evaluate (const Inputs &inputs, const Outputs &outputs) {
    std::copy(inputs.begin(), inputs.end(), data.begin());

    std::array<double, A> nodeI;
    for (uint i=0; i<nodes.size(); i++) {
      const Node &n = nodes[i];
      
      for (int j=0; j<A; j++) nodeI[j] = data[n.connection[j]];
      data[i] = n.function(nodeI);
    }
    
    for (int i=0; i<O; i++)
      outputs[i] = data[chromosome.outputConnection(i)];
  }
};

namespace functions {
double add (const std::array<double, 2> &inputs);
double del (const std::array<double, 2> &inputs);
double mul (const std::array<double, 2> &inputs);
double div (const std::array<double, 2> &inputs);
double abs (const std::array<double, 2> &inputs);
double sqrt (const std::array<double, 2> &inputs);
double sq (const std::array<double, 2> &inputs);

double exp (const std::array<double, 2> &inputs);
double sin (const std::array<double, 2> &inputs);
double cos (const std::array<double, 2> &inputs);
double tanh (const std::array<double, 2> &inputs);

double rand (const std::array<double, 2> &inputs);

double one (const std::array<double, 2> &inputs);
double zero (const std::array<double, 2> &inputs);
double pi (const std::array<double, 2> &inputs);

double gauss (const std::array<double, 2> &inputs);
double step (const std::array<double, 2> &inputs);
double tanh (const std::array<double, 2> &inputs);
}

}

#endif
