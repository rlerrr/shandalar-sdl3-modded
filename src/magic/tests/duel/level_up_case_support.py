"""Oracle level bands and paid Level Up actions for duel fixtures."""
import re

from counted_battlefield_damage_recipes import Game


LEVEL_UP = re.compile(r'^Level up (.*?) \((.*?): Put a level counter on this\. Level up only as a sorcery\.\)$')
FIRST_BAND = re.compile(r'^LEVEL (\d+)-(\d+)$')
SECOND_BAND = re.compile(r'^LEVEL (\d+)\+$')
STATS = re.compile(r'^(\d+)/(\d+)$')


def classify_leveler(record):
    from recipes import cost_from_oracle

    if not record or record.get('layout') != 'leveler' or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('Whole leveler Oracle record required')
    if not record['type_line'].startswith('Creature — ') or 'Level Up' not in record['keywords']:
        raise ValueError('Level Up creature required')
    lines = record['oracle_text'].splitlines()
    if len(lines) < 5:
        raise ValueError('Complete Level Up rules and level bands required')
    fee = LEVEL_UP.fullmatch(lines[0])
    lower = FIRST_BAND.fullmatch(lines[1])
    upper = [(index, match) for index, line in enumerate(lines) if (match := SECOND_BAND.fullmatch(line))]
    if not fee or fee[1] != fee[2] or not lower or len(upper) != 1:
        raise ValueError('Exact Level Up price and two bands required')
    (second, highest), = upper
    first_stats = STATS.fullmatch(lines[2])
    second_stats = STATS.fullmatch(lines[second + 1]) if 3 <= second < len(lines) - 1 else None
    if not 1 <= int(lower[1]) <= int(lower[2]) or int(lower[2]) + 1 != int(highest[1]) or not first_stats or not second_stats:
        raise ValueError('Two consecutive fixed-stat bands required')
    return dict(price=cost_from_oracle(record), activation=cost_from_oracle(dict(mana_cost=fee[1])),
                base=[int(record['power']), int(record['toughness'])],
                first=[int(lower[1]), int(first_stats[1]), int(first_stats[2])],
                second=[int(highest[1]), int(second_stats[1]), int(second_stats[2])],
                first_riders=lines[3:second], second_riders=lines[second + 2:])


class LevelUpBoard(Game):
    """Drive the common rules through normal cast, activation, and resolution."""

    def __init__(self, name, shape, mode, owner, rows, oracle):
        self.level = 0
        super().__init__(name, shape, mode, owner, rows, oracle)

    def build(self):
        """Optional witness setup before the game starts."""

    def source_stats(self, level=None):
        level = self.level if level is None else level
        if level >= self.shape['second'][0]:
            return self.shape['second'][1:]
        if level >= self.shape['first'][0]:
            return self.shape['first'][1:]
        return self.shape['base']

    def observe_level(self, level=None):
        self.emit('assert_card', ref='source', property='in_play', value=1)
        for attribute, value in zip(('power', 'toughness'), self.source_stats(level)):
            self.emit('assert_card', ref='source', property=attribute, value=value)

    def play_source(self):
        self.emit('turn', player=self.owner)
        self.fund(self.foe, [0]*6)
        self.cast('source')
        self.emit('resolve')
        self.cards['source']['zone'] = 'battlefield'
        self.emit('refresh')
        self.observe_level()

    def level_to(self, target, *, choose=None):
        """Pay each activation. Optional choose(board) scripts a real offered mode."""
        if not self.level <= target:
            raise ValueError('Level Up cannot remove counters')
        while self.level < target:
            self.fund(self.owner, self.shape['activation'])
            self.emit('require_can_activate', ref='source', value=1)
            if choose:
                choose(self)
            self.emit('activate', ref='source', value=1)
            self.emit('assert_stack', value=1)
            self.pools[self.owner] = [0]*6
            self.observe_mana()
            self.emit('assert_choices_consumed')
            self.observe_level()
            self.emit('resolve')
            self.level += 1
            self.emit('refresh')
            self.observe_level()

    def advance_turn(self):
        self.emit('advance_turn')
        self.pools = [[0]*6 for _ in (0, 1)]
