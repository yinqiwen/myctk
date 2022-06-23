// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
#include "didagle/di_container.h"

namespace didagle {

static DIObjectBuilderTable* g_builders = nullptr;
DIObjectBuilderTable& DIContainer::GetDIObjectBuilderTable() {
  if (nullptr == g_builders) {
    g_builders = new DIObjectBuilderTable;
  }
  return *g_builders;
}
int DIContainer::Init() {
  if (nullptr != g_builders) {
    for (auto& pair : *g_builders) {
      DIObjectBuilderValue& builder_val = pair.second;
      if (builder_val.inited) {
        continue;
      }
      int rc = builder_val.builder->Init();
      if (0 != rc) {
        DIDAGLE_ERROR("Failed to init DIObjectBuilder:{}", pair.first.id);
        return rc;
      }
      builder_val.inited = true;
    }
  }
  return 0;
}
void DIObject::DoInjectInputFields() {
  for (auto& pair : _field_inject_table) {
    pair.second();
  }
}
int DIObject::InitDIFields() {
  if (!_di_fields_inited) {
    _di_fields_inited = true;
    DoInjectInputFields();
  }
  return 0;
}
}  // namespace didagle
