package(default_visibility = ["//visibility:public"])

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

cc_library(
    name = "processor_lib",
    srcs = [
        "processor_0.cpp",
        "processor_1.cpp",
        "processor_2.cpp",
        "processor_3.cpp",
        "processor_4.cpp",
        "processor_5.cpp",
        "processor_6.cpp",
        "processor_expr.cpp",
        "recall_1.cpp",
        "recall_2.cpp",
        "recall_3.cpp",
        "recall_filter.cpp",
        "recall_merge.cpp",
    ],
    hdrs = ["expr.h"],
    deps = [
        "//didagle:didagle_core",
        "//ssexpr",
    ],
    alwayslink = True,
)

cc_binary(
    name = "graph_processor_dump",
    linkopts = LINKOPTS,
    deps = [
        ":processor_lib",
        "//didagle:didagle_processor_dump",
    ],
)

cc_binary(
    name = "dot_graph",
    srcs = ["dot_example.cpp"],
    linkopts = LINKOPTS,
    deps = [
        ":processor_lib",
        "//didagle:didagle_core",
    ],
)

cc_binary(
    name = "run_dsl",
    srcs = ["run_dsl.cpp"],
    linkopts = LINKOPTS,
    deps = [
        ":processor_lib",
        "//didagle:didagle_core",
    ],
)

cc_library(
    name = "coro_processor_lib",
    srcs = [
        "coro_processor_0.cpp",
        "coro_processor_1.cpp",
        "coro_processor_2.cpp",
    ],
    hdrs = ["expr.h"],
    deps = [
        "//didagle:didagle_core",
        "//ssexpr",
    ],
    alwayslink = True,
)

cc_binary(
    name = "coro_run_dsl",
    srcs = ["coro_run_dsl.cpp"],
    linkopts = LINKOPTS,
    deps = [
        ":coro_processor_lib",
    ],
)
