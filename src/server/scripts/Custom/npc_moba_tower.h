#ifndef NPC_MOBA_TOWER_H
#define NPC_MOBA_TOWER_H

class Creature;
class Player;

// Called by the global aggro-override hook (moba_tower_aggro.cpp) when an
// enemy player damages or hard-CCs an allied player within a tower's range.
void MobaTowerAggroOverride(Creature* tower, Player* offender);

#endif
