package(default_visibility = ["//visibility:public"])

cc_library(
    name = "wrdk_graph",
    hdrs = ["data.h", "vertex.h", "graph.h", "processor_api.h", "executor.h", "log.h", "processor.h"],
    srcs = ["vertex.cpp", "graph.cpp", "processor.cpp", "executor.cpp", "data.cpp"],
    deps = [
        "//config:wrdk_config",
        "@com_github_fmtlib//:fmt",
    ],
    includes=["./"],
)