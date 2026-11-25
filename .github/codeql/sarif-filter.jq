# Post-process CodeQL SARIF before upload (see GHA-05 workflow):
# - drop results whose primary location is under tests/
# - drop data-flow results whose every flow starts under tests/: the unit
#   tests link the driver code, so test data read with fscanf() would
#   otherwise show up as "untrusted input" in the drivers
# - rewrite *.py copies of Python templates back to the template file names
#   ($map: copy path -> template path, from python-templates.txt); the copies
#   keep the templates' line numbers, so the alerts point at real files
def in_tests: (. // "") | startswith("tests/");
def flow_sources:
    [ .codeFlows[]?.threadFlows[]?.locations[0].location.physicalLocation.artifactLocation.uri ];
.runs |= map(
    .results |= map(select(
        ((.locations // [])[0].physicalLocation.artifactLocation.uri | in_tests | not)
        and (flow_sources as $s | ($s | length) == 0 or ($s | any(in_tests | not)))
    ))
)
| (.. | objects | select(has("uri") and (.uri | type) == "string") | .uri)
    |= ($map[.] // .)
