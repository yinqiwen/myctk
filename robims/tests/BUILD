load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "test_bsi",
    size = "small",
    srcs = ["test_bsi.cpp"],
    deps = [
        "//robims",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_proto_library(
    name = "user_cc_proto",
    deps = [":user_proto"],
)

proto_library(
    name = "user_proto",
    srcs = ["user.proto"],
)

cc_binary(
    name = "example",
    srcs = ["example.cpp"],
    includes = ["./"],
    deps = [
        ":user_cc_proto",
        "//robims",
    ],
)
