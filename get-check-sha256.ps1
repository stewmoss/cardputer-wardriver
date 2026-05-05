param (
    [Parameter(Mandatory=$true, HelpMessage="Enter the path to the file you want to hash.")]
    [string]$filePath
)

Write-Host "Calculating SHA256 hash for: $filePath" -ForegroundColor Yellow

if (Test-Path $filePath) {
    # Generate the SHA256 hash
    $fileHash = Get-FileHash -Path $filePath -Algorithm SHA256
    
    # Format: "Hash  Filename" (Standard checksum utility format)
    $standardFormat = "$($fileHash.Hash)  $([System.IO.Path]::GetFileName($filePath))"
    
    # Define the output destination
    $checksumFile = "$filePath.sha256"
    
    # Save to file (UTF8 encoding ensures compatibility)
    $standardFormat | Out-File -FilePath $checksumFile -Encoding utf8
    
    # Output to console for confirmation
    Write-Host "Success!" -ForegroundColor Green
    Write-Host "Hash saved to: $checksumFile" -ForegroundColor Cyan
    Write-Host "Content: $standardFormat"
}
else {
    Write-Host "[Error]: File not found at $filePath" -ForegroundColor Red	
	exit 1
}

## Now check It

if (Test-Path $checksumFile) {
    # 1. Read the stored hash from the file
    # We split by space and take the first element to get just the hash string
    $storedContent = Get-Content $checksumFile -Raw
    $storedHash = ($storedContent -split " ")[0].Trim()

    # 2. Generate a new hash from the current file
    if (Test-Path $filePath) {
        $currentHash = (Get-FileHash -Path $filePath -Algorithm SHA256).Hash

        # 3. Compare the two
        if ($currentHash -eq $storedHash) {
            Write-Host "Verification Success: Hashes match!" -ForegroundColor Green
            Write-Host "Hash: $currentHash" -ForegroundColor Gray
        }
        else {
            Write-Host "Verification FAILED: Hashes do not match!" -ForegroundColor Red
            Write-Host "Stored:  $storedHash"
            Write-Host "Current: $currentHash"
			exit 2
        }
    }
    else {
        Write-Host "[Error]: The original file ($filePath) was not found to verify against." -ForegroundColor Red
		exit 3
    }
}
else {
    Write-Host "[Error]: Checksum file ($checksumFile) not found." -ForegroundColor Red
	exit 4
}

exit 0