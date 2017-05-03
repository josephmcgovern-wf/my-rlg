#ifndef __PLAYER_H
#define __PLAYER_H

#include <vector>
#include "character.h"
#include "object.h"

using namespace std;

class Player : public Character {
    private:
        static const int MAX_INVENTORY_SIZE = 10;
        static const int DEFAULT_MAX_CARRYING_WEIGHT = 250;
        static const int MAX_HITPOINTS = 500;
        static const int DEFAULT_LIGHT_RADIUS = 5;
        vector<Object *> inventory;
        vector<Object *> equipment;
        int getIndexToSwapEquipmentWith(string type);
        vector<int> getIndexOfEquipmentType(string type);
        string getEquipmentTypeFromIndex(int index);
        int getEquipmentWeight();
        int getInventoryWeight();
        int getMaxCarryWeight();

    public:
        bool hasObject(Object *);
        string getHudInfo();
        bool isOverEncumbered();
        int getLightRadius();
        int getWeight();
        int getSpeed();
        int getAttackDamage();
        int getRangedAttackDamage();
        bool canPickUpObject();
        void addObjectToInventory(Object *);
        string viewInventoryObjectAt(int index);
        void equipObjectAt(int index);
        int getNumberOfItemsInInventory();
        bool objectExistsInInventoryAt(int index);
        Object * getInventoryItemAt(int index);
        int equipmentSlots();
        string equipmentSlotToString(int index);
        bool equipmentExistsAt(int index);
        Object * getEquipmentAt(int index);
        void takeOffEquipment(int index);
        void removeInventoryItemAt(int index);
        bool hasRangedWeapon();
        void addExperience(int xp);
        int getExperienceRequiredForNextLevel();
        void levelUpSkill(string skill);
        bool willDodgeAttack();

        // Variables
        int level;
        int skill_points;
        int strength_level;
        int dexterity_level;
        int intelligence_level;

        // Constructors
        Player();
        ~Player();
};
#endif
