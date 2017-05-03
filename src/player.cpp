#include "player.h"
#include <iostream>
#include <math.h>

void Player :: addExperience(int xp) {
    int xp_required = getExperienceRequiredForNextLevel();
    experience += xp;
    if (experience >= xp_required) {
        int diff = experience - xp_required;
        experience = diff;
        level ++;
    }
}

int Player :: getExperienceRequiredForNextLevel() {
    return ceil(40 * pow(level + 1, 0.6));
}

int Player :: getLightRadius() {
    int radius = DEFAULT_LIGHT_RADIUS;
    int index = getIndexOfEquipmentType("LIGHT")[0];
    Object * light_item = equipment[index];
    if (light_item) {
        radius += light_item->special_attribute;
    }
    return radius;
}

int Player :: getSpeed() {
    int my_speed = speed;
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
    info += "Health: " + to_string(hitpoints) + "/" + to_string(MAX_HITPOINTS) + "\n";
    info += "Carry: " + to_string(getWeight()) + "/" + to_string(DEFAULT_MAX_CARRYING_WEIGHT) + "\n";
    info += "XP: " + to_string(experience) + "/" + to_string(getExperienceRequiredForNextLevel());
    return info;

}

bool Player :: isOverEncumbered() {
   return getWeight() > DEFAULT_MAX_CARRYING_WEIGHT;
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
    for (size_t i = 1; i < equipment.size(); i++) {
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
    return range->damage_bonus->roll();
}

bool Player :: canPickUpObject() {
    return inventory.size() < (size_t) MAX_INVENTORY_SIZE;
}

void Player :: addObjectToInventory (Object * object) {
    inventory.push_back(object);
}

string Player :: viewInventoryObjectAt(int index) {
    return inventory[index]->description;
}

void Player :: equipObjectAt(int index) {
    Object * object = inventory[index];
    if (object) {
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

bool Player :: hasRangedWeapon() {
    return equipment[2] != NULL;
}

Player :: Player() : Character() {
    experience = 0;
    level = 1;
    attack_damage = new Numeric("0+1d4");
    speed = 30;
    hitpoints = MAX_HITPOINTS;
    max_hitpoints = MAX_HITPOINTS;
    x = 0;
    y = 0;
    for (int i = 0; i < 12; i++) {
        if (i == 0) {
            Object * obj = new Object();
            obj->name = "Sword";
            obj->description = "Boss sword";
            obj->type = "WEAPON";
            obj->color = "BLUE";
            obj->hit_bonus = 5;
            obj->damage_bonus = new Numeric("50+20d10");
            obj->defense_bonus = 5;
            obj->weight = 15;
            obj->speed_bonus = 0;
            obj->special_attribute = 0;
            obj->value = 1000;
            equipment.push_back(obj);
        }
        else {
            equipment.push_back(NULL);
        }
    }
}

Player :: ~Player() {
    inventory.clear();
}
