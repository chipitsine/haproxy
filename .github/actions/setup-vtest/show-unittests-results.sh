#!/bin/sh
# Post-run step to show unit test results on failure

for result in ${TMPDIR:-/tmp}/ha-unittests-*/results/res.*; do
  printf "::group::"
  cat "$result"
  echo "::endgroup::"
done
exit 1
