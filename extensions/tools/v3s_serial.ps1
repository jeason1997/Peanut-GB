# Serial shell transport for a known, already logged-in Linux console.
# Does not change baud rate on the device or modify network/boot configuration.
param(
    [string]$Port = 'COM15',
    [int]$Baud = 115200,
    [string]$Send = '',
    [int]$ReadSeconds = 3,
    [string]$Upload = '',
    [string]$Destination = ''
)
$ErrorActionPreference = 'Stop'
$serial = [IO.Ports.SerialPort]::new($Port,$Baud,'None',8,'One')
$serial.ReadTimeout=1000
$serial.WriteTimeout=10000
$serial.Open()
try {
    if($Upload) {
        if($Destination -notmatch '^/tmp/[a-zA-Z0-9_./-]+$' -or $Destination.Contains('..')) {
            throw 'Upload destination must be an explicit /tmp/ path without shell metacharacters or ..'
        }
        $encoded=[Convert]::ToBase64String([IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Upload)))
        $serial.Write("stty -echo; base64 -d > '$Destination' <<'PGB_UPLOAD_EOF'`r")
        Start-Sleep -Milliseconds 300
        for($offset=0;$offset -lt $encoded.Length;$offset+=512) {
            $serial.Write($encoded.Substring($offset,[Math]::Min(512,$encoded.Length-$offset))+"`n")
            Start-Sleep -Milliseconds 50
            $null=$serial.ReadExisting()
        }
        $serial.Write("PGB_UPLOAD_EOF`nstty echo; sha256sum '$Destination'`r")
        Write-Output "Local SHA256: $((Get-FileHash -LiteralPath $Upload -Algorithm SHA256).Hash.ToLower())"
    }
    if($Send){$serial.Write($Send+"`r")}
    $deadline=(Get-Date).AddSeconds($ReadSeconds)
    while((Get-Date)-lt $deadline) {
        $chunk=$serial.ReadExisting()
        if($chunk){Write-Output $chunk}
        Start-Sleep -Milliseconds 50
    }
} finally {$serial.Close()}
