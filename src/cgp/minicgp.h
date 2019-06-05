#ifndef MINI_CGP_H
#define MINI_CGP_H

#include <iomanip>
#include <list>

#include "kgd/settings/configfile.h"
#include "kgd/utils/functions.h"

namespace cgp {
namespace functions {
using ID = std::array<unsigned char, 4>;

constexpr ID FID (const std::string_view &s) {
  if (std::tuple_size<ID>::value < s.size())
    utils::doThrow<std::invalid_argument>("Function name '", s, "' is too long");

  ID fid {};
  uint i = 0;
  for (; i<fid.size() - s.size(); i++) fid[i] = '_';
  for (; i<fid.size(); i++) fid[i] = s[i-fid.size()+s.size()];

  return fid;
}

std::ostream& operator<< (std::ostream &os, const ID &id);
std::istream& operator>> (std::istream &os, ID &id);

} // end of namespace functions

using FuncID = functions::ID;
} // end of namespace cgp

namespace config {

struct CONFIG_FILE(CGP) {
  using FunctionSet = std::set<cgp::FuncID>;
  DECLARE_PARAMETER(FunctionSet, functionSet)
};

} // end of namespace config

namespace cgp {

using NodeID = uint;

enum NodeType { IN, INTERNAL, OUT };

template <uint S>
using FuncInputs_t = std::array<double, S>;

template <uint S>
using UpstreamNodes_t = std::array<NodeID, S>;

template <uint A>
struct Node_t {
  using UpstreamNodes = UpstreamNodes_t<A>;
  using FInputs = FuncInputs_t<A>;
  using F = std::function<double(const FInputs &)>;
  
  UpstreamNodes connections;

  FuncID fid;
  F function;
  
  Node_t (void) : Node_t({}, functions::FID("!!!!"), nullptr) {}
  Node_t (const UpstreamNodes &c, FuncID id, const F f)
    : connections(c), fid(id), function(f) {}


  friend void assertEqual (const Node_t &lhs, const Node_t &rhs) {
    using utils::assertEqual;
    assertEqual(lhs.connections, rhs.connections);
    assertEqual(lhs.fid, rhs.fid);
  }
};

template <typename IEnum, uint N, typename OEnum, uint A = 2>
struct CGP {
  using IUtils = EnumUtils<IEnum>;
  static constexpr uint I = IUtils::size();

  using OUtils = EnumUtils<OEnum>;
  static constexpr uint O = OUtils::size();

  using Node = Node_t<A>;
  using Nodes = std::array<Node, N>;
  Nodes nodes;

  using UpstreamNodes = std::array<NodeID, O>;
  UpstreamNodes outputConnections;

  std::vector<double> persistentData;
  std::map<NodeID, uint> usedNodes; /// Maps into node id into data

  using Function = typename Node::F;
  using FunctionsMap = std::map<FuncID, Function>;
  static FunctionsMap functionsMap; // Stateless functions
  FunctionsMap localFunctionsMap;   // Context-dependent (e.g. rand)
  
  static std::map<FuncID, uint> arities;

  rng::FastDice ldice;

  using Inputs = std::array<double, I>;
  using Outputs = std::array<double, O>;
  
  void clear (void) {
    std::fill(data.begin(), data.end(), 0);
  }

  void prepare (void) {
    usedNodes.clear();
    uint d = 0;
    
    std::array<bool, N> toEvaluate {false};
    
    for (uint i=0; i<O; i++)
      toEvaluate[outputConnections[i]] = true;
    
    for (int i=N-1; i>=0; i--) {
      if (toEvaluate[i]) {
        Node &n = nodes[i];
        for (NodeID c: n.connections) toEvaluate[c] = true;
        usedNodes[i] = I+d++;
      }
    }

    persistentData.resize(I+usedNodes.size(), 0);
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
      outputs[i] = data[outputConnections[i]];
  }

  CGP clone (void) const {
    return CGP(*this);
  }

  void mutate (rng::AbstractDice &dice) {

  }

