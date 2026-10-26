# Network UPS Tools: AppVeyor CI recipe step (see appveyor.yml), NUT for Windows
# with MSYS2/MinGW x64: enforce ccache max_size before the cache save, and
# report ccache statistics of this build (counters are zeroed in
# 03-before_build.bat) in the job messages, which the GitHub notification
# template in appveyor.yml includes for passed builds.

# Run ccache through MSYS2 bash: CCACHE_DIR from appveyor.yml is a POSIX path,
# which the MSYS2 runtime converts for ccache.exe like during the build itself.
# Keep the bash command free of double quotes (Windows PowerShell 5.1 mangles
# them in native command arguments), and print everything to stdout.
$env:CHERE_INVOKING = "yes"
$env:MSYSTEM = "MINGW64"

try {
    # AppVeyor saves the build cache right after the on_success scripts, so
    # clean up first (later steps also compiled tests and bundles after the
    # cleanup at the end of 04-build_script.bat); then the reported cache size
    # is what gets saved.
    $out = & C:\msys64\usr\bin\bash -lc 'date -u ; PATH=/mingw64/bin:$PATH ; export PATH ; if ! ccache -c ; then echo SKIP: Could not clean up ccache ; fi ; ccache --print-stats'
    if ($LASTEXITCODE -ne 0) { throw "ccache --print-stats exited with code $LASTEXITCODE" }
    $s = @{}
    $out | ForEach-Object {
        $kv = $_ -split "`t", 2
        if ($kv.Count -eq 2) { $s[$kv[0]] = [long]$kv[1] } else { echo $_ }
    }
    if ($s.Count -eq 0) { throw "no ccache statistics" }

    $hits = $s['direct_cache_hit'] + $s['preprocessed_cache_hit']
    $total = $hits + $s['cache_miss']
    $pct = 0
    if ($total -gt 0) { $pct = [math]::Round(100.0 * $hits / $total, 1) }
    # Invariant culture: same number formatting whatever the worker locale is
    $msg = [string]::Format([System.Globalization.CultureInfo]::InvariantCulture,
        "ccache: {0} of {1} compilations were cache hits ({2}%); cache size {3:F0} of {4:F0} MiB after cleanup",
        $hits, $total, $pct, ($s['cache_size_kibibyte'] / 1024), ($s['max_cache_size_kibibyte'] / 1024))
    echo $msg
    Add-AppveyorMessage $msg
} catch {
    # Cleanup and statistics are best-effort, do not fail the build over them
    echo "SKIP: Could not report ccache statistics: $_"
    $global:LASTEXITCODE = 0
}
