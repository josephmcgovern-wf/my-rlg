#ifndef __PLAYER_H
#define __PLAYER_H

#include <vector>
#include "character.h"
#include "object.h"

using namespace std;

class Player : public Character {
    private:
        static const int MAX_INVENTORY_SIZE = 10;
        static const int DEFAULT_MAX_CARRYING_WEIGHT = 150;
        static const int MAX_HITPOINTS = 150;
        static const int DEFAULT_MAX_STAMINA = 50;
        static const int DEFAULT_LIGHT_RADIUS = 5;
        static const int DEFAULT_MAX_MAGIC = 50;
        vector<Object *> inventory;
        vector<Object *> equipment;
        int getIndexToSwapEquipmentWith(string type);
        int getEquipmentWeight();
        int getInventoryWeight();
        int getMaxCarryWeight();
        void addSpell(Object * spell);

    public:
        vector<int> getIndexOfEquipmentType(string type);
        string getEquipmentTypeFromIndex(int index);
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
        int getDefense();
        bool hitWillConnect();
        bool hasEnoughStaminaForAttack(int damage);
        void reduceStaminaFromDamage(int amount);
        void regenerateStamina(int turn);
        string getStatusProgressBar(float base_percentage);
        int getDamageForSpell(Object * spell);
        void reduceMagicFromSpell(int damage);
        void regenerateMagic(int turn);
        bool hasEnoughMagicForSpell(int damage);
        void restoreHealth(int amount);

        // Variables
        int level;
        int skill_points;
        int strength_level;
        int dexterity_level;
        int intelligence_level;
        int max_stamina_points;
        int stamina_points;
        int max_magic;
        int magic;
        vector<Object *> spells;

        // Constructors
        Player();
};
#endif
