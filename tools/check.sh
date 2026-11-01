#!/usr/bin/env bash
# Coil local quality gate (SPEC-0001).
#   tools/check.sh                  all gates: format, configure, build, tidy, tests
#   tools/check.sh format [files…]  format gate only (default: all sources)
#   tools/check.sh tidy [files…]    lint gate only (default: all sources; needs a configured build)
# Environment: COIL_PRESET (default: dev), CLANG_FORMAT and CLANG_TIDY (tool path overrides).
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
preset="${COIL_PRESET:-dev}"
build_dir="${repo_root}/build/${preset}"

# Prints the path of tool <name>: the <override> if set, else PATH, else Homebrew's LLVM.
find_tool() {
    local name="$1" override="$2"
    if [[ -n "${override}" ]]; then
        echo "${override}"
        return 0
    fi
    if command -v "${name}" >/dev/null 2>&1; then
        command -v "${name}"
        return 0
    fi
    local dir
    for dir in /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin; do
        if [[ -x "${dir}/${name}" ]]; then
            echo "${dir}/${name}"
            return 0
        fi
    done
    echo "check.sh: required tool '${name}' not found; install LLVM or set its path override" >&2
    return 1
}

# Prints all C++ sources covered by the gates (SPEC-0001, rule 6).
all_sources() {
    local roots=""
    local dir
    for dir in modules backends; do
        if [[ -d "${repo_root}/${dir}" ]]; then
            roots="${roots} ${repo_root}/${dir}"
        fi
    done
    # Paths contain no spaces by repository convention (snake_case file names).
    find ${roots} -type f \( -name '*.hpp' -o -name '*.cpp' \) -not -path '*/tests/fixtures/*' |
        sort
}

run_format() {
    local clang_format
    clang_format="$(find_tool clang-format "${CLANG_FORMAT:-}")"
    if [[ $# -eq 0 ]]; then
        set -- $(all_sources)
    fi
    echo "== format ($# files)"
    "${clang_format}" --dry-run --Werror --style="file:${repo_root}/.clang-format" "$@"
}

run_tidy() {
    local clang_tidy
    clang_tidy="$(find_tool clang-tidy "${CLANG_TIDY:-}")"
    # macOS: Homebrew's clang-tidy needs the SDK sysroot to find system headers.
    local sysroot_arg="--extra-arg=-Wno-unused-command-line-argument"
    if [[ "$(uname)" == "Darwin" ]]; then
        sysroot_arg="--extra-arg=-isysroot$(xcrun --show-sdk-path)"
    fi
    if [[ $# -gt 0 ]]; then
        # Explicit files (e.g. gate fixtures) may not be in the compilation database.
        echo "== tidy ($# files, standalone)"
        "${clang_tidy}" --quiet "${sysroot_arg}" "$@" -- -std=c++23
        return
    fi
    if [[ ! -f "${build_dir}/compile_commands.json" ]]; then
        echo "check.sh: ${build_dir}/compile_commands.json not found; configure first" >&2
        return 1
    fi
    set -- $(all_sources | grep '\.cpp$')
    echo "== tidy ($# translation units)"
    "${clang_tidy}" --quiet -p "${build_dir}" "${sysroot_arg}" "$@"
}

run_all() {
    run_format
    echo "== configure (${preset})"
    cmake --preset "${preset}"
    echo "== build (${preset})"
    cmake --build --preset "${preset}"
    run_tidy
    echo "== test (${preset})"
    ctest --preset "${preset}"
    echo "== all gates passed"
}

command="${1:-all}"
[[ $# -gt 0 ]] && shift
case "${command}" in
    all) run_all ;;
    format) run_format "$@" ;;
    tidy) run_tidy "$@" ;;
    *)
        echo "usage: tools/check.sh [all|format|tidy] [files…]" >&2
        exit 2
        ;;
esac