  friend std::ostream& operator<< (std::ostream &os, const CGP &cgp) {
    auto flags = os.flags();
    const auto pad = [] { return std::setw(4); };

    os << "CGP {\n\t" << std::setfill(' ');
    for (IEnum i: IUtils::iterator()) os << pad() << IUtils::getName(i) << " ";
    os << "\n\t" << std::setfill('0');
    for (uint i=0; i<I; i++)  os << pad() << i << " ";
    os << "\n\n\tFunc In...\n";
    for (uint i=0; i<N; i++) {
      os << pad() << I+i << "\t";
      const Node &n = cgp.nodes[i];
      for (auto c: n.fid)  os << c;
      for (uint i=0; i<A; i++)
        os << " " << pad() << n.connections[i];
      os << "\n";
    }
    os << "\n\t" << std::setfill(' ');
    for (OEnum o: OUtils::iterator()) os << pad() << OUtils::getName(o) << " ";
    os << "\n\t" << std::setfill('0');
    for (NodeID nid: cgp.outputConnections)
      os << pad() << nid << " ";
    os << "\n}" << std::endl;

    os.flags(flags);
    return os;
  }

  /// FIXME Far from working
  std::string toTex (void) const {
    std::ostringstream oss;
    oss << "\\begin{align*}\n";

    for (uint o=0; o<O; o++) {
      std::set<NodeID> inputs;
      oss << OUtils::getName(o);

      std::ostringstream oss2;
      std::list<NodeID> pending { outputConnections[o] };
      while (!pending.empty()) {
        NodeID nid = pending.front();
        const Node &node = nodes[nid-I];
        pending.pop_front();

        using functions::operator<<;
        oss2 << node.fid;

        std::cerr << "Processing " << nid << " (" << node.fid << ") " << std::endl;

        int a = std::min(A, arities.at(node.fid));
        if (a > 0) {
          oss2 << "(";
          for (int i=a-1; i>=0; i--) {
            std::cerr << "\tNeed to see " << node.connections[i] << std::endl;
            pending.push_front(node.connections[i]);
            if (i < a)  oss2 << ", ";
          }
        }
      }

      if (!inputs.empty()) {
        oss << "(" << *inputs.begin();
        for (auto it = std::next(inputs.begin()); it != inputs.end(); ++it)
          oss << ", " << *it;
        oss << ") ";
      }

      oss << " &= " << oss2.str() << " \\\\\n";
    }

    oss << "\\end{align*}\n";
    return oss.str();
  }

  enum DotOptions {
    DEFAULT = 0,
    FULL = 1<<1,
    NO_ARITY = 1<<2,
    SHOW_DATA = 1<<3
  };
  friend DotOptions operator| (DotOptions lhs, DotOptions rhs) {
    return DotOptions(int(lhs) | int(rhs));
  }

