#include "player.h"
#include "util.h"
#include <iostream>
#include <math.h>

void Player :: restoreHealth(int amount) {
    hitpoints = min(max_hitpoints, hitpoints + amount);
}

void Player :: reduceMagicFromSpell(int damage) {
    int actual_amount = ceil(damage * 0.7);
    int bonus = 0;
    if (intelligence_level) {
        bonus = intelligence_level + ceil(pow(intelligence_level + 1, 1.5));
    }
    actual_amount -= bonus;
    magic = max(0, magic - actual_amount);

}

void Player :: regenerateMagic(int turn) {
    int turn_delta = turn - turn_health_regenerated;
    int regen = (max_magic * 0.009) * turn_delta;
    magic = min(magic + regen, max_magic);
}

bool Player :: hasEnoughMagicForSpell(int damage) {
    int bonus = 0;
    if (intelligence_level) {
        bonus = intelligence_level + ceil(pow(intelligence_level + 1, 1.5));
    }
    return (ceil(damage * 0.5) - bonus) <= magic;
}

int Player :: getDamageForSpell(Object * spell) {
    int damage = spell->damage_bonus->roll();
    int bonus = 0;
    if (intelligence_level) {
        bonus = intelligence_level + ceil(pow(intelligence_level + 1, 1.5));
    }
    return damage + bonus;
}

void Player :: regenerateStamina(int turn) {
    int turn_delta = turn - turn_health_regenerated;
    int regen = (max_stamina_points * 0.009) * turn_delta;
    stamina_points = min(stamina_points + regen, max_stamina_points);
}

bool Player :: hasEnoughStaminaForAttack(int damage) {
    int bonus = 0;
    if (dexterity_level) {
        bonus = dexterity_level + ceil(pow(dexterity_level + 1, 1.5));
    }
    return (ceil(damage * 0.5) - bonus) <= stamina_points;
}

void Player :: reduceStaminaFromDamage(int amount) {
    int actual_amount = ceil(amount * 0.7);
    int bonus = 0;
    if (dexterity_level) {
        bonus = dexterity_level + ceil(pow(dexterity_level + 1, 1.5));
    }
    actual_amount -= bonus;
    stamina_points = max(0, stamina_points - actual_amount);
}

bool Player :: hitWillConnect() {
    int bonus = 0;
    for (int i = 0; i < equipment.size(); i++) {
        Object * item = equipment[i];
        if (item) {
            bonus += item->hit_bonus;
        }
    }
    int chance = min(70 + bonus, 100);
    return random_int(1, 100) <= chance;
}

int Player :: getDefense() {
    int defense = 0;
    for (int i = 0; i < equipment.size(); i++) {
        Object * item = equipment[i];
        if (item) {
            defense += item->defense_bonus;
        }
    }
    int bonus = 0;
    if  (strength_level) {
        bonus = random_int(0, strength_level + ceil(pow(strength_level + 1, 1.5)));
    }
    return bonus + defense;
}

bool Player :: willDodgeAttack() {
    int summed_chance = 0;
    for (int i = 0; i < equipment.size(); i++) {
        Object * object = equipment[i];
        if (object) {
            summed_chance += object->dodge_bonus;
        }
    }
    int bonus = 0;
    if (dexterity_level) {
        bonus = ceil(pow(dexterity_level + 1, 1.5));
    }
    int chance = 1 + summed_chance + bonus;
    return random_int(1, 100) <= chance;
}

int Player :: getMaxCarryWeight() {
    return DEFAULT_MAX_CARRYING_WEIGHT + ceil(strength_level * 50);
}

void Player :: levelUpSkill(string skill) {
    if (!skill.compare("Strength")) {
        strength_level ++;
        max_hitpoints += 50;
        hitpoints += 50;
    }
    else if(!skill.compare("Dexterity")) {
        dexterity_level ++;
        max_stamina_points += 50;
        stamina_points += 50;
    }
    else if(!skill.compare("Intelligence")) {
        intelligence_level ++;
    }
    else {
        throw "Invalid skill: " + skill;
    }
    hitpoints = max_hitpoints;
    stamina_points = max_stamina_points;
    magic = max_magic;
    skill_points --;
}

void Player :: addExperience(int xp) {
    int xp_required = getExperienceRequiredForNextLevel();
    int extra_xp = random_int(0, ceil(intelligence_level * 5));
    experience += xp + extra_xp;
    if (experience >= xp_required) {
        int diff = experience - xp_required;
        experience = diff;
        level ++;
        skill_points ++;
    }
}

