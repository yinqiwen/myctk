package(default_visibility = ["//visibility:public"])

cc_library(
    name = "ssexpr2",
    srcs = ["spirit_jit_expression.cpp"],
    hdrs = [
        "jit_err.h",
        "jit_struct.h",
        "jit_struct_helper.h",
        "spirit_jit_expression.h",
    ],
    includes = ["./"],
    deps = [
        "@com_github_google_flatbuffers//:flatbuffers",
        "@com_github_xbyak//:xbyak",
    ],
)
