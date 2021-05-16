package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "example",
    srcs = ["example.cpp"],
    linkopts = [],
    deps = [
        "//ssexpr",
    ],
)

cc_binary(
    name = "example2",
    srcs = ["test2.cpp"],
    linkopts = [],
    deps = [
        "//ssexpr",
        "@com_github_google_benchmark//:benchmark",
    ],
)
