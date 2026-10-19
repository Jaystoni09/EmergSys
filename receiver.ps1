Add-Type -AssemblyName System.Net
$tcp = New-Object System.Net.Sockets.TcpListener([System.Net.IPAddress]::Any, 8000)
$tcp.Start()

$state = [ordered]@{
  adminId = 'admin'
  adminPassword = 'admin123'
  active = $false
  severity = 'WAITING'
  room = '--'
  symptoms = 'No alert received'
  receivedAt = '--'
  response = 'PENDING'
  history = @()
}
$lastAlertKey = ''
$lastAlertAt = [DateTime]::MinValue

function Send-Http($stream, $status, $type, $body) {
  $bytes = [System.Text.Encoding]::UTF8.GetBytes($body)
  $header = "HTTP/1.0 $status`r`nContent-Type: $type`r`nContent-Length: $($bytes.Length)`r`nAccess-Control-Allow-Origin: *`r`nConnection: close`r`n`r`n"
  $headerBytes = [System.Text.Encoding]::ASCII.GetBytes($header)
  $stream.Write($headerBytes, 0, $headerBytes.Length)
  $stream.Write($bytes, 0, $bytes.Length)
  $stream.Flush()
}

function Get-StaticContent($path) {
  $relativePath = if ($path -eq '/') { 'index.html' } else { $path.TrimStart('/') }
  $filePath = Join-Path $PSScriptRoot $relativePath
  if (!(Test-Path $filePath -PathType Leaf)) { return $null }
  $extension = [System.IO.Path]::GetExtension($filePath).ToLowerInvariant()
  $contentType = switch ($extension) {
    '.html' { 'text/html; charset=utf-8' }
    '.css' { 'text/css; charset=utf-8' }
    '.js' { 'text/javascript; charset=utf-8' }
    default { 'application/octet-stream' }
  }
  return @{ type = $contentType; body = [System.IO.File]::ReadAllText($filePath) }
}

Write-Host 'Emergency receiver listening on http://localhost:8000'
Write-Host 'Set the ESP32 receiverUrl to the computer LAN IP on port 8000'

