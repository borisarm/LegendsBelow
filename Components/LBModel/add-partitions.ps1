$vcxproj = "E:\ws\LegendsBelow\Components\LBModel\LBModel.vcxproj"
$lines = Get-Content $vcxproj
$newLines = @()

foreach ($line in $lines) {
    $newLines += $line
    if ($line -match 'Spell.ixx') {
        $newLines += '    <ClCompile Include="Weapon.ixx" />'
        $newLines += '    <ClCompile Include="Armor.ixx" />'
        $newLines += '    <ClCompile Include="StatusEffect.ixx" />'
        $newLines += '    <ClCompile Include="CombatResult.ixx" />'
    }
}

$newLines | Set-Content $vcxproj
Write-Host "Added new partition files to project"
