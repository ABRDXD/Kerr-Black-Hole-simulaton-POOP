param(
    [ValidateSet('Debug','Release','Sanitize')][string]$Configuration = 'Release',
    [int]$Device = -1,
    [string]$Csv = '',
    [switch]$Window,
    [string]$Capture = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $projectRoot ('out/build/windows-clang-'+$Configuration.ToLowerInvariant())
$layerRoot = Join-Path $projectRoot '.tools/vulkan-validation-1.4.321.1/Bin'
$previousLayerPath = $env:VK_LAYER_PATH
$previousLayerEnables = $env:VK_LAYER_ENABLES
try {
    if (-not $env:VK_LAYER_PATH -and (Test-Path -LiteralPath (Join-Path $layerRoot 'VkLayer_khronos_validation.json'))) {
        $env:VK_LAYER_PATH = $layerRoot
    }
    if (-not $env:VK_LAYER_ENABLES) { $env:VK_LAYER_ENABLES = 'VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT' }
    $validator = Join-Path $layerRoot 'spirv-val.exe'
    if (Test-Path -LiteralPath $validator) {
        & $validator --target-env vulkan1.3 (Join-Path $buildRoot 'shaders/metric_v1.spv')
        if ($LASTEXITCODE -ne 0) { throw 'SPIR-V validation failed' }
        if ($Window) {
            & $validator --target-env vulkan1.3 (Join-Path $buildRoot 'shaders/presentation_test.spv')
            if ($LASTEXITCODE -ne 0) { throw 'Presentation SPIR-V validation failed' }
        }
    }
    $arguments = @('--validation','on')
    if ($Device -ge 0) { $arguments += @('--device',$Device.ToString()) }
    elseif ($Device -ne -1) { throw '-Device must be nonnegative or -1 (automatic)' }
    if ($Csv) { $arguments += @('--csv',$Csv) }
    & (Join-Path $buildRoot 'kerr_gpu.exe') @arguments
    if ($LASTEXITCODE -ne 0) { throw 'GPU validation failed' }
    if ($Window) {
        $arguments = @('--smoke','--validation','on')
        if ($Device -ge 0) { $arguments += @('--device',$Device.ToString()) }
        if (-not $Capture) { $Capture = Join-Path $buildRoot ('captures/device-'+$Device+'/final.ppm') }
        $arguments += @('--capture',$Capture)
        & (Join-Path $buildRoot 'kerr_window.exe') @arguments
        if ($LASTEXITCODE -ne 0) { throw 'Window validation failed or was cancelled before completion' }
    } elseif ($Capture) { throw '-Capture requires -Window' }
} finally {
    $env:VK_LAYER_PATH = $previousLayerPath
    $env:VK_LAYER_ENABLES = $previousLayerEnables
}