int Player :: getExperienceRequiredForNextLevel() {
    return ceil(20 * pow(level + 1, 0.6));
}

int Player :: getLightRadius() {
    int bonus = 0;
    if (intelligence_level) {
        bonus = ceil(pow(intelligence_level + 1, 1.5));
    }
    int radius = DEFAULT_LIGHT_RADIUS + bonus;
    int index = getIndexOfEquipmentType("LIGHT")[0];
    Object * light_item = equipment[index];
    if (light_item) {
        radius += light_item->special_attribute;
    }
    return radius;
}

int Player :: getSpeed() {
    int bonus = 0;
    if (dexterity_level) {
        bonus = ceil(pow(dexterity_level + 1, 1.8));
    }
    int my_speed = speed + bonus;
    for (size_t i = 0; i < equipment.size(); i++) {
        Object * object = equipment[i];
        if (object) {
            my_speed += object->speed_bonus;
        }
    }
    if (isOverEncumbered()) {
        my_speed = my_speed / 4;
    }
    return my_speed;
}

string Player :: getHudInfo() {
    string info = "Level: " + to_string(level) + "\n";
    info += "Carry: " + to_string(getWeight()) + "/" + to_string(getMaxCarryWeight()) + "\n";
    info += "Speed: " + to_string(getSpeed()) + "\n";
    info += "Armour: " + to_string(getDefense()) + "\n";
    info += "Strength Level: " + to_string(strength_level) + "\n";
    info += "Dexterity Level: " + to_string(dexterity_level) + "\n";
    info += "Intelligence Level: " + to_string(intelligence_level) + "\n";
    return info;
}

string Player :: getStatusProgressBar(float base_percentage) {
    string str = "|";
    int percentage = base_percentage * 30;
    for (int i = 1; i <= 30; i++) {
        if (i <= percentage) {
            str += "#";
        }
        else {
            str += " ";
        }
    }
    return str + "|";
}

bool Player :: isOverEncumbered() {
    return getWeight() > getMaxCarryWeight();
}

int Player :: getEquipmentWeight() {
    int weight = 0;
    for (size_t i = 0; i < equipment.size(); i++) {
        Object * object = equipment[i];
        if (object) {
            weight += object->weight;
        }
    }
    return weight;
}

int Player :: getInventoryWeight() {
    int weight = 0;
    for (size_t i = 0; i < inventory.size(); i++) {
        Object * object = inventory[i];
        weight += object->weight;
    }
    return weight;
}

int Player :: getWeight() {
    return getEquipmentWeight() + getInventoryWeight();
}

int Player :: getAttackDamage() {
    Numeric * dice = attack_damage;
    if (equipment[0]) {
        dice = equipment[0]->damage_bonus;
    }
    int damage = dice->roll();

    int bonus = 0;
    if  (strength_level) {
        bonus = strength_level + ceil(pow(strength_level + 1, 1.5));
    }
    int extra_damage = ceil(strength_level * 5);
    damage += extra_damage;
    for (int i = 1; i < equipment.size(); i++) {
        Object * object = equipment[i];
        if (object && i != 2) { // ignore ranged
            damage += object->damage_bonus->roll();
        }
    }
    return damage;
}

int Player :: getRangedAttackDamage() {
    if (!equipment[2]) {
        return 0;
    }
    Object * range = equipment[2];
    int range_damage = range->damage_bonus->roll();
    int bonus = 0;
    if (bonus) {
        bonus = dexterity_level + ceil(pow(dexterity_level + 1, 1.5));
    }
    return range_damage + bonus;
}

bool Player :: canPickUpObject() {
    return inventory.size() < MAX_INVENTORY_SIZE;
}

void Player :: addObjectToInventory (Object * object) {
    inventory.push_back(object);
}

string Player :: viewInventoryObjectAt(int index) {
    return inventory[index]->description;
}

void Player :: addSpell(Object * spell) {
    for(int i = 0; i < spells.size(); i++) {
        Object * existing_spell = spells[i];
        if (existing_spell->name.compare(spell->name) == 0) {
            throw "You have already learned that spell!";
        }
    }
    spells.push_back(spell);
}

void Player :: equipObjectAt(int index) {
    Object * object = inventory[index];
    if (object && object->type.compare("SPELL") == 0) {
        addSpell(object);
        inventory.erase(inventory.begin() + index);
    }
    else if (object) {
        int equipmentIndex = getIndexToSwapEquipmentWith(object->type);
        Object * equippedObject = equipment[equipmentIndex];
        if (equippedObject) {
            inventory[index] = equippedObject;
        }
        else {
            inventory.erase(inventory.begin() + index);
        }
        equipment[equipmentIndex] = object;
    }
}

