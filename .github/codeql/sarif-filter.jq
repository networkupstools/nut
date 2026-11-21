# Post-process CodeQL SARIF before upload (see GHA-05 workflow):
# - drop results whose primary location is under tests/
# - rewrite *.py copies of Python templates back to the template file names
#   ($map: copy path -> template path, from python-templates.txt); the copies
#   keep the templates' line numbers, so the alerts point at real files
.runs |= map(
    .results |= map(select(
        ((.locations // [])[0].physicalLocation.artifactLocation.uri // "")
        | startswith("tests/") | not
    ))
)
| (.. | objects | select(has("uri") and (.uri | type) == "string") | .uri)
    |= ($map[.] // .)
