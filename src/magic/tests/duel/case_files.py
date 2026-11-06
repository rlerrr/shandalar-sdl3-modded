"""Load small, independently authored card and scenario case files."""
import json
import re
from functools import lru_cache
from pathlib import Path


def select_case_files(directory, *, case_files=None, csvids=None):
    """Select whole authoring files without expanding recipes outside the selection.

    File selectors are relative to data/cases (or use manifest paths beginning
    with cases/). CSVID selectors refer to file-level card homes, not scenarios
    which happen to mention the card. Selectors are a union, and every requested
    file/card must exist so a typo cannot silently produce an incomplete run.
    """
    directory = Path(directory).resolve()
    selected = set()
    if not case_files and not csvids:
        raise ValueError('A case subset requires at least one case file or CSVID')
    for value in case_files or ():
        relative = Path(value)
        if relative.is_absolute() or '..' in relative.parts:
            raise ValueError(f'Case file selectors must stay relative to {directory}: {value}')
        if relative.parts and relative.parts[0] == directory.name:
            relative = Path(*relative.parts[1:])
        path = (directory / relative).resolve()
        if not path.is_relative_to(directory) or path.suffix != '.json' or not path.is_file():
            raise ValueError(f'Unknown case file selector: {value}')
        selected.add(path)
    wanted = set(csvids or ())
    if any(type(value) is not int or value < 0 for value in wanted):
        raise ValueError('Case subset CSVIDs must be nonnegative integers')
    if wanted:
        found = set()
        # Reading the small declarations is cheap; expanding all their recipes
        # here would defeat source-only authoring on the full catalog.
        for path in sorted(directory.rglob('*.json')):
            try:
                document = json.loads(path.read_text(encoding='utf-8'))
            except (OSError, ValueError) as error:
                raise ValueError(f'{path}: {error}') from error
            if isinstance(document, dict) and document.get('card') is not None and document.get('csvid') in wanted:
                selected.add(path.resolve())
                found.add(document['csvid'])
        if wanted - found:
            raise ValueError(f'No card case home for CSVID selectors: {sorted(wanted - found)}')
    return sorted(selected)


def load_cases(directory, registry, oracle=None, aliases=None, *, _files=None, _homes=None):
    directory = Path(directory)
    files = sorted(directory.rglob("*.json")) if _files is None else _files
    if not files:
        raise ValueError(f"No behavior case files found in {directory}")
    cases = []
    names = {}
    card_homes = {} if _homes is None else _homes
    for path in files:
        source = path.relative_to(directory.parent).as_posix()
        try:
            document = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError) as error:
            raise ValueError(f"{source}: {error}") from error
        if not isinstance(document, dict) or document.get("version") != 1:
            raise ValueError(f"{source}: unsupported case file schema")
        entries = document.get("cases", [])
        declarations = document.get("recipes", [])
        if not isinstance(entries, list) or not isinstance(declarations, list) or not (entries or declarations):
            raise ValueError(f"{source}: expected a nonempty cases or recipes array")
        if any(isinstance(e, dict) and any(k.startswith("_") for k in e) for e in entries):
            raise ValueError(f"{source}: underscore fields are reserved for generated provenance")
        card = document.get("card")
        if card is not None:
            if card not in registry or document.get("csvid") != registry[card]["csvid"]:
                raise ValueError(f"{source}: card name and CSVID must match the engine registry")
            csvid = document["csvid"]
            if csvid in card_homes:
                raise ValueError(f"{source}: duplicate card home for CSVID {csvid}: {card_homes[csvid]}")
            card_homes[csvid] = source
        elif "csvid" in document:
            raise ValueError(f"{source}: CSVID requires a file-level card name")
        if declarations:
            if card is None or oracle is None:
                raise ValueError(f"{source}: recipes require a card home and pinned Oracle context")
            if registry[card].get("kind", "card") in ("alternate", "internal", "token", "effect"):
                raise ValueError(f"{source}: Oracle card recipes require a primary card; use authored scenarios for alternate forms, tokens and internal effects")
            from recipes import expand
            record = oracle.get((aliases or {}).get(card, card))
            entries = list(entries)
            for declaration in declarations:
                try:
                    entries.extend(expand(card, document["csvid"], declaration, record, registry, oracle_context=oracle))
                except ValueError as error:
                    raise ValueError(f"{source}: {error}") from error
        for entry in entries:
            if not isinstance(entry, dict):
                raise ValueError(f"{source}: each case must be an object")
            case = dict(entry)
            if card is not None:
                if case.get("card", card) != card:
                    raise ValueError(f"{source}: case card conflicts with its card file")
                case["card"] = card
            if case.get("card") not in registry:
                raise ValueError(f"{source}: case has an unknown or missing card name")
            name = case.get("name")
            if not isinstance(name, str) or not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", name):
                raise ValueError(f"{source}: invalid case name {name!r}")
            if name in names:
                raise ValueError(f"{source}: duplicate case {name}, also defined in {names[name]}")
            names[name] = source
            case["_source_file"] = source
            cases.append(case)
    # Moving files or assigning a set must not change execution order or generated IDs.
    return sorted(cases, key=lambda case: case["name"])


class CaseStream:
    """Index names once, then expand only a bounded number of card files.

    Global name order and generated IDs match load_cases exactly. Keeping the
    index separate from full step dictionaries avoids retaining the expanded
    catalog in memory during C++ emission.
    """
    def __init__(self, directory, registry, oracle=None, aliases=None, cache_size=4, *, files=None):
        if type(cache_size) is not int or cache_size < 1:
            raise ValueError('case stream cache requires a positive file count')
        self.directory=Path(directory)
        self.arguments=(registry,oracle,aliases)
        files=sorted(self.directory.rglob('*.json')) if files is None else sorted(files)
        if not files:raise ValueError(f'No behavior case files found in {self.directory}')
        self.index=[];self.recipe_count=0;homes={};names={}
        for path in files:
            cases=load_cases(self.directory,*self.arguments,_files=[path],_homes=homes)
            for i,case in enumerate(cases):
                name=case['name'];source=case['_source_file']
                if name in names:raise ValueError(f'{source}: duplicate case {name}, also defined in {names[name]}')
                names[name]=source
                self.index.append((name,path,i))
                self.recipe_count += '_recipe' in case
        self.index.sort()
        self.cache_size=cache_size

    def __len__(self):return len(self.index)

    def __iter__(self):
        @lru_cache(maxsize=self.cache_size)
        def expand_file(path):
            return load_cases(self.directory,*self.arguments,_files=[path])
        for name,path,index in self.index:
            cases=expand_file(path)
            if index>=len(cases) or cases[index]['name']!=name:
                raise ValueError(f'{path}: case inventory changed during generation')
            yield cases[index]
