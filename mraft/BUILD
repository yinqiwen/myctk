package(default_visibility = ["//visibility:public"])

load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_test")

cc_proto_library(
    name = "mraft_cc_proto",
    deps = [":mraft_proto"],
)

proto_library(
    name = "mraft_proto",
    srcs = ["mraft.proto"],
)

cc_library(
    name = "mraft",
    srcs = [
        "iobuf.cpp",
        "log_entry.cpp",
        "log_storage.cpp",
        "logger.cpp",
        "mraft.cpp",
        "segment_log_storage.cpp",
        "snapshot.cpp",
        "utils.cpp",
    ],
    hdrs = [
        "c_raft.h",
        "iobuf.h",
        "log_entry.h",
        "log_storage.h",
        "logger.h",
        "mraft.h",
        "segment_log_storage.h",
        "snapshot.h",
        "utils.h",
    ],
    includes = ["./"],
    linkopts = ["-lfolly -lfmt -lgflags -lglog -ldouble-conversion -lrt"],
    deps = [
        ":mraft_cc_proto",
        "@com_github_redislabs_raft//:libraft",
        "@com_github_spdlog//:spdlog",
        "@com_google_absl//absl/container:btree",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "utils_tests",
    size = "small",
    srcs = ["utils_tests.cc"],
    deps = [
        ":mraft",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "segment_log_storage_tests",
    size = "small",
    srcs = ["segment_log_storage_tests.cc"],
    deps = [
        ":mraft",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "snapshot_tests",
    size = "small",
    srcs = ["snapshot_tests.cc"],
    deps = [
        ":mraft",
        "@com_google_googletest//:gtest_main",
    ],
)
