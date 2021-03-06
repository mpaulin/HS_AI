#ifndef __ACTIONS_H__
#define __ACTIONS_H__
#include "common.h"

// ---------- Actions ---------------

struct Action {
  SET_ENGINE();
  const int cost;
  const char need_slot;
  const Target target;

  Action(int cost = 0, char need_slot = false, Target target = 0) :
    cost(cost), need_slot(need_slot), target(target) {}
  
  virtual string tostr() const = 0;

  bool need_target() const { return target.is_targetable(); }
  virtual bool is_valid(const Player* pl) const { return true; }

  //virtual bool neighbors() { return false; } // return True if minion with neighbor aura / battlecry involved
  virtual int get_cost() const { return cost; }
  
  //  # number of possible outcomes
  virtual int randomness() { return 1; }

  virtual bool execute(Instance* caster, Instance* choice, const Slot& slot) const;
};



/// Basic game actions

struct Act_EndTurn : public Action {
  Act_EndTurn() : Action(0) {}

  virtual string tostr() const { return "End turn"; }
  virtual bool execute(Instance* caster, Instance* choice, const Slot& slot) const;
};


/// Play card

struct Act_PlayCard : public Action { 
  const Card * const card;
  const FuncAction actions;

  Act_PlayCard(const Card* card, const bool need_slot, FuncAction actions, Target targets = 0);

  virtual bool execute(Instance* caster, Instance* choice, const Slot& slot) const;
};


// -------- Minions -----------------

struct Act_PlayMinionCard : public Act_PlayCard {
  const Card_Minion* card_minion() const;

  Act_PlayMinionCard(const Card_Minion* card);

  virtual string tostr() const;
  virtual bool is_valid(const Player* pl) const;
  virtual bool execute(Instance* caster, Instance* choice, const Slot& slot) const;
};

// battlecry are actions!--------------

struct Act_Battlecry : public Action {
  const FuncAction action;

  Act_Battlecry(FuncAction action, Target target, char need_slot=0) :
    Action(0, need_slot, target), action(action) {}

  virtual void execute(Instance* from, Instance* target, const Slot& slot) {
    action(this, from, target, slot);
  }
};

struct Act_BC_Damage : public Act_Battlecry {
  const int damage;
  Act_BC_Damage(Target target, int damage);
  virtual string tostr() const;
};

struct Act_BC_Buff : public Act_Battlecry {
  const string desc;
  const int atq, hp;
  const int static_effects;

  Act_BC_Buff(string desc, Target target, int atq, int hp, int static_eff=0);

  virtual string tostr() const;
};

struct Act_BC_Ngh : public Act_BC_Buff {
  Act_BC_Ngh(string desc, int atq, int hp, int static_eff = 0):
    Act_BC_Buff(desc, Target::neighbors, atq, hp, static_eff) {}
};

struct Act_BC_DrawCard : public Act_Battlecry {
  const int nb_card;

  Act_BC_DrawCard(Target target, int nb);

  virtual string tostr() const;
};

/*struct Act_PlayMinionCard_BC(Act_PlayMinionCard) :
  ''' hero plays a minion card with battlecry '''
  def __init__(card, battlecry, chosable_targets = None, hidden_target = None) :
  Act_PlayMinionCard.___init___(card)
  self.choices = [self.engine.board.get_free_slots(card.owner)]
  self.hidden_target = None
  if chosable_targets : self.choices += [chosable_targets]
    elif hidden_target : self.hidden_target = hidden_target
    self.battlecry = battlecry
    def neighbors() :
    return self.hidden_target == 'neighbors' or Act_PlayMinionCard.neighbors()
    def execute() :
    Act_PlayCard.execute()
    pos = self.choices[0]
    assert type(pos).__name__ == 'Slot', pdb.set_trace()
    from creatures import Minion
    minion = Minion(self.card)
    actions = [Msg_AddMinion(self.caster, minion, pos)]
    if self.hidden_target == 'neighbors':
for target in self.card.owner.minions[max(0, pos.index - 1):pos.index + 1] :
actions.append(Msg_BindEffect(minion, target, self.battlecry()))
elif len(self.choices)>1 or self.hidden_target:
target = self.hidden_target or self.choices[1]
actions.append(Msg_BindEffect(minion, target, self.battlecry()))
self.engine.send_message(actions)*/


