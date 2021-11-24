load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "test_kcfg",
    size = "small",
    srcs = ["test_kcfg.cpp"],
    deps = [
        "//config:kcfg",
        "@com_google_googletest//:gtest_main",
    ],
)
