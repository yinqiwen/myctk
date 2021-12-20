// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
#include "graph_processor.h"
#include <stdlib.h>
#include <string>
#include <utility>
#include <vector>

#include "didagle_log.h"
#include "graph_processor_api.h"
#include "graph_processor_di.h"

namespace didagle {
typedef std::unordered_map<std::string, ProcessorCreator> CreatorTable;
static CreatorTable *g_creator_table = nullptr;

void deleteCreatorTable() { delete g_creator_table; }
CreatorTable &GetCreatorTable() {
  if (nullptr == g_creator_table) {
    g_creator_table = new CreatorTable;
    atexit(deleteCreatorTable);
  }
  return *g_creator_table;
}
DAGEventTracker *GraphDataContext::GetEventTracker() const {
  if (_event_tracker) {
    return _event_tracker.get();
  }
  if (_parent) {
    return _parent->GetEventTracker();
  }
  return nullptr;
}
bool GraphDataContext::EnableEventTracker() {
  if (!_event_tracker) {
    _event_tracker.reset(new DAGEventTracker);
  }
  return true;
}
void GraphDataContext::Reset() {
  _data_table.clear();
  // _parent.reset();
  _parent = nullptr;
  for (size_t i = 0; i < _executed_childrens.size(); i++) {
    _executed_childrens[i] = nullptr;
  }
  _disable_entry_creation = false;
}

DataValue *GraphDataContext::GetValue(const DIObjectKeyView &key, GraphDataGetOptions opt,
                                      ExcludeGraphDataContextSet *excludes) {
  auto found = _data_table.find(key);
  if (found != _data_table.end()) {
    return &(found->second);
  }
  std::unique_ptr<ExcludeGraphDataContextSet> empty_execludes =
      std::make_unique<ExcludeGraphDataContextSet>();
  ExcludeGraphDataContextSet *new_excludes = excludes;
  if (nullptr == new_excludes) {
    new_excludes = empty_execludes.get();
  }
  DataValue *r = nullptr;
  new_excludes->insert(this);
  if (opt.with_parent && _parent) {
    GraphDataGetOptions parent_opt;
    parent_opt.with_parent = 1;
    parent_opt.with_children = opt.with_children;
    parent_opt.with_di_container = 0;
    if (new_excludes->count(_parent) == 0) {
      r = const_cast<GraphDataContext *>(_parent)->GetValue(key, parent_opt, new_excludes);
    }
    if (r) {
      return r;
    }
  }
  if (opt.with_children) {
    GraphDataGetOptions child_opt;
    child_opt.with_children = 1;
    opt.with_parent = 0;
    opt.with_di_container = 0;
    for (const GraphDataContext *child_ctx : _executed_childrens) {
      if (nullptr != child_ctx && new_excludes->count(child_ctx) == 0) {
        r = const_cast<GraphDataContext *>(child_ctx)->GetValue(key, child_opt, new_excludes);
        if (r) {
          return r;
        }
      }
    }
  }
  return nullptr;
}

int GraphDataContext::Move(const DIObjectKey &from, const DIObjectKey &to) {
  DIObjectKeyView from_key{from.name, from.id};
  DIObjectKeyView to_key{to.name, to.id};
  DataValue *from_value = GetValue(from_key);
  DataValue *to_value = GetValue(to_key);
  if (nullptr == from_value || nullptr == to_value) {
    return -1;
  }
  to_value->val.store(from_value->val.load());
  to_value->_sval = from_value->_sval;
  from_value->val.store(nullptr);
  from_value->_sval.reset();
  return 0;
}

void GraphDataContext::ReserveChildCapacity(size_t n) { _executed_childrens.resize(n); }
void GraphDataContext::SetChild(const GraphDataContext *c, size_t idx) {
  if (idx >= _executed_childrens.size()) {
    return;
  }
  _executed_childrens[idx] = c;
}
void GraphDataContext::RegisterData(const DIObjectKey &id) {
  DataValue dv;
  dv.name.reset(new std::string(id.name));
  DIObjectKeyView key = {*dv.name, id.id};
  auto found = _data_table.find(key);
  if (found == _data_table.end()) {
    _data_table[key] = dv;
  }
}
ProcessorFactory g_processor_factory;
Processor::~Processor() {}
int Processor::Setup(const Params &args) { return OnSetup(args); }
void Processor::Reset() {
  // _data_ctx.reset();
  _data_ctx = nullptr;
  OnReset();
  for (auto &reset : _reset_funcs) {
    reset();
  }
}
int Processor::Execute(const Params &args) {
  for (auto &f : _params_settings) {
    f(args);
  }
  return OnExecute(args);
}
void Processor::AsyncExecute(const Params &args, DoneClosure &&done) {
  for (auto &f : _params_settings) {
    f(args);
  }
  OnAsyncExecute(args, std::move(done));
}

size_t Processor::RegisterParam(const std::string &name, const std::string &type,
                                const std::string &deafult_value, const std::string &desc,
                                ParamSetFunc &&f) {
  ParamInfo info;
  info.name = name;
  info.type = type;
  info.default_value = deafult_value;
  info.desc = desc;
  _params.emplace_back(info);
  _params_settings.emplace_back(f);
  return _params.size();
}

size_t Processor::AddResetFunc(ResetFunc &&f) {
  _reset_funcs.emplace_back(f);
  return _reset_funcs.size();
}
int Processor::InjectInputField(GraphDataContext &ctx, const std::string &field_name,
                                const std::string_view &data_name, bool move) {
  auto found = _field_inject_table.find(field_name);
  if (found == _field_inject_table.end()) {
    DIDAGLE_DEBUG("[{}]InjectInputField field:{} not found", Name(), field_name);
    return -1;
  }
  int rc = found->second(ctx, data_name, move);
  // DIDAGLE_DEBUG("[{}]InjectInputField field:{}, id:{}, rc:{}", Name(),
  // field_name, data_name, rc);
  return rc;
}
int Processor::EmitOutputField(GraphDataContext &ctx, const std::string &field_name,
                               const std::string_view &data_name) {
  auto found = _field_emit_table.find(field_name);
  if (found == _field_emit_table.end()) {
    DIDAGLE_DEBUG("[{}]EmitOutputField field:{} not found", Name(), field_name);
    return -1;
  }
  int rc = found->second(ctx, data_name);
  // DIDAGLE_DEBUG("[{}]EmitOutputField field:{}, id:{}, rc:{}", Name(),
  // field_name, data_name, rc);
  return rc;
}

void ProcessorFactory::Register(std::string_view name, const ProcessorCreator &creator) {
  std::string name_str(name.data(), name.size());
  GetCreatorTable().emplace(std::make_pair(name_str, creator));
}
Processor *ProcessorFactory::GetProcessor(const std::string &name) {
  auto found = GetCreatorTable().find(name);
  if (found != GetCreatorTable().end()) {
    return found->second();
  }
  return nullptr;
}
void ProcessorFactory::GetAllMetas(std::vector<ProcessorMeta> &all_metas) {
  for (auto &pair : GetCreatorTable()) {
    ProcessorMeta meta;
    meta.name = pair.first;
    Processor *p = pair.second();
    meta.input = p->GetInputIds();
    meta.output = p->GetOutputIds();
    meta.params = p->GetParams();
    meta.desc = p->Desc();
    meta.isIOProcessor = p->isIOProcessor();
    delete p;
    all_metas.push_back(meta);
  }
}
int ProcessorFactory::DumpAllMetas(const std::string &file) {
  std::vector<ProcessorMeta> all_metas;
  GetAllMetas(all_metas);
  return kcfg::WriteToJsonFile(all_metas, file, true, false);
}

ProcessorRegister::ProcessorRegister(std::string_view name, const ProcessorCreator &creator) {
  ProcessorFactory::Register(name, creator);
}

ProcessorRunResult run_processor(GraphDataContext &ctx, const std::string &proc,
                                 const ProcessorRunOptions &opts) {
  ProcessorRunResult result;
  Processor *p = ProcessorFactory::GetProcessor(proc);
  if (nullptr == p) {
    DIDAGLE_ERROR("No processor:{} found", proc);
    result.rc = -1;
    return result;
  }
  result.processor.reset(p);
  ProcessorDI di(p);
  std::vector<GraphData> config_inputs;
  if (!opts.map_aggregate_ids.empty()) {
    for (const auto &[field, aggregate_ids] : opts.map_aggregate_ids) {
      GraphData data;
      data.id = field;
      data.field = field;
      data.aggregate = aggregate_ids;
      config_inputs.emplace_back(std::move(data));
    }
  }
  di.PrepareInputs(config_inputs);
  di.PrepareOutputs();
  di.InjectInputs(ctx, opts.params);
  Params empty;
  result.rc = p->Setup(nullptr == opts.params ? empty : *opts.params);
  if (0 != result.rc) {
    DIDAGLE_ERROR("Processor:{} setup failed with rc:{}", proc, result.rc);
    return result;
  }
  result.rc = p->Execute(nullptr == opts.params ? empty : *opts.params);
  di.CollectOutputs(ctx, opts.params);
  return result;
}
}  // namespace didagle