load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def clean_dep(dep):
    return str(Label(dep))

def myctk_workspace(path_prefix = "", tf_repo_name = "", **kwargs):
    http_archive(
        name = "bazel_skylib",
        urls = [
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.0.3/bazel-skylib-1.0.3.tar.gz",
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.0.3/bazel-skylib-1.0.3.tar.gz",
        ],
        sha256 = "1c531376ac7e5a180e0237938a2536de0c54d93f5c278634818e0efc952dd56c",
    )
    http_archive(
        name = "rules_proto",
        sha256 = "602e7161d9195e50246177e7c55b2f39950a9cf7366f74ed5f22fd45750cd208",
        strip_prefix = "rules_proto-97d8af4dc474595af3900dd85cb3a29ad28cc313",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_proto/archive/97d8af4dc474595af3900dd85cb3a29ad28cc313.tar.gz",
            "https://github.com/bazelbuild/rules_proto/archive/97d8af4dc474595af3900dd85cb3a29ad28cc313.tar.gz",
        ],
    )
    http_archive(
        name = "rules_python",
        url = "https://github.com/bazelbuild/rules_python/releases/download/0.2.0/rules_python-0.2.0.tar.gz",
        sha256 = "778197e26c5fbeb07ac2a2c5ae405b30f6cb7ad1f5510ea6fdac03bded96cc6f",
    )
    http_archive(
        name = "rules_foreign_cc",
        strip_prefix = "rules_foreign_cc-0.6.0",
        url = "https://github.com/bazelbuild/rules_foreign_cc/archive/0.6.0.zip",
    )
    git_repository(
        name = "rules_cc",
        commit = "40548a2974f1aea06215272d9c2b47a14a24e556",
        remote = "https://github.com/bazelbuild/rules_cc.git",
    )

    _TOML11_BUILD_FILE = """
cc_library(
    name = "toml",
    hdrs = glob([
        "**/*.hpp",
    ]),
    visibility = [ "//visibility:public" ],
)
"""
    toml11_ver = kwargs.get("toml11_ver", "3.6.1")
    toml11_name = "toml11-{ver}".format(ver = toml11_ver)
    http_archive(
        name = "com_github_toml11",
        strip_prefix = toml11_name,
        urls = [
            "https://mirrors.tencent.com/github.com/ToruNiina/toml11/archive/v{ver}.tar.gz".format(ver = toml11_ver),
            "https://github.com/ToruNiina/toml11/archive/v{ver}.tar.gz".format(ver = toml11_ver),
        ],
        build_file_content = _TOML11_BUILD_FILE,
    )

    _RAPIDJSON_BUILD_FILE = """
cc_library(
    name = "rapidjson",
    hdrs = glob(["include/rapidjson/**/*.h"]),
    includes = ["include"],
    defines = ["RAPIDJSON_HAS_STDSTRING=1"],
    visibility = [ "//visibility:public" ],
)
"""
    rapidjson_ver = kwargs.get("rapidjson_ver", "1.1.0")
    rapidjson_name = "rapidjson-{ver}".format(ver = rapidjson_ver)
    http_archive(
        name = "com_github_tencent_rapidjson",
        strip_prefix = rapidjson_name,
        urls = [
            "https://mirrors.tencent.com/github.com/Tencent/rapidjson/archive/v{ver}.tar.gz".format(ver = rapidjson_ver),
            "https://github.com/Tencent/rapidjson/archive/v{ver}.tar.gz".format(ver = rapidjson_ver),
        ],
        build_file_content = _RAPIDJSON_BUILD_FILE,
    )

    _FMT_BUILD_FILE = """
cc_library(
    name = "fmtlib",
    hdrs = glob([
        "include/fmt/*.h",
    ]),
    srcs = glob([
        "src/*.cc",
    ]),
    includes=["./include"],
    visibility = [ "//visibility:public" ],
)
"""
    fmtlib_ver = kwargs.get("fmtlib_ver", "7.1.3")
    fmtlib_name = "fmt-{ver}".format(ver = fmtlib_ver)
    http_archive(
        name = "com_github_fmtlib_fmt",
        strip_prefix = fmtlib_name,
        urls = [
            "https://mirrors.tencent.com/github.com/fmtlib/fmt/archive/{ver}.tar.gz".format(ver = fmtlib_ver),
            "https://github.com/fmtlib/fmt/archive/{ver}.tar.gz".format(ver = fmtlib_ver),
        ],
        build_file_content = _FMT_BUILD_FILE,
    )

    _SPDLOG_BUILD_FILE = """
cc_library(
    name = "spdlog",
    hdrs = glob([
        "include/**/*.h",
    ]),
    srcs= glob([
        "src/*.cpp",
    ]),
    defines = ["SPDLOG_FMT_EXTERNAL", "SPDLOG_COMPILED_LIB"],
    includes = ["include"],
    visibility = ["//visibility:public"],
    deps = ["@com_github_fmtlib_fmt//:fmtlib"],
)
"""
    spdlog_ver = kwargs.get("spdlog_ver", "1.8.5")
    spdlog_name = "spdlog-{ver}".format(ver = spdlog_ver)
    http_archive(
        name = "com_github_spdlog",
        strip_prefix = spdlog_name,
        urls = [
            "https://mirrors.tencent.com/github.com/gabime/spdlog/archive/v{ver}.tar.gz".format(ver = spdlog_ver),
            "https://github.com/gabime/spdlog/archive/v{ver}.tar.gz".format(ver = spdlog_ver),
        ],
        build_file_content = _SPDLOG_BUILD_FILE,
    )

    _XBYAK_BUILD_FILE = """
cc_library(
    name = "xbyak",
    hdrs = glob([
        "**/*.h",
    ]),
    includes=["./"],
    visibility = [ "//visibility:public" ],
)
"""
    xbyak_ver = kwargs.get("xbyak_ver", "5.992")
    xbyak_name = "xbyak-{ver}".format(ver = xbyak_ver)
    http_archive(
        name = "com_github_xbyak",
        strip_prefix = xbyak_name,
        urls = [
            "https://mirrors.tencent.com/github.com/herumi/xbyak/archive/v{ver}.tar.gz".format(ver = xbyak_ver),
            "https://github.com/herumi/xbyak/archive/v{ver}.tar.gz".format(ver = xbyak_ver),
        ],
        build_file_content = _XBYAK_BUILD_FILE,
    )

    fbs_ver = kwargs.get("fbs_ver", "2.0.0")
    fbs_name = "flatbuffers-{ver}".format(ver = fbs_ver)
    http_archive(
        name = "com_github_google_flatbuffers",
        strip_prefix = fbs_name,
        urls = [
            "https://mirrors.tencent.com/github.com/google/flatbuffers/archive/v{ver}.tar.gz".format(ver = fbs_ver),
            "https://github.com/google/flatbuffers/archive/v{ver}.tar.gz".format(ver = fbs_ver),
        ],
    )

    bench_ver = kwargs.get("fbs_ver", "1.5.3")
    bench_name = "benchmark-{ver}".format(ver = bench_ver)
    http_archive(
        name = "com_github_google_benchmark",
        strip_prefix = bench_name,
        urls = [
            "https://mirrors.tencent.com/github.com/google/benchmark/archive/v{ver}.tar.gz".format(ver = bench_ver),
            "https://github.com/google/benchmark/archive/v{ver}.tar.gz".format(ver = bench_ver),
        ],
    )

    protobuf_ver = kwargs.get("protobuf_ver", "3.17.0")
    protobuf_name = "protobuf-{ver}".format(ver = protobuf_ver)
    http_archive(
        name = "com_google_protobuf",
        strip_prefix = protobuf_name,
        urls = [
            "https://mirrors.tencent.com/github.com/protocolbuffers/protobuf/archive/v{ver}.tar.gz".format(ver = protobuf_ver),
            "https://github.com/protocolbuffers/protobuf/archive/v{ver}.tar.gz".format(ver = protobuf_ver),
        ],
    )

    roaring_bitmap_ver = kwargs.get("roaring_bitmap_ver", "0.3.4")
    roaring_bitmap_name = "CRoaring-{ver}".format(ver = roaring_bitmap_ver)
    http_archive(
        name = "com_github_roaringbitmap_croaring",
        strip_prefix = roaring_bitmap_name,
        build_file = clean_dep("//:roaring_bitmap.BUILD"),
        urls = [
            "https://mirrors.tencent.com/github.com/RoaringBitmap/CRoaring/archive/v{ver}.tar.gz".format(ver = roaring_bitmap_ver),
            "https://github.com/RoaringBitmap/CRoaring/archive/refs/tags/v{ver}.tar.gz".format(ver = roaring_bitmap_ver),
        ],
    )

    _CQ_BUILD_FILE = """
cc_library(
    name = "concurrentqueue",
    hdrs = glob([
        "**/*.h",
    ]),
    includes=["./"],
    visibility = [ "//visibility:public" ],
)
"""
    concurrentqueue_ver = kwargs.get("concurrentqueue_ver", "1.0.3")
    concurrentqueue_name = "concurrentqueue-{ver}".format(ver = concurrentqueue_ver)
    http_archive(
        name = "com_github_cameron314_concurrentqueue",
        strip_prefix = concurrentqueue_name,
        build_file_content = _CQ_BUILD_FILE,
        urls = [
            "https://mirrors.tencent.com/github.com/cameron314/concurrentqueue/archive/v{ver}.tar.gz".format(ver = concurrentqueue_ver),
            "https://github.com/cameron314/concurrentqueue/archive/refs/tags/v{ver}.tar.gz".format(ver = concurrentqueue_ver),
        ],
    )

    _SIMD_JSON_BUILD_FILE = """
cc_library(
    name = "simdjson",
    hdrs = glob([
        "singleheader/simdjson.h",
    ]),
    srcs= glob([
        "singleheader/simdjson.cpp",
    ]),
    includes = ["singleheader"],
    visibility = ["//visibility:public"],
)
"""
    simd_json_ver = kwargs.get("simd_json_ver", "0.9.7")
    simdjson_name = "simdjson-{ver}".format(ver = simd_json_ver)
    http_archive(
        name = "com_github_simdjson_simdjson",
        strip_prefix = simdjson_name,
        build_file_content = _SIMD_JSON_BUILD_FILE,
        urls = [
            "https://mirrors.tencent.com/github.com/simdjson/simdjson/archive/v{ver}.tar.gz".format(ver = simd_json_ver),
            "https://github.com/simdjson/simdjson/archive/refs/tags/v{ver}.tar.gz".format(ver = simd_json_ver),
        ],
    )

    abseil_ver = kwargs.get("abseil_ver", "20210324.2")
    abseil_name = "abseil-cpp-{ver}".format(ver = abseil_ver)
    http_archive(
        name = "com_google_absl",
        strip_prefix = abseil_name,
        urls = [
            "https://mirrors.tencent.com/github.com/abseil/abseil-cpp/archive/{ver}.tar.gz".format(ver = abseil_ver),
            "https://github.com/abseil/abseil-cpp/archive/refs/tags/{ver}.tar.gz".format(ver = abseil_ver),
        ],
    )

    gtest_ver = kwargs.get("gtest_ver", "1.10.0")
    gtest_name = "googletest-release-{ver}".format(ver = gtest_ver)
    http_archive(
        name = "com_google_googletest",
        strip_prefix = gtest_name,
        urls = [
            "https://mirrors.tencent.com/github.com/google/googletest/archive/release-{ver}.tar.gz".format(ver = gtest_ver),
            "https://github.com/google/googletest/archive/release-{ver}.tar.gz".format(ver = gtest_ver),
        ],
    )
