package(default_visibility = ["//visibility:public"])

cc_library(
    name = "kcfg",
    hdrs = [
        "kcfg.hpp",
        "kcfg_common.h",
        "kcfg_json.h",
        "kcfg_toml.h",
        "kcfg_yaml.h",
    ],
    includes = ["./"],
    deps = [
        "@com_github_jbeder_yaml_cpp//:yaml-cpp",
        "@com_github_tencent_rapidjson//:rapidjson",
        "@com_github_toml11//:toml",
    ],
)
