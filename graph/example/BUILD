package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "dot_graph",
    srcs = ["dot_example.cpp"],
    deps = [
        "//graph:wrdk_graph",
    ],
    linkopts=["-lrt -ltbb"],
)

cc_library(
    name = "processor_lib",
    srcs = ["processor_0.cpp","processor_1.cpp","processor_2.cpp","processor_3.cpp","processor_4.cpp"],
    deps = [
        "//graph:wrdk_graph",
    ],
    alwayslink=True,
)

cc_binary(
    name = "run_dsl",
    srcs = ["run_dsl.cpp"],
    deps = [
        "//graph:wrdk_graph",
        ":processor_lib",
    ],
    linkopts=["-lrt -ltbb -lpthread"],
)