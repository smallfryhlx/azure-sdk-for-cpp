# TODO: Mirror these in our storage account
param(
    [string] $SquidSetupUrl = 'https://packages.diladele.com/squid/4.14/squid.msi',
    [string] $CygwinSetupUrl = 'https://www.cygwin.com/setup-x86_64.exe'
)

Start-BitsTransfer -Source $SquidSetupUrl -Destination ./squid.msi
Start-BitStransfer -Source $CygwinSetupUrl -Destination ./cygwin-setup.exe

./cygwin-setup.exe --packages libcrypt2 --quiet-mode
msiexec /i ./squid.msi /qn

