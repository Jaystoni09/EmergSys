$listener = New-Object System.Net.HttpListener
$listener.Prefixes.Add('http://+:8000/')
$listener.Start()

$state = @{
  active = $false
  severity = 'WAITING'
  room = '--'
  symptoms = 'No alert received'
  receivedAt = '--'
}

$mimeTypes = @{
  '.html' = 'text/html; charset=utf-8'
  '.css' = 'text/css; charset=utf-8'
  '.js' = 'text/javascript; charset=utf-8'
  '.ico' = 'image/x-icon'
}

function Write-Response($context, $statusCode, $contentType, $body) {
  $bytes = [System.Text.Encoding]::UTF8.GetBytes($body)
  $context.Response.StatusCode = $statusCode
  $context.Response.ContentType = $contentType
  $context.Response.ContentLength64 = $bytes.Length
  $context.Response.Headers.Add('Access-Control-Allow-Origin', '*')
  $context.Response.OutputStream.Write($bytes, 0, $bytes.Length)
  $context.Response.Close()
}

Write-Host 'Emergency receiver listening on http://localhost:8000'
Write-Host 'Set the ESP32 receiverUrl to http://YOUR_COMPUTER_IP:8000/api/alert'

try {
  while ($listener.IsListening) {
    $context = $listener.GetContext()
    $path = $context.Request.Url.AbsolutePath

    if ($context.Request.HttpMethod -eq 'OPTIONS') {
      $context.Response.StatusCode = 204
      $context.Response.Headers.Add('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
      $context.Response.Headers.Add('Access-Control-Allow-Headers', 'Content-Type')
      $context.Response.Close()
      continue
    }

    if ($path -eq '/api/status' -and $context.Request.HttpMethod -eq 'GET') {
      Write-Response $context 200 'application/json; charset=utf-8' ($state | ConvertTo-Json -Compress)
      continue
    }

    if ($path -eq '/api/alert' -and $context.Request.HttpMethod -eq 'POST') {
      $reader = New-Object System.IO.StreamReader($context.Request.InputStream)
      $payload = $reader.ReadToEnd() | ConvertFrom-Json
      $state.active = [bool]$payload.active
      $state.severity = [string]$payload.severity
      $state.room = [string]$payload.room
      $state.symptoms = [string]$payload.symptoms
      $state.receivedAt = (Get-Date).ToString('HH:mm:ss')
      Write-Host ("Alert received: {0} symptoms, Room {1}" -f $state.severity, $state.room)
      Write-Response $context 200 'application/json; charset=utf-8' '{"ok":true}'
      continue
    }

    $relativePath = if ($path -eq '/') { 'index.html' } else { $path.TrimStart('/') }
    $filePath = Join-Path $PSScriptRoot $relativePath
    if (Test-Path $filePath -PathType Leaf) {
      $extension = [System.IO.Path]::GetExtension($filePath).ToLowerInvariant()
      $contentType = if ($mimeTypes.ContainsKey($extension)) { $mimeTypes[$extension] } else { 'application/octet-stream' }
      $body = [System.IO.File]::ReadAllText($filePath)
      Write-Response $context 200 $contentType $body
    } else {
      Write-Response $context 404 'text/plain; charset=utf-8' 'Not found'
    }
  }
} finally {
  $listener.Stop()
  $listener.Close()
}
