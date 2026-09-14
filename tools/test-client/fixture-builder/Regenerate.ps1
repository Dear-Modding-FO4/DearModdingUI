[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$toolsRoot = Join-Path $repoRoot '.Build\fixture-tools'
$downloads = Join-Path $toolsRoot 'downloads'
$dependencies = Import-PowerShellDataFile (Join-Path $PSScriptRoot 'Dependencies.psd1')
$fixtureData = Join-Path $repoRoot 'tools\shared\fixtures\mcm\data'
$scripts = Join-Path $PSScriptRoot 'scripts'

function Confirm-Hash {
    param(
        [Parameter(Mandatory)]
        [string] $Path,
        [Parameter(Mandatory)]
        [ValidateSet('SHA256', 'SHA512')]
        [string] $Algorithm,
        [Parameter(Mandatory)]
        [string] $Expected
    )

    $actual = (Get-FileHash -LiteralPath $Path -Algorithm $Algorithm).Hash.ToLowerInvariant()
    if ($actual -ne $Expected.ToLowerInvariant()) {
        throw "$Algorithm mismatch for $Path. Expected $Expected, got $actual."
    }
}

function Get-PinnedFile {
    param(
        [Parameter(Mandatory)]
        [hashtable] $Dependency,
        [Parameter(Mandatory)]
        [string] $FileName,
        [Parameter(Mandatory)]
        [ValidateSet('SHA256', 'SHA512')]
        [string] $Algorithm
    )

    $path = Join-Path $downloads $FileName
    if (-not (Test-Path -LiteralPath $path)) {
        & curl.exe -L --fail --silent --show-error --output $path $Dependency.Url
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to download $($Dependency.Url)."
        }
    }

    Confirm-Hash -Path $path -Algorithm $Algorithm -Expected $Dependency["Sha$($Algorithm.Substring(3))"]
    return $path
}

function Confirm-Contains {
    param(
        [Parameter(Mandatory)]
        [string] $Text,
        [Parameter(Mandatory)]
        [string[]] $Patterns,
        [Parameter(Mandatory)]
        [string] $Description
    )

    foreach ($pattern in $Patterns) {
        if ($Text -notmatch $pattern) {
            throw "$Description is missing expected pattern: $pattern"
        }
    }
}

function Get-OptionalProperty {
    param(
        [Parameter(Mandatory)]
        [object] $InputObject,
        [Parameter(Mandatory)]
        [string] $Name
    )

    $property = $InputObject.PSObject.Properties[$Name]
    if ($null -eq $property) {
        return $null
    }
    return $property.Value
}

New-Item -ItemType Directory -Force $downloads | Out-Null

$capricaArchive = Get-PinnedFile `
    -Dependency $dependencies.Caprica `
    -FileName "Caprica.v$($dependencies.Caprica.Version).7z" `
    -Algorithm SHA256
$champollionArchive = Get-PinnedFile `
    -Dependency $dependencies.Champollion `
    -FileName "Champollion.v$($dependencies.Champollion.Version).zip" `
    -Algorithm SHA256
$sdkArchive = Get-PinnedFile `
    -Dependency $dependencies.DotNetSdk `
    -FileName "dotnet-sdk-$($dependencies.DotNetSdk.Version)-win-x64.zip" `
    -Algorithm SHA512
$mutagenPackage = Get-PinnedFile `
    -Dependency $dependencies.MutagenBethesdaFallout4 `
    -FileName "Mutagen.Bethesda.Fallout4.$($dependencies.MutagenBethesdaFallout4.Version).nupkg" `
    -Algorithm SHA256

$capricaRoot = Join-Path $toolsRoot 'caprica'
$caprica = Join-Path $capricaRoot 'Caprica.exe'
if (-not (Test-Path -LiteralPath $caprica)) {
    New-Item -ItemType Directory -Force $capricaRoot | Out-Null
    & tar.exe -xf $capricaArchive -C $capricaRoot
    if ($LASTEXITCODE -ne 0) {
        throw 'Failed to extract Caprica.'
    }
}

