load("@rules_cc//cc:defs.bzl", "cc_test")

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
    name = "mmap_file",
    srcs = [
        "mmap_file.cc",
    ],
    hdrs = [
        "mmap_file.h",
    ],
    deps = [
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "rdict",
    hdrs = [
        "rdict.h",
    ],
    deps = [
        ":mmap_file",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "test_rdict",
    size = "small",
    srcs = ["test_rdict.cc"],
    linkopts = LINKOPTS,
    deps = [
        ":rdict",
        "@com_google_googletest//:gtest_main",
    ],
)
