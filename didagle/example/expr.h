// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/04/21
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once
#include <string>
#include "expr_struct.h"

DEFINE_EXPR_STRUCT(RecmdEnv, (std::string)user_type, (std::string)user_id, (int)expid)