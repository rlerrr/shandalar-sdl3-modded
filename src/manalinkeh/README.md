# Static ManalinkEh fork

This directory is a source-only import from `C:\dev\manalink3-src` for the
modded Shandalar target.  It intentionally excludes old object files, DLL
exports, the linker script, injection patches, and other build products.

`ManalinkEh.asm` is retained as the authoritative ordered veneer list, but is
not compiled.  The build generates a native registry for all 11,814 veneers.
The smaller assembly helpers under `cards/` and `functions/` are also retained
as porting references and are initially excluded from the static target.

`mok_manalink_redirects.h` replaces MOK's executable jump patches with source
name mappings.  All live `EXE_*` function and data uses in the compiled source
set have been replaced with ordinary named references.  The declarations in
`shandalar_exe_functions.h` and `shandalar_exe_globals.h` form the temporary
host boundary while the two engines are joined.

The remaining absolute pointers are the two AI speculation snapshots in
`functions/ai.c`.  They copy large contiguous regions of MOK global storage;
those must become field-by-field snapshots because the SDL3 engine deliberately
does not preserve the executable's global layout.

The static archive builds on its own.  Linking it into `shandalar` and removing
the 73 replaced original implementations is the next integration step.
