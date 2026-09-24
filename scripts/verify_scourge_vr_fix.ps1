param(
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$ErrorActionPreference = 'Stop'

function Assert-Contains {
    param(
        [string]$Path,
        [string]$Pattern,
        [string]$Message
    )

    $content = Get-Content -Raw -LiteralPath (Join-Path $Root $Path)
    if ($content -notmatch $Pattern) {
        throw $Message
    }
}

function Assert-NotContains {
    param(
        [string]$Path,
        [string]$Pattern,
        [string]$Message
    )

    $content = Get-Content -Raw -LiteralPath (Join-Path $Root $Path)
    if ($content -match $Pattern) {
        throw $Message
    }
}

Assert-Contains 'CMakeLists.txt' 'project\(\s*GLXRM_SCOURGE\s+VERSION' 'CMake project/output name must be GLXRM_SCOURGE.'
Assert-Contains 'src\PapyrusManager.cpp' 'QueueActorRecalculation' 'Actor recalculation must be queued through a main-thread task helper.'
Assert-Contains 'src\PapyrusManager.cpp' 'F4SE::GetTaskInterface\(\)->AddTask' 'Actor recalculation must use the F4SE task interface.'
Assert-Contains 'src\PapyrusManager.cpp' 'QueueGlobalRecalculation' 'Global recalculation must be queued through a main-thread task helper.'
Assert-Contains 'src\PapyrusManager.cpp' 'RunGlobalRecalculation\(a_skipAlreadyTracked\)' 'Global recalculation must execute actor updates inside the queued task.'
Assert-Contains 'src\PapyrusManager.cpp' 'middleHighActorHandles' 'Global recalculation must scan every process-list bucket, not only high actors.'
Assert-Contains 'src\PapyrusManager.cpp' 'middleLowActorHandles' 'Global recalculation must scan every process-list bucket, not only high actors.'
Assert-Contains 'src\PapyrusManager.cpp' 'RunActorRecalculation\(currentActor,\s*a_skipAlreadyTracked\)' 'Global recalculation must execute actor updates inside the queued task.'
Assert-NotContains 'src\PapyrusManager.cpp' 'SCOURGE_Recalculate_Actor\(std::monostate\{\}, currentActor\)' 'Global recalculation must not call the Papyrus native directly.'

Write-Host 'SCOURGE VR regression checks passed.'
