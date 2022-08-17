Write-Host "Downloading..."
Start-BitsTransfer -Source 'https://aka.ms/wslubuntu2004' -Destination 'Ubuntu2004.appx'
Move-Item ./Ubuntu2004.appx ./Ubuntu2004.appx.zip

Write-Host "Expanding..."
Expand-Archive ./Ubuntu2004.appx.zip ./ubuntu-staging
$targetAppxFile = (Get-ChildItem ./ubuntu-staging).Where({ $_.Name -like '*_x64.appx'})
Write-Host $targetAppxFile
Copy-Item "./ubuntu-staging/$targetAppxFile" "./ubuntu-staging/$targetAppxFile.zip"

Write-Host "Installing appx package..."
Add-AppxPackage "./ubuntu-staging/$targetAppxFile"

Write-Host "Expanding appx pacakge to get to ubuntu.exe..."
Expand-Archive "./ubuntu-staging/$targetAppxFile.zip" ./ubuntu

Write-Host "Running ubuntu.exe first time..."
./ubuntu/ubuntu.exe install --root
./ubuntu/ubuntu.exe run 'apt update && apt install -y squid'

function createWslPath ($file) {
    # "c:\example\file.txt" -> "example/file.txt"
    $rootRelativePath = (Resolve-Path $file).ToString().Substring(3).Replace('\', '/')
    return "/mnt/c/$rootRelativePath"
}

Write-host "Copying squid config"
$ubuntuPathSquidConf = createWslPath "$PSScriptRoot\remoteproxy\squid.conf"
$ubuntuPathSquidPasswordConf = createWslPath "$PSScriptRoot\remoteproxy.passwd\squid.conf"
$ubuntuPathSquidPasswordPwdFile = createWslPath "$PSScriptRoot\remoteproxy.passwd\proxypasswd"

Write-Host "./ubuntu/ubuntu.exe run `"cp $ubuntuPathSquidConf /etc/squid/squid.conf`""
./ubuntu/ubuntu.exe run "cp $ubuntuPathSquidConf /etc/squid/squid.conf"

Write-Host "./ubuntu/ubuntu.exe run `"cp $ubuntuPathSquidPasswordConf /etc/squid/squid.passwd.conf`""
./ubuntu/ubuntu.exe run "cp $ubuntuPathSquidPasswordConf /etc/squid/squid.passwd.conf"

Write-Host "./ubuntu/ubuntu.exe run `"cp $ubuntuPathSquidPasswordPwdFile /etc/squid/passwords`""
./ubuntu/ubuntu.exe run "cp $ubuntuPathSquidPasswordPwdFile /etc/squid/passwords"

./ubuntu/ubuntu.exe run "ls /etc/squid/"

Write-Host "Starting proxy"
./ubuntu/ubuntu.exe run 'squid -f /etc/squid/squid.conf'
./ubuntu/ubuntu.exe run 'squid -f /etc/squid/squid.passwd.conf'
./ubuntu/ubuntu.exe run 'sleep 10'
