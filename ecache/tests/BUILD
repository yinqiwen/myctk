load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "ecache_tests",
    size = "small",
    srcs = ["ecache_tests.cpp"],
    deps = [
        "//ecache",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_binary(
    name = "example",
    srcs = ["example.cpp"],
    includes = ["./"],
    deps = [
        "//ecache",
    ],
)