while ($true) {
  $client = $tcp.AcceptTcpClient()
  $stream = $client.GetStream()
  try {
    $reader = New-Object System.IO.StreamReader($stream)
    $requestLine = $reader.ReadLine()
    if (!$requestLine) { $client.Close(); continue }
    $requestParts = $requestLine.Split(' ')
    $method = $requestParts[0]
    $path = $requestParts[1]
    $contentLength = 0
    while (($headerLine = $reader.ReadLine()) -ne '') {
      if ($headerLine -match '^Content-Length:\s*(\d+)') { $contentLength = [int]$matches[1] }
    }

    if ($method -eq 'POST' -and $path -eq '/api/login') {
      $body = if ($contentLength -gt 0) { $buffer = New-Object char[] $contentLength; [void]$reader.Read($buffer, 0, $contentLength); -join $buffer } else { '{}' }
      $payload = $body | ConvertFrom-Json
      $valid = [string]$payload.adminId -eq $state.adminId -and [string]$payload.password -eq $state.adminPassword
      if ($valid) { Send-Http $stream '200 OK' 'application/json; charset=utf-8' '{"ok":true}' } else { Send-Http $stream '401 Unauthorized' 'application/json; charset=utf-8' '{"ok":false,"error":"Invalid admin ID or password."}' }
    } elseif ($method -eq 'POST' -and $path -eq '/api/password/change') {
      $body = if ($contentLength -gt 0) { $buffer = New-Object char[] $contentLength; [void]$reader.Read($buffer, 0, $contentLength); -join $buffer } else { '{}' }
      $payload = $body | ConvertFrom-Json
      $valid = [string]$payload.adminId -eq $state.adminId -and [string]$payload.currentPassword -eq $state.adminPassword
      if (!$valid) { Send-Http $stream '401 Unauthorized' 'application/json; charset=utf-8' '{"ok":false,"error":"Admin ID or current password is incorrect."}' }
      else { $state.adminPassword = [string]$payload.newPassword; Write-Host 'Admin password changed'; Send-Http $stream '200 OK' 'application/json; charset=utf-8' '{"ok":true}' }
    } elseif ($method -eq 'GET' -and $path -eq '/api/status') {
      $publicState = [ordered]@{
        active = $state.active
        severity = $state.severity
        room = $state.room
        symptoms = $state.symptoms
        receivedAt = $state.receivedAt
        response = $state.response
        history = $state.history
      }
      Send-Http $stream '200 OK' 'application/json; charset=utf-8' ($publicState | ConvertTo-Json -Compress)
    } elseif ($method -eq 'POST' -and $path -eq '/api/alert') {
      $body = if ($contentLength -gt 0) { $buffer = New-Object char[] $contentLength; [void]$reader.Read($buffer, 0, $contentLength); -join $buffer } else { '{}' }
      $payload = $body | ConvertFrom-Json
      $state.active = [bool]$payload.active
      $state.severity = [string]$payload.severity
      $state.room = [string]$payload.room
      $state.symptoms = [string]$payload.symptoms
      $state.receivedAt = (Get-Date).ToString('HH:mm:ss')
      $state.response = 'PENDING'
      $alertKey = "$($state.room)|$($state.severity)|$($state.symptoms)"
      $now = Get-Date
      $isDuplicate = $alertKey -eq $lastAlertKey -and ($now - $lastAlertAt).TotalSeconds -lt 5
      if (!$isDuplicate) {
        $historyEntry = [ordered]@{
          receivedAt = $state.receivedAt
          room = $state.room
          severity = $state.severity
          symptoms = $state.symptoms
          response = 'PENDING'
        }
        $state.history = @($historyEntry) + @($state.history)
        if ($state.history.Count -gt 20) { $state.history = @($state.history | Select-Object -First 20) }
        $lastAlertKey = $alertKey
        $lastAlertAt = $now
      }
      Write-Host ("Alert received: {0} symptoms, Room {1}" -f $state.severity, $state.room)
      Send-Http $stream '200 OK' 'application/json; charset=utf-8' '{"ok":true}'
    } elseif ($method -eq 'POST' -and $path -eq '/api/response') {
      $body = if ($contentLength -gt 0) { $buffer = New-Object char[] $contentLength; [void]$reader.Read($buffer, 0, $contentLength); -join $buffer } else { '{}' }
      $payload = $body | ConvertFrom-Json
      $requestedDecision = [string]$payload.decision
      $accepted = $state.response -eq 'PENDING'
      if ($accepted) {
        $state.response = $requestedDecision
        if ($state.history.Count -gt 0) { $state.history[0].response = $state.response }
        Write-Host ("Operator response accepted: {0}" -f $state.response)
      } else {
        Write-Host ("Operator response ignored; already decided: {0}" -f $state.response)
      }
      $responseBody = [ordered]@{ ok = $accepted; response = $state.response } | ConvertTo-Json -Compress
      Send-Http $stream '200 OK' 'application/json; charset=utf-8' $responseBody
    } elseif ($method -eq 'POST' -and $path -eq '/api/history/clear') {
      $state.history = @()
      Write-Host 'Alert history cleared by operator'
      Send-Http $stream '200 OK' 'application/json; charset=utf-8' '{"ok":true}'
    } else {
      $static = if ($method -eq 'GET') { Get-StaticContent $path } else { $null }
      if ($static) { Send-Http $stream '200 OK' $static.type $static.body } else { Send-Http $stream '404 Not Found' 'text/plain; charset=utf-8' 'Not found' }
    }
  } catch {
    Write-Host "Request error: $($_.Exception.Message)"
  } finally {
    $stream.Close()
    $client.Close()
  }
}