$champollionRoot = Join-Path $toolsRoot 'champollion'
$champollion = Join-Path $champollionRoot 'Champollion.exe'
if (-not (Test-Path -LiteralPath $champollion)) {
    New-Item -ItemType Directory -Force $champollionRoot | Out-Null
    Expand-Archive -LiteralPath $champollionArchive -DestinationPath $champollionRoot
}

$dotnetRoot = Join-Path $toolsRoot 'dotnet'
$dotnet = Join-Path $dotnetRoot 'dotnet.exe'
if (-not (Test-Path -LiteralPath $dotnet)) {
    New-Item -ItemType Directory -Force $dotnetRoot | Out-Null
    Expand-Archive -LiteralPath $sdkArchive -DestinationPath $dotnetRoot
}

$imports = Join-Path $toolsRoot 'papyrus-imports'
$compiled = Join-Path $toolsRoot 'papyrus-output'
$flags = Join-Path $toolsRoot 'PapyrusFlags.flg'
Remove-Item -Recurse -Force $imports, $compiled -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $imports, $compiled | Out-Null

[IO.File]::WriteAllText(
    (Join-Path $imports 'ScriptObject.psc'),
    "Scriptname ScriptObject Native Hidden`r`n`r`nEvent OnInit()`r`nEndEvent`r`n")
[IO.File]::WriteAllText(
    (Join-Path $imports 'Form.psc'),
    "Scriptname Form extends ScriptObject Native Hidden`r`n")
[IO.File]::WriteAllText(
    (Join-Path $imports 'Quest.psc'),
    "Scriptname Quest extends Form Native Hidden`r`n")
[IO.File]::WriteAllText(
    (Join-Path $imports 'Game.psc'),
    "Scriptname Game Native Hidden`r`n`r`nForm Function GetFormFromFile(Int aiFormID, String asFilename) Native Global`r`n")
[IO.File]::WriteAllText(
    (Join-Path $imports 'Debug.psc'),
    "Scriptname Debug Native Hidden`r`n`r`nFunction Trace(String asTextToPrint, Int aiSeverity = 0) Native Global`r`n")
[IO.File]::WriteAllText($flags, "Flag Hidden 0 { Script Property }`r`n")

Push-Location $scripts
try {
    & $caprica `
        --game=fallout4 `
        --strict `
        --all-warnings-as-errors `
        --release `
        --final `
        --enable-debug-info=0 `
        --ignorecwd `
        "--flags=$flags" `
        "--import=$scripts;$imports" `
        "--output=$compiled" `
        'DMUITestQuest.psc' `
        'DMUITestFunctions.psc'
    if ($LASTEXITCODE -ne 0) {
        throw 'Caprica failed to compile the Papyrus sources.'
    }
}
finally {
    Pop-Location
}

$env:DOTNET_CLI_HOME = $toolsRoot
$env:DOTNET_CLI_TELEMETRY_OPTOUT = '1'
$env:NUGET_PACKAGES = Join-Path $toolsRoot 'nuget-packages'
$project = Join-Path $PSScriptRoot 'FixtureBuilder.csproj'
$intermediate = Join-Path $toolsRoot 'fixture-builder-obj'
$binaryOutput = Join-Path $toolsRoot 'fixture-builder-bin'
$validationRoot = Join-Path $toolsRoot 'fixture-validation'
$staging = Join-Path $validationRoot 'staging'
$validationAssembly = Join-Path $validationRoot 'assembly'
$validationSource = Join-Path $validationRoot 'source'
Remove-Item -Recurse -Force $validationRoot -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $staging, $validationAssembly, $validationSource | Out-Null

& $dotnet restore `
    $project `
    --locked-mode `
    --packages $env:NUGET_PACKAGES `
    "-p:BaseIntermediateOutputPath=$intermediate\"
if ($LASTEXITCODE -ne 0) {
    throw 'Failed to restore the pinned Mutagen fixture generator.'
}

