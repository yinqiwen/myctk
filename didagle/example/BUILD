package(default_visibility = ["//visibility:public"])

cc_library(
    name = "processor_lib",
    hdrs = ["expr.h"],
    srcs = [
        "processor_0.cpp",
        "processor_1.cpp",
        "processor_2.cpp",
        "processor_3.cpp",
        "processor_4.cpp", 
        "processor_5.cpp", 
        "processor_6.cpp", 
        "processor_expr.cpp",
    ],
    deps = [
        "//didagle:didagle_core",
        "//reflect:expr_struct",
        "//ssexpr:ssexpr",
    ],
    alwayslink=True,
)

cc_binary(
    name = "dot_graph",
    srcs = ["dot_example.cpp"],
    deps = [
       "//didagle:didagle_core",
        ":processor_lib",
    ],
    linkopts=["-lrt -ltbb"],
)

cc_binary(
    name = "run_dsl",
    srcs = ["run_dsl.cpp"],
    deps = [
        "//didagle:didagle_core",
        ":processor_lib",
    ],
    linkopts=["-lrt -ltbb -lpthread"],
)