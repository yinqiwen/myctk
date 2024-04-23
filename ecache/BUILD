# package(default_visibility = ["//visibility:public"])

# load("@rules_proto//proto:defs.bzl", "proto_library")

# cc_proto_library(
#     name = "ecache_cc_proto",
#     deps = [":ecache_proto"],
# )

# proto_library(
#     name = "ecache_proto",
#     srcs = ["ecache.proto"],
# )

# cc_library(
#     name = "ecache",
#     srcs = [
#         "ecache_common.cpp",
#         "ecache_log.cpp",
#         "ecache_manager.cpp",
#         "ecache_types.cpp",
#     ],
#     hdrs = [
#         "ecache.h",
#         "ecache_common.h",
#         "ecache_impl.hpp",
#         "ecache_log.h",
#         "ecache_manager.h",
#         "ecache_types.h",
#     ],
#     includes = ["./"],
#     linkopts = ["-lcachelib_allocator -lcachelib_shm -lcachelib_navy -lcachelib_datatype -lcachelib_common -lfolly -lfmt -lglog -lthrift-core -lthriftcpp2 -lthriftprotocol -lboost_context -ldouble-conversion -lrt"],
#     deps = [
#         ":ecache_cc_proto",
#         "@com_github_google_flatbuffers//:flatbuffers",
#         "@com_github_spdlog//:spdlog",
#     ],
# )
