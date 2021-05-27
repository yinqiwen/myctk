load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "test_di",
    size = "small",
    srcs = ["test_di.cpp"],
    linkopts = ["-lrt -ltbb -lpthread"],
    deps = [
        "//didagle:didagle_core",
        "@com_google_googletest//:gtest_main",
    ],
)
