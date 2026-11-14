#!/bin/zsh

set -euxo pipefail

_build_dir="$(pwd)"
_source_root="${0:A:h:h}"

rm -rf profiles
LLVM_PROFILE_FILE="$(pwd)/profiles/%m-%p.profraw" ctest -j 10 "$@" -VV
llvm-profdata merge -sparse profiles/*.profraw -o default.profdata

_objects=()
for o in $(find . -type f -executable -a ! -wholename './CMakeFiles/*'); do
  echo "$o"
  _objects+=("--object=$o")
done

_ignore_regex=('--ignore-filename-regex=(^|/)(test|tools)/|-test\.cpp|-test\..*.*\.cpp|ebur128\.test_signals|queuetest\.c|\.test\.h')

llvm-cov show ${_objects[*]} -instr-profile=default.profdata ${_ignore_regex[*]} \
  --format=html \
  --show-instantiations=0 --show-line-counts-or-regions --show-expansions --show-branches=count \
  --Xdemangler=llvm-cxxfilt > show.html
llvm-cov export ${_objects[*]} -instr-profile=default.profdata ${_ignore_regex[*]} \
  --format=lcov --Xdemangler=llvm-cxxfilt > show.raw.lcov
python3 "$_source_root/tools/filter_lcov.py" show.raw.lcov show.lcov
rm -f show.raw.lcov
llvm-cov report ${_objects[*]} -instr-profile=default.profdata ${_ignore_regex[*]} \
  --num-threads=1 |& tee report.txt

rm -rf "$_build_dir/lcov-report"
(
  cd "$_source_root"
  genhtml "$_build_dir/show.lcov" \
    --prefix "$_source_root" \
    --output-directory "$_build_dir/lcov-report" \
    --branch-coverage \
    --legend \
    --title "libebur128 coverage"
)
