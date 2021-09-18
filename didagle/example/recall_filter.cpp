#include "didagle_log.h"
#include "graph_processor_api.h"

GRAPH_OP_BEGIN(recall_filter)
GRAPH_OP_INPUT((std::string), merge_out)
GRAPH_OP_EXTERN_INPUT(std::shared_ptr<std::string>, tstr)
GRAPH_OP_EXTERN_INPUT(std::string, ustr)
int OnSetup(const Params& args) override { return 0; }
int OnExecute(const Params& args) override {
  if (nullptr == merge_out) {
    DIDAGLE_DEBUG("nullptr merge_out");
  } else {
    DIDAGLE_DEBUG("merge_out:{}", *merge_out);
  }
  if (nullptr == tstr) {
    DIDAGLE_DEBUG("nullptr tstr");
  } else {
    DIDAGLE_DEBUG("tstr:{}", *tstr);
  }
  if (nullptr == ustr) {
    DIDAGLE_DEBUG("nullptr ustr");
  } else {
    DIDAGLE_DEBUG("ustr:{}", *ustr);
  }
  return 0;
}
GRAPH_OP_END