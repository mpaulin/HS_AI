'''
Generalistic classes
'''
import pdb
from messages import *
from copy import deepcopy


### ------------ generalistic thing (hero, weapon, minon) -------

class Thing (object):
  @classmethod
  def set_engine(cls, engine):
      cls.engine = engine

  def __init__(self, card, owner=None ):
      self.card = card
      self.owner = owner or card.owner
      self.hp = self.max_hp = card.hp
      self.atq = self.max_atq = card.atq

      # some status
      self.n_max_atq = 1 # number of times we can attack per turn
      self.n_atq = 99 # number of times we attacked already
      self.dead = False
      self.status_id = 0
      self.frozen_count = 0 # number of turns before getting unfrozen

      # what is below is what can be silenced
      self.action_filters = []  # reacting to actions [(Act_class, handler),...]
      self.modifiers = [] # modifying messages, list: [(Msg_Class, event),...]
      self.triggers = [] # reacting to messages, list: [(Msg_Class, event),...]
      self.effects = []  # list of effects without triggers: ['taunt','stealth',...]
      self.add_effects(deepcopy(card.effects), inform=False)

  def add_effects(self, effects, inform=True):
      for e in effects:  # we have to initalize effects
        if type(e)==str:  
          if e not in self.effects: # useless otherwise
            self.effects.append(e)
            if e in ('charge','windfury'):
              self.popup()  # redo popup to set n_atq/n_max_atq
            elif e=='frozen':
              self.frozen_count = 3
        else:
          e.bind_to(self)
      if inform:
        self.engine.send_message(Msg_Status(self,'effects'),immediate=True)

  def __str__(self):
      return "%s %s (%X) %d/%d" %(type(self).__name__, self.card.name, id(self), self.atq, self.hp)

  def list_actions(self):
      assert 0, 'must be overloaded'

  def filter_action(self,action):
      for trigger,event in self.action_filters:
        if issubclass(type(action),trigger):
          action = event.filter(action)
      return action # default = do nothing

  def modify_msg(self, msg):
      for trigger,event in self.modifiers:
        if issubclass(type(msg),trigger):
          msg = event.modify(msg)
      return msg

  def react_msg(self, msg):
      if issubclass(type(msg), Msg_StartTurn) and self.has_effect('frozen'):
        # hard-coded (native) trigger for frozen effect
        self.frozen_count -= 1
        if self.frozen_count<=0:
          self.effects.remove('frozen')
          self.engine.send_message(Msg_Status(self,'effects'),immediate=True)
      for trigger,event in list(self.triggers): #copy because modified online
        if type(trigger)==str: continue
        if issubclass(type(msg),trigger):
          event.trigger(msg)

  def has_effect(self, effect):
      return effect in self.effects

  def popup(self):  # executed when created
    self.n_max_atq = 2 if self.has_effect('windfury') else 1
    if self.has_effect('charge') and self.n_atq==99:
      self.n_atq = 0  # we can attack !
  
  def start_turn(self):
      self.n_atq = 0  # didn't attack yet this turn

  def end_turn(self):
      pass

  def hurt(self, damage):
      assert damage>0, pdb.set_trace()
      self.hp -= damage
      self.engine.send_message(Msg_Status(self,'hp'),immediate=True)
      self.check_dead()

  def heal(self, hp):
      assert hp>0, pdb.set_trace()
      self.hp = min(self.max_hp, self.hp+hp)
      self.engine.send_message(Msg_Status(self,'hp'),immediate=True)

  def change_hp(self, hp):
        self.max_hp += hp
        assert self.max_hp>=1, pdb.set_trace()
        self.hp += max(0,hp)  # only add if positive
        self.hp = min(self.hp, self.max_hp)
        self.engine.send_message(Msg_Status(self,'hp max_hp'),immediate=True)
        self.check_dead()

  def change_atq(self, atq):
        self.atq += atq
        self.max_atq += atq
        self.engine.send_message(Msg_Status(self,'atq max_atq'),immediate=True)

  def silence(self):
      self.action_filter = []
      self.modifiers = []
      self.triggers = []
      while self.effects:
        e = self.effects.pop()
        if type(e)!=str:  
          e.undo()
        elif e=="windfury":
          self.n_max_atq = 1  # undo windfury
      self.effects = ['silence']
      if not self.dead:
        self.engine.send_message(Msg_Status(self,'hp max_hp atq max_atq effects'),immediate=True)

  def check_dead(self):
      if self.hp <= 0:
        self.dead = True
        self.engine.send_message( Msg_CheckDead(self) )

  def ask_for_death(self):
      self.engine.send_message(Msg_Dead(self),immediate=True)

  def death(self):
      self.silence()
      self.engine.board.remove_thing(self)


### ------------ Secret ----------

class Secret (Thing):
  def __init__(self, card, hero ):
      Thing.__init__(self, card )
      self.hero = hero
      self.active = False

  def list_actions(self):
      return None

  def ask_for_death(self):
      self.engine.send_message( Msg_DeadSecret(self), immediate=True)


### ------------ Weapon ----------

class Weapon (Thing):
  def __init__(self, card, owner=None ):
      Thing.__init__(self, card, owner=owner )
      self.hero = card.owner.hero

  def list_actions(self):
      if self.hero.n_atq>=self.hero.n_max_atq or self.hero.has_effect('frozen'):
        return []
      else:
        from actions import Act_WeaponAttack
        return [Act_WeaponAttack(self, self.engine.board.get_attackable_characters(self.owner))]

  def attacks(self, target):
      self.hero.n_atq += 1
      assert self.hero.n_atq<=self.hero.n_max_atq
      msgs = [Msg_Damage(self, target, self.atq)]
      if target.atq: msgs.append(Msg_Damage(target, self.hero, target.atq))
      self.hurt(1)
      self.engine.send_message([Msg_StartAttack(self.hero,target),
                                msgs,
                                Msg_EndAttack(self.hero)])

  def ask_for_death(self):
      self.engine.send_message( Msg_DeadWeapon(self), immediate=True)


### ------------ Creature (hero or minion) ----------

class Creature (Thing):
  def __init__(self, card, owner=None ):
      Thing.__init__(self, card, owner=owner )

  def hurt(self, damage):
      assert damage>0, pdb.set_trace()
      if self.has_effect('divine_shield'):
        self.effects.remove('divine_shield')
        self.engine.send_message(Msg_Status(self,'effects'),immediate=True)
      else:
        Thing.hurt(self,damage)

  def attacks(self, target):
      self.n_atq += 1
      assert self.n_atq<=self.n_max_atq
      if self.has_effect('stealth'):
        self.effects.remove('stealth')
        self.engine.send_message(Msg_Status(self,'effects'),immediate=True)
      msgs = [Msg_Damage(self, target, self.atq)]
      if target.atq: msgs.append(Msg_Damage(target, self, target.atq))
      self.engine.send_message([Msg_StartAttack(self,target),
                                msgs,
                                Msg_EndAttack(self)])




### ------------ Minion ----------


class Minion (Creature):
  def __init__(self, card, owner=None ):
      Creature.__init__(self, card, owner=owner )

  def has_taunt(self):
      return 'taunt' in self.effects

  def has_stealth(self):
      return 'stealth' in self.effects

  def list_actions(self):
      if self.n_atq>=self.n_max_atq or self.atq==0 or self.has_effect('frozen'):
        return []
      else:
        from actions import Act_MinionAttack
        return [Act_MinionAttack(self, self.engine.board.get_attackable_characters(self.owner))]

  def ask_for_death(self):
      self.engine.send_message( Msg_DeadMinion(self), immediate=True)

















