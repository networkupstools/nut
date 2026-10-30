#!/usr/bin/env bash

# Result reporting shared by nut-sh-matrix.sh and nut-personalities.sh,
# which source this file (the shebang is there so file(1), and with it
# GHA-07 and check-scripts-syntax, see a bash script). Prints one line
# per check; in GitHub Actions
# (GITHUB_ACTIONS=true) also prints failures as error annotations, on the
# source file and line when known, and writes the job summary
# (GITHUB_STEP_SUMMARY) when the script exits:
#
#   report_init "<summary heading>"
#   step "<check name>"; ...; ok | bad "<one-line reason>"
#   annotate <repo-relative file> <line> "<message>"   (after bad, optional)
#   detail "<text, e.g. a log excerpt>"                (after bad, optional)
#   report_exit         prints the total time and PASS/FAIL, exits 1 on FAIL

fail=0 STEP="" T0=0 REPORT_TITLE=""
REPORT_ROWS=() REPORT_FAILS=()
now() { date +%s%N; }
elapsed() { local d=$(( ($(now) - T0) / 100000000 )); printf '%d.%d' $((d / 10)) $((d % 10)); }
in_gha() { [ "${GITHUB_ACTIONS-}" = true ]; }
md_cell() { printf '%s' "$*" | tr '|\n' '/ ' | cut -c1-300; }
# workflow command values: % and newlines escaped, and in properties also , and :
gha_data() { local s=${1//%/%25}; s=${s//$'\r'/%0D}; printf '%s' "${s//$'\n'/%0A}"; }
gha_prop() { local s; s=$(gha_data "$1"); s=${s//,/%2C}; printf '%s' "${s//:/%3A}"; }

step() { STEP="$*"; printf '%-58s' "$STEP"; T0=$(now); }
ok() {
  local t; t=$(elapsed); printf 'ok   %5ss\n' "$t"
  REPORT_ROWS+=("| $(md_cell "$STEP") | ok | ${t}s |")
}
bad() {
  local t; t=$(elapsed); printf 'FAIL %5ss  %s\n' "$t" "$*"; fail=1
  REPORT_ROWS+=("| $(md_cell "$STEP") | **FAIL** | ${t}s |")
  REPORT_FAILS+=("#### $STEP" "" "$*" "")
  ! in_gha || printf '::error title=%s::%s\n' "$(gha_prop "$STEP")" "$(gha_data "$*")"
}
annotate() {  # annotate <file> <line> <message>: an error on that source line
  REPORT_FAILS+=("- \`$1:$2\`: $(md_cell "$3")" "")
  ! in_gha || printf '::error file=%s,line=%s,title=%s::%s\n' \
    "$(gha_prop "$1")" "$2" "$(gha_prop "$STEP")" "$(gha_data "$3")"
}
detail() {  # detail <text>: shown under the last failure in the summary
  REPORT_FAILS+=("<details><summary>details</summary>" "" '```' "$*" '```' "" "</details>" "")
}

# the first line number in shell or make error output: "file: line 12:",
# "file: 12:", "file:12:", "file[12]:" (the forms dash, bash, ksh, mksh,
# yash, posh and busybox use)
err_line() { grep -o -m1 -E '(line |: ?|\[)[0-9]+' <<<"$1" | head -1 | grep -o -E '[0-9]+'; }

# the line of shell -n output to report: the first that says "error" (ksh
# prints warnings first), else the first one, minus ksh93's backtick notes
err_msg() {
  local o
  # shellcheck disable=SC2016 # a literal "$(" in the pattern, not an expansion
  o=$(grep -v 'obsolete, use \$(' <<<"$1")
  grep -i -m1 'error' <<<"$o" || head -1 <<<"$o"
}

# lines around line $2 of file $1, numbered
excerpt() { local a=$(( $2 > 4 ? $2 - 4 : 1 )); sed -n "$a,$(( $2 + 4 ))p" "$1" | nl -ba -v "$a" -w6 -s'  '; }

report_write() {
  [ -n "${GITHUB_STEP_SUMMARY-}" ] || return 0
  local n=${#REPORT_ROWS[@]} nf=$(( ${#REPORT_FAILS[@]} > 0 ? 1 : 0 )) bad_n=0 r
  for r in "${REPORT_ROWS[@]}"; do case $r in *'**FAIL**'*) bad_n=$((bad_n + 1));; esac; done
  {
    if [ "$fail" = 0 ]; then echo "### PASS: $REPORT_TITLE, all $n checks passed"
    else echo "### FAIL: $REPORT_TITLE, $bad_n of $n checks failed"; fi
    echo
    [ $nf = 0 ] || printf '%s\n' "${REPORT_FAILS[@]}"
    echo "<details><summary>All checks</summary>"; echo
    echo "| Check | Result | Time |"; echo "|---|---|---|"
    printf '%s\n' "${REPORT_ROWS[@]}"
    echo; echo "</details>"; echo
  } >> "$GITHUB_STEP_SUMMARY"
}

report_init() { REPORT_TITLE=$1; REPORT_T0=$(date +%s); trap report_write EXIT; }
report_exit() {
  local r=PASS; [ "$fail" = 0 ] || r=FAIL
  echo "total $(( $(date +%s) - REPORT_T0 ))s, $r"
  exit "$fail"
}
