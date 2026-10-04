# Configuring adventure cards

The adventure reads `Shandalar.ini` from its asset directory (normally
`C:\dev\assets_modded`). Native and WASM builds use the same rules. The WASM
startup bundle includes this file when present; rebuild the WASM target after
editing that bundled configuration. Additional `[Configs]` files can also be
loaded through the browser's existing asset fetcher.

The pool is initialized during shared startup, after `CARDS.DAT` and raw card
rarities are loaded. Calls before the database exists leave it uninitialized;
they cannot cache an empty pool. Campaign rarities are restored after raw
rarities are copied, before applying the configuration.

Missing configuration or an empty `[Sets]` enables all implemented, playable
cards supported by the adventure save format. The existing asset configuration
also restricts individual cards and selects its own 30 dungeon treasures.
The real asset database currently yields **15,938 cards eligible for random
rewards**, plus **30 dungeon treasures**.

## Set rules

```ini
[Sets]
--Ice Age=enable
--=disable
```

Names come from `@EXPANSIONNAMES` in `Menus.txt`, beginning with Promo. The first
eight format entries are excluded. Supported ranges are `Legends`,
`Legends--Ice Age`, `--Ice Age`, `Chronicles--`, and `--`.

Rules retain file order, including duplicate keys. The first rule matching any
printing of a card wins. Unmatched cards default to `enable`.

| Disposition | Starters / shops | Card chooser | Random rewards |
| --- | --- | --- | --- |
| `enable` | Yes | Yes | Yes |
| `nosale` | No | Yes | Yes |
| `nochoose` | No | No | Yes |
| `disable` | No | No | No |

`ignore` removes the named sets from other rules' matching, even if the ignore
rule appears later. A reprint in another set can still match. `--` matches every
card, including cards whose printings are ignored or absent from `Rarity.dat`.
The binary rarity header supplies the record stride; unnamed trailing sets in
the supplied `Rarity.dat` are tolerated.

## Individual cards and included files

`[Enable]`, `[Nosale]`, `[Nochoose]`, `[Disable]`, and `[DungeonTreasure]` take
precedence over set rules, in that order of increasing precedence, regardless
of section order. List one full card name or numeric **CSV ID** (from
`card_id.h`) per line:

```ini
[Enable]
Lightning Bolt

[Nochoose]
Channel

[Disable]
Duh
```

The five original basic lands are always enabled. Individual overrides cannot
make tokens, effects, unimplemented cards, transformed back faces, or IDs that
cannot fit in a savegame into adventure rewards. Restrictions affect card
acquisition; enemy decks and already-owned cards retain their card handlers.

`[DungeonTreasure]` removes its cards from the ordinary acquisition pool and
places the first 30 distinct eligible entries into the ten treasure dungeons.
Additional entries stay excluded. With no entries, the documented default
treasure list is used. The supplied INI's custom list replaces that default.

`[Configs]` lists additional filenames, one per line, relative to the asset
directory. Each parent file's entries precede its included files, which are read
in listed order. Repeated filenames and cycles are skipped. A maximum of 64
configuration files bounds erroneous include chains.

The registry's existing INI parser now has an ordered-entry API for this
configuration. It accepts bare names and preserves duplicate rules; the ordinary
`registry.ini` API retains its strict key/value syntax. Invalid set rules or
malformed configuration log a diagnostic and retain the original campaign pool.
Unavailable individual card entries are logged and skipped.

## Compatibility and validation

The old `[Cards] AllowAllCreatures` and `AllowAllCards` switches described vanilla
proxies in Korath's fork. They do not gate this fork's full Manalink handlers.
Other INI gameplay and appearance options are outside this card-pool change.

Adventure save words currently reserve bits 14 and higher for flags, so internal
card IDs above `0x3fff` are excluded from generated cards. Expanding that limit
requires a separate save-format change; selecting such an ID would otherwise
silently turn it into another card.

Starters scan finite candidate pools. Missing spell/creature/rare categories
fall back to enabled basic lands, preserving deck size. Shops also scan finite
pools and leave a slot empty if no candidate fits. The chooser uses bounded
pages with Previous/Next controls instead of overflowing its old 300-card
collection or 64-entry visible-card array. Other legacy reward routines can
still require particular colors or rarities; configurations should retain those
categories.

`card_pool_test` covers reprints, rule order, ignores, overrides, includes,
treasure limits, binary stride, and strict registry parsing.
`campaign_new_game_test` loads the real card database and INI, exercises all
five wizard colors and four difficulty levels, then repeats with only basics
enabled. It also checks shop selection and empty categories.
It verifies that an early opening-menu initialization attempt cannot disable
every card and that the database-loaded callback enables the pool afterwards.