  void toDot (const stdfs::path &outfile, DotOptions options = DEFAULT) const {

    auto ext = outfile.extension().string().substr(1);
    auto dotfile = outfile;
    if (ext != "dot")
      dotfile.replace_extension("dot");

    std::ofstream ofs (dotfile);
    if (!ofs) {
      std::cerr << "Failed to open '" << dotfile << "' for writing in "
                << __PRETTY_FUNCTION__ << std::endl;
      return;
    }

    ofs << "digraph {\n";

    bool d = !(options & DotOptions::SHOW_DATA);

    for (auto i: IUtils::iteratorUValues())
      printDotNodeOrIO(ofs, i, IUtils::getName(i), data<IN>(i, d));

    ofs << "\t{rank=source";
    for (uint i=0; i<I; i++)  ofs << " " << i;
    ofs << "}\n\n";

    if (options & DotOptions::FULL) {
      for (uint n=0; n<N; n++) {
        uint i = I+n;
        const Node &node = nodes[n];
        printDotNode(ofs, i, node.fid, data<INTERNAL>(i, d));
        uint a = A;
        if (!(options & DotOptions::NO_ARITY))
          a = std::min(a, arities.at(node.fid));
        for (uint j=0; j<a; j++)
          ofs << "\t" << node.connections[j] << " -> " << i << ";\n";
      }
      for (uint o=0; o<O; o++)
        ofs << "\t" << outputConnections[o] << " -> " << I+N+o << ";\n";

    } else { // only print used nodes
      std::set<NodeID> seen, pending;
      for (uint o=0; o<outputConnections.size(); o++) {
        NodeID nid = outputConnections[o];
        pending.insert(nid);
        ofs << "\t" << nid << " -> " << o+N+I << ";\n";
      }
      while (!pending.empty()) {
        NodeID nid = *pending.begin();
        pending.erase(pending.begin());
        if (seen.find(nid) != seen.end())
          continue;

        seen.insert(nid);
        if (I <= nid) {
          uint i = nid-I;
          const Node &n = nodes[i];
          printDotNode(ofs, nid, n.fid, data<INTERNAL>(nid, d));
          uint a = A;
          if (!(options & DotOptions::NO_ARITY))
            a = std::min(a, arities.at(n.fid));
          for (uint j=0; j<a; j++) {
            NodeID c = n.connections[j];
            pending.insert(c);
            ofs << "\t" << c << " -> " << nid << "\n";
          }
        }
      }
    }
    ofs << "\n";

    ofs << "\t0 -> " << I+N << " [style=invis];\n\n";

    for (auto o: OUtils::iteratorUValues())
      printDotNodeOrIO(ofs, I+N+o, OUtils::getName(o), data<OUT>(o, d));

    ofs << "\t{rank=sink";
    for (uint o=0; o<O; o++)  ofs << " " << I+N+o;
    ofs << "}\n";

    ofs << "}" << std::endl;

    if (ext != "dot") {
      std::ostringstream oss;
      oss << "dot -T" << ext << " -o " << outfile << " " << dotfile;
      std::string cmd = oss.str();
      auto res = system(cmd.c_str());
      if (res == 0)
        std::cout << "Saved cgp to " << dotfile << " and rendering to "
                  << outfile << std::endl;
      else
        std::cerr << "Executing '" << cmd << "' resulted in error code " << res
                  << std::endl;
    }
  }

  static CGP null (rng::AbstractDice &dice) {
    CGP cgp (dice);
    FuncID fid = functions::FID("zero");
    cgp.nodes[0].fid = fid;
    cgp.nodes[0].function = functionsMap.at(fid);
    for (NodeID &o: cgp.outputConnections)  o = I;
    cgp.prepare();
    return cgp;
  }

  friend void assertEqual (const CGP &lhs, const CGP &rhs) {
    using utils::assertEqual;
    assertEqual(lhs.nodes, rhs.nodes);
    assertEqual(lhs.outputConnections, rhs.outputConnections);
    assertEqual(lhs.persistentData, rhs.persistentData);
    assertEqual(lhs.usedNodes, rhs.usedNodes);
//    assertEqual(lhs.localFunctionsMap, rhs.localFunctionsMap);
    assertEqual(lhs.ldice, rhs.ldice);
  }

private:
  CGP (void) = delete;

  CGP (rng::AbstractDice &dice) {
    using functions::FID;
    localFunctionsMap[FID("rand")] = [this] (auto) { return ldice(-1., 1.); };

    nodes = make_array<Node, N>([this, &dice] (auto i) {
      typename Node::UpstreamNodes connections =
        make_array<NodeID, A>([&dice, i] (auto) {
          return dice(0u, uint(I+i-1));
        });

      FuncID fid = *dice(config::CGP::functionSet());
      Function function = functionfromID(fid);

      return Node(connections, fid, function);

    });

    outputConnections = make_array<NodeID, O>([&dice] (auto) {
      return dice(0u, I+N-1);
    });
  }

  template <NodeType T> double data (uint i, bool nanOnly) const {
    if (nanOnly)  return NAN;
    else  return data<T>(i);
  }

  template <NodeType T> auto data (uint i) const {
    return data_helper(i, std::integral_constant<NodeType, T>());
  }

  auto data_helper (uint i, std::integral_constant<NodeType, IN>) const {
    return persistentData[i];
  }

