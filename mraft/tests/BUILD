package(default_visibility = ["//visibility:public"])

cc_library(
    name = "local_raft",
    srcs = [
        "local_raft.cpp",
    ],
    hdrs = [
        "local_raft.h",
    ],
    deps = [
        "//mraft",
    ],
)

cc_binary(
    name = "local_test",
    srcs = [
        "local_test.cpp",
    ],
    deps = [
        ":local_raft",
    ],
)
