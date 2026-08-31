#!/bin/sh
# iris.h is copied into each sketch folder so a student needs no install step.
# Copies drift. This says so, loudly, and can fix it.
#
#   sh sync-iris.sh          check only, non-zero exit if they disagree
#   sh sync-iris.sh --fix    re-copy from the library
set -e
SRC=${IRIS_SRC:-../iris/iris.h}
[ -f "$SRC" ] || { echo "cannot find the library at $SRC"; echo "set IRIS_SRC to point at iris.h"; exit 2; }
want=$(md5 -q "$SRC" 2>/dev/null || md5sum "$SRC" | cut -d' ' -f1)
bad=0
# find(1), not a glob. `*/iris.h` only reaches one level down, so the copies
# under boilerplate/ were silently outside the guard -- a drift check that does
# not cover every copy is not a drift check.
for f in $(find . -name iris.h -not -path './.git/*' | sort); do
  got=$(md5 -q "$f" 2>/dev/null || md5sum "$f" | cut -d' ' -f1)
  if [ "$got" = "$want" ]; then
    echo "ok      $f"
  elif [ "$1" = "--fix" ]; then
    cp "$SRC" "$f"; echo "updated $f"
  else
    echo "DRIFT   $f  (run: sh sync-iris.sh --fix)"; bad=1
  fi
done
[ $bad -eq 0 ] || exit 1
echo "all copies match $SRC"
