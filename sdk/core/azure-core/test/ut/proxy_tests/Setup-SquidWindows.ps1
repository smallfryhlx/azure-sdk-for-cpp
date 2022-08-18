# TODO: Mirror these in our storage account
param(
    [string] $SquidSetupUrl = 'https://packages.diladele.com/squid/4.14/squid.msi',
    [string] $CygwinSetupUrl = 'https://www.cygwin.com/setup-x86_64.exe'
)

Start-BitsTransfer -Source $SquidSetupUrl -Destination ./squid.msi
Start-BitStransfer -Source $CygwinSetupUrl -Destination ./cygwin-setup.exe

./cygwin-setup.exe `
    --download `
    --packages libcrypt2 `
    --quiet-mode `
    --prune-install `
    --site 'http://www.gtlib.gatech.edu/pub/cygwin/'
msiexec /i ./squid.msi /passive /quiet | Wait-Process

Copy-Item "$PSScriptRoot/localproxy/squid.conf" "c:\squid\etc\squid\squid.conf"
Copy-Item "$PSSCriptRoot/localproxy.passwd/squid.conf" "c:\squid\etc\squid\squid.passwd.conf"
Copy-Item "$PSSCriptRoot/localproxy.passwd/proxypasswd" "c:\squid\etc\squid\passwords"