& $dotnet run `
    --project $project `
    --no-restore `
    "-p:BaseIntermediateOutputPath=$intermediate\" `
    "-p:BaseOutputPath=$binaryOutput\" `
    -- `
    $compiled `
    $fixtureData `
    $staging
if ($LASTEXITCODE -ne 0) {
    throw 'Fixture generation or Mutagen ESP validation failed.'
}

$questPex = Join-Path $staging 'Scripts\DMUITestQuest.pex'
$functionsPex = Join-Path $staging 'Scripts\DMUITestFunctions.pex'
$questInfo = (& $champollion --print-info $questPex 2>&1) -join "`n"
$functionsInfo = (& $champollion --print-info $functionsPex 2>&1) -join "`n"
if ($LASTEXITCODE -ne 0) {
    throw 'Champollion could not parse the compiled PEX headers.'
}

Confirm-Contains -Text $questInfo -Description 'DMUITestQuest.pex header' -Patterns @(
    'Game:\s+Fallout 4',
    'Game Version:\s+3\.9',
    'GameID:\s+2',
    'Compilation Time:\s+0\b',
    '(?m)Source File:[ \t]+DMUITestQuest\.psc\r?$',
    '(?m)User Name:[ \t]*\r?$',
    '(?m)Computer Name:[ \t]*\r?$'
)
Confirm-Contains -Text $functionsInfo -Description 'DMUITestFunctions.pex header' -Patterns @(
    'Game:\s+Fallout 4',
    'Game Version:\s+3\.9',
    'GameID:\s+2',
    'Compilation Time:\s+0\b',
    '(?m)Source File:[ \t]+DMUITestFunctions\.psc\r?$',
    '(?m)User Name:[ \t]*\r?$',
    '(?m)Computer Name:[ \t]*\r?$'
)

& $champollion `
    --asm $validationAssembly `
    --psc $validationSource `
    $questPex `
    $functionsPex | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw 'Champollion could not disassemble the compiled PEX files.'
}

$questAssembly = [IO.File]::ReadAllText((Join-Path $validationAssembly 'DMUITestQuest.pas'))
$functionsAssembly = [IO.File]::ReadAllText((Join-Path $validationAssembly 'DMUITestFunctions.pas'))
Confirm-Contains -Text $questAssembly -Description 'DMUITestQuest.pex assembly' -Patterns @(
    '\.object DMUITestQuest Quest',
    '\.property Enabled Bool auto',
    '\.property IntegerValue Int auto',
    '\.property FloatValue Float auto',
    '\.property TextValue String auto',
    '\.property ChoiceValue Int auto',
    '\.property ActionCount Int auto',
    '\.property LastAction String auto',
    '\.function OnInit',
    '\.function IncrementCounter',
    '\.function RecordGlobalAction',
    '\.function SetIntegerValue',
    '\.function ResetFixture',
    'assign ::LastAction_var "Member action"',
    'assign ::LastAction_var "Global action"'
)
Confirm-Contains -Text $functionsAssembly -Description 'DMUITestFunctions.pex assembly' -Patterns @(
    '\.object DMUITestFunctions ScriptObject',
    '\.function IncrementCounter',
    'callstatic game GetFormFromFile .* 2048 "DMUITests\.esp"',
    'callmethod RecordGlobalAction',
    'callstatic debug Trace .* "DMUITests: could not resolve DMUITestQuest from DMUITests\.esp\|800" 2'
)

$canonicalConfigPath = Join-Path $fixtureData 'MCM\Config\DMUITests\config.json'
$canonicalKeybindsPath = Join-Path $fixtureData 'MCM\Config\DMUITests\keybinds.json'
$canonicalConfig = Get-Content -Raw -LiteralPath $canonicalConfigPath | ConvertFrom-Json
$canonicalKeybinds = Get-Content -Raw -LiteralPath $canonicalKeybindsPath | ConvertFrom-Json
if ($canonicalConfig.modName -ne 'DMUITests' -or $canonicalKeybinds.modName -ne 'DMUITests') {
    throw 'Canonical config and keybinds must both use modName DMUITests.'
}
$assemblyByScript = @{
    DMUITestQuest = $questAssembly
    DMUITestFunctions = $functionsAssembly
}
$referencedActions = [Collections.Generic.List[object]]::new()

