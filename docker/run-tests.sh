#!/bin/sh

# Used in the Docker image to run tests

set -u

PGDATA=/tmp/pg_libphonenumber_test_data
pg_ctl=/usr/lib/postgresql/${PG_MAJOR}/bin/pg_ctl
initdb=/usr/lib/postgresql/${PG_MAJOR}/bin/initdb
if [ ! -s "${PGDATA}/PG_VERSION" ]; then
    mkdir -p "${PGDATA}"
    "$initdb" "--pgdata=${PGDATA}"
fi
"$pg_ctl" "--pgdata=${PGDATA}" start -w
trap '"$pg_ctl" "--pgdata=${PGDATA}" stop -m fast >/dev/null 2>&1 || true' EXIT HUP INT TERM
test_status=0
make installcheck || test_status=$?
if [ -f regression.diffs ]; then
    cat regression.diffs
fi
exit "$test_status"
