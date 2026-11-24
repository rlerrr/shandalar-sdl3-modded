"""Shared paid cast and announcement steps for cast-trigger recipes."""

from counted_battlefield_damage_recipes import Game, types


class CastTriggerBoard(Game):
    def build(self):
        """Optional witness setup."""

    def play_source(self, *, target=None, before_resolve=None):
        self.emit('turn', player=self.owner)
        self.fund(self.foe, [0] * 6)
        self.cast('source', target=target)
        if before_resolve is not None:
            before_resolve(self)
        self.emit('resolve')
        self.cards['source']['zone'] = 'battlefield'
        self.emit('refresh')
        self.emit('assert_card', ref='source', property='in_play', value=1)

    def cast_trigger_spell(self, ref, *, target=None, funding=None, response=False,
                           choices=None, resolve=True, destination=None):
        from recipes import COLORS

        spell = self.cards[ref]
        record = self.oracle[spell['name']]
        self.cast(ref, target=target, funding=funding, response=response)
        self.emit('assert_card', ref=ref, property='color', value=sum(1 << COLORS[color] for color in record['colors']))
        if choices is not None:
            choices(self)
        self.emit('announce_spell', ref=ref)
        if resolve:
            self.emit('resolve_response' if response else 'resolve')
            self.emit('refresh')
            self.emit('assert_choices_consumed')
            self.emit('assert_stack', value=int(response))
            if destination is None:
                destination = 'graveyard' if set(types(record)) & {'Instant', 'Sorcery'} else 'battlefield'
            spell['zone'] = destination
