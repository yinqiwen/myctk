package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "example1",
    srcs = ["test.cpp"],
    linkopts = [],
    deps = [
        "//ssexpr2",
    ],
)

cc_binary(
    name = "example2",
    srcs = ["test2.cpp"],
    linkopts = [],
    deps = [
        "//ssexpr2",
        "@com_github_google_benchmark//:benchmark",
    ],
)
