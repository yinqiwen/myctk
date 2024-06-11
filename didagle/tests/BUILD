load("@rules_cc//cc:defs.bzl", "cc_test")

LINKOPTS = [
    "-L/usr/local/lib",
    "-L/usr/local/lib64",
    "-lfolly",
    "-lfmt",
    "-lgflags",
    "-lglog",
    "-ldouble-conversion",
    "-liberty",
    "-levent",
    "-lunwind",
    "-lcrypto",
    "-lssl",
    "-ldl",
    "-lrt",
    "-lstdc++fs",
    "-lboost_context",
    "-lboost_filesystem",
]

cc_test(
    name = "test_di",
    size = "small",
    srcs = ["test_di.cpp"],
    linkopts = LINKOPTS,
    deps = [
        "//didagle:didagle_core",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "test_proc",
    size = "small",
    srcs = ["test_processor.cpp"],
    linkopts = LINKOPTS,
    deps = [
        "//didagle:didagle_core",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_binary(
    name = "test_proc_bench",
    srcs = ["test_proc_bench.cpp"],
    linkopts = LINKOPTS,
    deps = [
        "//didagle:didagle_core",
        "@com_github_google_benchmark//:benchmark",
    ],
)

cc_binary(
    name = "test_dsl",
    srcs = [
        "test_dsl.cpp",
        "test_dsl_processors.cpp",
    ],
    linkopts = LINKOPTS,
    deps = [
        "//didagle:didagle_core",
    ],
)
