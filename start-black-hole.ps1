[CmdletBinding()]
param(
    [ValidateRange(1024, 65535)][int]$Port = 4173,
    [switch]$Build,
    [switch]$NoOpen,
    [switch]$Browser
)
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$demoRoot = Join-Path $projectRoot 'web-demo'
$desktopExe = Join-Path $projectRoot 'out\desktop\KerrLab\KerrLab.exe'
if (-not $Browser -and -not $Build -and (Test-Path -LiteralPath $desktopExe)) {
    if (-not $NoOpen) { Start-Process -FilePath $desktopExe }
    return
}
$nodeCommand = Get-Command node -ErrorAction SilentlyContinue
$nodeExe = if ($nodeCommand) { $nodeCommand.Source } else {
    Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe'
}
if (-not (Test-Path -LiteralPath $nodeExe)) {
    throw 'Node.js is required. Install a current Node.js LTS release from https://nodejs.org and run this launcher again.'
}
$savedPath = $env:PATH
$env:PATH = (Split-Path $nodeExe) + ';' + $env:PATH
Push-Location -LiteralPath $demoRoot
try {
    if ($Build -or -not (Test-Path -LiteralPath (Join-Path $demoRoot 'dist\index.html')) -or (-not $Browser -and -not (Test-Path -LiteralPath $desktopExe))) {
        $npmCommand = Get-Command npm.cmd -ErrorAction SilentlyContinue
        $npmCli = Join-Path $projectRoot '.tools\npm-10.9.4\package\bin\npm-cli.js'
        if (-not $npmCommand -and -not (Test-Path -LiteralPath $npmCli)) {
            # Keep the package manager local; do not modify the system PATH.
            $npmRoot = Join-Path $projectRoot '.tools\npm-10.9.4'
            New-Item -ItemType Directory -Force -Path $npmRoot | Out-Null
            $metadata = Invoke-RestMethod -Uri 'https://registry.npmjs.org/npm/10.9.4'
            $archive = Join-Path $npmRoot 'npm.tgz'
            Invoke-WebRequest -UseBasicParsing -Uri $metadata.dist.tarball -OutFile $archive
            $hasher = [Security.Cryptography.SHA512]::Create()
            try { $actual = [Convert]::ToBase64String($hasher.ComputeHash([IO.File]::ReadAllBytes($archive))) }
            finally { $hasher.Dispose() }
            if ($metadata.dist.integrity -ne ('sha512-' + $actual)) { throw 'npm archive integrity mismatch.' }
            & tar -xzf $archive -C $npmRoot
            if ($LASTEXITCODE -ne 0) { throw 'npm extraction failed.' }
        }
        function Invoke-DemoNpm([string[]]$NpmArguments) {
            if ($npmCommand) { & $npmCommand.Source @NpmArguments }
            else { & $nodeExe $npmCli @NpmArguments }
            if ($LASTEXITCODE -ne 0) { throw "npm failed: $NpmArguments" }
        }
        if (-not (Test-Path -LiteralPath 'node_modules\vite\bin\vite.js') -or (-not $Browser -and -not (Test-Path -LiteralPath 'node_modules\electron\install.js'))) {
            Invoke-DemoNpm @('ci', '--ignore-scripts', '--no-audit', '--no-fund')
        }
        if (-not $Browser -and -not (Test-Path -LiteralPath 'node_modules\electron\dist\electron.exe')) {
            & $nodeExe node_modules\electron\install.js
            if ($LASTEXITCODE -ne 0) { throw 'Electron runtime installation failed.' }
        }
        Invoke-DemoNpm @('run', 'build')
        if (-not $Browser) {
            & $nodeExe (Join-Path $projectRoot 'tools\package-desktop.cjs')
            if ($LASTEXITCODE -ne 0) { throw 'Desktop packaging failed. Close KerrLab before rebuilding.' }
        }
    }
    if (-not $Browser) {
        if (-not $NoOpen) { Start-Process -FilePath $desktopExe }
        return
    }
    if (-not $NoOpen) { Start-Process "http://127.0.0.1:$Port/" }
    & $nodeExe (Join-Path $projectRoot 'tools\serve-black-hole.cjs') $Port
    if ($LASTEXITCODE -ne 0) { throw 'The preview server stopped with an error.' }
}
finally {
    Pop-Location
    $env:PATH = $savedPath
}
