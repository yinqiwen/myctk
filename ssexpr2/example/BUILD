load("@com_github_google_flatbuffers//:build_defs.bzl", "flatbuffer_cc_library")
load("@rules_cc//cc:defs.bzl", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

package(default_visibility = ["//visibility:public"])

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

cc_binary(
    name = "example1",
    srcs = ["test.cpp"],
    linkopts = [],
    deps = [
        ":test_data_cc_proto",
        ":test_fbs",
        "//ssexpr2",
    ],
)

cc_binary(
    name = "example2",
    srcs = ["test2.cpp"],
    linkopts = [],
    deps = [
        "//ssexpr2",
        "@com_github_google_benchmark//:benchmark",
    ],
)
