package(default_visibility = ["//visibility:public"])

cc_library(
    name = "ssexpr",
    srcs = ["spirit_expression.cpp"],
    hdrs = [
        "expr_macro.h",
        "expr_struct.h",
        "expr_struct_helper.h",
        "spirit_expression.h",
    ],
    includes = ["./"],
    deps = [
        "@com_github_google_flatbuffers//:flatbuffers",
    ],
)
