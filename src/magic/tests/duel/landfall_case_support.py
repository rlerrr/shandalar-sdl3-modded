"""Shared actual land-play and phase steps for Landfall fixtures."""

from cast_trigger_case_support import CastTriggerBoard


def landfall_lines(record):
    lines = record['oracle_text'].splitlines()
    if (record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or 'Landfall' not in record.get('keywords', ())
            or sum(line.startswith('Landfall — ') for line in lines) != 1):
        raise ValueError('Whole pinned Oracle record with a single Landfall clause required')
    return lines


class LandfallBoard(CastTriggerBoard):
    def __init__(self, name, shape, mode, owner, rows, oracle):
        self.active = owner
        super().__init__(name, shape, mode, owner, rows, oracle)

    def play_land(self, ref='land', *, choices=None, allowed=True):
        land = self.cards[ref]
        if land['player'] != self.active or land['zone'] != 'hand':
            raise ValueError('Land play requires a named land in the active seat’s hand')
        if choices is not None:
            choices(self)
        self.emit('play_land', ref=ref, value=int(allowed))
        if allowed:
            land['zone'] = 'battlefield'
            self.emit('resolve')
            self.emit('refresh')
            self.emit('assert_card', ref=ref, property='in_play', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)

    def advance_turn(self):
        if any(map(any, self.pools)):
            raise ValueError('Real untap requires empty mana pools')
        self.emit('advance_turn')
        self.active = 1 - self.active
        self.emit('untap_step', player=self.active)
        for item in self.cards.values():
            item['damage'] = 0
            item['boost'] = 0
            if item['player'] == self.active and item['zone'] == 'battlefield':
                item['tapped'] = False
        self.emit('turn', player=self.active)
