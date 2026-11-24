"""Paid permanent casts and mana fixtures for Devoid duel cases."""

from counted_battlefield_damage_recipes import Game, types


DEVOID = 'Devoid (This card has no color.)'


def devoid_rules(record):
    lines = record.get('oracle_text', '').splitlines()
    if (record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or record.get('colors') != [] or 'Devoid' not in record.get('keywords', ())
            or lines.count(DEVOID) != 1):
        raise ValueError('Whole colorless Devoid Oracle record required')
    return [line for line in lines if line != DEVOID]


class DevoidBoard(Game):
    def build(self):
        """Optional witness setup."""

    def play_source(self, *, target=None, before_resolve=None):
        source = self.cards['source']
        oracle = self.oracle[source['name']]
        devoid_rules(oracle)
        if not set(types(oracle)).intersection(('Artifact', 'Creature', 'Enchantment', 'Planeswalker')):
            raise ValueError('Expected a Devoid permanent spell')
        self.emit('turn', player=self.owner)
        self.fund(self.foe, [0] * 6)
        self.cast('source', target=target)
        if before_resolve is not None:
            before_resolve(self)
        self.emit('resolve')
        source['zone'] = 'battlefield'
        self.emit('refresh')
        self.emit('assert_card', ref='source', property='in_play', value=1)
        self.emit('assert_matching_permanents', player=self.owner, index=self.rows[source['name']]['csvid'],
                  property='color', value=0)

    def fund_colorless(self, colorless, generic=0, *, generic_color=0, player=None):
        if not 0 <= generic_color < 6 or min(colorless, generic) < 0:
            raise ValueError('Nonnegative actual colorless and generic funding required')
        pool = [0] * 6
        pool[0] = colorless
        pool[generic_color] += generic
        self.fund(self.owner if player is None else player, pool)
