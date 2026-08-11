# Regenerate the AtlasCube text fonts (components/ui/fonts/lv_font_montserrat_*_eu.c).
#
# Needs lv_font_conv on PATH:  npm i -g lv_font_conv
# Sources are the TTF/WOFF that ship with LVGL, so nothing extra to download.
#
# Runs from the LVGL font dir so the "Opts:" header comment each generated file
# carries records bare filenames — that comment is the record of how the file
# was produced, and absolute Windows paths there would be noise.
#
# The digit-only _72/_80/_96/_120 clock fonts are NOT generated here; they are
# one-offs whose ranges live in their own headers.

$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
$work = Join-Path $repo "managed_components\lvgl__lvgl\scripts\built_in_font"
$dest = Join-Path $repo "components\ui\fonts"

if (-not (Test-Path $work)) {
    throw "LVGL sources not found at $work - run 'idf.py reconfigure' to fetch managed_components first."
}

$mont = "Montserrat-Medium.ttf"
$fa   = "FontAwesome5-Solid+Brands+Regular.woff"

# Every Latin-script alphabet used in Europe: ASCII, Latin-1 Supplement (German,
# French, Spanish, Portuguese, Italian, Nordic, Icelandic), Latin Extended-A
# (Polish, Czech, Slovak, Hungarian, Croatian, Slovenian, Baltic, Turkish,
# Maltese, Welsh) and the Romanian comma-below forms that live outside it.
#
# Whole contiguous blocks on purpose, not a hand-picked set of code points:
# lv_font_conv turns a contiguous range into a compact cmap (a base offset),
# while scattered code points become a sparse list costing 2 extra bytes per
# glyph. Cherry-picking "only what we need" would cost more, not less.
$eu = @("--range", "0x20-0x7E",
        "--range", "0x00A0-0x00FF",
        "--range", "0x0100-0x017F",
        "--range", "0x0218-0x021B")

# Per-size FontAwesome glyphs, carried over from the fonts these replaced.
$icons = @{
     8 = @()
    10 = @()
    12 = @()
    14 = @("--range", "0xF001", "--range", "0xF293", "--range", "0xF0F3", "--range", "0xF7C2")
    18 = @("--range", "0xF293")
    24 = @("--range", "0xF293")
}

# 48 px stands in for LVGL's built-in montserrat_48 (disabled in
# sdkconfig.defaults), so it must also carry the full LV_SYMBOL_* set - the
# control and hub overlays render their button glyphs at this size. The list is
# lifted verbatim from LVGL's own built-in font generator, as are the degree
# sign (inside Latin-1 Supplement already) and the bullet.
$sym = "61441,61448,61451,61452,61452,61453,61457,61459,61461,61465,61468," +
       "61473,61478,61479,61480,61502,61507,61512,61515,61516,61517,61521," +
       "61522,61523,61524,61543,61544,61550,61552,61553,61556,61559,61560," +
       "61561,61563,61587,61589,61636,61637,61639,61641,61664,61671,61674," +
       "61683,61724,61732,61787,61931,62016,62017,62018,62019,62020,62087," +
       "62099,62212,62189,62810,63426,63650"

Push-Location $work
try {
    foreach ($size in 8, 10, 12, 14, 18, 24, 48) {
        $out = "lv_font_montserrat_${size}_eu.c"
        $a = @("--font", $mont, "--size", "$size", "--bpp", "4") + $eu

        if ($size -eq 48) {
            $a += @("--range", "0x2022", "--font", $fa, "--size", "48", "--range", $sym)
        } elseif ($icons[$size].Count -gt 0) {
            $a += @("--font", $fa, "--size", "$size") + $icons[$size]
        }

        $a += @("--format", "lvgl", "--no-compress", "-o", $out)

        Write-Host "==> $out"
        & lv_font_conv @a
        if ($LASTEXITCODE -ne 0) { throw "lv_font_conv failed for $out" }

        # lv_font_conv emits an "lvgl/lvgl.h" fallback include; this project puts
        # lvgl.h straight on the include path. Written with an explicit BOM-less
        # UTF-8 encoder because Set-Content on Windows PowerShell 5.1 would
        # prepend a BOM.
        $text = (Get-Content $out -Raw).Replace('#include "lvgl/lvgl.h"', '#include "lvgl.h"')
        [IO.File]::WriteAllText((Join-Path $dest $out), $text, (New-Object Text.UTF8Encoding $false))
        Remove-Item $out
    }
} finally {
    Pop-Location
}

Write-Host "done - regenerated 7 fonts in components/ui/fonts/"
