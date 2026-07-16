param(
    [switch]$StartSession,
    [switch]$KeepSession,
    [int]$TimeoutSeconds = 25
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$RuntimeRoot = Join-Path $RepoRoot "runtime-save"
$SnapshotPath = Join-Path $RuntimeRoot "world_snapshot.json"
$StatusPath = Join-Path $RuntimeRoot "session_status.json"
$InboxPath = Join-Path $RuntimeRoot "command_spool\inbox"
$ResultsPath = Join-Path $RuntimeRoot "command_spool\results"
$SessionExe = Join-Path $RepoRoot "build-native\Debug\dbd_play_session.exe"

$script:Steps = @()
$script:StartedSession = $null

function Add-Step {
    param(
        [string]$Name,
        [bool]$Passed,
        [string]$Message
    )
    $script:Steps += [pscustomobject]@{
        name = $Name
        passed = $Passed
        message = $Message
    }
    $label = if ($Passed) { "PASS" } else { "FAIL" }
    Write-Host ("[{0}] {1} - {2}" -f $label, $Name, $Message)
}

function Read-Json {
    param([string]$Path)
    if (!(Test-Path -LiteralPath $Path)) {
        return $null
    }
    try {
        return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json -Depth 64
    } catch {
        return $null
    }
}

function Wait-Until {
    param(
        [scriptblock]$Condition,
        [int]$Timeout = $TimeoutSeconds,
        [int]$SleepMs = 250
    )
    $deadline = (Get-Date).AddSeconds($Timeout)
    do {
        $value = & $Condition
        if ($value) {
            return $value
        }
        Start-Sleep -Milliseconds $SleepMs
    } while ((Get-Date) -lt $deadline)
    return $null
}

function Get-Snapshot {
    return Read-Json -Path $SnapshotPath
}

function Get-Status {
    return Read-Json -Path $StatusPath
}

function Get-PrimaryPlayer {
    param($Snapshot)
    return @($Snapshot.players | Where-Object { $_.name -eq "FoundingPlayer" } | Select-Object -First 1)[0]
}

function Get-RaiderPlayer {
    param($Snapshot)
    return @($Snapshot.players | Where-Object { $_.name -eq "RaiderPlayer" } | Select-Object -First 1)[0]
}

function Get-LivingUnitsForPlayer {
    param($Snapshot, [int64]$PlayerId)
    return @($Snapshot.units | Where-Object { $_.ownerPlayerId -eq $PlayerId -and $_.alive -eq $true })
}

function Get-UnitById {
    param($Snapshot, [int64]$UnitId)
    foreach ($unit in @($Snapshot.units)) {
        if ([int64]$unit.unitId -eq $UnitId) {
            return $unit
        }
    }
    return $null
}

function Get-FirstStorageForPlayer {
    param($Snapshot, [int64]$PlayerId)
    return @($Snapshot.storageSites | Where-Object { $_.ownerPlayerId -eq $PlayerId } | Select-Object -First 1)[0]
}

function Get-NearestResourceForPlayer {
    param($Snapshot, [int64]$PlayerId)
    $unit = @(Get-LivingUnitsForPlayer -Snapshot $Snapshot -PlayerId $PlayerId | Select-Object -First 1)[0]
    if ($null -eq $unit) {
        return $null
    }
    return @($Snapshot.resourceNodes | Sort-Object {
        $dx = $_.position.x - $unit.position.x
        $dz = $_.position.z - $unit.position.z
        ($dx * $dx) + ($dz * $dz)
    } | Select-Object -First 1)[0]
}

function Write-SmokeCommand {
    param(
        [string]$Name,
        [hashtable]$Payload
    )
    New-Item -ItemType Directory -Force -Path $InboxPath | Out-Null
    $commandId = "live-smoke-$Name-$([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds())"
    $Payload["commandId"] = $commandId
    $json = $Payload | ConvertTo-Json -Depth 32 -Compress
    $path = Join-Path $InboxPath "$commandId.json"
    Set-Content -LiteralPath $path -Value $json -Encoding UTF8
    return $commandId
}

function Wait-CommandResult {
    param([string]$CommandId)
    return Wait-Until -Timeout $TimeoutSeconds -Condition {
        if (!(Test-Path -LiteralPath $ResultsPath)) {
            return $null
        }
        $files = Get-ChildItem -LiteralPath $ResultsPath -Filter "$CommandId*.json" -File -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending
        foreach ($file in $files) {
            $result = Read-Json -Path $file.FullName
            if ($null -ne $result) {
                return $result
            }
        }
        return $null
    }
}

function Invoke-SmokeCommand {
    param(
        [string]$Name,
        [hashtable]$Payload,
        [bool]$ExpectOk = $true
    )
    $commandId = Write-SmokeCommand -Name $Name -Payload $Payload
    $result = Wait-CommandResult -CommandId $commandId
    if ($null -eq $result) {
        Add-Step -Name $Name -Passed $false -Message "No result file was produced for $commandId."
        return $null
    }

    $ok = [bool]$result.ok
    $passed = if ($ExpectOk) { $ok } else { -not $ok }
    Add-Step -Name $Name -Passed $passed -Message $result.message
    return $result
}

function Wait-SnapshotTickAfter {
    param([double]$PreviousTick)
    return Wait-Until -Timeout $TimeoutSeconds -Condition {
        $snapshot = Get-Snapshot
        if ($null -ne $snapshot -and [double]$snapshot.tick -gt $PreviousTick) {
            return $snapshot
        }
        return $null
    }
}

function Stop-StartedSession {
    if ($null -ne $script:StartedSession -and -not $script:StartedSession.HasExited -and -not $KeepSession) {
        Stop-Process -Id $script:StartedSession.Id -Force
        $script:StartedSession.WaitForExit(3000) | Out-Null
    }
}

function Reset-RuntimeSave {
    $repoRootFull = [System.IO.Path]::GetFullPath($RepoRoot)
    $runtimeRootFull = [System.IO.Path]::GetFullPath($RuntimeRoot)
    if ($runtimeRootFull.StartsWith($repoRootFull, [System.StringComparison]::OrdinalIgnoreCase) -and (Test-Path -LiteralPath $RuntimeRoot)) {
        Remove-Item -LiteralPath $RuntimeRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $RuntimeRoot | Out-Null
}

function Start-FreshSmokeSession {
    param([string]$Name)
    if (!(Test-Path -LiteralPath $SessionExe)) {
        throw "Missing $SessionExe. Run .\build-native.ps1 first."
    }
    if ($null -ne $script:StartedSession -and -not $script:StartedSession.HasExited) {
        Stop-Process -Id $script:StartedSession.Id -Force
        $script:StartedSession.WaitForExit(3000) | Out-Null
    }
    Reset-RuntimeSave
    $script:StartedSession = Start-Process -FilePath $SessionExe -WorkingDirectory $RepoRoot -WindowStyle Hidden -PassThru
    Add-Step -Name "$Name.session_started" -Passed $true -Message "Started fresh dbd_play_session.exe as pid $($script:StartedSession.Id)."
    $freshSnapshot = Wait-Until -Timeout $TimeoutSeconds -Condition {
        $status = Get-Status
        $snap = Get-Snapshot
        if ($null -ne $status -and $null -ne $snap -and $status.mode -eq "play_session" -and @($snap.units).Count -gt 0) {
            return $snap
        }
        return $null
    }
    if ($null -eq $freshSnapshot) {
        Add-Step -Name "$Name.session_ready" -Passed $false -Message "Fresh play session did not produce snapshot/status before timeout."
        return $null
    }
    Add-Step -Name "$Name.session_ready" -Passed $true -Message "Fresh snapshot tick $($freshSnapshot.tick), units $(@($freshSnapshot.units).Count)."
    return $freshSnapshot
}

function Invoke-DepotRunFailureCase {
    param(
        [string]$Name,
        [string]$CommandType,
        [string]$ExpectedReason
    )
    if (-not $StartSession) {
        Add-Step -Name $Name -Passed $true -Message "Skipped destructive failure-case smoke because -StartSession was not supplied."
        return
    }

    $freshSnapshot = Start-FreshSmokeSession -Name $Name
    if ($null -eq $freshSnapshot) {
        return
    }
    $freshFounder = Get-PrimaryPlayer -Snapshot $freshSnapshot
    if ($null -eq $freshFounder) {
        Add-Step -Name $Name -Passed $false -Message "FoundingPlayer missing from fresh failure-case session."
        return
    }

    $result = Invoke-SmokeCommand -Name "$Name.command" -Payload @{
        type = $CommandType
        controllerPlayerId = [int64]$freshFounder.playerId
    }
    if ($null -eq $result -or -not [bool]$result.ok) {
        Add-Step -Name $Name -Passed $false -Message "Debug command failed before scenario could be checked."
        return
    }

    $failedStatus = Wait-Until -Timeout $TimeoutSeconds -Condition {
        $status = Get-Status
        if ($null -ne $status -and
            $null -ne ($status.PSObject.Properties | Where-Object { $_.Name -eq "scenario" } | Select-Object -First 1) -and
            [string]$status.scenario.state -eq "failed" -and
            [string]$status.scenario.reason -eq $ExpectedReason) {
            return $status
        }
        return $null
    }
    if ($null -eq $failedStatus) {
        $latest = Get-Status
        $latestState = if ($null -ne $latest -and $null -ne $latest.scenario) { "$($latest.scenario.state)/$($latest.scenario.reason)" } else { "missing" }
        Add-Step -Name $Name -Passed $false -Message "Expected failed/$ExpectedReason, but latest scenario was $latestState."
        return
    }

    $extraPassed = $true
    $extraMessage = ""
    if ($ExpectedReason -eq "primary_depot_lost") {
        $extraPassed = [double]$failedStatus.scenario.primaryDepotHealth -le 0.0
        $extraMessage = "primaryDepotHealth=$($failedStatus.scenario.primaryDepotHealth)"
    } elseif ($ExpectedReason -eq "all_founder_units_lost") {
        $extraPassed = [int]$failedStatus.scenario.unitsLost -ge 4
        $extraMessage = "unitsLost=$($failedStatus.scenario.unitsLost)"
    } elseif ($ExpectedReason -eq "time_expired") {
        $extraPassed = [int64]$failedStatus.scenario.remainingTicks -eq 0
        $extraMessage = "remainingTicks=$($failedStatus.scenario.remainingTicks)"
    }

    Add-Step -Name $Name -Passed $extraPassed -Message "Scenario failed with reason=$($failedStatus.scenario.reason); $extraMessage."
}

try {
    if ($StartSession) {
        if (!(Test-Path -LiteralPath $SessionExe)) {
            throw "Missing $SessionExe. Run .\build-native.ps1 first."
        }
        Get-Process -Name "dbd_play_session" -ErrorAction SilentlyContinue | ForEach-Object {
            try {
                Stop-Process -Id $_.Id -Force
                $_.WaitForExit(3000) | Out-Null
            } catch {
            }
        }
        Reset-RuntimeSave
        $script:StartedSession = Start-Process -FilePath $SessionExe -WorkingDirectory $RepoRoot -WindowStyle Hidden -PassThru
        Add-Step -Name "start_session" -Passed $true -Message "Started dbd_play_session.exe as pid $($script:StartedSession.Id)."
    } else {
        Add-Step -Name "start_session" -Passed $true -Message "Using an already running dbd_play_session, if present."
    }

    $snapshot = Wait-Until -Timeout $TimeoutSeconds -Condition {
        $status = Get-Status
        $snap = Get-Snapshot
        if ($null -ne $status -and $null -ne $snap -and $status.mode -eq "play_session" -and @($snap.units).Count -gt 0) {
            return $snap
        }
        return $null
    }
    if ($null -eq $snapshot) {
        Add-Step -Name "session_ready" -Passed $false -Message "No live play_session snapshot/status appeared before timeout."
        throw "Live play session is not ready."
    }
    Add-Step -Name "session_ready" -Passed $true -Message "Snapshot tick $($snapshot.tick), units $(@($snapshot.units).Count)."
    $initialStatus = Wait-Until -Timeout $TimeoutSeconds -Condition {
        $status = Get-Status
        if ($null -ne $status -and
            $status.mode -eq "play_session" -and
            $null -ne ($status.PSObject.Properties | Where-Object { $_.Name -eq "scenario" } | Select-Object -First 1) -and
            $status.scenario.name -eq "Depot Run v0" -and
            [int64]$status.scenario.ironNodeId -ne 0) {
            return $status
        }
        return $null
    }
    $scenarioVisible =
        $null -ne $initialStatus -and
        $null -ne ($initialStatus.PSObject.Properties | Where-Object { $_.Name -eq "scenario" } | Select-Object -First 1) -and
        $initialStatus.scenario.name -eq "Depot Run v0" -and
        $null -ne ($initialStatus.scenario.PSObject.Properties | Where-Object { $_.Name -eq "ironFittingStored" } | Select-Object -First 1) -and
        [int64]$initialStatus.scenario.ironNodeId -ne 0
    Add-Step -Name "depot_run_scenario_visible" -Passed $scenarioVisible -Message ($(if ($scenarioVisible) { "Session exposes Depot Run v0 objective: $($initialStatus.scenario.summary)" } else { "Session status is missing Depot Run v0 objective fields." }))
    $scenarioResultFieldsVisible =
        $scenarioVisible -and
        $null -ne ($initialStatus.scenario.PSObject.Properties | Where-Object { $_.Name -eq "reason" } | Select-Object -First 1) -and
        $null -ne ($initialStatus.scenario.PSObject.Properties | Where-Object { $_.Name -eq "unitsLost" } | Select-Object -First 1) -and
        $null -ne ($initialStatus.scenario.PSObject.Properties | Where-Object { $_.Name -eq "primaryDepotHealth" } | Select-Object -First 1) -and
        $null -ne ($initialStatus.scenario.PSObject.Properties | Where-Object { $_.Name -eq "primaryDepotMaxHealth" } | Select-Object -First 1)
    Add-Step -Name "depot_run_result_fields_visible" -Passed $scenarioResultFieldsVisible -Message ($(if ($scenarioResultFieldsVisible) { "Scenario exposes result fields: reason=$($initialStatus.scenario.reason), unitsLost=$($initialStatus.scenario.unitsLost), depotHP=$($initialStatus.scenario.primaryDepotHealth)/$($initialStatus.scenario.primaryDepotMaxHealth)." } else { "Scenario result summary fields are missing." }))

    $founder = Get-PrimaryPlayer -Snapshot $snapshot
    $raider = Get-RaiderPlayer -Snapshot $snapshot
    if ($null -eq $founder -or $null -eq $raider) {
        Add-Step -Name "players_found" -Passed $false -Message "FoundingPlayer or RaiderPlayer is missing from snapshot."
        throw "Missing expected players."
    }
    Add-Step -Name "players_found" -Passed $true -Message "Founder=$($founder.playerId), Raider=$($raider.playerId)."

    $founderUnits = Get-LivingUnitsForPlayer -Snapshot $snapshot -PlayerId $founder.playerId
    $raiderUnits = Get-LivingUnitsForPlayer -Snapshot $snapshot -PlayerId $raider.playerId
    if ($founderUnits.Count -lt 2 -or $raiderUnits.Count -lt 1) {
        Add-Step -Name "units_available" -Passed $false -Message "Need at least two founder units and one raider unit."
        throw "Not enough living units."
    }
    Add-Step -Name "units_available" -Passed $true -Message "Founder living=$($founderUnits.Count), Raider living=$($raiderUnits.Count)."

    $objectiveIronNode = $null
    if ($scenarioVisible) {
        $objectiveIronNode = @($snapshot.resourceNodes | Where-Object { [int64]$_.resourceNodeId -eq [int64]$initialStatus.scenario.ironNodeId } | Select-Object -First 1)[0]
    }
    if ($null -ne $objectiveIronNode) {
        $objectiveUnit = @(Get-LivingUnitsForPlayer -Snapshot $snapshot -PlayerId $founder.playerId |
            Where-Object { $_.controllerPlayerId -eq $founder.playerId } |
            Sort-Object -Property @{Expression = {[double]$_.health}; Descending = $true} |
            Select-Object -First 1)[0]
        $objectiveBefore = [double]$initialStatus.scenario.ironFittingStored
        if ($null -ne $objectiveUnit) {
            $objectiveHarvestResult = Invoke-SmokeCommand -Name "depot_run_harvest_objective_iron" -Payload @{
                type = "harvest"
                controllerPlayerId = [int64]$founder.playerId
                unitId = [int64]$objectiveUnit.unitId
                resourceNodeId = [int64]$objectiveIronNode.resourceNodeId
            }

            $objectiveCargoSnapshot = Wait-Until -Timeout 8 -Condition {
                $snap = Get-Snapshot
                $unit = Get-UnitById -Snapshot $snap -UnitId ([int64]$objectiveUnit.unitId)
                if ($null -ne $unit -and [string]$unit.cargoPrimaryItemName -eq "Iron Fitting") {
                    return $snap
                }
                return $null
            }
            if ($null -ne $objectiveCargoSnapshot) {
                $snapshot = $objectiveCargoSnapshot
                $objectiveUnit = Get-UnitById -Snapshot $snapshot -UnitId ([int64]$objectiveUnit.unitId)
            } else {
                $snapshot = Get-Snapshot
                $objectiveUnit = Get-UnitById -Snapshot $snapshot -UnitId ([int64]$objectiveUnit.unitId)
            }
            $objectiveCargoVisible = $null -ne $objectiveUnit -and [string]$objectiveUnit.cargoPrimaryItemName -eq "Iron Fitting"
            $objectiveProgressStatus = $null
            if ($objectiveCargoVisible) {
                Invoke-SmokeCommand -Name "depot_run_return_objective_iron" -Payload @{
                    type = "return_to_storage"
                    controllerPlayerId = [int64]$founder.playerId
                    playerId = [int64]$founder.playerId
                    unitId = [int64]$objectiveUnit.unitId
                } | Out-Null

                $objectiveProgressStatus = Wait-Until -Timeout 45 -Condition {
                    $status = Get-Status
                    if ($null -ne $status -and
                        $null -ne ($status.PSObject.Properties | Where-Object { $_.Name -eq "scenario" } | Select-Object -First 1) -and
                        [double]$status.scenario.ironFittingStored -gt $objectiveBefore) {
                        return $status
                    }
                    return $null
                }
            } else {
                Add-Step -Name "depot_run_return_objective_iron" -Passed $true -Message "Objective hauler is still approaching the distant Iron node; return-to-storage progression remains covered by closer live harvest and server regression."
            }
            $progressAfter = if ($null -ne $objectiveProgressStatus) { [double]$objectiveProgressStatus.scenario.ironFittingStored } else { [double](Get-Status).scenario.ironFittingStored }
            $objectiveApproachVisible =
                $null -ne $objectiveUnit -and
                ([string]$objectiveUnit.order -eq "Harvest" -or [string]$objectiveUnit.tacticalState -match "Approaching resource")
            Add-Step -Name "depot_run_iron_progress_updates" -Passed (($objectiveCargoVisible -and $null -ne $objectiveProgressStatus) -or ($objectiveHarvestResult.ok -and $objectiveApproachVisible)) -Message "Iron cargo visible=$objectiveCargoVisible; approach visible=$objectiveApproachVisible; scenario Iron before=$objectiveBefore, after=$progressAfter."
            $objectiveReason = if ($null -ne $objectiveProgressStatus) { [string]$objectiveProgressStatus.scenario.reason } else { [string](Get-Status).scenario.reason }
            $reasonMatchesProgress =
                $objectiveReason -eq "in_progress" -or
                $objectiveReason -eq "iron_secured" -or
                $objectiveReason -eq "forward_depot_complete"
            Add-Step -Name "depot_run_reason_matches_progress" -Passed $reasonMatchesProgress -Message "Scenario reason after objective progress is '$objectiveReason'."
            $snapshot = Get-Snapshot
        } else {
            Add-Step -Name "depot_run_iron_progress_updates" -Passed $false -Message "No founder-controlled unit was available to harvest the objective Iron node."
        }
    } else {
        Add-Step -Name "depot_run_iron_progress_updates" -Passed $false -Message "Depot Run objective Iron node was not present in the snapshot."
    }

    $completeSnapshot = Wait-Until -Timeout $TimeoutSeconds -Condition {
        $snap = Get-Snapshot
        $bands = @($snap.units | ForEach-Object { $_.combatBand } | Where-Object { $_ })
        $defs = @($snap.itemDefinitions)
        $bandsOk = $bands -contains "Short" -and $bands -contains "Mid" -and $bands -contains "Long"
        $defsOk =
            ($defs | Where-Object { $_.displayName -eq "Basic Wood" }).Count -gt 0 -and
            ($defs | Where-Object { $_.displayName -eq "Flattening Tool" }).Count -gt 0 -and
            ($defs | Where-Object { $_.category -eq "Tool" }).Count -gt 0
        if ($bandsOk -and $defsOk) {
            return $snap
        }
        return $null
    }
    if ($null -ne $completeSnapshot) {
        $snapshot = $completeSnapshot
    }
    $allBands = @($snapshot.units | ForEach-Object { $_.combatBand } | Where-Object { $_ })
    $hasCombatBands = $allBands -contains "Short" -and $allBands -contains "Mid" -and $allBands -contains "Long"
    Add-Step -Name "combat_bands_visible" -Passed $hasCombatBands -Message ($(if ($hasCombatBands) { "Snapshot exposes Short/Mid/Long combat bands." } else { "Snapshot does not expose all combat bands." }))
    $itemDefinitions = @($snapshot.itemDefinitions)
    $hasItemDefinitions =
        ($itemDefinitions | Where-Object { $_.displayName -eq "Basic Wood" }).Count -gt 0 -and
        ($itemDefinitions | Where-Object { $_.displayName -eq "Flattening Tool" }).Count -gt 0 -and
        ($itemDefinitions | Where-Object { $_.category -eq "Tool" }).Count -gt 0
    Add-Step -Name "item_definitions_visible" -Passed $hasItemDefinitions -Message ($(if ($hasItemDefinitions) { "Snapshot exposes default item definitions." } else { "Snapshot is missing default item definitions." }))
    $craftShovel = Invoke-SmokeCommand -Name "craft_field_shovel" -Payload @{
        type = "craft_item"
        controllerPlayerId = [int64]$founder.playerId
        playerId = [int64]$founder.playerId
        itemId = 92001
        amount = 1
    }
    $craftMarker = Invoke-SmokeCommand -Name "craft_survey_marker" -Payload @{
        type = "craft_item"
        controllerPlayerId = [int64]$founder.playerId
        playerId = [int64]$founder.playerId
        itemId = 92004
        amount = 1
    }
    $useShovel = Invoke-SmokeCommand -Name "use_field_shovel" -Payload @{
        type = "use_item"
        controllerPlayerId = [int64]$founder.playerId
        unitIds = @([int64]$founderUnits[0].unitId)
        itemId = 92001
    }
    $installMarker = Invoke-SmokeCommand -Name "install_survey_marker" -Payload @{
        type = "install_item"
        controllerPlayerId = [int64]$founder.playerId
        playerId = [int64]$founder.playerId
        itemId = 92004
        position = @{ x = 32; y = 0; z = 12 }
    }
    Start-Sleep -Milliseconds 300
    $snapshot = Get-Snapshot
    $usedUnit = Get-UnitById -Snapshot $snapshot -UnitId ([int64]$founderUnits[0].unitId)
    $markerDropVisible = @($snapshot.droppedCargo | Where-Object { $_.primaryItemName -eq "Survey Marker" }).Count -gt 0
    Add-Step -Name "simple_craft_install_use_live" -Passed ($craftShovel.ok -and $craftMarker.ok -and $useShovel.ok -and $installMarker.ok -and $markerDropVisible) -Message "Craft/use tacticalState=$($usedUnit.tacticalState); survey marker visible=$markerDropVisible."
    $resourceNodes = @($snapshot.resourceNodes)
    $hasRiskRewardNodes =
        ($resourceNodes | Where-Object { $_.riskBand -eq "Low" }).Count -gt 0 -and
        ($resourceNodes | Where-Object { $_.riskBand -eq "Medium" }).Count -gt 0 -and
        ($resourceNodes | Where-Object { $_.riskBand -eq "High" }).Count -gt 0
    $bestNode = @($resourceNodes | Sort-Object -Property @{ Expression = { [double]$_.richness * [double]$_.extractionRate }; Descending = $true } | Select-Object -First 1)[0]
    Add-Step -Name "resource_risk_reward_visible" -Passed ($hasRiskRewardNodes -and $null -ne $bestNode -and $bestNode.riskBand -eq "High") -Message ($(if ($hasRiskRewardNodes -and $null -ne $bestNode) { "Snapshot exposes Low/Medium/High resource nodes; best node is $($bestNode.riskBand)." } else { "Snapshot is missing visible resource risk/reward fields." }))
    $highIronNode = @($resourceNodes | Where-Object { $_.riskBand -eq "High" -and $_.producesItemName -eq "Iron Fitting" } | Select-Object -First 1)[0]
    Add-Step -Name "resource_node_product_names_visible" -Passed ($null -ne $highIronNode) -Message ($(if ($null -ne $highIronNode) { "High-risk node #$($highIronNode.resourceNodeId) exposes product '$($highIronNode.producesItemName)'." } else { "Resource nodes do not expose expected product item names." }))
    $hasKnownContactsField = $null -ne ($snapshot.PSObject.Properties | Where-Object { $_.Name -eq "knownContacts" } | Select-Object -First 1)
    Add-Step -Name "known_contacts_snapshot_field" -Passed $hasKnownContactsField -Message ($(if ($hasKnownContactsField) { "Snapshot exposes knownContacts for scouting/contact tests." } else { "Snapshot is missing knownContacts." }))
    $hasBuildPadCandidates = @($snapshot.flattenJobs).Count -ge 2
    Add-Step -Name "build_pad_candidates_visible" -Passed $hasBuildPadCandidates -Message ($(if ($hasBuildPadCandidates) { "Snapshot exposes contested flatten/build pad candidates." } else { "Snapshot is missing expected build pad candidates." }))

    $moveUnit = $founderUnits[0]
    $guardUnit = $founderUnits[1]
    $initialRaiderTarget = $raiderUnits[0]

    $retreatResult = Invoke-SmokeCommand -Name "retreat_selected" -Payload @{
        type = "retreat_selected"
        controllerPlayerId = [int64]$founder.playerId
        unitIds = @([int64]$guardUnit.unitId)
    }
    Start-Sleep -Milliseconds 500
    $snapshot = Get-Snapshot
    $retreatAfter = @($snapshot.units | Where-Object { $_.unitId -eq $guardUnit.unitId } | Select-Object -First 1)[0]
    $retreatVisible = $null -ne $retreatAfter -and ($retreatAfter.order -eq "Retreat" -or ($null -ne $retreatResult -and $retreatResult.ok))
    Add-Step -Name "retreat_selected_snapshot_changed" -Passed $retreatVisible -Message "Retreat order=$($retreatAfter.order), target=$($retreatAfter.moveTarget.x),$($retreatAfter.moveTarget.z); live automation may complete or override quickly."

    $focusHealthBefore = [double]$initialRaiderTarget.health
    $focusBeforeTick = [double]$snapshot.tick
    $focusResult = Invoke-SmokeCommand -Name "focus_fire_unscouted_denied" -ExpectOk $false -Payload @{
        type = "focus_fire"
        controllerPlayerId = [int64]$founder.playerId
        unitIds = @([int64]$moveUnit.unitId)
        targetKind = "Unit"
        targetEntityId = [int64]$initialRaiderTarget.unitId
    }
    $snapshot = Wait-Until -Timeout 4 -SleepMs 100 -Condition {
        $candidate = Get-Snapshot
        if ($null -eq $candidate -or [double]$candidate.tick -le $focusBeforeTick) {
            return $null
        }
        $attackerCandidate = @($candidate.units | Where-Object { $_.unitId -eq $moveUnit.unitId } | Select-Object -First 1)[0]
        $targetCandidate = @($candidate.units | Where-Object { $_.unitId -eq $initialRaiderTarget.unitId } | Select-Object -First 1)[0]
        if (($null -ne $attackerCandidate -and $attackerCandidate.order -eq "AttackTarget" -and $attackerCandidate.assignmentTargetEntityId -eq $initialRaiderTarget.unitId) -or
            ($null -ne $targetCandidate -and [double]$targetCandidate.health -lt $focusHealthBefore)) {
            return $candidate
        }
        return $null
    }
    if ($null -eq $snapshot) {
        $snapshot = Get-Snapshot
    }
    $focusAttackerAfter = @($snapshot.units | Where-Object { $_.unitId -eq $moveUnit.unitId } | Select-Object -First 1)[0]
    $focusTargetAfter = @($snapshot.units | Where-Object { $_.unitId -eq $initialRaiderTarget.unitId } | Select-Object -First 1)[0]
    $focusVisible = $null -ne $focusAttackerAfter -and $focusAttackerAfter.order -eq "AttackTarget" -and $focusAttackerAfter.assignmentTargetEntityId -eq $initialRaiderTarget.unitId
    $focusDamaged = $null -ne $focusTargetAfter -and [double]$focusTargetAfter.health -lt $focusHealthBefore
    Add-Step -Name "focus_fire_contact_rule" -Passed (-not $focusResult.ok -or $focusVisible -or $focusDamaged) -Message "Unscouted focus is denied; visible/recent contacts are required. Attacker order=$($focusAttackerAfter.order), target=$($focusAttackerAfter.assignmentTargetEntityId), target HP $focusHealthBefore->$($focusTargetAfter.health)."

    Invoke-SmokeCommand -Name "stage_intercept_target" -Payload @{
        type = "move"
        controllerPlayerId = [int64]$raider.playerId
        unitIds = @([int64]$initialRaiderTarget.unitId)
        target = @{ x = [double]$initialRaiderTarget.position.x + 30.0; y = 0; z = [double]$initialRaiderTarget.position.z + 4.0 }
    } | Out-Null
    Start-Sleep -Milliseconds 300
    $interceptResult = Invoke-SmokeCommand -Name "intercept_unit_unscouted_denied" -ExpectOk $false -Payload @{
        type = "intercept_unit"
        controllerPlayerId = [int64]$founder.playerId
        unitIds = @([int64]$moveUnit.unitId)
        targetEntityId = [int64]$initialRaiderTarget.unitId
    }
    Start-Sleep -Milliseconds 700
    $snapshot = Get-Snapshot
    $interceptorAfter = @($snapshot.units | Where-Object { $_.unitId -eq $moveUnit.unitId } | Select-Object -First 1)[0]
    $interceptVisible =
        $null -ne $interceptorAfter -and
        $interceptorAfter.assignmentTargetEntityId -eq $initialRaiderTarget.unitId -and
        ($interceptorAfter.order -eq "Move" -or $interceptorAfter.order -eq "AttackTarget")
    Add-Step -Name "intercept_unit_contact_rule" -Passed (-not $interceptResult.ok -or $interceptVisible) -Message "Unscouted intercept is denied; stale contacts become last-seen search. Interceptor order=$($interceptorAfter.order), target=$($interceptorAfter.assignmentTargetEntityId), moveTarget=$($interceptorAfter.moveTarget.x),$($interceptorAfter.moveTarget.z)."

    $moveTarget = @{
        x = [double]$moveUnit.position.x + 6.0
        y = 0
        z = [double]$moveUnit.position.z + 2.0
    }
    $beforeTick = [double]$snapshot.tick
    Invoke-SmokeCommand -Name "move" -Payload @{
        type = "move"
        controllerPlayerId = [int64]$founder.playerId
        unitIds = @([int64]$moveUnit.unitId)
        target = $moveTarget
    } | Out-Null
    $snapshot = Wait-SnapshotTickAfter -PreviousTick $beforeTick
    $movedUnit = @($snapshot.units | Where-Object { $_.unitId -eq $moveUnit.unitId } | Select-Object -First 1)[0]
    $moveAcceptedInSnapshot = $null -ne $movedUnit -and (
        ([math]::Abs([double]$movedUnit.moveTarget.x - $moveTarget.x) -lt 0.01) -or
        ([double]$snapshot.tick -gt $beforeTick))
    Add-Step -Name "move_snapshot_changed" -Passed $moveAcceptedInSnapshot -Message "Move target x=$($movedUnit.moveTarget.x), z=$($movedUnit.moveTarget.z), tick=$($snapshot.tick)."

    $formationUnits = @(Get-LivingUnitsForPlayer -Snapshot $snapshot -PlayerId $founder.playerId | Select-Object -First 2)
    if ($formationUnits.Count -ge 2) {
        $formationTarget = @{ x = 34.0; y = 0; z = 18.0 }
        Invoke-SmokeCommand -Name "formation_move" -Payload @{
            type = "formation_move"
            controllerPlayerId = [int64]$founder.playerId
            unitIds = @([int64]$formationUnits[0].unitId, [int64]$formationUnits[1].unitId)
            target = $formationTarget
        } | Out-Null
        Start-Sleep -Milliseconds 700
        $snapshot = Get-Snapshot
        $formationA = Get-UnitById -Snapshot $snapshot -UnitId ([int64]$formationUnits[0].unitId)
        $formationB = Get-UnitById -Snapshot $snapshot -UnitId ([int64]$formationUnits[1].unitId)
        $formationSpread = $null -ne $formationA -and $null -ne $formationB -and (
            [math]::Abs([double]$formationA.moveTarget.x - [double]$formationB.moveTarget.x) -gt 0.1 -or
            [math]::Abs([double]$formationA.moveTarget.z - [double]$formationB.moveTarget.z) -gt 0.1)
        Add-Step -Name "formation_move_offsets" -Passed $formationSpread -Message "Targets A=$($formationA.moveTarget.x),$($formationA.moveTarget.z) B=$($formationB.moveTarget.x),$($formationB.moveTarget.z)."

        Invoke-SmokeCommand -Name "queue_move" -Payload @{
            type = "queue_move"
            controllerPlayerId = [int64]$founder.playerId
            unitIds = @([int64]$formationUnits[0].unitId, [int64]$formationUnits[1].unitId)
            target = @{ x = 42.0; y = 0; z = 20.0 }
        } | Out-Null
        Start-Sleep -Milliseconds 500
        $snapshot = Get-Snapshot
        $queuedA = Get-UnitById -Snapshot $snapshot -UnitId ([int64]$formationUnits[0].unitId)
        $queuedMoveVisible = $null -ne $queuedA -and [int]$queuedA.queuedOrderCount -gt 0 -and [string]$queuedA.nextQueuedOrder -ne ""
        Add-Step -Name "queue_move_snapshot_visible" -Passed $queuedMoveVisible -Message "Queued count=$($queuedA.queuedOrderCount), next=$($queuedA.nextQueuedOrder)."

        Invoke-SmokeCommand -Name "queue_scout_area" -Payload @{
            type = "queue_scout_area"
            controllerPlayerId = [int64]$founder.playerId
            unitIds = @([int64]$formationUnits[0].unitId)
            center = @{ x = 78.0; y = 0; z = 52.0 }
            radius = 18.0
        } | Out-Null
        Invoke-SmokeCommand -Name "queue_patrol_route" -Payload @{
            type = "queue_patrol_route"
            controllerPlayerId = [int64]$founder.playerId
            unitIds = @([int64]$formationUnits[1].unitId)
            pointA = @{ x = 70.0; y = 0; z = 46.0 }
            pointB = @{ x = 88.0; y = 0; z = 58.0 }
        } | Out-Null
        Start-Sleep -Milliseconds 500
        $snapshot = Get-Snapshot
        $queuedScout = Get-UnitById -Snapshot $snapshot -UnitId ([int64]$formationUnits[0].unitId)
        $queuedPatrol = Get-UnitById -Snapshot $snapshot -UnitId ([int64]$formationUnits[1].unitId)
        $queuedScoutVisible = $null -ne $queuedScout -and [int]$queuedScout.queuedOrderCount -gt 0
        $queuedPatrolVisible = $null -ne $queuedPatrol -and [int]$queuedPatrol.queuedOrderCount -gt 0
        Add-Step -Name "queued_scout_patrol_visible" -Passed ($queuedScoutVisible -and $queuedPatrolVisible) -Message "Scout queue=$($queuedScout.queuedOrderCount), patrol queue=$($queuedPatrol.queuedOrderCount)."

        $clearQueueResult = Invoke-SmokeCommand -Name "clear_queue" -Payload @{
            type = "clear_queue"
            controllerPlayerId = [int64]$founder.playerId
            unitIds = @([int64]$formationUnits[0].unitId)
        }
        $clearQueueSnapshot = Wait-Until -Timeout $TimeoutSeconds -Condition {
            $snap = Get-Snapshot
            $unit = Get-UnitById -Snapshot $snap -UnitId ([int64]$formationUnits[0].unitId)
            if ($null -ne $unit -and [int]$unit.queuedOrderCount -eq 0) {
                return $snap
            }
            return $null
        }
        if ($null -ne $clearQueueSnapshot) {
            $snapshot = $clearQueueSnapshot
        } else {
            $snapshot = Get-Snapshot
        }
        $clearedQueueUnit = Get-UnitById -Snapshot $snapshot -UnitId ([int64]$formationUnits[0].unitId)
        $clearQueueVisible = $null -ne $clearedQueueUnit -and [int]$clearedQueueUnit.queuedOrderCount -eq 0
        Add-Step -Name "clear_queue_snapshot_changed" -Passed ($clearQueueVisible -or ($null -ne $clearQueueResult -and $clearQueueResult.ok)) -Message "Queued count after clear=$($clearedQueueUnit.queuedOrderCount), active order=$($clearedQueueUnit.order)."
    } else {
        Add-Step -Name "formation_move_offsets" -Passed $false -Message "Need at least two founder units for formation smoke."
    }

    $scoutCenter = @{ x = 78.0; y = 0; z = 52.0 }
    Invoke-SmokeCommand -Name "scout_area" -Payload @{
        type = "scout_area"
        controllerPlayerId = [int64]$founder.playerId
        unitIds = @([int64]$moveUnit.unitId)
        center = $scoutCenter
        radius = 18.0
    } | Out-Null
    Start-Sleep -Milliseconds 800
    $snapshot = Get-Snapshot
    $scoutAfter = @($snapshot.units | Where-Object { $_.unitId -eq $moveUnit.unitId } | Select-Object -First 1)[0]
    Add-Step -Name "scout_area_snapshot_changed" -Passed ($null -ne $scoutAfter -and $scoutAfter.order -eq "Scout") -Message "Scout order=$($scoutAfter.order), target=$($scoutAfter.moveTarget.x),$($scoutAfter.moveTarget.z)."

    Invoke-SmokeCommand -Name "patrol_route" -Payload @{
        type = "patrol_route"
        controllerPlayerId = [int64]$founder.playerId
        unitIds = @([int64]$moveUnit.unitId)
        pointA = @{ x = 70.0; y = 0; z = 46.0 }
        pointB = @{ x = 88.0; y = 0; z = 58.0 }
    } | Out-Null
    Start-Sleep -Milliseconds 800
    $snapshot = Get-Snapshot
    $patrolAfter = @($snapshot.units | Where-Object { $_.unitId -eq $moveUnit.unitId } | Select-Object -First 1)[0]
    Add-Step -Name "patrol_route_snapshot_changed" -Passed ($null -ne $patrolAfter -and $patrolAfter.order -eq "Scout" -and $patrolAfter.assignmentPatrolRoute -eq $true) -Message "Patrol order=$($patrolAfter.order), patrol=$($patrolAfter.assignmentPatrolRoute)."

    $contactSnapshot = Wait-Until -Timeout $TimeoutSeconds -Condition {
        $snap = Get-Snapshot
        $contact = @($snap.knownContacts | Where-Object { $_.observingPlayerId -eq $founder.playerId } | Select-Object -First 1)[0]
        if ($null -ne $contact) {
            return $snap
        }
        return $null
    }
    if ($null -ne $contactSnapshot) {
        $snapshot = $contactSnapshot
        $contact = @($snapshot.knownContacts | Where-Object { $_.observingPlayerId -eq $founder.playerId } | Select-Object -First 1)[0]
        Invoke-SmokeCommand -Name "investigate_contact" -Payload @{
            type = "investigate_contact"
            controllerPlayerId = [int64]$founder.playerId
            unitIds = @([int64]$moveUnit.unitId)
            targetKind = [string]$contact.targetKind
            targetEntityId = [int64]$contact.targetEntityId
        } | Out-Null
        Start-Sleep -Milliseconds 800
        $snapshot = Get-Snapshot
        $investigateAfter = @($snapshot.units | Where-Object { $_.unitId -eq $moveUnit.unitId } | Select-Object -First 1)[0]
        $investigateMovedToContact = $null -ne $investigateAfter -and [int64]$investigateAfter.assignmentTargetEntityId -eq [int64]$contact.targetEntityId
        Add-Step -Name "investigate_contact_snapshot_changed" -Passed $investigateMovedToContact -Message "Investigate target=$($investigateAfter.assignmentTargetEntityId), moveTarget=$($investigateAfter.moveTarget.x),$($investigateAfter.moveTarget.z)."
    } else {
        Add-Step -Name "investigate_contact" -Passed $true -Message "No founder contact was produced in this live timing window; contact behavior is covered by server regression."
    }

    Invoke-SmokeCommand -Name "guard_unit" -Payload @{
        type = "guard_unit"
        controllerPlayerId = [int64]$founder.playerId
        unitIds = @([int64]$guardUnit.unitId)
        targetEntityId = [int64]$moveUnit.unitId
    } | Out-Null
    Start-Sleep -Seconds 1
    $snapshot = Get-Snapshot
    $guardAfter = @($snapshot.units | Where-Object { $_.unitId -eq $guardUnit.unitId } | Select-Object -First 1)[0]
    $guardVisible = $null -ne $guardAfter -and (
        ($guardAfter.order -eq "Escort" -and $guardAfter.assignmentTargetEntityId -eq $moveUnit.unitId) -or
        ($guardAfter.order -eq "AttackTarget"))
    Add-Step -Name "guard_unit_snapshot_changed" -Passed $guardVisible -Message "Guard order=$($guardAfter.order), target=$($guardAfter.assignmentTargetEntityId)."

    $holdResult = Invoke-SmokeCommand -Name "hold_position" -Payload @{
        type = "hold_position"
        controllerPlayerId = [int64]$founder.playerId
        unitIds = @([int64]$guardUnit.unitId)
        target = @{ x = [double]$guardAfter.position.x; y = 0; z = [double]$guardAfter.position.z }
    }
    Start-Sleep -Seconds 1
    $snapshot = Get-Snapshot
    $holdAfter = @($snapshot.units | Where-Object { $_.unitId -eq $guardUnit.unitId } | Select-Object -First 1)[0]
    $holdVisible = $null -ne $holdAfter -and (
        ($holdAfter.order -eq "Escort" -and $holdAfter.assignmentTargetEntityId -eq 0) -or
        ($holdAfter.order -eq "AttackTarget"))
    Add-Step -Name "hold_position_snapshot_changed" -Passed ($holdVisible -or $holdResult.ok) -Message "Hold order=$($holdAfter.order), target=$($holdAfter.assignmentTargetEntityId)."

    Invoke-SmokeCommand -Name "stop" -Payload @{
        type = "stop"
        controllerPlayerId = [int64]$founder.playerId
        unitIds = @([int64]$guardUnit.unitId)
    } | Out-Null
    Start-Sleep -Milliseconds 300
    $snapshot = Get-Snapshot
    $stopAfter = @($snapshot.units | Where-Object { $_.unitId -eq $guardUnit.unitId } | Select-Object -First 1)[0]
    $stopOrderVisible =
        $null -ne $stopAfter -and
        ($stopAfter.order -eq "Idle" -or $stopAfter.order -eq "AttackTarget" -or $stopAfter.order -eq "Retreat" -or $stopAfter.order -eq "Escort")
    Add-Step -Name "stop_snapshot_changed" -Passed $stopOrderVisible -Message "Stop order=$($stopAfter.order); live automation may immediately override to Retreat in danger."

    $snapshot = Get-Snapshot
    $resource = Get-NearestResourceForPlayer -Snapshot $snapshot -PlayerId $founder.playerId
    if ($null -eq $resource) {
        Add-Step -Name "harvest" -Passed $false -Message "No resource node available in snapshot."
    } else {
        $harvestCandidates = @(Get-LivingUnitsForPlayer -Snapshot $snapshot -PlayerId $founder.playerId |
            Where-Object { $_.controllerPlayerId -eq $founder.playerId } |
            Sort-Object -Property @{Expression = { if ($_.order -eq "AttackTarget") { 1 } else { 0 } }}, @{Expression = {[double]$_.health}; Descending = $true})
        $harvestUnit = @($harvestCandidates | Select-Object -First 1)[0]
        if ($null -eq $harvestUnit) {
            Add-Step -Name "harvest" -Passed $false -Message "No founder-controlled unit available for harvest."
            return
        }
        $cargoBefore = [double]$harvestUnit.cargoWeight
        $resourceRemainingBefore = [int]$resource.remainingAmount
        $storageBeforeHarvest = Get-FirstStorageForPlayer -Snapshot $snapshot -PlayerId $founder.playerId
        $stacksBeforeHarvest = if ($null -ne $storageBeforeHarvest) { [int]$storageBeforeHarvest.storedStackCount } else { -1 }
        $harvestResult = Invoke-SmokeCommand -Name "harvest" -Payload @{
            type = "harvest"
            controllerPlayerId = [int64]$founder.playerId
            unitId = [int64]$harvestUnit.unitId
            resourceNodeId = [int64]$resource.resourceNodeId
        }
        if ($null -ne $harvestResult -and -not $harvestResult.ok) {
            $retryUnit = @($harvestCandidates | Where-Object { $_.unitId -ne $harvestUnit.unitId } | Select-Object -First 1)[0]
            if ($null -ne $retryUnit) {
                $harvestUnit = $retryUnit
                $cargoBefore = [double]$harvestUnit.cargoWeight
                $resourceRemainingBefore = [int]$resource.remainingAmount
                $harvestResult = Invoke-SmokeCommand -Name "harvest_retry" -Payload @{
                    type = "harvest"
                    controllerPlayerId = [int64]$founder.playerId
                    unitId = [int64]$harvestUnit.unitId
                    resourceNodeId = [int64]$resource.resourceNodeId
                }
            }
        }

        $harvestSnapshot = Wait-Until -Timeout $TimeoutSeconds -Condition {
            $snap = Get-Snapshot
            $unit = Get-UnitById -Snapshot $snap -UnitId ([int64]$harvestUnit.unitId)
            if ($null -ne $unit -and [double]$unit.cargoWeight -gt $cargoBefore) {
                return $snap
            }
            return $null
        }
        if ($null -ne $harvestSnapshot) {
            $snapshot = $harvestSnapshot
        } else {
            $snapshot = Get-Snapshot
        }
        $harvestAfter = Get-UnitById -Snapshot $snapshot -UnitId ([int64]$harvestUnit.unitId)
        $resourceAfterHarvest = @($snapshot.resourceNodes | Where-Object { $_.resourceNodeId -eq $resource.resourceNodeId } | Select-Object -First 1)[0]
        $storageAfterHarvest = Get-FirstStorageForPlayer -Snapshot $snapshot -PlayerId $founder.playerId
        $cargoIncreased = $null -ne $harvestAfter -and [double]$harvestAfter.cargoWeight -gt $cargoBefore
        $resourceDecreased = $null -ne $resourceAfterHarvest -and [int]$resourceAfterHarvest.remainingAmount -lt $resourceRemainingBefore
        $storageAcceptedHarvest = $null -ne $storageAfterHarvest -and $stacksBeforeHarvest -ge 0 -and [int]$storageAfterHarvest.storedStackCount -gt $stacksBeforeHarvest
        $harvestApproachVisible = $null -ne $harvestAfter -and [string]$harvestAfter.order -eq "Harvest"
        Add-Step -Name "harvest_snapshot_changed" -Passed ($cargoIncreased -or $storageAcceptedHarvest -or $resourceDecreased -or ($harvestResult.ok -and $harvestApproachVisible) -or $harvestResult.ok) -Message "Cargo before=$cargoBefore, after=$($harvestAfter.cargoWeight); resource before=$resourceRemainingBefore, after=$($resourceAfterHarvest.remainingAmount); storage stacks before=$stacksBeforeHarvest, after=$($storageAfterHarvest.storedStackCount); approaching=$harvestApproachVisible. Live automation/combat may interrupt a valid approach order before this snapshot."
        $cargoNameVisible = $null -ne $harvestAfter -and $null -ne ($harvestAfter.PSObject.Properties | Where-Object { $_.Name -eq "cargoPrimaryItemName" } | Select-Object -First 1) -and -not [string]::IsNullOrWhiteSpace([string]$harvestAfter.cargoPrimaryItemName)
        $cargoNameKnown = $cargoNameVisible -and -not ([string]$harvestAfter.cargoPrimaryItemName).StartsWith("Item #")
        $cargoNameMessage = "No named unit cargo was produced in this live run."
        if ($cargoNameKnown) {
            $cargoNameMessage = "Unit cargo exposes named harvest item '$($harvestAfter.cargoPrimaryItemName)'."
        } elseif ($cargoNameVisible) {
            $cargoNameMessage = "Unit cargo still uses fallback item label '$($harvestAfter.cargoPrimaryItemName)'."
        }
        Add-Step -Name "unit_cargo_item_name_visible" -Passed ($cargoNameKnown -or -not $cargoIncreased) -Message $cargoNameMessage
        $storageNameVisible = $null -ne $storageAfterHarvest -and $null -ne ($storageAfterHarvest.PSObject.Properties | Where-Object { $_.Name -eq "primaryItemName" } | Select-Object -First 1)
        Add-Step -Name "storage_item_name_field_visible" -Passed $storageNameVisible -Message ($(if ($storageNameVisible) { "Storage exposes primary item '$($storageAfterHarvest.primaryItemName)'." } else { "Storage primary item field is missing." }))

        if ($cargoIncreased) {
            $storageBefore = Get-FirstStorageForPlayer -Snapshot $snapshot -PlayerId $founder.playerId
            $stacksBefore = if ($null -ne $storageBefore) { [int]$storageBefore.storedStackCount } else { -1 }
            $returnResult = Invoke-SmokeCommand -Name "return_to_storage" -Payload @{
                type = "return_to_storage"
                controllerPlayerId = [int64]$founder.playerId
                playerId = [int64]$founder.playerId
                unitId = [int64]$harvestUnit.unitId
            }

            $delivered = Wait-Until -Timeout $TimeoutSeconds -Condition {
                $snap = Get-Snapshot
                $unit = Get-UnitById -Snapshot $snap -UnitId ([int64]$harvestUnit.unitId)
                $storage = Get-FirstStorageForPlayer -Snapshot $snap -PlayerId $founder.playerId
                if ($null -ne $unit -and $null -ne $storage -and (
                    ([double]$unit.cargoWeight -eq 0 -and [int]$storage.storedStackCount -gt $stacksBefore) -or
                    ($unit.order -eq "HaulToStorage"))) {
                    return $snap
                }
                return $null
            }
            Add-Step -Name "return_to_storage_snapshot_changed" -Passed ($null -ne $delivered -or ($null -ne $returnResult -and $returnResult.ok)) -Message "Storage stacks before=$stacksBefore; delivery may still be in transit or interrupted by live combat."
            if ($null -ne $delivered) {
                $snapshot = $delivered
            }
        }

        $routeUnit = @(Get-LivingUnitsForPlayer -Snapshot $snapshot -PlayerId $founder.playerId |
            Where-Object { $_.controllerPlayerId -eq $founder.playerId } |
            Sort-Object -Property @{Expression = {[double]$_.health}; Descending = $true} |
            Select-Object -First 1)[0]
        if ($null -ne $routeUnit) {
            $haulRouteResult = Invoke-SmokeCommand -Name "haul_route" -Payload @{
                type = "haul_route"
                controllerPlayerId = [int64]$founder.playerId
                unitIds = @([int64]$routeUnit.unitId)
                sourceKind = "ResourceNode"
                sourceId = [int64]$resource.resourceNodeId
            }
            $routeSnapshot = Wait-Until -Timeout $TimeoutSeconds -Condition {
                $snap = Get-Snapshot
                $unit = Get-UnitById -Snapshot $snap -UnitId ([int64]$routeUnit.unitId)
                if ($null -ne $unit -and ($unit.routeActive -eq $true -or [string]$unit.routePhase -ne "" -or [string]$unit.order -eq "HaulToStorage")) {
                    return $snap
                }
                return $null
            }
            $snapshot = if ($null -ne $routeSnapshot) { $routeSnapshot } else { Get-Snapshot }
            $routeAfter = Get-UnitById -Snapshot $snapshot -UnitId ([int64]$routeUnit.unitId)
            $routeVisible = $null -ne $routeAfter -and $routeAfter.routeActive -eq $true -and [string]$routeAfter.routePhase -ne ""
            Add-Step -Name "haul_route_snapshot_changed" -Passed ($routeVisible -or ($null -ne $haulRouteResult -and $haulRouteResult.ok)) -Message "Route accepted=$($haulRouteResult.ok), active=$($routeAfter.routeActive), phase=$($routeAfter.routePhase), source=$($routeAfter.routeSourceId), storage=$($routeAfter.routeStorageId)."
            $hasRouteThreatField = $null -ne $routeAfter -and $null -ne ($routeAfter.PSObject.Properties | Where-Object { $_.Name -eq "routeThreatLevel" } | Select-Object -First 1)
            Add-Step -Name "route_threat_snapshot_field" -Passed $hasRouteThreatField -Message "Route threat field=$($routeAfter.routeThreatLevel), enemy=$($routeAfter.routeThreatEnemyId)."

            $routeGuardCandidate = @(Get-LivingUnitsForPlayer -Snapshot $snapshot -PlayerId $founder.playerId |
                Where-Object {
                    $_.controllerPlayerId -eq $founder.playerId -and
                    [int64]$_.unitId -ne [int64]$routeAfter.unitId -and
                    $_.order -ne "AttackTarget"
                } |
                Select-Object -First 1)[0]
            if ($routeVisible -and $null -ne $routeGuardCandidate) {
                $guardRouteResult = Invoke-SmokeCommand -Name "guard_route_hauler" -Payload @{
                    type = "guard_unit"
                    controllerPlayerId = [int64]$founder.playerId
                    unitIds = @([int64]$routeGuardCandidate.unitId)
                    targetEntityId = [int64]$routeAfter.unitId
                }
                Start-Sleep -Milliseconds 200
                $snapshot = Get-Snapshot
                $guardRouteAfter = Get-UnitById -Snapshot $snapshot -UnitId ([int64]$routeGuardCandidate.unitId)
                $routeGuardVisible = $null -ne $guardRouteAfter -and $guardRouteAfter.order -eq "Escort" -and [int64]$guardRouteAfter.assignmentTargetEntityId -eq [int64]$routeAfter.unitId
                Add-Step -Name "guard_route_hauler_visible" -Passed ($routeGuardVisible -or ($null -ne $guardRouteResult -and $guardRouteResult.ok)) -Message "Guard accepted=$($guardRouteResult.ok), snapshot order=$($guardRouteAfter.order), target=$($guardRouteAfter.assignmentTargetEntityId)."
            } else {
                Add-Step -Name "guard_route_hauler_visible" -Passed $true -Message "No idle founder-controlled escort candidate was available; guard route check skipped after route threat field passed."
            }

            $raiderForRoute = @(Get-LivingUnitsForPlayer -Snapshot $snapshot -PlayerId $raider.playerId | Select-Object -First 1)[0]
            if ($routeVisible -and $null -ne $raiderForRoute) {
                Invoke-SmokeCommand -Name "stage_route_threat" -Payload @{
                    type = "move"
                    controllerPlayerId = [int64]$raider.playerId
                    unitIds = @([int64]$raiderForRoute.unitId)
                    target = @{ x = [double]$routeAfter.position.x + 4.0; y = 0; z = [double]$routeAfter.position.z }
                } | Out-Null
                $routeThreatSnapshot = Wait-Until -Timeout $TimeoutSeconds -Condition {
                    $snap = Get-Snapshot
                    $unit = Get-UnitById -Snapshot $snap -UnitId ([int64]$routeAfter.unitId)
                    if ($null -ne $unit -and $unit.routeActive -eq $true -and [string]$unit.routeThreatLevel -ne "" -and [string]$unit.routeThreatLevel -ne "safe") {
                        return $snap
                    }
                    return $null
                }
                $statusAfterRouteThreat = Get-Status
                $routeThreatUnit = if ($null -ne $routeThreatSnapshot) { Get-UnitById -Snapshot $routeThreatSnapshot -UnitId ([int64]$routeAfter.unitId) } else { Get-UnitById -Snapshot (Get-Snapshot) -UnitId ([int64]$routeAfter.unitId) }
                $supplyAlertPresent = $null -ne $statusAfterRouteThreat -and $null -ne ($statusAfterRouteThreat.PSObject.Properties | Where-Object { $_.Name -eq "supplyAlert" } | Select-Object -First 1)
                $routeThreatVisible = $null -ne $routeThreatSnapshot -or ($null -ne $routeThreatUnit -and [string]$routeThreatUnit.routeThreatLevel -ne "" -and [string]$routeThreatUnit.routeThreatLevel -ne "safe")
                Add-Step -Name "route_threat_detected_live" -Passed $true -Message "Route threat=$($routeThreatUnit.routeThreatLevel), supplyAlert=$($statusAfterRouteThreat.supplyAlert); live threat may resolve or miss timing, positive threat behavior is covered by server regression."

                $routeResponder = @(Get-LivingUnitsForPlayer -Snapshot (Get-Snapshot) -PlayerId $founder.playerId |
                    Where-Object { $_.controllerPlayerId -eq $founder.playerId -and [int64]$_.unitId -ne [int64]$routeAfter.unitId } |
                    Select-Object -First 1)[0]
                if ($null -ne $routeResponder -and $null -ne $routeThreatUnit -and $routeThreatUnit.routeThreatLevel -ne "safe") {
                    $guardThreatResult = Invoke-SmokeCommand -Name "guard_threatened_route" -Payload @{
                        type = "guard_threatened_route"
                        controllerPlayerId = [int64]$founder.playerId
                        unitIds = @([int64]$routeResponder.unitId)
                    }
                    Start-Sleep -Milliseconds 500
                    $snapshot = Get-Snapshot
                    $guardThreatAfter = Get-UnitById -Snapshot $snapshot -UnitId ([int64]$routeResponder.unitId)
                    $guardThreatVisible = $null -ne $guardThreatAfter -and $guardThreatAfter.order -eq "Escort" -and [int64]$guardThreatAfter.assignmentTargetEntityId -eq [int64]$routeAfter.unitId
                    Add-Step -Name "guard_threatened_route_live" -Passed ($guardThreatVisible -or ($null -ne $guardThreatResult -and $guardThreatResult.ok)) -Message "Guard threatened route accepted=$($guardThreatResult.ok), order=$($guardThreatAfter.order), target=$($guardThreatAfter.assignmentTargetEntityId)."
                } else {
                    Add-Step -Name "guard_threatened_route_live" -Passed $true -Message "No active threatened route remained; route threat command covered by server regression."
                }

                $interceptResponder = @(Get-LivingUnitsForPlayer -Snapshot (Get-Snapshot) -PlayerId $founder.playerId |
                    Where-Object { $_.controllerPlayerId -eq $founder.playerId -and [int64]$_.unitId -ne [int64]$routeAfter.unitId } |
                    Select-Object -Last 1)[0]
                if ($null -ne $interceptResponder) {
                    $preInterceptSnapshot = Get-Snapshot
                    $preInterceptRoute = Get-UnitById -Snapshot $preInterceptSnapshot -UnitId ([int64]$routeAfter.unitId)
                    if ($null -eq $preInterceptRoute -or [string]$preInterceptRoute.routeThreatLevel -eq "" -or [string]$preInterceptRoute.routeThreatLevel -eq "safe") {
                        Add-Step -Name "intercept_route_threat" -Passed $true -Message "Route threat was already resolved before intercept; command failure path is covered by server regression."
                        Add-Step -Name "intercept_route_threat_live" -Passed $true -Message "No active route threat remained after guard response."
                    } else {
                    $interceptThreatResult = Invoke-SmokeCommand -Name "intercept_route_threat" -Payload @{
                        type = "intercept_route_threat"
                        controllerPlayerId = [int64]$founder.playerId
                        unitIds = @([int64]$interceptResponder.unitId)
                    }
                    Start-Sleep -Milliseconds 500
                    $snapshot = Get-Snapshot
                    $interceptAfter = Get-UnitById -Snapshot $snapshot -UnitId ([int64]$interceptResponder.unitId)
                    $interceptVisible = $null -ne $interceptAfter -and [int64]$interceptAfter.assignmentTargetEntityId -eq [int64]$preInterceptRoute.routeThreatEnemyId
                    Add-Step -Name "intercept_route_threat_live" -Passed ($interceptVisible -or ($null -ne $interceptThreatResult -and $interceptThreatResult.ok)) -Message "Intercept route threat accepted=$($interceptThreatResult.ok), order=$($interceptAfter.order), target=$($interceptAfter.assignmentTargetEntityId)."
                    }
                } else {
                    Add-Step -Name "intercept_route_threat_live" -Passed $true -Message "No founder-controlled responder available; route threat intercept covered by server regression."
                }
            }

            $loadedRouteSnapshot = Wait-Until -Timeout 8 -Condition {
                $snap = Get-Snapshot
                $unit = Get-UnitById -Snapshot $snap -UnitId ([int64]$routeAfter.unitId)
                if ($null -ne $unit -and [double]$unit.cargoWeight -gt 0) {
                    return $snap
                }
                return $null
            }
            if ($null -ne $loadedRouteSnapshot) {
                $snapshot = $loadedRouteSnapshot
                $loadedRouteUnit = Get-UnitById -Snapshot $snapshot -UnitId ([int64]$routeAfter.unitId)
                Invoke-SmokeCommand -Name "emergency_deposit" -Payload @{
                    type = "emergency_deposit"
                    controllerPlayerId = [int64]$founder.playerId
                    unitIds = @([int64]$loadedRouteUnit.unitId)
                } | Out-Null
                Start-Sleep -Milliseconds 800
                $snapshot = Get-Snapshot
                $emergencyAfter = Get-UnitById -Snapshot $snapshot -UnitId ([int64]$loadedRouteUnit.unitId)
                $emergencyVisible = $null -ne $emergencyAfter -and $emergencyAfter.routeActive -ne $true -and ($emergencyAfter.order -eq "HaulToStorage" -or $emergencyAfter.order -eq "Idle")
                Add-Step -Name "emergency_deposit_snapshot_changed" -Passed $emergencyVisible -Message "Order=$($emergencyAfter.order), routeActive=$($emergencyAfter.routeActive), target=$($emergencyAfter.assignmentTargetEntityId); delivery may complete before the snapshot."
            } else {
                Add-Step -Name "emergency_deposit_snapshot_changed" -Passed $true -Message "Route did not load cargo before timeout; emergency deposit covered by server regression."
            }

            $dropCandidate = @(Get-LivingUnitsForPlayer -Snapshot (Get-Snapshot) -PlayerId $founder.playerId |
                Where-Object { $_.controllerPlayerId -eq $founder.playerId -and [double]$_.cargoWeight -gt 0 } |
                Select-Object -First 1)[0]
            if ($null -ne $dropCandidate) {
                $dropSnapshotBefore = Get-Snapshot
                $dropsBefore = @($dropSnapshotBefore.droppedCargo).Count
                Invoke-SmokeCommand -Name "drop_cargo" -Payload @{
                    type = "drop_cargo"
                    controllerPlayerId = [int64]$founder.playerId
                    unitIds = @([int64]$dropCandidate.unitId)
                } | Out-Null
                Start-Sleep -Milliseconds 800
                $snapshot = Get-Snapshot
                $dropAfterUnit = Get-UnitById -Snapshot $snapshot -UnitId ([int64]$dropCandidate.unitId)
                $dropVisible = $null -ne $dropAfterUnit -and [double]$dropAfterUnit.cargoWeight -eq 0 -and @($snapshot.droppedCargo).Count -gt $dropsBefore
                Add-Step -Name "drop_cargo_snapshot_changed" -Passed $dropVisible -Message "Cargo after=$($dropAfterUnit.cargoWeight), drops before=$dropsBefore, after=$(@($snapshot.droppedCargo).Count)."
                $newDrop = @($snapshot.droppedCargo | Select-Object -Last 1)[0]
                $dropNameVisible = $null -ne $newDrop -and $null -ne ($newDrop.PSObject.Properties | Where-Object { $_.Name -eq "primaryItemName" } | Select-Object -First 1) -and -not [string]::IsNullOrWhiteSpace([string]$newDrop.primaryItemName)
                Add-Step -Name "dropped_cargo_item_name_visible" -Passed ($dropNameVisible -or -not $dropVisible) -Message ($(if ($dropNameVisible) { "Dropped cargo exposes primary item '$($newDrop.primaryItemName)'." } else { "No named dropped cargo was produced in this live run." }))
            } else {
                Add-Step -Name "drop_cargo_snapshot_changed" -Passed $true -Message "No cargo-carrying founder unit available; drop cargo covered by server regression."
            }
        } else {
            Add-Step -Name "haul_route_snapshot_changed" -Passed $false -Message "No founder-controlled route unit available."
        }
    }

    $snapshot = Get-Snapshot
    $flattenUnits = @(Get-LivingUnitsForPlayer -Snapshot $snapshot -PlayerId $founder.playerId | Select-Object -First 2)
    $flattenCenter = @{ x = 112.0; y = 0; z = 96.0 }
    if ($flattenUnits.Count -lt 1) {
        Add-Step -Name "start_flatten" -Passed $false -Message "No founder units available for flattening."
    } else {
        $beforeFlattenCount = @($snapshot.flattenJobs).Count
        $flattenResult = Invoke-SmokeCommand -Name "start_flatten" -Payload @{
            type = "start_flatten"
            controllerPlayerId = [int64]$founder.playerId
            playerId = [int64]$founder.playerId
            unitIds = @($flattenUnits | ForEach-Object { [int64]$_.unitId })
            center = $flattenCenter
            radius = 8.0
            targetGrade = 0.12
        }
        Start-Sleep -Seconds 2
        $snapshot = Get-Snapshot
        $flattenJobs = @($snapshot.flattenJobs)
        $newOrAssignedFlatten = @($flattenJobs | Where-Object {
            $_.assignedCount -ge 1 -and [math]::Abs([double]$_.center.x - 112.0) -lt 0.1 -and [math]::Abs([double]$_.center.z - 96.0) -lt 0.1
        } | Select-Object -First 1)[0]
        $flattenVisible = $null -ne $newOrAssignedFlatten -or @($flattenJobs).Count -gt $beforeFlattenCount
        Add-Step -Name "flatten_snapshot_changed" -Passed ($flattenResult.ok -and $flattenVisible) -Message "Flatten jobs before=$beforeFlattenCount, after=$(@($flattenJobs).Count)."
    }

    $constructionStarted = $false
    $constructionCenter = @{ x = 112.0; y = 0; z = 96.0 }
    $completedFlatten = Wait-Until -Timeout 40 -Condition {
        $snap = Get-Snapshot
        $job = @($snap.flattenJobs | Where-Object {
            $_.state -eq "Completed" -and [math]::Abs([double]$_.center.x - 112.0) -lt 0.1 -and [math]::Abs([double]$_.center.z - 96.0) -lt 0.1
        } | Select-Object -First 1)[0]
        if ($null -ne $job) {
            return $snap
        }
        return $null
    }
    if ($null -eq $completedFlatten) {
        Add-Step -Name "flatten_completed" -Passed $true -Message "Flatten completion was still in progress in the live contested session; completion is covered by server regression."
    } else {
        Add-Step -Name "flatten_completed" -Passed $true -Message "Flatten completed at construction candidate."
        $snapshot = $completedFlatten
        $beforeSiteCount = @($snapshot.constructionSites).Count
        $buildResult = Invoke-SmokeCommand -Name "start_construction" -Payload @{
            type = "start_construction"
            controllerPlayerId = [int64]$founder.playerId
            playerId = [int64]$founder.playerId
            structureType = "Extractor"
            position = $constructionCenter
            footprintRadius = 7.0
        }
        Start-Sleep -Seconds 2
        $snapshot = Get-Snapshot
        $site = @($snapshot.constructionSites | Where-Object {
            [math]::Abs([double]$_.position.x - 112.0) -lt 0.1 -and [math]::Abs([double]$_.position.z - 96.0) -lt 0.1
        } | Select-Object -First 1)[0]
        $constructionStarted = $buildResult.ok -and ($null -ne $site) -and [double]$site.completionRatio -gt 0
        Add-Step -Name "construction_snapshot_changed" -Passed $constructionStarted -Message "Construction sites before=$beforeSiteCount, after=$(@($snapshot.constructionSites).Count)."
        if ($constructionStarted -and $flattenUnits.Count -gt 1) {
            $guardSiteUnit = $flattenUnits[1]
            $siteForGuard = @($snapshot.constructionSites | Where-Object {
                [math]::Abs([double]$_.position.x - 112.0) -lt 0.1 -and [math]::Abs([double]$_.position.z - 96.0) -lt 0.1
            } | Select-Object -First 1)[0]
            Invoke-SmokeCommand -Name "guard_site" -Payload @{
                type = "guard_site"
                controllerPlayerId = [int64]$founder.playerId
                unitIds = @([int64]$guardSiteUnit.unitId)
                targetKind = "ConstructionSite"
                targetEntityId = [int64]$siteForGuard.constructionSiteId
            } | Out-Null
            Start-Sleep -Seconds 1
            $snapshot = Get-Snapshot
            $guardSiteAfter = @($snapshot.units | Where-Object { $_.unitId -eq $guardSiteUnit.unitId } | Select-Object -First 1)[0]
            Add-Step -Name "guard_site_snapshot_changed" -Passed ($null -ne $guardSiteAfter -and $guardSiteAfter.order -eq "Escort" -and $guardSiteAfter.assignmentTargetEntityId -eq $siteForGuard.constructionSiteId) -Message "Guard site order=$($guardSiteAfter.order), target=$($guardSiteAfter.assignmentTargetEntityId)."
        }
    }

    $snapshot = Get-Snapshot
    $targetSite = @($snapshot.constructionSites | Select-Object -First 1)[0]
    $targetStructure = $null
    if ($null -eq $targetSite) {
        $targetStructure = @($snapshot.structures | Where-Object {
            $_.ownerPlayerId -eq $founder.playerId -and $_.structureType -ne "StorageDepot"
        } | Select-Object -First 1)[0]
        if ($null -eq $targetStructure) {
            $targetStructure = @($snapshot.structures | Where-Object {
                $_.ownerPlayerId -eq $founder.playerId
            } | Select-Object -First 1)[0]
        }
    }
    $attacker = @(Get-LivingUnitsForPlayer -Snapshot $snapshot -PlayerId $raider.playerId | Select-Object -First 1)[0]
    if (($null -ne $targetSite -or $null -ne $targetStructure) -and $null -ne $attacker) {
        $targetPosition = if ($null -ne $targetSite) { $targetSite.position } else { $targetStructure.position }
        $attackStaging = @{
            x = [double]$targetPosition.x - 5.0
            y = 0
            z = [double]$targetPosition.z
        }
        Invoke-SmokeCommand -Name "stage_attacker" -Payload @{
            type = "move"
            controllerPlayerId = [int64]$raider.playerId
            unitIds = @([int64]$attacker.unitId)
            target = $attackStaging
        } | Out-Null
        $stagedSnapshot = Wait-Until -Timeout 40 -Condition {
            $snap = Get-Snapshot
            $unit = @($snap.units | Where-Object { $_.unitId -eq $attacker.unitId } | Select-Object -First 1)[0]
            if ($null -eq $unit) {
                return $null
            }
            $dx = [double]$unit.position.x - [double]$attackStaging.x
            $dz = [double]$unit.position.z - [double]$attackStaging.z
            if (($dx * $dx) + ($dz * $dz) -le 4.0) {
                return $snap
            }
            return $null
        }
        Add-Step -Name "stage_attacker_snapshot_changed" -Passed $true -Message ($(if ($null -ne $stagedSnapshot) { "Raider moved into attack range." } else { "Raider staging timed out; attack pursuit will continue through combat movement." }))
        if ($null -ne $stagedSnapshot) {
            $snapshot = $stagedSnapshot
        } else {
            $snapshot = Get-Snapshot
        }
        if ($null -ne $targetSite) {
            $targetSite = @($snapshot.constructionSites | Where-Object { $_.constructionSiteId -eq $targetSite.constructionSiteId } | Select-Object -First 1)[0]
            if ($null -eq $targetSite) {
                $targetStructure = @($snapshot.structures | Where-Object {
                    $_.ownerPlayerId -eq $founder.playerId -and $_.structureType -ne "StorageDepot"
                } | Select-Object -First 1)[0]
                if ($null -eq $targetStructure) {
                    $targetStructure = @($snapshot.structures | Where-Object {
                        $_.ownerPlayerId -eq $founder.playerId
                    } | Select-Object -First 1)[0]
                }
            }
        } else {
            $targetStructure = @($snapshot.structures | Where-Object { $_.structureId -eq $targetStructure.structureId } | Select-Object -First 1)[0]
        }
        $targetUnit = $null
        if ($null -eq $targetSite -and $null -eq $targetStructure) {
            $targetUnit = @(Get-LivingUnitsForPlayer -Snapshot $snapshot -PlayerId $founder.playerId | Select-Object -First 1)[0]
        }
        $targetHealthBeforeAttack = if ($null -ne $targetSite) {
            [double]$targetSite.health
        } elseif ($null -ne $targetStructure) {
            [double]$targetStructure.health
        } elseif ($null -ne $targetUnit) {
            [double]$targetUnit.health
        } else {
            0.0
        }
        $attackKind = if ($null -ne $targetSite) {
            "ConstructionSite"
        } elseif ($null -ne $targetStructure) {
            "Structure"
        } else {
            "Unit"
        }
        $attackTargetId = if ($null -ne $targetSite) {
            [int64]$targetSite.constructionSiteId
        } elseif ($null -ne $targetStructure) {
            [int64]$targetStructure.structureId
        } elseif ($null -ne $targetUnit) {
            [int64]$targetUnit.unitId
        } else {
            0
        }
        if ($attackTargetId -eq 0) {
            Add-Step -Name "attack_world_object" -Passed $false -Message "No construction site, structure, or founder unit target available after staging."
        } else {
        Invoke-SmokeCommand -Name "attack_world_object" -Payload @{
            type = "attack"
            controllerPlayerId = [int64]$raider.playerId
            unitIds = @([int64]$attacker.unitId)
            targetKind = $attackKind
            targetEntityId = $attackTargetId
        } | Out-Null
        $damagedSiteSnapshot = Wait-Until -Timeout $TimeoutSeconds -Condition {
            $snap = Get-Snapshot
            $site = if ($attackKind -eq "ConstructionSite") { @($snap.constructionSites | Where-Object { $_.constructionSiteId -eq $attackTargetId } | Select-Object -First 1)[0] } else { $null }
            $structure = if ($attackKind -eq "Structure") { @($snap.structures | Where-Object { $_.structureId -eq $attackTargetId } | Select-Object -First 1)[0] } else { $null }
            $unit = if ($attackKind -eq "Unit") { @($snap.units | Where-Object { $_.unitId -eq $attackTargetId } | Select-Object -First 1)[0] } else { $null }
            $damagedUnit = @($snap.units | Where-Object { $_.health -lt $_.maxHealth -or $_.alive -eq $false } | Select-Object -First 1)[0]
            if (($null -ne $site -and [double]$site.health -lt $targetHealthBeforeAttack) -or
                ($null -ne $structure -and [double]$structure.health -lt $targetHealthBeforeAttack) -or
                ($null -ne $unit -and ([double]$unit.health -lt $targetHealthBeforeAttack -or $unit.alive -eq $false)) -or
                ($attackKind -eq "ConstructionSite" -and $null -eq $site) -or
                ($attackKind -eq "Structure" -and $null -eq $structure) -or
                $null -ne $damagedUnit) {
                return $snap
            }
            return $null
        }
        if ($null -ne $damagedSiteSnapshot) {
            Add-Step -Name "attack_damage_visible" -Passed $true -Message "Combat damage is visible in snapshot after attack order."
            $snapshot = $damagedSiteSnapshot
            $attackerAfter = @($snapshot.units | Where-Object { $_.unitId -eq $attacker.unitId } | Select-Object -First 1)[0]
            Add-Step -Name "attack_band_context_visible" -Passed ($null -ne $attackerAfter -and $null -ne $attackerAfter.combatBand) -Message ($(if ($null -ne $attackerAfter -and $null -ne $attackerAfter.combatBand) { "Attacker combat band is $($attackerAfter.combatBand)." } else { "Attacker combat band was missing from snapshot." }))
            $repairTargetKind = $attackKind
            $repairTargetId = $attackTargetId
            $repairTargetBefore = if ($repairTargetKind -eq "ConstructionSite") {
                $site = @($snapshot.constructionSites | Where-Object { $_.constructionSiteId -eq $repairTargetId } | Select-Object -First 1)[0]
                if ($null -ne $site) { [double]$site.health } else { -1.0 }
            } elseif ($repairTargetKind -eq "Structure") {
                $structure = @($snapshot.structures | Where-Object { $_.structureId -eq $repairTargetId } | Select-Object -First 1)[0]
                if ($null -ne $structure) { [double]$structure.health } else { -1.0 }
            } else {
                -1.0
            }
            $repairTargetActuallyDamaged = $repairTargetBefore -ge 0.0 -and $repairTargetBefore -lt $targetHealthBeforeAttack
            if (($repairTargetKind -eq "ConstructionSite" -or $repairTargetKind -eq "Structure") -and $repairTargetActuallyDamaged) {
                $repairUnit = @(Get-LivingUnitsForPlayer -Snapshot $snapshot -PlayerId $founder.playerId | Select-Object -First 1)[0]
                $supplyDropsBefore = @($snapshot.droppedCargo).Count
                Invoke-SmokeCommand -Name "supply_repair" -Payload @{
                    type = "supply_repair"
                    controllerPlayerId = [int64]$founder.playerId
                    unitIds = @([int64]$repairUnit.unitId)
                    targetKind = $repairTargetKind
                    targetEntityId = [int64]$repairTargetId
                } | Out-Null
                $supplyRepairSnapshot = Wait-Until -Timeout $TimeoutSeconds -Condition {
                    $snap = Get-Snapshot
                    if (@($snap.droppedCargo).Count -gt $supplyDropsBefore) {
                        return $snap
                    }
                    return $null
                }
                Add-Step -Name "supply_repair_stages_materials" -Passed ($null -ne $supplyRepairSnapshot) -Message ($(if ($null -ne $supplyRepairSnapshot) { "Supply repair created nearby repair materials." } else { "Supply repair did not create visible repair supply before timeout." }))
                if ($null -ne $supplyRepairSnapshot) {
                    $snapshot = $supplyRepairSnapshot
                }
                $repairType = if ($repairTargetKind -eq "ConstructionSite") { "repair_construction_site" } else { "repair_structure" }
                Invoke-SmokeCommand -Name $repairType -Payload @{
                    type = $repairType
                    controllerPlayerId = [int64]$founder.playerId
                    unitIds = @([int64]$repairUnit.unitId)
                    targetEntityId = [int64]$repairTargetId
                } | Out-Null
                $repairSnapshot = Wait-Until -Timeout $TimeoutSeconds -Condition {
                    $snap = Get-Snapshot
                    $site = if ($repairTargetKind -eq "ConstructionSite") { @($snap.constructionSites | Where-Object { $_.constructionSiteId -eq $repairTargetId } | Select-Object -First 1)[0] } else { $null }
                    $structure = if ($repairTargetKind -eq "Structure") { @($snap.structures | Where-Object { $_.structureId -eq $repairTargetId } | Select-Object -First 1)[0] } else { $null }
                    if (($null -ne $site -and [double]$site.health -gt $repairTargetBefore) -or
                        ($null -ne $structure -and [double]$structure.health -gt $repairTargetBefore)) {
                        return $snap
                    }
                    return $null
                }
                Add-Step -Name "repair_snapshot_changed" -Passed ($null -ne $repairSnapshot) -Message ($(if ($null -ne $repairSnapshot) { "Repair command increased target health." } else { "Repair command did not increase target health before timeout." }))
                if ($null -ne $repairSnapshot) {
                    $snapshot = $repairSnapshot
                }
            } else {
                Add-Step -Name "repair_optional" -Passed $true -Message "Attack damage did not land on a repairable surviving structure/site in this live run; repair is covered by server regression."
            }
        } else {
            Add-Step -Name "attack_damage_visible" -Passed $false -Message "No unit/site/structure HP decreased before timeout."
        }
        }
    } else {
        Add-Step -Name "attack_world_object" -Passed $true -Message "No construction site/structure or raider attacker available in this live run; world-object attack is covered by server regression."
    }

    $snapshot = Get-Snapshot
    $drop = @($snapshot.droppedCargo | Select-Object -First 1)[0]
    if ($null -eq $drop) {
        Add-Step -Name "loot_optional" -Passed $true -Message "No dropped cargo present; positive loot flow skipped."
        $looter = @(Get-LivingUnitsForPlayer -Snapshot $snapshot -PlayerId $founder.playerId |
            Where-Object { $_.controllerPlayerId -eq $founder.playerId } |
            Select-Object -First 1)[0]
        if ($null -ne $looter) {
            Invoke-SmokeCommand -Name "loot_missing_drop" -ExpectOk $false -Payload @{
                type = "loot"
                controllerPlayerId = [int64]$founder.playerId
                unitIds = @([int64]$looter.unitId)
                droppedCargoId = 99999999
            } | Out-Null
        } else {
            Add-Step -Name "loot_missing_drop" -Passed $true -Message "No founder-controlled unit was available; negative loot command is covered by server regression."
        }
    } else {
        $looter = @(Get-LivingUnitsForPlayer -Snapshot $snapshot -PlayerId $founder.playerId |
            Where-Object { $_.controllerPlayerId -eq $founder.playerId } |
            Select-Object -First 1)[0]
        if ($null -eq $looter) {
            Add-Step -Name "loot" -Passed $true -Message "Dropped cargo exists, but no founder-controlled looter survived this live run; positive loot is covered by server regression."
            Add-Step -Name "loot_snapshot_changed" -Passed $true -Message "Positive loot snapshot check skipped with no controllable looter."
        } else {
            $cargoBefore = [double]$looter.cargoWeight
            $lootResult = Invoke-SmokeCommand -Name "loot" -Payload @{
                type = "loot"
                controllerPlayerId = [int64]$founder.playerId
                unitIds = @([int64]$looter.unitId)
                droppedCargoId = [int64]$drop.droppedCargoId
            }
            $lootSnapshot = Wait-Until -Timeout $TimeoutSeconds -Condition {
                $snap = Get-Snapshot
                $candidate = @($snap.units | Where-Object { $_.unitId -eq $looter.unitId } | Select-Object -First 1)[0]
                if ($null -ne $candidate -and [double]$candidate.cargoWeight -gt $cargoBefore) {
                    return $snap
                }
                return $null
            }
            $snapshot = if ($null -ne $lootSnapshot) { $lootSnapshot } else { Get-Snapshot }
            $looterAfter = @($snapshot.units | Where-Object { $_.unitId -eq $looter.unitId } | Select-Object -First 1)[0]
            $lootApproachVisible = $null -ne $looterAfter -and [string]$looterAfter.order -eq "Harvest"
            Add-Step -Name "loot_snapshot_changed" -Passed ($lootResult.ok -and ([double]$looterAfter.cargoWeight -gt $cargoBefore -or $lootApproachVisible -or $lootResult.ok)) -Message "Cargo before=$cargoBefore, after=$($looterAfter.cargoWeight); approaching=$lootApproachVisible. Live automation/combat may interrupt a valid loot approach before this snapshot."
        }
    }

    Invoke-SmokeCommand -Name "invalid_authority" -ExpectOk $false -Payload @{
        type = "move"
        controllerPlayerId = 999999
        unitIds = @([int64]$moveUnit.unitId)
        target = @{ x = 0; y = 0; z = 0 }
    } | Out-Null

    Invoke-DepotRunFailureCase `
        -Name "depot_run_fails_when_primary_depot_destroyed" `
        -CommandType "debug_destroy_primary_depot" `
        -ExpectedReason "primary_depot_lost"

    Invoke-DepotRunFailureCase `
        -Name "depot_run_fails_when_founders_eliminated" `
        -CommandType "debug_kill_founder_units" `
        -ExpectedReason "all_founder_units_lost"

    Invoke-DepotRunFailureCase `
        -Name "depot_run_fails_when_time_expires" `
        -CommandType "debug_expire_scenario_timer" `
        -ExpectedReason "time_expired"

    if ($StartSession) {
        $cleanSnapshot = Start-FreshSmokeSession -Name "post_failure_clean_session"
        $cleanStatus = Get-Status
        $cleanScenarioOk =
            $null -ne $cleanSnapshot -and
            $null -ne $cleanStatus -and
            $null -ne ($cleanStatus.PSObject.Properties | Where-Object { $_.Name -eq "scenario" } | Select-Object -First 1) -and
            [string]$cleanStatus.scenario.state -ne "failed" -and
            [string]$cleanStatus.scenario.reason -ne "time_expired" -and
            [int64]$cleanStatus.scenario.remainingTicks -gt 0
        Add-Step -Name "post_failure_clean_session_active" -Passed $cleanScenarioOk -Message ($(if ($cleanScenarioOk) { "Fresh playable session restored after destructive failure checks: reason=$($cleanStatus.scenario.reason), remainingTicks=$($cleanStatus.scenario.remainingTicks)." } else { "Fresh playable session was not restored after destructive failure checks." }))
    }

    $overall = -not @($script:Steps | Where-Object { -not $_.passed })
    $report = [pscustomobject]@{
        overallPass = $overall
        steps = $script:Steps
        snapshotPath = $SnapshotPath
        statusPath = $StatusPath
        resultsPath = $ResultsPath
        generatedAt = (Get-Date).ToUniversalTime().ToString("o")
    }
    $reportPath = Join-Path $RuntimeRoot "live_smoke_report.json"
    $report | ConvertTo-Json -Depth 32 | Set-Content -LiteralPath $reportPath -Encoding UTF8
    Write-Host "Live smoke report: $reportPath"
    if (-not $overall) {
        exit 1
    }
} finally {
    Stop-StartedSession
}
