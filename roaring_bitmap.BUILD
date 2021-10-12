package(default_visibility = ["//visibility:public"])

load("@rules_cc//cc:defs.bzl", "cc_binary")
load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")

filegroup(
    name = "all_srcs",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

cmake(
    name = "roaring_bitmap",
    lib_source = ":all_srcs",
    out_lib_dir = "lib64",
    out_static_libs = [
        "libroaring.a",
    ],
)
