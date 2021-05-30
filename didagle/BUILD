package(default_visibility = ["//visibility:public"])

cc_library(
    name = "didagle_core",
    srcs = [
        "di_container.cpp",
        "graph.cpp",
        "graph_data.cpp",
        "graph_executor.cpp",
        "graph_processor.cpp",
         "graph_processor_di.cpp",
        "graph_vertex.cpp",
    ],
    hdrs = [
        "di_container.h",
        "didagle_log.h",
        "graph.h",
        "graph_data.h",
        "graph_executor.h",
        "graph_processor.h",
        "graph_processor_di.h",
        "graph_processor_api.h",
        "graph_vertex.h",
    ],
    includes = ["./"],
    deps = [
        "//config:wrdk_config",
        "@com_github_fmtlib//:fmt",
    ],
)
