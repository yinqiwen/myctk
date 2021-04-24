#include <stdint.h>
#include <tbb/concurrent_hash_map.h>
#include <tbb/concurrent_queue.h>
#include <atomic>
#include <boost/functional/hash.hpp>
#include <functional>
#include <memory>
#include <unordered_set>
#include "executor.h"
#include "toml_helper.h"
#include "vertex.h"

namespace didagle {

struct GraphCluster;
struct Graph {
  std::string name;
  std::vector<Vertex> vertex;

  typedef std::unordered_map<std::string, Vertex*> VertexTable;
  std::vector<std::shared_ptr<Vertex>> _gen_vertex;
  VertexTable _nodes;
  VertexTable _data_mapping_table;
  int64_t _idx = 0;
  GraphCluster* _cluster = nullptr;

  WRDK_TOML_DEFINE_FIELDS(name, vertex)
  std::string generateNodeId();
  Vertex* geneatedCondVertex(const std::string& cond);
  Vertex* FindVertexByData(const std::string& data);
  Vertex* FindVertexById(const std::string& id);

  int Build();
  int DumpDot(std::string& s);
  bool TestCircle();
  ~Graph();
};

struct GraphManager;
class GraphClusterContext;
struct GraphCluster {
  std::string desc;
  bool strict_dsl = true;
  std::string default_expr_processor;
  int64_t default_context_pool_size = 256;
  std::vector<Graph> graph;
  std::vector<ConfigSetting> config_setting;

  std::string _name;
  typedef std::map<std::string, Graph*> GraphTable;
  GraphTable _graphs;
  GraphManager* _graph_manager = nullptr;

  tbb::concurrent_queue<GraphClusterContext*> _graph_cluster_context_pool;
  WRDK_TOML_DEFINE_FIELDS(desc, strict_dsl, default_expr_processor, default_context_pool_size,
                          graph, config_setting)

  int Build();
  bool ContainsConfigSetting(const std::string& name);
  int DumpDot(std::string& s);
  Graph* FindGraphByName(const std::string& name);
  GraphClusterContext* GetContext();
  void ReleaseContext(GraphClusterContext* p);
  ~GraphCluster();
};

class GraphContext;
class GraphManager {
 private:
  typedef tbb::concurrent_hash_map<std::string, std::shared_ptr<GraphCluster>> ClusterGraphTable;
  ClusterGraphTable _graphs;

 public:
  std::shared_ptr<GraphCluster> Load(const std::string& file);
  std::shared_ptr<GraphCluster> FindGraphClusterByName(const std::string& name);
  GraphClusterContext* GetGraphClusterContext(const std::string& cluster);
  int Execute(const GraphExecuteOptions& options, std::shared_ptr<GraphDataContext> data_ctx,
              const std::string& cluster, const std::string& graph, DoneClosure&& done);
};
}  // namespace didagle