struct Act_Attack : public Action {
  Creature* const creature;
  Act_Attack(Creature* creature) :
    Action(0, false, Target::attackable | Target::enemy | Target::characters), 
    creature(creature) {}

  virtual string tostr() const;
  virtual bool execute(Instance* caster, Instance* choice, const Slot& slot) const;
};

// ------------- Weapon cards ------------------------

struct Act_PlayWeaponCard : public Act_PlayCard {
  const Card_Weapon* card_weapon() const;

  /// hero plays a weapon card 
  Act_PlayWeaponCard(const Card_Weapon* card);

  virtual string tostr() const;
  virtual bool is_valid(const Player* pl) const { return true; }
  virtual bool execute(Instance* caster, Instance* choice, const Slot& slot) const;
};


/// ------------- Card Spells ------------------------

struct Act_SpellCard : public Act_PlayCard {
  /// hero plays a generic spell card, specified using "actions"
  Act_SpellCard(const Card_Spell* card, FuncAction actions, Target targets = 0);

  virtual bool execute(Instance* caster, Instance* choice, const Slot& slot) const;
};

struct Act_TargetedSpellCard : public Act_SpellCard {
  Act_TargetedSpellCard(const Card_TargetedSpell* card, FuncAction actions, Target targets);

  virtual string tostr() const;

  virtual bool execute(Instance* caster, Instance* choice, const Slot& slot) const {
    assert(choice);
    return Act_SpellCard::execute(caster, choice, slot);
  }
};

struct Act_AreaSpellCard : public Act_SpellCard {
  Act_AreaSpellCard(const Card_AreaSpell* card, FuncAction actions, Target targets);

  virtual string tostr() const;

  virtual bool execute(Instance* caster, Instance* choice, const Slot& slot) const {
    assert(!choice);
    return Act_SpellCard::execute(caster, choice, slot);
  }
};


//struct Act_SingleSpellDamageCard(Act_PlaySpellCard) :
//  ''' inflict damage to a single target'''
//  def __init__(card, Target, damage) :
//  def actions() :
//  target = self.choices[0]
//  assert type(target) != list
//  return[Msg_SpellDamage(self.caster, target, self.damage)]
//  Act_PlaySpellCard.__init__(card, Target, actions)
//  self.damage = damage
//
//struct Act_MultiSpellDamageCard(Act_PlaySpellCard) :
//  ''' inflict damage to multiple Target'''
//  def __init__(target, card, damage) :
//  Act_PlaySpellCard.__init__(card, Target, damage = damage)
//  def execute() :
//  Act_PlayCard.execute()
//  self.engine.send_message([
//    Msg_StartSpell(self.caster, self.card),
//      [Msg_SpellDamage(self.caster, t, self.damage) for t in self.choices[0]],
//      Msg_EndSpell(self.caster, self.card),
//  ])
//
//struct Act_RandomSpellDamageCard(Act_PlaySpellCard) :
//  ''' inflict damage to random Target'''
//  def __init__(card, target, damage) :
//  Act_PlaySpellCard.__init__(card, target, damage = damage)
//  def execute() :
//  Act_PlaySpellCard.execute()
//  self.engine.send_message([
//    Msg_StartSpell(self.caster, self.card),
//      [Msg_MultiRandomSpellDamage(self.caster, self.choices[0], self.damage)],
//      Msg_EndSpell(self.caster, self.card),
//  ])
//
//
//struct Act_PlaySecretCard(Act_PlaySpellCard) :
//  pass

//### ------------------- Hero powers -----------------

struct Act_HeroPower : public Action {
  const Card_HeroAbility * const card;
  FuncAction action;

  Act_HeroPower(const Card_HeroAbility* card, int cost, FuncAction action, Target target) :
    Action(cost, false, target), card(card), action(action) {}

  virtual string tostr() const;
  virtual bool execute(Instance* caster, Instance* choice, const Slot& slot) const;
};


#endif