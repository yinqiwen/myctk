// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
#include "processor.h"
#include <stdlib.h>
#include "log.h"
#include "processor_api.h"

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
    DIDAGLE_DEBUG("[{}]InjectInputField field:{} not found", Name());
    return -1;
  }
  int rc = found->second(ctx, data_name, move);
  DIDAGLE_DEBUG("[{}]InjectInputField field:{}, id:{}, rc:{}", Name(), field_name, data_name, rc);
  return rc;
}
int Processor::EmitOutputField(GraphDataContext& ctx, const std::string& field_name,
                               const std::string& data_name) {
  DIDAGLE_DEBUG("[{}]EmitOutputField field:{}, id:{}", Name(), field_name, data_name);
  auto found = _field_emit_table.find(field_name);
  if (found == _field_emit_table.end()) {
    return -1;
  }
  return found->second(ctx, data_name);
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
ProcessorRegister::ProcessorRegister(const char* name, const ProcessorCreator& creator) {
  ProcessorFactory::Register(name, creator);
}
}  // namespace didagle