#include "didagle_log.h"
#include "graph_processor_api.h"

GRAPH_OP_BEGIN(recall_merge)
GRAPH_OP_INPUT((std::string), r2)
GRAPH_OP_INPUT((std::string), r1)
GRAPH_OP_MAP_INPUT(std::string, r3)
int OnSetup(const Params& args) override { return 0; }
int OnExecute(const Params& args) override {
  if (nullptr == r1) {
    DIDAGLE_DEBUG("nullptr r1");
  } else {
    DIDAGLE_DEBUG("r1:{}", *r1);
  }
  if (nullptr == r2) {
    DIDAGLE_DEBUG("nullptr r2");
  } else {
    DIDAGLE_DEBUG("r2:{}", *r2);
  }
  return 0;
}
GRAPH_OP_END