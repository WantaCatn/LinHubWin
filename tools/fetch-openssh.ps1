# Download Microsoft Win32-OpenSSH 8.1 (Win7 x64 / Win10 / Win11 client).
$ErrorActionPreference = "Stop"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$dest = Join-Path $root "third_party"
$out = Join-Path $dest "openssh"
$zip = Join-Path $dest "OpenSSH-Win64.zip"
$url = "https://github.com/PowerShell/Win32-OpenSSH/releases/download/v8.1.0.0p1-Beta/OpenSSH-Win64.zip"
New-Item -ItemType Directory -Path $dest -Force | Out-Null
Write-Host "Downloading $url"
Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing
Add-Type -AssemblyName System.IO.Compression.FileSystem
$extract = Join-Path $dest "_openssh_extract"
if (Test-Path $extract) { Remove-Item $extract -Recurse -Force }
[IO.Compression.ZipFile]::ExtractToDirectory($zip, $extract)
$src = Join-Path $extract "OpenSSH-Win64"
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Path $out -Force | Out-Null
foreach ($f in @(
    "ssh.exe","scp.exe","sftp.exe","ssh-add.exe","ssh-agent.exe",
    "ssh-keygen.exe","ssh-keyscan.exe","libcrypto.dll"
)) {
    Copy-Item (Join-Path $src $f) $out -Force
}
@"
Win32-OpenSSH 8.1.0.0p1-Beta (64-bit)
Source: https://github.com/PowerShell/Win32-OpenSSH/releases/tag/v8.1.0.0p1-Beta
License: BSD (OpenSSH / LibreSSL)
Client tools only; sshd is not bundled.
"@ | Set-Content (Join-Path $out "README.txt") -Encoding UTF8
Remove-Item $extract -Recurse -Force
Remove-Item $zip -Force
Write-Host "Wrote $out"
& (Join-Path $out "ssh.exe") -V
