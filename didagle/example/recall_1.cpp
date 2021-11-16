#include "didagle_log.h"
#include "graph_processor_api.h"

GRAPH_OP_BEGIN(recall_1)
GRAPH_OP_OUTPUT((std::string), r1)
GRAPH_OP_EXTERN_INPUT((std::string), r99)
GRAPH_OP_EXTERN_IN_OUT((std::string), r100)
GRAPH_OP_EXTERN_INPUT(std::shared_ptr<std::string>, tstr)
GRAPH_PARAMS_int(x, 101, "e");
GRAPH_PARAMS_int_vector(vec1, ({1, 2}), "int vector");
GRAPH_PARAMS_double_vector(vec2, ({1.1, 2.2}), "double vector");
GRAPH_PARAMS_string_vector(vec3, ({"hello", "world"}), "string vector");
GRAPH_PARAMS_bool_vector(vec4, ({true, false}), "bool vector");

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
  if (nullptr != tstr) {
    DIDAGLE_DEBUG("tstr = {}", *tstr);
  } else {
    DIDAGLE_DEBUG("tstr null");
  }
  DIDAGLE_DEBUG("vec1 size:{}, vec1[0]={}", PARAMS_vec1.size(), PARAMS_vec1[0]);
  DIDAGLE_DEBUG("vec2 size:{}, vec2[0]={}", PARAMS_vec2.size(), PARAMS_vec2[0]);
  DIDAGLE_DEBUG("vec3 size:{}, vec3[0]={}", PARAMS_vec3.size(), PARAMS_vec3[0]);
  DIDAGLE_DEBUG("vec4 size:{}, vec4[0]={}", PARAMS_vec4.size(), PARAMS_vec4[0]);
  return 0;
}
GRAPH_OP_END