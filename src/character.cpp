#include "character.h"
#include "util.h"

int Character :: damage(int amount) {
    int with_defense = amount - getDefense();
    int actual_amount = max(0, with_defense);
    hitpoints -= actual_amount;
    return actual_amount;
}

int Character :: getDefense() {
    return 0;
}

void Character :: regenerateHealth(int turn) {
    int turn_delta = turn - turn_health_regenerated;
    int regen = (max_hitpoints * 0.0001) * turn_delta;
    hitpoints = min(hitpoints + regen, max_hitpoints);
    turn_health_regenerated = turn;
}

bool Character :: isAlive() {
    return hitpoints > 0;
}

bool Character :: is(Character * other) {
    return other != NULL && id.compare(other->id) == 0;
}

Character :: Character() {
    id = generateRandomString(40);
    turn_health_regenerated = 0;
}
