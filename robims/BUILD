# package(default_visibility = ["//visibility:public"])

# load("@rules_proto//proto:defs.bzl", "proto_library")

# cc_proto_library(
#     name = "robims_cc_proto",
#     deps = [":robims_proto"],
# )

# proto_library(
#     name = "robims_proto",
#     srcs = ["robims.proto"],
# )

# cc_library(
#     name = "robims",
#     srcs = [
#         "robims_bsi.cpp",
#         "robims_cache.cpp",
#         "robims_common.cpp",
#         "robims_db_impl.cpp",
#         "robims_field.cpp",
#         "robims_field_bool.cpp",
#         "robims_field_bsi.cpp",
#         "robims_field_set.cpp",
#         "robims_field_weight_set.cpp",
#         "robims_log.cpp",
#         "robims_query.cpp",
#         "robims_simple_id_mapping.cpp",
#         "robims_table.cpp",
#         "robims_table_creation.cpp",
#     ],
#     hdrs = [
#         "robims_bsi.h",
#         "robims_cache.h",
#         "robims_common.h",
#         "robims_db.h",
#         "robims_db_impl.h",
#         "robims_err.h",
#         "robims_field.h",
#         "robims_id_mapping.h",
#         "robims_log.h",
#         "robims_query.h",
#         "robims_simple_id_mapping.h",
#         "robims_table.h",
#         "robims_table_creation.h",
#     ],
#     includes = ["./"],
#     linkopts = ["-lfolly -lglog"],
#     deps = [
#         ":robims_cc_proto",
#         "@com_github_cameron314_concurrentqueue//:concurrentqueue",
#         "@com_github_roaringbitmap_croaring//:roaring_bitmap",
#         "@com_github_simdjson_simdjson//:simdjson",
#         "@com_github_spdlog//:spdlog",
#         "@com_google_absl//absl/container:btree",
#     ],
# )
