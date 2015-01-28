#ifndef __PLAYERS_H__
#define __PLAYERS_H__
#include "common.h"

#include "creatures.h"
#include "decks.h"
#include "cards.h"

typedef vector<float> ArrFloat;
class Engine;

struct Player {
  SET_ENGINE();
  string name;
  Deck* const deck;
  struct State {
    int mana, max_mana;
    ListPCard cards;
    PHero hero;
    PWeapon weapon;
    ListPMinion minions;
    ArrFloat minions_pos;
    ListPSecret secrets;
  } state;

  Player(PHero hero, string name, Deck* deck) :
    name(name), deck(deck), state{ 0 } {
    state.hero = hero;
    state.minions_pos = { 0.f, 1000.f };

    state.hero->set_owner(this);
    deck->set_owner(this);
  }

  /*def save_state(num = 0) :
  state.deck.save_state(num)
  state.saved[num] = dict(cards = list(state.cards), minions = list(state.minions), minions_pos = list(state.minions_pos),
  mana = state.mana, max_mana = state.max_mana, weapon = state.weapon, secrets = list(state.secrets))
  def restore_state(num = 0) :
  state.deck.restore_state(num)
  state.__dict__.update(state.saved[num])
  state.cards = list(state.cards)
  state.minions = list(state.minions)
  state.minions_pos = list(state.minions_pos)
  state.secrets = list(state.secrets)
  def end_simulation() :
  state.saved = dict()*/

  string tostr() const {
    return state.hero->tostr();
  }

  bool add_thing(PInstance thing, Slot pos) {
    if (issubclass(thing, Hero)) {
      assert(!"todo");
    }
    else if (issubclass(thing, Weapon)) {
      if (state.weapon) // kill it
        state.weapon.ask_for_death();
      state.weapon = dynamic_pointer_cast<Weapon>(thing);
      engine->send_message(Msg_WeaponPopup(state.weapon));
    }
    else if (issubclass(thing, Secret)) {
      state.secrets.push_back(dynamic_pointer_cast<Secret>(thing));
      engine->send_message(Msg_SecretPopup(state.secrets.back()));
    }
    else if (issubclass(thing, Minion) && state.minions.size() < 7) {
      auto n = state.minions_pos.size();
      if (pos.fpos >= 1000)  // helper
        pos.fpos = (state.minions_pos[n - 2] + state.minions_pos[n - 1]) / 2;
      int i = searchsorted(state.minions_pos, pos.fpos);
      if (state.minions_pos[i - 1] == pos.fpos) // already exist, so create new number
        pos.fpos = (pos.fpos + mp[i]) / 2;
      state.minions_pos.insert(i, pos.fpos);
      state.minions.insert(i - 1, dynamic_pointer_cast<Minion>(thing));
      engine->send_message(Msg_MinionPopup(state.minions[i - 1], i - 1));
    }
    else
      return false;
    return true;
  }

  void remove_thing(PInstance thing) {
    if (thing == state.weapon)
      state.weapon = nullptr;
    else if (issubclass(thing, Secret)
      state.secrets.remove(thing);
    else if (issubclass(thing, Minion) {
      int i = state.minions.index(thing);
      state.minions_pos.erase(i + 1);
      state.minions.pop(i);
    }
  }

  void add_mana_crystal(int nb, bool useit = false) {
    state.mana = min(10, state.mana + nb);
    state.max_mana = min(10, state.max_mana + nb);
    if (useit) use_mana(nb);
  }

  void use_mana(int nb) {
    state.mana -= nb;
    assert(state.mana >= 0);
  }

  void gain_mana(int nb) {
    state.mana = min(10, state.mana + nb);
  }

  void start_turn() {
    // activated by Msg_StartTurn(player)
    state.hero->start_turn();
    for (m : state.minions)
      m->start_turn();
    if (state.weapon)
      state.weapon->start_turn();
    state.mana = state.max_mana;
    add_mana_crystal(1);
    engine->send_message(Msg_DrawCard(this));
  }

  void end_turn() {
    state.hero->end_turn();
    for (m : state.minions)
      m->end_turn();
  }

  ListPAction list_actions() {
    ListPAction res = { make_shared<Act_EndTurn>(this) };
    // first, hero power
    res.push_back(state.hero->list_action());
    // then, all card's actions
    for (card : state.cards)
      res.push_back(card->list_action());
    // then, weapon / hero's attack (if any)
    if (state.weapon)
      res.push_back(state.weapon->list_action());
    // then, all minions actions
    for (m : state.minions)
      res.push_back(m.list_action());
    return res;
  }

  void draw_card() {
    PCard card = deck->draw_one_card();
    if (card) {
      if (state.cards.size() < 10) {
        state.cards.append(card);
        SEND_MSG(Msg_CardDrawn, this, card);
      }
      else
        SEND_MSG(Msg_DrawBurnCard, this, card);
    }
  }

  void give_card(PCard card, Instance* origin) {
    state.cards.append(card);
    SEND_MSG(Msg_CardDrawn, this, card, origin);
  }

  void throw_card(PCard card) {
    state.cards.remove(card);
  }

  virtual void mulligan(ListPCard cards) = 0;

  void draw_init_cards(int nb, bool coin = false) {
    state.cards = deck->draw_init_cards(nb, mulligan);
    if (coin)
      state.cards.puh_back(NEWP(Card_Coin, this));
    for (c : state.cards)
      SEND_MSG(Msg_CardDrawn, this, c);
  }

  virtual PAction choose_actions(ListPAction actions) = 0;

  float score_situation() {
    float res = state.hero->score_situation();
    for (card : state.cards)
      res += card.score;
    if (state.weapon)
      res += state.weapon->score_situation();
    for (m : state.minions)
      res += m->score_situation();
    for (m : state.secrets)
      res += m->score_situation();
    return res;
  }
};


// ----------- Manual (human) player -----------

//struct HumanPlayer : public Player:
//
//  ''' human player : ask the player what to do'''
//    def mulligan(cards) :
//    assert 0, "to be implemented by user interface"
//
//    def choose_actions(actions) :
//    assert 0, "to be implemented by user interface"


// ------ stupidest player ever ------------

struct RandomPlayer : public Player {
  // random player : just do random things

  virtual ListPCard mulligan(ListPCard cards) {
    // keep everything without changing
    return{};
  }

  virtual PAction choose_actions(ListPAction actions) {
    // select one action in the list
    int r = randint(0, len(actions) - 1);
    PAction action = actions[r];
    // select one target for this action
    for (ch : action.choices) {
      if ch :
      action.select(i, random.randint(0, len(ch) - 1));
    }
    return action;
  }
};



#endif