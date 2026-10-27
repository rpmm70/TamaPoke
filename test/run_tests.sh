#!/usr/bin/env bash
#region Usage
# Runs the whole test battery (firmware logic + tools).
#   ./test/run_tests.sh          normal
#   ./test/run_tests.sh --asan   also with AddressSanitizer + UBSan
#endregion

#region Setup
set -uo pipefail
cd "$(dirname "$0")"

failed=0
#endregion

#region Firmware logic
echo "== firmware logic (pet.cpp / pet_storage.cpp / i18n.cpp / battle.cpp / dex.h) =="
make --no-print-directory run || failed=1

if [[ "${1:-}" == "--asan" ]]; then
  echo
  echo "== the same battery with AddressSanitizer + UBSan =="
  make --no-print-directory asan || failed=1
fi
#endregion

#region Tools and generated files
echo
echo "== tools and generated files (tools/) =="
python3 test_tools.py || failed=1
#endregion

#region Summary
echo
if [[ $failed -eq 0 ]]; then
  echo "ALL OK"
else
  echo "THERE ARE FAILURES"
fi
exit $failed
#endregion