  double data_helper (uint i, std::integral_constant<NodeType, INTERNAL>) const {
    auto it = usedNodes.find(i);
    if (it != usedNodes.end())
      return persistentData[it->second];
    return NAN;
  }

  auto data_helper (uint i, std::integral_constant<NodeType, OUT>) const {
    return data<INTERNAL>(outputConnections[i]);
  }

  Function functionfromID (const FuncID &fid) const {
    auto lit = localFunctionsMap.find(fid);
    if (lit != localFunctionsMap.end()) return lit->second;

    auto rit = functionsMap.find(fid);
    if (rit != functionsMap.end())      return rit->second;

    utils::doThrow<std::invalid_argument>("Unknown function ", fid);
    return nullptr;
  }

  static void printDotNodeOrIO (std::ostream &os, NodeID id,
                                const std::string &data, double value = NAN) {
    os << "\t" << id << " [shape=none, label=<"
        << "<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\""
                 " VALIGN=\"MIDDLE\">"
         << "<TR><TD><FONT POINT-SIZE=\"8\"><I>\\N</I></FONT></TD>"
            "<TD><B>" << data << "</B></TD></TR>";
    if (!std::isnan(value))
      os << "<TR><TD COLSPAN=\"2\">"
             "<FONT POINT-SIZE=\"10\">" << value << "</FONT>"
            "</TD></TR>";
    os << "</TABLE>>];\n";
  }

  static void printDotNode (std::ostream &os, int id, const FuncID &fid,
                            double value) {
    std::string fidStr (fid.begin(), fid.end());
    fidStr.erase(std::remove(fidStr.begin(), fidStr.end(), '_'), fidStr.end());
    printDotNodeOrIO(os, id, fidStr, value);
  }

  template <typename T, typename F, size_t... Is>
  static auto make_array (F f, std::index_sequence<Is...>)
    -> std::array<T, sizeof...(Is)> {
    return {{f(std::integral_constant<std::size_t, Is>{})...}};
  }

  template <typename T, size_t COUNT, typename F>
  static auto make_array (F f) {
    return make_array<T>(f, std::make_index_sequence<COUNT>{});
  }
};


namespace functions {

namespace oary {
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

template <uint S>
double hgss (const FuncInputs_t<S> &inputs) {
  return utils::gauss(inputs[0], 0., inputs[1]);
}
} // end of namespace binary

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


template <typename IE, uint N, typename OE, uint A>
std::map<FuncID, typename Node_t<A>::F> CGP<IE,N,OE,A>::functionsMap {
#define FUNC(X) std::make_pair(functions::FID(#X), &functions::ARITY::X<A>)
#define ARITY oary
//  FUNC(rand)
  FUNC(one),  FUNC(zero), FUNC(pi),
#undef ARITY

#define ARITY unary
  FUNC(id),   FUNC(abs),  FUNC(sq),   FUNC(sqrt), FUNC(exp),  FUNC(sin),
  FUNC(cos),  FUNC(tan),  FUNC(step),
#undef ARITY

#define ARITY binary
  FUNC(del),  FUNC(div),  FUNC(hgss),
#undef ARITY

#define ARITY nary
  FUNC(add),  FUNC(mult)
#undef ARITY
#undef FUNC
};


template <typename IE, uint N, typename OE, uint A>
std::map<FuncID, uint> CGP<IE,N,OE,A>::arities {
#define AR(X) std::make_pair(functions::FID(#X), ARITY)
#define ARITY 0
  AR(rand), AR(one),  AR(zero), AR(pi),
#undef ARITY
#define ARITY 1
  AR(id),   AR(abs),  AR(sq),  AR(sqrt), AR(exp),  AR(sin),
  AR(cos),  AR(tan),  AR(step),
#undef ARITY
#define ARITY 2
  AR(del),  AR(div),  AR(hgss),
#undef ARITY
#define ARITY -1
  AR(add),  AR(mult)
#undef ARITY
#undef AR
};

} // end of namespace cgp

#endif
