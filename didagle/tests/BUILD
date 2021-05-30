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

cc_test(
    name = "test_proc",
    size = "small",
    srcs = ["test_processor.cpp"],
    linkopts = ["-lrt -ltbb -lpthread"],
    deps = [
        "//didagle:didagle_core",
        "@com_google_googletest//:gtest_main",
    ],
)


cc_binary(
    name = "test_proc_bench",
    srcs = ["test_proc_bench.cpp"],
    linkopts = [],
    deps = [
        "//didagle:didagle_core",
        "@com_github_google_benchmark//:benchmark",
    ],
)

