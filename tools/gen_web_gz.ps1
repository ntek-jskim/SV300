# 내장 웹 페이지 gzip 생성기 (Keil "Before Build" 에서 자동 실행)
#  원본: src/web.c 의 INDEX_HTML C 문자열(#if 0 안, 컴파일 안 됨) — 화면 수정은 계속 여기서 한다
#  결과: src/web_index_gz.h (INDEX_HTML_GZ[]) — 내용이 바뀔 때만 다시 쓴다(불필요한 재컴파일 방지)
#  수동 실행: powershell -NoProfile -ExecutionPolicy Bypass -File tools\gen_web_gz.ps1
param(
	[string]$Src = (Join-Path $PSScriptRoot '..\src\web.c'),
	[string]$Out = (Join-Path $PSScriptRoot '..\src\web_index_gz.h')
)
$ErrorActionPreference = 'Stop'
try {
	$lines = [IO.File]::ReadAllLines($Src, [Text.Encoding]::UTF8)
	$s = -1
	for ($i = 0; $i -lt $lines.Length; $i++) {
		if ($lines[$i].StartsWith('static const char INDEX_HTML[] =')) { $s = $i; break }
	}
	if ($s -lt 0) { throw "INDEX_HTML not found in $Src" }
	$e = -1
	for ($i = $s + 1; $i -lt $lines.Length; $i++) {
		if ($lines[$i].TrimEnd().EndsWith('";')) { $e = $i; break }
	}
	if ($e -lt 0) { throw 'INDEX_HTML end ("; line) not found' }
	$t = ($lines[($s + 1)..$e] -join "`n")

	# C 토큰 해석: 주석은 건너뛰고 문자열 리터럴만 이어 붙인다(이스케이프 해석)
	$sb = New-Object Text.StringBuilder
	$n = $t.Length; $k = 0
	while ($k -lt $n) {
		$c = $t[$k]
		if ($c -eq '/' -and $k + 1 -lt $n -and $t[$k + 1] -eq '*') {
			$k = $t.IndexOf('*/', $k + 2); if ($k -lt 0) { throw 'unterminated comment' }; $k += 2; continue
		}
		if ($c -eq '/' -and $k + 1 -lt $n -and $t[$k + 1] -eq '/') {
			$k = $t.IndexOf("`n", $k); if ($k -lt 0) { $k = $n }; continue
		}
		if ($c -ne '"') { $k++; continue }
		$k++
		while ($true) {
			if ($k -ge $n) { throw 'unterminated string' }
			$c = $t[$k]
			if ($c -eq '"') { $k++; break }
			if ($c -eq "`n") { throw 'newline inside string literal' }
			if ($c -eq '\') {
				$d = $t[$k + 1]; $k += 2
				switch -CaseSensitive ($d) {
					'n' { [void]$sb.Append("`n") }
					't' { [void]$sb.Append("`t") }
					'r' { [void]$sb.Append("`r") }
					'\' { [void]$sb.Append('\') }
					'"' { [void]$sb.Append('"') }
					"'" { [void]$sb.Append("'") }
					'?' { [void]$sb.Append('?') }
					default { throw "unsupported escape \$d" }
				}
				continue
			}
			[void]$sb.Append($c); $k++
		}
	}
	$raw = (New-Object Text.UTF8Encoding $false).GetBytes($sb.ToString())

	$ms = New-Object IO.MemoryStream
	$gz = New-Object IO.Compression.GZipStream($ms, [IO.Compression.CompressionLevel]::Optimal)
	$gz.Write($raw, 0, $raw.Length); $gz.Close()
	$z = $ms.ToArray()

	$o = New-Object Text.StringBuilder
	[void]$o.Append("/* AUTO-GENERATED - DO NOT EDIT. source: web.c INDEX_HTML, generator: tools/gen_web_gz.ps1 (Keil Before Build) */`n")
	[void]$o.Append("/* html $($raw.Length) bytes -> gzip $($z.Length) bytes */`n")
	[void]$o.Append("#define INDEX_HTML_RAW_LEN $($raw.Length)u`n")
	[void]$o.Append("static const unsigned char INDEX_HTML_GZ[$($z.Length)] = {`n")
	for ($i = 0; $i -lt $z.Length; $i += 16) {
		$m = [Math]::Min(16, $z.Length - $i)
		$row = for ($j = 0; $j -lt $m; $j++) { '0x{0:x2}' -f $z[$i + $j] }
		[void]$o.Append(($row -join ',') + ",`n")
	}
	[void]$o.Append("};`n")
	$new = $o.ToString()

	$old = if (Test-Path $Out) { [IO.File]::ReadAllText($Out, [Text.Encoding]::UTF8) } else { '' }
	if ($old -ne $new) {
		[IO.File]::WriteAllText($Out, $new, (New-Object Text.UTF8Encoding $false))
		Write-Output "gen_web_gz: html $($raw.Length) -> gzip $($z.Length) bytes (updated)"
	} else {
		Write-Output "gen_web_gz: up to date ($($z.Length) bytes)"
	}
	exit 0
} catch {
	Write-Output "gen_web_gz: ERROR $($_.Exception.Message)"
	exit 1
}
