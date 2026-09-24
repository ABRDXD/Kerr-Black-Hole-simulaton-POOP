param([string]$Python = '', [switch]$Validation, [switch]$Window)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolRoot = Join-Path $projectRoot '.tools'
$downloadRoot = Join-Path $toolRoot 'downloads'
New-Item -ItemType Directory -Path $downloadRoot -Force | Out-Null
$dependencies = @(
    @{ Owner='KhronosGroup'; Name='Vulkan-Headers'; Revision='2cd90f9d20df57eac214c148f3aed885372ddcfe'; Hash='004547a27896a80eef9d9a172e0a99f528fcee4301c899097143a1a14e9a77fd' },
    @{ Owner='KhronosGroup'; Name='glslang'; Revision='46ef757e048e760b46601e6e77ae0cb72c97bd2f'; Hash='913ba37e638ef573023e12e79e194e91beb4297b51016fdad6e4ba0b19cb62e0' }
)
if ($Window) {
    $dependencies += @{ Owner='glfw'; Name='glfw'; Revision='7b6aead9fb88b3623e3b3725ebb42670cbe4c579'; Hash='699bf0b3d0bd422c0212263f30c4b6fc1ab4f67320824b27854f1e5c6949a2a0' }
}
foreach ($dependency in $dependencies) {
    $archive = Join-Path $downloadRoot ($dependency.Name+'-'+$dependency.Revision+'.zip')
    if (-not (Test-Path -LiteralPath $archive)) {
        Invoke-WebRequest -Uri ('https://codeload.github.com/'+$dependency.Owner+'/'+$dependency.Name+'/zip/'+$dependency.Revision) -OutFile $archive
    }
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash -ne $dependency.Hash) {
        throw "Archive checksum mismatch: $archive. Inspect/remove this archive before retrying."
    }
    Expand-Archive -LiteralPath $archive -DestinationPath $toolRoot -Force
}
if (-not $Python) {
    $runtimePython = Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
    if (Test-Path -LiteralPath $runtimePython) { $Python = $runtimePython }
    else {
        $command = Get-Command python -ErrorAction SilentlyContinue
        if ($command) { $Python = $command.Source }
    }
}
if (-not $Python -or -not (Test-Path -LiteralPath $Python)) { throw 'Provide a working Python 3 executable with -Python PATH.' }
& $Python --version
if ($LASTEXITCODE -ne 0) { throw 'Python 3 must be installed to build glslang.' }
$cmake = Join-Path $toolRoot 'cmake/bin/cmake.exe'
$compiler = Join-Path $toolRoot 'llvm-mingw/bin'
$glslangBuild = Join-Path $toolRoot 'glslang-build'
& $cmake -S (Join-Path $toolRoot 'glslang-46ef757e048e760b46601e6e77ae0cb72c97bd2f') -B $glslangBuild -G Ninja `
    '-DCMAKE_BUILD_TYPE=Release' "-DCMAKE_C_COMPILER=$compiler/clang.exe" "-DCMAKE_CXX_COMPILER=$compiler/clang++.exe" `
    "-DCMAKE_MAKE_PROGRAM=$toolRoot/ninja/ninja.exe" "-DPython3_EXECUTABLE=$Python" `
    -DENABLE_OPT=OFF -DBUILD_EXTERNAL=OFF -DGLSLANG_TESTS=OFF -DENABLE_HLSL=OFF -DENABLE_SPVREMAPPER=OFF
if ($LASTEXITCODE -ne 0) { throw 'glslang configuration failed' }
& $cmake --build $glslangBuild --target glslang-standalone -j 6
if ($LASTEXITCODE -ne 0) { throw 'glslang build failed' }
foreach ($runtime in @('libc++.dll','libunwind.dll','libwinpthread-1.dll')) {
    Copy-Item -LiteralPath (Join-Path $compiler $runtime) -Destination (Join-Path $glslangBuild 'StandAlone')
}
& (Join-Path $glslangBuild 'StandAlone/glslang.exe') --version
if ($LASTEXITCODE -ne 0) { throw 'glslang launch failed' }
if ($Validation) {
    $archive = Join-Path $downloadRoot 'VulkanSDK-1.4.321.1-Installer.exe'
    if (-not (Test-Path -LiteralPath $archive)) {
        Invoke-WebRequest -Uri 'https://sdk.lunarg.com/sdk/download/1.4.321.1/windows/vulkan_sdk.exe' -OutFile $archive
    }
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash -ne 'baaa4f7ca11ed3d82aa1c102b21208915485bbaa473068c763daa425cca468bd') {
        throw 'Vulkan SDK archive checksum mismatch.'
    }
    $sevenZip = Join-Path $env:ProgramFiles '7-Zip/7z.exe'
    if (-not (Test-Path -LiteralPath $sevenZip)) { throw 'Install 7-Zip to extract the optional local validation tools.' }
    $validationRoot = Join-Path $toolRoot 'vulkan-validation-1.4.321.1'
    & $sevenZip x $archive "-o$validationRoot" '-i!Bin\VkLayer_khronos_validation.dll' '-i!Bin\VkLayer_khronos_validation.json' '-i!Bin\spirv-val.exe' '-i!Bin\spirv-dis.exe' -y
    if ($LASTEXITCODE -gt 1) { throw 'Validation tools extraction failed' }
    if (-not (Test-Path -LiteralPath (Join-Path $validationRoot 'Bin/VkLayer_khronos_validation.dll'))) { throw 'Validation layer was not extracted' }
    # Only extract local tools. Never execute the SDK installer or change registry/system PATH.
}
Write-Output 'Pinned dependencies ready. Run .\build.ps1 -Configuration Release -Vulkan (or -Window for presentation)'

// refactor: polish bootstrap-vulkan.ps1 implementation details
