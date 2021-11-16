#include "didagle_log.h"
#include "graph_processor_api.h"

GRAPH_OP_BEGIN(recall_2)
GRAPH_OP_OUTPUT((std::string), r2)
int OnSetup(const Params& args) override { return 0; }
int OnExecute(const Params& args) override {
  r2 = "recall2";
  sleep(5);
  return 0;
}
GRAPH_OP_END