foreach ($page in $canonicalConfig.pages) {
    if ($page.id -eq 'failures') {
        continue
    }

    foreach ($item in $page.content) {
        $action = Get-OptionalProperty -InputObject $item -Name 'action'
        if ($null -eq $action) {
            continue
        }

        $configuredParams = Get-OptionalProperty -InputObject $action -Name 'params'
        $paramCount = if ($null -eq $configuredParams) { 0 } else { @($configuredParams).Count }
        if ($action.type -eq 'CallFunction') {
            $configuredScriptName = Get-OptionalProperty -InputObject $action -Name 'scriptName'
            $scriptName = if ($configuredScriptName) {
                $configuredScriptName
            }
            else {
                'DMUITestQuest'
            }
            $referencedActions.Add([pscustomobject]@{
                Script = $scriptName
                Function = $action.function
                ParamCount = $paramCount
                Source = "config.json page '$($page.id)' item '$($item.id)'"
            })
        }
        elseif ($action.type -eq 'CallGlobalFunction') {
            $referencedActions.Add([pscustomobject]@{
                Script = $action.script
                Function = $action.function
                ParamCount = $paramCount
                Source = "config.json page '$($page.id)' item '$($item.id)'"
            })
        }
    }
}

foreach ($keybind in $canonicalKeybinds.keybinds) {
    if ($keybind.action.type -eq 'CallGlobalFunction') {
        $configuredParams = Get-OptionalProperty -InputObject $keybind.action -Name 'params'
        $paramCount = if ($null -eq $configuredParams) { 0 } else { @($configuredParams).Count }
        $referencedActions.Add([pscustomobject]@{
            Script = $keybind.action.script
            Function = $keybind.action.function
            ParamCount = $paramCount
            Source = "keybinds.json keybind '$($keybind.id)'"
        })
    }
}

foreach ($reference in $referencedActions) {
    if (-not $assemblyByScript.ContainsKey($reference.Script)) {
        throw "$($reference.Source) references missing compiled script $($reference.Script)."
    }

    $escapedFunction = [Regex]::Escape($reference.Function)
    $functionPattern = '(?ms)^\s*\.function\s+' + $escapedFunction +
        '\s*\r?\n(?<Body>.*?)^\s*\.endFunction\s*;' + $escapedFunction + '\s*$'
    $functionMatch = [Regex]::Match($assemblyByScript[$reference.Script], $functionPattern)
    if (-not $functionMatch.Success) {
        throw "$($reference.Source) references missing compiled function $($reference.Script).$($reference.Function)."
    }

    $compiledParamCount = [Regex]::Matches(
        $functionMatch.Groups['Body'].Value,
        '(?m)^\s*\.param\s+'
    ).Count
    if ($compiledParamCount -ne $reference.ParamCount) {
        throw "$($reference.Source) passes $($reference.ParamCount) parameters to " +
            "$($reference.Script).$($reference.Function), which declares $compiledParamCount."
    }
}

$fixtureScripts = Join-Path $fixtureData 'Scripts'
New-Item -ItemType Directory -Force $fixtureScripts | Out-Null
Copy-Item -LiteralPath (Join-Path $staging 'DMUITests.esp') -Destination $fixtureData -Force
Copy-Item -LiteralPath $questPex -Destination $fixtureScripts -Force
Copy-Item -LiteralPath $functionsPex -Destination $fixtureScripts -Force

Get-Item `
    (Join-Path $fixtureData 'DMUITests.esp'), `
    (Join-Path $fixtureScripts 'DMUITestQuest.pex'), `
    (Join-Path $fixtureScripts 'DMUITestFunctions.pex') |
    Select-Object FullName, Length
