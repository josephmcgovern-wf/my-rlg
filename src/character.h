#ifndef __CHARACTER_H
#define __CHARACTER_H
#include "numeric.h"
#include "board_element.h"

class Character : public BoardElement {
    public:
        string id;
        int turn_health_regenerated;
        int speed;
        int max_hitpoints;
        int hitpoints;
        int experience;
        Numeric * attack_damage;
        int damage(int amount);
        bool isAlive();
        bool is(Character * other);
        void regenerateHealth(int turn);
        int getDefense();
        bool hitWillConnect();
        Character();
};
#endif
