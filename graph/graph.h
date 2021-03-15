#include "toml_helper.h"

namespace wrdk {
struct GraphNodeId {
  std::string function;
  std::string id;
  std::string idx = 0;
};

struct OptionalData {
  std::string cond;
  std::string cond_id;
  std::vector<std::string> data;
  WRDK_TOML_DEFINE_FIELDS(cond, cond_id, data)

  const char* GetFieldName(const char* v) {
    static std::map<std::string, std::string> __field_mapping__ = {{"cond", "if"},
                                                                   {"cond_id", "ifid"}};
    auto found = __field_mapping__.find(v);
    if (found != __field_mapping__.end()) {
      return found->second.c_str();
    }
    return v;
  }
};

struct GraphNode {
  std::string id;

  // phase node
  std::string phase;
  std::string args;

  // script func node
  std::string script;
  std::string function;

  // cond node
  std::string cond;
  std::set<std::string> consequent;
  std::set<std::string> alternative;

  // dependents or successors
  std::set<std::string> successor;
  std::set<std::string> dependents;
  // std::map<std::string, std::set<std::string>> optional_successor;

  std::vector<std::string> input;
  std::vector<std::string> output;
  std::vector<OptionalData> optional;  // optional input data

  const char* GetFieldName(const char* v) {
    static std::map<std::string, std::string> __field_mapping__ = {{"consequent", "if"},
                                                                   {"alternative", "else"}};
    auto found = __field_mapping__.find(v);
    if (found != __field_mapping__.end()) {
      return found->second.c_str();
    }
    return v;
  }

  std::string ToString() const;

  WRDK_TOML_DEFINE_FIELDS(id, phase, args, successor, dependents, script, function, cond,
                          consequent, alternative, input, output, optional)
};

struct GraphFunction {
  typedef std::map<std::string, GraphNode*> NodeTable;
  NodeTable _nodes;
  NodeTable _data_mapping_table;
  int64_t _idx = 0;

  std::string name;
  std::vector<GraphNode> nodes;

  WRDK_TOML_DEFINE_FIELDS(name, nodes)
  std::string GetNodeId(const std::string& id);
  int Init();
  int DumpDot(std::string& s);
};

struct GraphScript {
  std::string desc;
  std::vector<GraphFunction> function;
  typedef std::map<std::string, GraphFunction*> GraphFunctionTable;
  GraphFunctionTable _funcs;
  WRDK_TOML_DEFINE_FIELDS(desc, function)

  int Init();
  int DumpDot(std::string& s);
};
}  // namespace wrdk