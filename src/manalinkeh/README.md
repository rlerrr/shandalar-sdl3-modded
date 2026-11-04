# Static ManalinkEh fork

This directory integrates the October 2022 recovered source from
`rlerrr/manalink3-src` commit `c6797b6061f69ac0c21efaac3f6500b783ec9f70`.
The SDL host originally used the December 2020 tree at
`cad36b4807cc7353754332a1697e2c97fd3ec9d5`.

`ManalinkEh.asm` is the authoritative ordered list of 15,169 veneers. It is
input to the C registry generator; no injection assembly or DLL is loaded.
The excluded source aggregates listed in CMake remain historical references.

`mok_manalink_redirects.h` replaces the EXE jump patches with source mappings.
`shandalar_host_names.h` maps shared engine storage to its host owner. Typed
function/global declarations in `shandalar_exe_*.h` were resolved against the
MOK address annotations in `C:/dev/shandalar-decomp/Ghidra/mok.csv`.
EXE addresses are provenance, not runtime pointers.

Card extensions live in source-owned banks, including separate display banks.
Card copies preserve each destination's extension storage. The AI snapshots
save these banks and the imported rules, mana, and dice state by named fields;
they do not copy contiguous EXE address ranges.

Reconstruct the card registry using `tools/import_carddata_exe.py` with the
October 2022 `Magic.exe` and its adjacent `Cards.dat`. Original internal IDs,
null bindings, and sixteen dynamic slots are preserved. The reviewed Skyclave
Basilica pointer repair is pinned to the executable hash and original record.
See `tools/README.md` for reproduction commands.

Native x86 and WebAssembly builds pass the focused 139-case regression set,
including haste permissions, suspend, AI rollback, bindings, and Skyclave
Basilica. All 177 previously unimplemented behavior cases with recovered
handlers pass. The deterministic 849-case behavior sample has 843 passes,
five existing known failures, and one existing unimplemented card, with no
unexpected failures. The full behavior and catalog suites have not been run.
See PORTING.md for validation details and remaining limits.
