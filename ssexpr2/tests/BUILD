load("@rules_cc//cc:defs.bzl", "cc_test")
load("@com_github_google_flatbuffers//:build_defs.bzl", "flatbuffer_cc_library")
load("@rules_cc//cc:defs.bzl", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

flatbuffer_cc_library(
    name = "test_fbs",
    srcs = ["test_data.fbs"],
    flatc_args = [
        "--gen-object-api",
    ],
)

cc_proto_library(
    name = "test_data_cc_proto",
    deps = [":test_data_proto"],
)

proto_library(
    name = "test_data_proto",
    srcs = ["test_data.proto"],
)

cc_test(
    name = "test_jit_struct",
    size = "small",
    srcs = ["test_jit_struct.cpp"],
    deps = [
        ":test_data_cc_proto",
        ":test_fbs",
        "//ssexpr2",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "test_jit_expr",
    size = "small",
    srcs = ["test_jit_expr.cpp"],
    deps = [
        "//ssexpr2",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_binary(
    name = "expr_bench",
    srcs = ["expr_bench.cpp"],
    linkopts = [],
    deps = [
        "//ssexpr2",
        "@com_github_google_benchmark//:benchmark",
    ],
)
