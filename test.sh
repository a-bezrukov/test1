#!/usr/bin/env bash

log_failure() {
  printf "${RED}✖ %s${NORMAL}\n" "$@" >&2
}

assert_eq() {
  local expected="$1"
  local actual="$2"
  local msg="${3-}"

  if [ "$expected" == "$actual" ]; then
    return 0
  else
    [ "${#msg}" -gt 0 ] && log_failure "$expected == $actual :: $msg" || true
    return 1
  fi
}


assert_eq "4 10 12 15 23 44" "$(build/test1 -f test_data/test1.txt -c 4)"
assert_eq "4 10 12 15 23 44" "$(build/test1 -f test_data/test1.txt -c 4 -s)"

assert_eq "1 2 4 5 6 9 10 12" "$(build/test1 -f test_data/test2.txt -c 6)"
assert_eq "1 2 4 5 6 9 10 12" "$(build/test1 -f test_data/test2.txt -c 6 -s)"
