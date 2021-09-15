#include "didagle_log.h"
#include "graph_processor_api.h"

GRAPH_OP_BEGIN(recall_1)
GRAPH_OP_OUTPUT((std::string), r1)
GRAPH_OP_EXTERN_INPUT((std::string), r99)
GRAPH_OP_EXTERN_IN_OUT((std::string), r100)
int OnSetup(const Params& args) override { return 0; }
int OnExecute(const Params& args) override {
  r1 = "recall1";
  auto* rr100 = r100;
  if (nullptr != r99) {
    DIDAGLE_DEBUG("r99 = {}", *r99);
  } else {
    DIDAGLE_DEBUG("r99 null");
  }
  if (nullptr != rr100) {
    DIDAGLE_DEBUG("r100 = {}", *rr100);
  } else {
    DIDAGLE_DEBUG("r100 null");
  }
  return 0;
}
GRAPH_OP_END