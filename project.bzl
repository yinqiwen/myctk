load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def clean_dep(dep):
    return str(Label(dep))

def myctk_workspace(path_prefix = "", tf_repo_name = "", **kwargs):

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

    _FMT_BUILD_FILE = """
cc_library(
    name = "fmt",
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
        name = "com_github_fmtlib",
        strip_prefix = fmtlib_name,
        urls = [
            "https://mirrors.tencent.com/github.com/fmtlib/fmt/archive/{ver}.tar.gz".format(ver = fmtlib_ver),
            "https://github.com/fmtlib/fmt/archive/{ver}.tar.gz".format(ver = fmtlib_ver),
        ],
        build_file_content = _FMT_BUILD_FILE,
    )