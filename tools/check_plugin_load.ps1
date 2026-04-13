param(
    [Parameter(Mandatory = $true)]
    [string]$DllPath
)

$resolved = Resolve-Path $DllPath -ErrorAction Stop

$signature = @"
using System;
using System.Runtime.InteropServices;

public static class NativeLoader
{
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern IntPtr LoadLibrary(string lpFileName);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool FreeLibrary(IntPtr hModule);

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern int FormatMessage(
        int flags,
        IntPtr source,
        int messageId,
        int languageId,
        System.Text.StringBuilder buffer,
        int size,
        IntPtr arguments);
}
"@

Add-Type -TypeDefinition $signature | Out-Null

$dll = [string]$resolved
$module = [NativeLoader]::LoadLibrary($dll)
if ($module -eq [IntPtr]::Zero)
{
    $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
    $buffer = New-Object System.Text.StringBuilder 2048
    $null = [NativeLoader]::FormatMessage(0x00001000, [IntPtr]::Zero, $errorCode, 0, $buffer, $buffer.Capacity, [IntPtr]::Zero)

    Write-Host "Failed to load: $dll"
    Write-Host "Win32 error code: $errorCode"
    Write-Host "Message: $($buffer.ToString().Trim())"
    exit 1
}

[void][NativeLoader]::FreeLibrary($module)
Write-Host "LoadLibrary succeeded: $dll"