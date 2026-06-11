param(
    [string[]]$Task = @('test')
)

$ErrorActionPreference = 'Stop'

$javaHome = 'D:\Android Studio\jbr'
if (-not (Test-Path $javaHome)) {
    throw "Expected Android Studio JBR at '$javaHome'. Update dev.ps1 if your install lives elsewhere."
}

$env:JAVA_HOME = $javaHome
$env:PATH = "$env:JAVA_HOME\bin;$env:PATH"

& .\gradlew.bat @Task
