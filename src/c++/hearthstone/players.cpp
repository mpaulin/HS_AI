#include "engine.h"
#include "players.h"
#include "creatures.h"
#include "Cards.h"
#include "decks.h"
#include "messages.h"


Engine* Player::engine = nullptr;


Player::Player(PHero hero, string name, Deck* deck) :
  name(name), deck(deck), act_end_turn() {
  state.mana = state.max_mana = 0;
  state.hero = hero;
  state.minions_pos = { 0.f, 1000.f };
  state.spell_power = 0;
  state.auchenai = state.velen = false;

  state.hero->set_controller(this);
  deck->set_owner(this);
}

Player::~Player() {
  state.cards.clear();
}

string Player::tostr() const {
  return state.hero->tostr();
}

bool Player::add_thing(PInstance thing, Slot pos) {
  PMinion m;
  PWeapon w;
  if (issubclassP(thing, Hero)) {
    NI;
  }
  else if (w=issubclassP(thing, Weapon)) {
    if (state.weapon) // destroy it
      state.weapon->destroy();
    state.weapon = w;
    UPDATE_PLAYER("weapon", Msg_AddWeapon, w);
    engine->board.signal(w.get(), Event::AddWeapon);
  }
  else if (issubclassP(thing, Secret)) {
    NI;
    //state.secrets.push_back(dynamic_pointer_cast<Secret>(thing));
    //engine->send_message(Msg_SecretPopup(state.secrets.back()));
  }
  else if (state.minions.size() < 7 && (m = issubclassP(thing, Minion))) {
    const int n = state.minions_pos.size();
    if (pos.fpos<=0 || pos.fpos >= 1000)  // helper to insert at right by default
      pos.fpos = (state.minions_pos[n - 2] + state.minions_pos[n - 1]) / 2;
    const int i = pos.insert_after_pos(state.minions_pos);
    if (state.minions_pos[i - 1] == pos.fpos) // already exist, so create new number
      pos.fpos = (pos.fpos + state.minions_pos[i]) / 2;
    pos.pos = i - 1;
    state.minions_pos.insert(state.minions_pos.begin()+i, pos.fpos);
    state.minions.insert(state.minions.begin() + (i - 1), m);
    UPDATE_PLAYER("minions", Msg_AddMinion, m, pos);
    engine->board.signal(m.get(), Event::AddMinion);
  }
  else
    return false;
  return true;
}

Slot Player::remove_thing(PInstance thing) {
  PWeapon weapon;
  PSecret secret;
  PMinion minion;
  if (weapon = issubclassP(thing, Weapon)) {
    assert(weapon == state.weapon);
    state.hero->unequip_weapon();
    state.weapon = nullptr;
    UPDATE_PLAYER("remove weapon", Msg_RemoveWeapon, weapon);
    engine->board.signal(weapon.get(), Event::RemoveWeapon);
  }
  else if (minion = issubclassP(thing, Minion)) {
    Slot pos = engine->board.get_minion_pos(minion.get());
    state.minions_pos.erase(state.minions_pos.begin() + pos.pos + 1);
    state.minions.erase(state.minions.begin() + pos.pos);
    UPDATE_PLAYER("remove minions", Msg_RemoveMinion, minion, pos);
    engine->board.signal(minion.get(), Event::RemoveMinion);
    return pos;
  }
  else if (secret = issubclassP(thing, Secret)) {
    NI;
    remove(state.secrets, secret);
    engine->board.signal(secret.get(), Event::RemoveSecret);
  }
  return Slot();
}

void Player::draw_init_cards(int nb, bool coin) {
  ListPCard kept = deck->draw_init_cards(nb, &Player::mulligan);
  if (coin) {
    PCard coin = NEWP(Card_Coin);
    UPDATE_PLAYER("draw_init_cards coin", Msg_NewCard, nullptr, coin);
    kept.push_back(coin);
  }
  for (auto& card : kept) {
    state.cards.push_back(card);
    UPDATE_PLAYER("draw_init_cards", Msg_ReceiveCard, nullptr, card, this, engine->board.state.turn);
  }
}

void Player::draw_card(Instance* origin) {
  PCard card = deck->draw_one_card();
  if (card) give_card(card, origin);
}

void Player::give_card(PCard card, Instance* origin) {
  if (len(state.cards) < 10) {
    state.cards.push_back(card);
    UPDATE_PLAYER("give_card", Msg_ReceiveCard, GETP(origin), card, this, engine->board.state.turn);
  }
  else 
    UPDATE_PLAYER("give_card", Msg_BurnCard, GETP(origin), card, this);
}

void Player::throw_card(PCard card) {
  remove(state.cards, card);
  UPDATE_PLAYER("throw_card", Msg_ThrowCard, state.hero, card);
}

void Player::add_mana_crystal(int nb, bool useit) {
  state.mana = min(10, state.mana + nb);
  state.max_mana = min(10, state.max_mana + nb);
  if (useit) use_mana(nb);
  UPDATE_PLAYER_STATE("add_mana_crystal");
}

void Player::use_mana(int nb) {
  state.mana -= nb;
  assert(state.mana >= 0);
  UPDATE_PLAYER_STATE("use_mana");
}

void Player::gain_mana(int nb) {
  state.mana = min(10, state.mana + nb);
  UPDATE_PLAYER_STATE("gain_mana");
}

void Player::start_turn() {
  // then add mana crystal and draw a card
  add_mana_crystal(1);
  state.mana = state.max_mana;
  engine->board.draw_card(nullptr, this);
  // inform interface = last thing !
  UPDATE_PLAYER("start_turn",Msg_StartTurn, state.hero);
}

void Player::end_turn() {
  // inform interface (kinda useless)
  UPDATE_PLAYER("end_turn", Msg_EndTurn, state.hero);
}

ListAction Player::list_actions() {
  ListAction res = { &act_end_turn };
  // first, hero power
  state.hero->list_actions(res);
  // then, all card's actions
  for (auto& card : state.cards)
    card->list_actions(res);
  // then, weapon / hero's attack (if any)
  if (state.weapon)
    state.weapon->list_actions(res);
  // then, all minions actions
  for (auto& m : state.minions)
    m->list_actions(res);
  return res;
}

float Player::score_situation() {
  //float res = state.hero->score_situation();
  //for (card : state.cards)
  //  res += card->score;
  //if (state.weapon)
  //  res += state.weapon->score_situation();
  //for (m : state.minions)
  //  res += m->score_situation();
  //for (m : state.secrets)
  //  res += m->score_situation();
  //return res;
  assert(0); return 0;
}

const Action* RandomPlayer::choose_actions(ListAction actions, Instance*& choice, Slot& slot) {
  // select one action in the list
  int r = randint(0, len(actions) - 1);
  const Action* action = actions[r];

  // select one target for this action
  if (action->need_slot) {
    ListSlot slots = engine->board.get_free_slots(const_cast<RandomPlayer*>(this));
    slot = slots[randint(0, len(slots)-1)];
  }

  if (action->need_target()) {
    ListPInstance choices = action->target.resolve(const_cast<RandomPlayer*>(this));
    if (len(choices))
      choice = choices[randint(0, len(choices)-1)].get();
  }
  return action;
}