int Player :: getIndexToSwapEquipmentWith(string type) {
    vector<int> indexes = getIndexOfEquipmentType(type);
    for (size_t i = 0; i < indexes.size(); i++) {
        int index = indexes[i];
        if (!equipment[index]) {
            return index;
        }
    }
    return indexes[0];
}

vector<int> Player :: getIndexOfEquipmentType(string type) {
    vector<int> indexes;
    if (!type.compare("WEAPON")) {
        indexes.push_back(0);
    }
    if (!type.compare("OFFHAND")) {
        indexes.push_back(1);
    }
    if (!type.compare("RANGED")) {
        indexes.push_back(2);
    }
    if (!type.compare("ARMOR")) {
        indexes.push_back(3);
    }
    if (!type.compare("HELMET")) {
        indexes.push_back(4);
    }
    if (!type.compare("CLOAK")) {
        indexes.push_back(5);
    }
    if (!type.compare("GLOVES")) {
        indexes.push_back(6);
    }
    if (!type.compare("BOOTS")) {
        indexes.push_back(7);
    }
    if (!type.compare("AMULET")) {
        indexes.push_back(8);
    }
    if (!type.compare("LIGHT")) {
        indexes.push_back(9);
    }
    if (!type.compare("RING")) {
        indexes.push_back(10);
        indexes.push_back(11);
    }
    return indexes;
}


int Player :: getNumberOfItemsInInventory() {
    return inventory.size();
}

bool Player :: objectExistsInInventoryAt(int index) {
    if (index < 0 || (size_t) index > inventory.size() - 1) {
        return false;
    }
    return inventory[index] != NULL;
}

Object * Player :: getInventoryItemAt(int index) {
    return inventory[index];
}

int Player :: equipmentSlots() {
    return equipment.size();
}

string Player :: equipmentSlotToString(int index) {
    string type = getEquipmentTypeFromIndex(index);
    string message = type + ": ";
    Object * object = equipment[index];
    if (object) {
        message += object->name;
    }
    else {
        message += "Empty";
    }
    return message;
}

string Player :: getEquipmentTypeFromIndex(int index) {
    if (index == 0) {
        return "WEAPON";
    }
    if (index == 1) {
        return "OFFHAND";
    }
    if (index == 2) {
        return "RANGED";
    }
    if (index == 3) {
        return "ARMOR";
    }
    if (index == 4) {
        return "HELMET";
    }
    if (index == 5) {
        return "CLOAK";
    }
    if (index == 6) {
        return "GLOVES";
    }
    if (index == 7) {
        return "BOOTS";
    }
    if (index == 8) {
        return "AMULET";
    }
    if (index == 9) {
        return "LIGHT";
    }
    if (index == 10 || 11) {
        return "RING";
    }
    return "Unknown";
}

bool Player :: equipmentExistsAt(int index) {
    if (index < 0 || (size_t) index > equipment.size() - 1) {
        return false;
    }
    return equipment[index] != NULL;
}

Object * Player :: getEquipmentAt(int index) {
    return equipment[index];
}

void Player :: takeOffEquipment(int index) {
    Object * item = equipment[index];
    equipment[index] = NULL;
    inventory.push_back(item);
}

void Player :: removeInventoryItemAt(int index) {
    inventory.erase(inventory.begin() + index);
}

bool Player :: hasObject(Object * o) {
    for (int i = 0; i < inventory.size(); i++) {
        Object * item = inventory[i];
        if (item && item == o) {
            return true;
        }
    }
    for (int i = 0; i < equipment.size(); i++) {
        Object * item = equipment[i];
        if (item && item == o) {
            return true;
        }
    }
    return false;
}

bool Player :: hasRangedWeapon() {
    return equipment[2] != NULL;
}

Player :: Player() : Character() {
    experience = 0;
    //skill_points = 0;
    skill_points = 3;
    level = 1;
    attack_damage = new Numeric("0+1d10");
    speed = 20;
    strength_level = 0;
    dexterity_level = 0;
    intelligence_level = 0;
    hitpoints = MAX_HITPOINTS;
    max_hitpoints = MAX_HITPOINTS;
    stamina_points = DEFAULT_MAX_STAMINA;
    max_stamina_points = DEFAULT_MAX_STAMINA;
    max_magic = DEFAULT_MAX_MAGIC;
    magic = max_magic;
    x = 0;
    y = 0;
    for (int i = 0; i < 12; i++) {
        equipment.push_back(NULL);
    }
}

Player :: ~Player() {
    inventory.clear();
}
