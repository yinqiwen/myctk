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
        name = "rules_cc",
        sha256 = "35f2fb4ea0b3e61ad64a369de284e4fbbdcdba71836a5555abb5e194cf119509",
        strip_prefix = "rules_cc-624b5d59dfb45672d4239422fa1e3de1822ee110",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_cc/archive/624b5d59dfb45672d4239422fa1e3de1822ee110.tar.gz",
            "https://github.com/bazelbuild/rules_cc/archive/624b5d59dfb45672d4239422fa1e3de1822ee110.tar.gz",
        ],
    )

    # rules_proto defines abstract rules for building Protocol Buffers.
    http_archive(
        name = "rules_proto",
        sha256 = "2490dca4f249b8a9a3ab07bd1ba6eca085aaf8e45a734af92aad0c42d9dc7aaf",
        strip_prefix = "rules_proto-218ffa7dfa5408492dc86c01ee637614f8695c45",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_proto/archive/218ffa7dfa5408492dc86c01ee637614f8695c45.tar.gz",
            "https://github.com/bazelbuild/rules_proto/archive/218ffa7dfa5408492dc86c01ee637614f8695c45.tar.gz",
        ],
    )

    http_archive(
        name = "rules_foreign_cc",
        strip_prefix = "rules_foreign_cc-0.7.0",
        url = "https://github.com/bazelbuild/rules_foreign_cc/archive/0.7.0.zip",
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

    _CPP_LINENOISE_BUILD_FILE = """
cc_library(
    name = "cpp_linenoise",
    hdrs = ["linenoise.hpp"],
    visibility = [ "//visibility:public" ],
)
"""
    new_git_repository(
        name = "com_github_cpp_linenoise",
        commit = "4cd89adfbc07cedada1aa32be12991828919d91b",
        remote = "https://github.com/yhirose/cpp-linenoise.git",
        build_file_content = _CPP_LINENOISE_BUILD_FILE,
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
)
"""
    spdlog_ver = kwargs.get("spdlog_ver", "1.10.0")
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

    bench_ver = kwargs.get("bench_ver", "1.5.3")
    bench_name = "benchmark-{ver}".format(ver = bench_ver)
    http_archive(
        name = "com_github_google_benchmark",
        strip_prefix = bench_name,
        urls = [
            "https://mirrors.tencent.com/github.com/google/benchmark/archive/v{ver}.tar.gz".format(ver = bench_ver),
            "https://github.com/google/benchmark/archive/v{ver}.tar.gz".format(ver = bench_ver),
        ],
    )

    protobuf_ver = kwargs.get("protobuf_ver", "3.19.2")
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

    _RAFT_BUILD_FILE = """
cc_library(
    name = "libraft",
    hdrs = ["include/raft_log.h", "include/raft_private.h", "include/raft_types.h", "include/raft.h"],
    srcs= ["src/raft_server.c", "src/raft_log.c", "src/raft_node.c", "src/raft_server_properties.c"],
    includes = ["include"],
    alwayslink = True,
    visibility = ["//visibility:public"],
)
"""
    new_git_repository(
        name = "com_github_redislabs_raft",
        commit = "39f24a0525d41bdf9ef0ac0fb4ee9b05771faec7",
        remote = "https://github.com/RedisLabs/raft.git",
        build_file_content = _RAFT_BUILD_FILE,
    )

    _JUNGLE_BUILD_FILE = """
load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")

filegroup(
    name = "all_srcs",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

cmake(
    name = "libjungle",
    lib_source = ":all_srcs",
    out_lib_dir = "lib64",
    out_static_libs = ["libjungle.a"],
    deps = ["@com_github_kvstore_forestdb//:libforestdb"],
    visibility = ["//visibility:public"],
)
"""
    new_git_repository(
        name = "com_github_ebay_jungle",
        commit = "190de2fb06e17134ef82b00a46b33d0a512d5ca5",
        remote = "https://github.com/eBay/Jungle.git",
        build_file_content = _JUNGLE_BUILD_FILE,
    )

    _FORESTDB_BUILD_FILE = """
load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")

filegroup(
    name = "all_srcs",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

cmake(
    name = "libforestdb",
    lib_source = ":all_srcs",
    out_lib_dir = "lib",
    out_shared_libs = ["libforestdb.so"],
    visibility = ["//visibility:public"],
)
"""
    new_git_repository(
        name = "com_github_kvstore_forestdb",
        commit = "d41cc84934c355890337a37e615d99c0ce20a07f",
        remote = "https://github.com/ForestDB-KVStore/forestdb.git",
        build_file_content = _FORESTDB_BUILD_FILE,
    )
