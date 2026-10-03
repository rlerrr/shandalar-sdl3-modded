# Asset conversion tools

This directory contains utilities for preparing original game assets for the
SDL3 ports.

## `import_carddata.py`

Rebuilds `src/magic/src/carddata.c` and its count/effect-ID header from the
semicolon-separated `ct_all.csv` and `Manalink.csv` in a supplied directory:

```powershell
python tools/import_carddata.py "C:\Manalink\test build 12-1-2020"
```

The importer preserves internal card order, joins full names to CSV IDs and AI
values, packs the engine flags, and resolves DLL addresses through
`src/manalinkeh/ManalinkEh.asm`. Original executable addresses are mapped in
`carddata_legacy_handlers.json`. Unrecognized names or addresses stop the
import. Zero addresses are uncoded and excluded from the playable engine table;
vanilla cards use `card_dummy`.
The sixteen sentinel records remain available for runtime card copies.

`carddata_campaign_metadata.json` retains expansion eligibility and the
campaign-only extra flags from the original Shandalar table at `0x594208`,
keyed by CSV ID. Manalink's copies of these fields are unused and usually zero;
importing those zeros would leave the campaign without eligible starter lands.
The importer merges this metadata without changing card order or handlers.
Campaign cards must fit the adventure deck's 14-bit ID field; bits 14 and 15
remain the market and selection flags.

## `convert_statwin_assets.ps1`

Converts the original Statwin media into the `Statwin_SDL` layout consumed by
the SDL3 implementation and its asset test.

## `convert_wavs_to_ima_adpcm.ps1`

Recursively converts WAV assets to IMA ADPCM while preserving their paths.
