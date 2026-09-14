#!/bin/sh

set -eu

test_tmp="`mktemp -d "${TMPDIR-/tmp}/nut-monitor-test.XXXXXX"`"
trap 'rm -rf "$test_tmp"' 0 1 2 3 15

cp "${srcdir-.}/app/NUT-Monitor" "$test_tmp/NUT-Monitor"
chmod +x "$test_tmp/NUT-Monitor"

mkdir "$test_tmp/bin"
cat > "$test_tmp/bin/python-test" <<'EOF'
#!/bin/sh

if test "${1-}" = -c; then
    case "${2-}" in
        *PyQt5*) test "${HAVE_QT5-no}" = yes ;;
        *PyQt6*) test "${HAVE_QT6-no}" = yes ;;
        *gtk*) test "${HAVE_GTK-no}" = yes ;;
        *) exit 0 ;;
    esac
    exit
fi

basename "$1"
EOF
chmod +x "$test_tmp/bin/python-test"
ln -s python-test "$test_tmp/bin/python"
ln -s python-test "$test_tmp/bin/python2"
ln -s python-test "$test_tmp/bin/python3"

for variant in py2gtk2 py3qt5 py3qt6; do
    printf '#!python3\n' > "$test_tmp/NUT-Monitor-$variant"
    chmod +x "$test_tmp/NUT-Monitor-$variant"
done

run_ok() {
    expected="$1"
    have_qt5="$2"
    have_qt6="$3"
    requested="$4"
    prefer_py2="${5-false}"
    have_gtk="${6-no}"

    output="`env PATH="$test_tmp/bin:/usr/bin:/bin" \
        PYTHON2=python2 PYTHON3=python3 PREFER_PY2="$prefer_py2" \
        HAVE_GTK="$have_gtk" HAVE_QT5="$have_qt5" HAVE_QT6="$have_qt6" \
        NUT_MONITOR_QT="$requested" \
        "$test_tmp/NUT-Monitor" 2>&1`"
    echo "$output" | grep "$expected" >/dev/null
}

run_fail() {
    expected="$1"
    have_qt5="$2"
    have_qt6="$3"
    requested="$4"

    set +e
    output="`env PATH="$test_tmp/bin:/usr/bin:/bin" \
        PYTHON2=python2 PYTHON3=python3 PREFER_PY2=false \
        HAVE_QT5="$have_qt5" HAVE_QT6="$have_qt6" \
        NUT_MONITOR_QT="$requested" \
        "$test_tmp/NUT-Monitor" 2>&1`"
    result=$?
    set -e
    test "$result" -ne 0
    echo "$output" | grep "$expected" >/dev/null
}

run_ok 'NUT-Monitor-py3qt6' yes yes auto
run_ok 'NUT-Monitor-py3qt5' yes no auto
run_ok 'NUT-Monitor-py3qt6' no yes auto
run_ok 'NUT-Monitor-py3qt5' yes yes 5
run_ok 'NUT-Monitor-py3qt6' yes yes 6
run_ok 'NUT-Monitor-py2gtk2' yes yes auto true yes
run_ok 'NUT-Monitor-py3qt5' yes yes 5 true yes
run_fail 'No usable Python interpreter' no no auto
run_fail 'Unsupported value of NUT_MONITOR_QT' yes yes invalid
