param([string]$Log = "C:\Users\minec\Documents\MC PSP Project\Source Code V3.0.0\PSP Minecraft Source Code\Log.txt")
# The game writes perf lines with no trailing newline, so several land on ONE physical line and a
# line-anchored parse sees only the first. Match every occurrence in the whole text instead.
$raw = Get-Content $Log -Raw
$rx = [regex]'perf (\d+)s frames=\d+ fps=(\d+) worstFrame=(\d+)ms.*?scans=(\d+)\(\d+\) scanCells=(\d+) floods=(\d+) floodCalls=(\d+) frontierOverflow=(\d+)'
$rows = @()
foreach ($m in $rx.Matches($raw)) {
    $rows += [pscustomobject]@{
        s=[int]$m.Groups[1].Value; fps=[int]$m.Groups[2].Value; wf=[int]$m.Groups[3].Value
        scans=[int]$m.Groups[4].Value; scanCells=[int]$m.Groups[5].Value
        floods=[int]$m.Groups[6].Value; floodCalls=[int]$m.Groups[7].Value; overflow=[int]$m.Groups[8].Value
    }
}
"seconds parsed: $($rows.Count)"
$rows | Sort-Object s | Format-Table -AutoSize
"--- maxima ---"
"worstFrame  : {0} ms" -f ($rows | Measure-Object wf  -Maximum).Maximum
"floods      : {0}"    -f ($rows | Measure-Object floods -Maximum).Maximum
"floodCalls  : {0}"    -f ($rows | Measure-Object floodCalls -Maximum).Maximum
"overflow    : {0}"    -f ($rows | Measure-Object overflow -Maximum).Maximum
"fps min     : {0}"    -f ($rows | Measure-Object fps -Minimum).Minimum
"fps median  : {0}"    -f ($rows | Sort-Object fps)[[int]($rows.Count/2)].fps
