"""Paid source and spell announcements for Heroic duel cases."""

from counted_battlefield_damage_recipes import Game, types


def heroic_rules(record):
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or 'Heroic' not in record.get('keywords', ()) or 'Creature' not in types(record)):
        raise ValueError('Whole Heroic creature Oracle record required')
    lines = record['oracle_text'].splitlines()
    triggers = [line for line in lines if line.startswith('Heroic — Whenever you cast a spell that targets ')]
    if len(triggers) != 1:
        raise ValueError('One printed Heroic spell-target trigger required')
    return lines


class HeroicBoard(Game):
    """Drive cast and announce separately so trigger choices reach the right event."""

    def build(self):
        """Optional witness setup."""

    def play_source(self):
        record = self.oracle[self.cards['source']['name']]
        lines = heroic_rules(record)
        self.emit('turn', player=self.owner)
        self.fund(self.foe, [0] * 6)
        self.cast('source')
        self.emit('resolve')
        self.cards['source']['zone'] = 'battlefield'
        self.cards['source']['tapped'] = 'This creature enters tapped.' in lines
        self.emit('refresh')
        self.emit('assert_card', ref='source', property='in_play', value=1)
        self.emit('assert_card', ref='source', property='tapped', value=int(self.cards['source']['tapped']))

    def heroic_spell(self, own_spell=True, *, target='source', choices=None, spell='trigger',
                     resolve=True, funding=None, before_resolve=None, cost=None):
        from recipes import cost_from_oracle

        actor = self.owner if own_spell else self.foe
        if spell not in self.cards:
            self.add('Defiant Strike', spell, actor, 'hand')
        item = self.cards[spell]
        if item['player'] != actor or item['zone'] != 'hand':
            raise ValueError('Heroic driver must be in the casting player’s hand')
        if item['name'] == 'Defiant Strike' and not any(
                card['player'] == actor and card['zone'] == 'library' for card in self.cards.values()):
            self.add('Forest', f'{spell}_draw', actor, 'library')

        payment = cost_from_oracle(self.oracle[item['name']]) if cost is None else cost
        funds = payment if funding is None else funding
        self.fund(actor, funds)
        self.emit('choose_target', ref=target, player=actor)
        self.emit('cast', ref=spell, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        item['zone'] = 'stack'
        self.pools[actor] = [have - due for have, due in zip(funds, payment)]
        if min(self.pools[actor]) < 0:
            raise ValueError('Unfunded Heroic driver')
        self.observe_mana()

        if choices is not None:
            choices(self)
        self.emit('announce_spell', ref=spell)
        if before_resolve is not None:
            before_resolve(self)
        if resolve:
            self.resolve_heroic_spell(spell)

    def resolve_heroic_spell(self, spell='trigger'):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.cards[spell]['zone'] = 'graveyard'
