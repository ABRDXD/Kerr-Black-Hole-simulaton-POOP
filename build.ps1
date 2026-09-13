param(
    [ValidateSet('Debug','Release','Sanitize')]
    [string]$Configuration = 'Debug',
    [switch]$Vulkan,
    [switch]$Window,
    [switch]$RequireValidation
)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$cmakePath = Join-Path $projectRoot '.tools\cmake\bin\cmake.exe'
$ctestPath = Join-Path $projectRoot '.tools\cmake\bin\ctest.exe'
if (-not (Test-Path -LiteralPath $cmakePath)) {
    throw 'Local CMake is missing. See README.md for standard CMake build commands.'
}
$preset = 'windows-clang-' + $Configuration.ToLowerInvariant()
if ($Window) { $Vulkan = $true }
if ($RequireValidation -and -not $Vulkan) { throw '-RequireValidation requires -Vulkan or -Window' }
$previousLayerPath = $env:VK_LAYER_PATH
$previousLayerEnables = $env:VK_LAYER_ENABLES
$localLayers = Join-Path $projectRoot '.tools/vulkan-validation-1.4.321.1/Bin'
Push-Location -LiteralPath $projectRoot
try {
    if ($Vulkan) {
        if (-not $env:VK_LAYER_PATH -and (Test-Path -LiteralPath (Join-Path $localLayers 'VkLayer_khronos_validation.json'))) {
            $env:VK_LAYER_PATH = $localLayers
        }
        if (-not $env:VK_LAYER_ENABLES) { $env:VK_LAYER_ENABLES = 'VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT' }
    }
    $vulkanOption = if ($Vulkan) { 'ON' } else { 'OFF' }
    $validationOption = if ($RequireValidation) { 'ON' } else { 'OFF' }
    $windowOption = if ($Window) { 'ON' } else { 'OFF' }
    $headers = Join-Path $projectRoot '.tools/Vulkan-Headers-2cd90f9d20df57eac214c148f3aed885372ddcfe/include'
    $headerOption = @()
    if (Test-Path -LiteralPath $headers) { $headerOption = @("-DKERR_VULKAN_INCLUDE_DIR=$headers") }
    & $cmakePath --preset $preset "-DKERR_BUILD_VULKAN=$vulkanOption" "-DKERR_BUILD_RENDERER=$windowOption" "-DKERR_REQUIRE_GPU_VALIDATION=$validationOption" @headerOption
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
    & $cmakePath --build --preset $preset
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
    & $ctestPath --preset $preset --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Validation failed' }
} finally {
    $env:VK_LAYER_PATH = $previousLayerPath
    $env:VK_LAYER_ENABLES = $previousLayerEnables
    Pop-Location
}
