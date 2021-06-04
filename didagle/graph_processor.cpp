// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
#include "graph_processor.h"
#include <stdlib.h>
#include "didagle_log.h"
#include "graph_processor_api.h"
#include "graph_processor_di.h"

namespace didagle {
typedef std::unordered_map<std::string, ProcessorCreator> CreatorTable;
static CreatorTable* g_creator_table = nullptr;

void deleteCreatorTable() { delete g_creator_table; }
CreatorTable& GetCreatorTable() {
  if (nullptr == g_creator_table) {
    g_creator_table = new CreatorTable;
    atexit(deleteCreatorTable);
  }
  return *g_creator_table;
}
void GraphDataContext::Reset() {
  _data_table.clear();
  _parent.reset();
  _disable_entry_creation = false;
}
void GraphDataContext::RegisterData(const DIObjectKey& id) {
  DataValue dv;
  dv.name.reset(new std::string(id.name));
  DIObjectKeyView key = {*dv.name, id.id};
  auto found = _data_table.find(key);
  if (found == _data_table.end()) {
    _data_table[key] = dv;
  }
}
ProcessorFactory g_processor_factory;
Processor::~Processor() { Reset(); }
int Processor::Setup(const Params& args) { return OnSetup(args); }
void Processor::Reset() {
  _data_ctx.reset();
  if (ERR_UNIMPLEMENTED == OnReset()) {
    for (auto& reset : _reset_funcs) {
      reset();
    }
  }
}
int Processor::Execute(const Params& args) { return OnExecute(args); };
void Processor::AsyncExecute(const Params& args, DoneClosure&& done) {
  OnAsyncExecute(args, std::move(done));
};

size_t Processor::AddResetFunc(ResetFunc&& f) {
  _reset_funcs.emplace_back(f);
  return _reset_funcs.size();
}
int Processor::InjectInputField(GraphDataContext& ctx, const std::string& field_name,
                                const std::string& data_name, bool move) {
  auto found = _field_inject_table.find(field_name);
  if (found == _field_inject_table.end()) {
    DIDAGLE_DEBUG("[{}]InjectInputField field:{} not found", Name(), field_name);
    return -1;
  }
  int rc = found->second(ctx, data_name, move);
  // DIDAGLE_DEBUG("[{}]InjectInputField field:{}, id:{}, rc:{}", Name(), field_name, data_name,
  // rc);
  return rc;
}
int Processor::EmitOutputField(GraphDataContext& ctx, const std::string& field_name,
                               const std::string& data_name) {
  auto found = _field_emit_table.find(field_name);
  if (found == _field_emit_table.end()) {
    DIDAGLE_DEBUG("[{}]EmitOutputField field:{} not found", Name(), field_name);
    return -1;
  }
  int rc = found->second(ctx, data_name);
  // DIDAGLE_DEBUG("[{}]EmitOutputField field:{}, id:{}, rc:{}", Name(), field_name, data_name, rc);
  return rc;
}

void ProcessorFactory::Register(const std::string& name, const ProcessorCreator& creator) {
  GetCreatorTable().emplace(std::make_pair(name, creator));
}
Processor* ProcessorFactory::GetProcessor(const std::string& name) {
  auto found = GetCreatorTable().find(name);
  if (found != GetCreatorTable().end()) {
    return found->second();
  }
  return nullptr;
}
int ProcessorFactory::DumpAllMetas(const std::string& file) {
  std::vector<ProcessorMeta> all_metas;
  for (auto& pair : GetCreatorTable()) {
    ProcessorMeta meta;
    meta.name = pair.first;
    Processor* p = pair.second();
    meta.input = p->GetInputIds();
    meta.output = p->GetOutputIds();
    delete p;
    all_metas.push_back(meta);
  }
  return kcfg::WriteToJsonFile(all_metas, file);
}

ProcessorRegister::ProcessorRegister(const char* name, const ProcessorCreator& creator) {
  ProcessorFactory::Register(name, creator);
}

ProcessorRunResult run_processor(GraphDataContext& ctx, const std::string& proc,
                                 const Params* params) {
  ProcessorRunResult result;
  Processor* p = ProcessorFactory::GetProcessor(proc);
  if (nullptr == p) {
    DIDAGLE_ERROR("No processor:{} found", proc);
    return -1;
  }
  result.processor.reset(p);
  ProcessorDI di(p);
  di.PrepareInputs();
  di.PrepareOutputs();
  di.InjectInputs(ctx);
  Params empty;
  result.rc = p->Execute(nullptr == params ? empty : *params);
  di.CollectOutputs(ctx);
  return result;
}
}  // namespace didagle