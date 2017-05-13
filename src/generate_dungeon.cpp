#include <future>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <math.h>
#include <ncurses.h>
#include <netinet/in.h>
#include <limits.h>
#include <string>
#include <iostream>
#include <fstream>
#include <exception>
#include <map>
#include <cmath>
#include <thread>
#include <typeinfo>

#include "util.h"
#include "monster.h"
#include "object.h"
#include "monster_description_parser.h"
#include "monster_template.h"
#include "object_description_parser.h"
#include "player.h"
#include "message.h"
#include "board_element.h"

#include "priority_queue.h"

#define HEIGHT 105
#define WIDTH 160
#define NCURSES_HEIGHT 20
#define NCURSES_WIDTH 80
#define IMMUTABLE_ROCK 255
#define ROCK 200
#define ROOM 0
#define CORRIDOR 0
#define MIN_NUMBER_OF_ROOMS 25
#define MAX_NUMBER_OF_ROOMS 40
#define MIN_ROOM_WIDTH 7
#define DEFAULT_MAX_ROOM_WIDTH 20
#define MIN_ROOM_HEIGHT 5
#define DEFAULT_MAX_ROOM_HEIGHT 15
#define MIN_NUMBER_OF_MONSTERS 5
#define MAX_NUMBER_OF_MONSTERS 25
using namespace std;

static const string TYPE_ROOM = "room";
static const string TYPE_CORRIDOR = "corridor";
static const string TYPE_ROCK = "rock";
static const string TYPE_UPSTAIR = "upstair";
static const string TYPE_DOWNSTAIR = "downstair";

typedef struct {
    int tunneling_distance;
    int non_tunneling_distance;
    int hardness;
    string type;
    int x;
    int y;
    Monster *monster;
    Object * object;
} Board_Cell;

struct Room {
    int start_x;
    int end_x;
    int start_y;
    int end_y;
    bool has_explored;
};

static Board_Cell board[HEIGHT][WIDTH];
static Board_Cell player_board[HEIGHT][WIDTH];
static vector<struct Coordinate> placeable_areas;
static struct Coordinate ncurses_player_coord;
static struct Coordinate ncurses_start_coord;
static vector<struct Room> rooms;
static vector<Monster *> monsters;
static vector<MonsterTemplate> monster_templates;
static vector<Object *> objects;
static vector<ObjectTemplate> object_templates;
static PriorityQueue game_queue;
static map<string, int> color_map;
static Player * player;
static vector<Message *> all_messages;

string RLG_DIRECTORY = "";
static int IS_CONTROL_MODE = 1;
static int DO_QUIT = 0;
static int DO_SAVE = 0;
static int DO_LOAD = 0;
static int SHOW_HELP = 0;
static int MAX_ROOM_WIDTH = DEFAULT_MAX_ROOM_WIDTH;
static int MAX_ROOM_HEIGHT = DEFAULT_MAX_ROOM_HEIGHT;

void add_experience_to_player(int amount);
void display_magic_status_at_row(int row);
void add_temp_message(string message);
void handle_killed_monster(Monster * monster);
bool damage_monster_from_magic(Monster * monster, int damage);
bool damage_monster_from_melee_or_ranged(Monster * monster, int damage);
bool damage_monster(Monster * monster, int damage);
void init_color_pairs();
void generate_monsters_from_templates(int how_many);
void generate_objects_from_templates();
void print_usage();
struct Coordinate get_random_board_location();
void show_level_up_screen();
string get_level_up_screen_message();
void make_rlg_directory();
void make_monster_templates();
void make_object_templates();
void generate_new_board();
void generate_stairs();
void initialize_board();
void initialize_immutable_rock();
void load_board();
void save_board();
void place_player();
void set_placeable_areas();
void set_tunneling_distance_to_player();
void set_non_tunneling_distance_to_player();
void generate_monsters();
void print_non_tunneling_board();
void print_tunneling_board();
void add_message(string message);
void center_board_on_player();
int handle_user_input(int key);
void handle_user_input_for_look_mode(int key);
void print_board();
void print_cell(Board_Cell cell);
void dig_rooms(int number_of_rooms_to_dig);
void dig_room();
int room_is_valid(struct Room room);
void add_rooms_to_board();
void dig_cooridors();
void connect_rooms_at_indexes(int index1, int index2);
void move_player();
int get_room_index_player_is_in();
void move_monster(Monster * monster);
void print_on_clear_screen(string message);
bool cell_is_illuminated(Board_Cell cell);
bool is_in_line_of_sight(struct Coordinate coord1, struct Coordinate coord2);
void update_board_distances();
void update_distances_on_interval();
int get_number_of_explored_rooms();
void display_health_status_at(int row);
void display_stamina_status_at(int row);
void display_xp_status_at(int row);
int handle_cast_mode_input();
int cast_spell(Object * spell);
BoardElement * letPlayerSelectBoardElement();
string letPlayerSelectOption(string pre_text, vector<string> options, string post_text);


int main(int argc, char *args[]) {
    game_queue = PriorityQueue();
    player = new Player();
    struct option longopts[] = { {"save", no_argument, &DO_SAVE, 1},
        {"load", no_argument, &DO_LOAD, 1},
        {"help", no_argument, &SHOW_HELP, 'h'},
        {0, 0, 0, 0}
    };
    int c;
    while((c = getopt_long(argc, args, "h:", longopts, NULL)) != -1) {
        switch(c) {
            case 'h':
                SHOW_HELP = 1;
                break;
            default:
                break;
        }
    }
    if (SHOW_HELP) {
        print_usage();
        exit(0);
    }
    make_rlg_directory();
    make_monster_templates();
    make_object_templates();
    generate_new_board();
    initscr();
    noecho();
    start_color();
    init_color_pairs();
    center_board_on_player();
    move(ncurses_player_coord.y, ncurses_player_coord.x);
    refresh();
    thread t(update_distances_on_interval);
    t.detach();
    int game_turn = 1;
    while(monsters.size() > 0 && player->isAlive() && !DO_QUIT) {
        center_board_on_player();
        refresh();
        Node min = game_queue.extractMin();
        Character * character = min.character;
        int speed;
        if (character->is(player)) {
            string message = "It's your turn.";
            if (player->skill_points) {
                message += " Press ^ to level up.";
            }
            else if (player->isOverEncumbered()) {
                message += " You are overencumbered and move very slowly!";
            }
            add_temp_message(message);
            int success = 0;
            while (!success) {
                int ch = getch();
                success = handle_user_input(ch);
                while (!IS_CONTROL_MODE && !DO_QUIT) {
                    success = 0;
                    int ch = getch();
                    handle_user_input_for_look_mode(ch);
                    if (DO_QUIT) {
                        success = 1;
                    }
                }
            }
            if (DO_QUIT) {
                break;
            }
            int index = get_room_index_player_is_in();
            if (index != -1 && !rooms[index].has_explored) {
                rooms[index].has_explored = true;
                add_experience_to_player(player->level * 2);
            }
            center_board_on_player();
            refresh();
            if (success == 2) {
                continue;
            }
            //update_board_distances();

            /*
            if (random_int(0, 3) == 0) {
                thread t(update_board_distances);
                t.detach();
            }
            */
            /*
            update_non_tunneling_distance_in_background();
            update_tunneling_distance_in_background();
            */
            speed = player->getSpeed();
            player->regenerateStamina(game_turn);
            player->regenerateMagic(game_turn);
        }
        else {
            Monster * monster = (Monster *) character;
            if (monster == NULL) {
                continue;
            }
            add_temp_message("The monsters are moving towards you...");
            move_monster(monster);
            speed = monster->speed;
        }
        if (player->isAlive()) {
            move(ncurses_player_coord.y, ncurses_player_coord.x);
        }
        else {
            curs_set(0);
        }
        character->regenerateHealth(game_turn);
        game_turn ++;
        center_board_on_player();
        refresh();
        game_queue.insertWithPriority(character, (1000/speed) + min.priority);
    }

    if (!player->isAlive()) {
        add_message("You lost. The monsters killed you (press any key to exit)");
    }
    else if(!monsters.size()) {
        add_message("You won, killing all the monsters (press any key to exit)");
    }

    if (DO_SAVE) {
        save_board();
    }

    if (!DO_QUIT) {
        getch();
    }
    endwin();

    monsters.clear();
    objects.clear();
    monster_templates.clear();
    object_templates.clear();

    return 0;
}

void add_experience_to_player(int amount) {
    int prev_level = player->level;
    player->addExperience(amount);
    if (player->level != prev_level) {
        add_message("You leveled up!");
    }
}

int get_number_of_explored_rooms() {
    int num = 0;
    for (int i = 0; i < rooms.size(); i++) {
        if (rooms[i].has_explored) {
            num ++;
        }
    }
    return num;
}

void update_distances_on_interval() {
    while(1) {
        update_board_distances();
        usleep(1500000);
    }
}
void update_board_distances() {
    set_non_tunneling_distance_to_player();
    set_tunneling_distance_to_player();
}
void update_non_tunneling_distance_in_background() {
    thread t(set_non_tunneling_distance_to_player);
    t.detach();
}

void update_tunneling_distance_in_background() {
    thread t(set_tunneling_distance_to_player);
    t.detach();

}

bool is_in_line_of_sight(struct Coordinate start_coord, struct Coordinate end_coord) {
    /*
     *  This is Bresenham's line algorithm. It can be found here:
     *  http://www.roguebasin.com/index.php?title=Bresenham%27s_Line_Algorithm#C.2B.2B
     */
    int x1 = start_coord.x;
    int x2 = end_coord.x;
    int y1 = start_coord.y;
    int y2 = end_coord.y;
    int delta_x = (end_coord.x - start_coord.x);
    signed char const ix = (delta_x > 0) - (delta_x < 0);
    delta_x = abs(delta_x) << 1;

    int delta_y = (end_coord.y - start_coord.y);
    signed char const iy = (delta_y > 0) - (delta_y < 0);
    delta_y = abs(delta_y) << 1;

    if (delta_x >= delta_y) {
        int error = delta_y - (delta_x >> 1);
        while(x1 != x2) {
            if (error >= 0 && (error || (ix > 0))) {
                error -= delta_x;
                y1 += iy;
            }

            error += delta_y;
            x1 += ix;
            if (board[y1][x1].hardness > 0) {
                return false;
            }
        }
    }
    else {
        int error = delta_x - (delta_y >> 1);
        while(y1 != y2) {
            if (error >= 0 && (error || (iy > 0))) {
                error -= delta_y;
                x1 += ix;
            }

            error += delta_x;
            y1 += iy;
            if (board[y1][x1].hardness > 0) {
                return false;
            }
        }
    }
    return true;
}

bool damage_monster_from_magic(Monster * monster, int damage) {
    player->reduceMagicFromSpell(damage);
    return damage_monster(monster, damage);
}

bool damage_monster_from_melee_or_ranged(Monster * monster, int damage) {
    player->reduceStaminaFromDamage(damage);
    return damage_monster(monster, damage);
}

bool damage_monster(Monster * monster, int damage) {
    if (!monster->hitWillConnect()) {
        add_message("You fail to hit the monster!");
        return true;
    }
    monster->damage(damage);
    add_experience_to_player(ceil(damage * 0.1));
    int remaining = monster->hitpoints;
    add_message("You deal " + to_string(damage) + " points of damage to the monster (" + to_string(max(remaining, 0)) + " pts remain)");
    bool isAlive = monster->isAlive();
    if (!monster->isAlive()) {
        add_message("You killed a monster!");
        handle_killed_monster(monster);
    }
    return isAlive;

}

void init_color_pairs() {
    color_map["RED"] = COLOR_RED;
    color_map["GREEN"] = COLOR_GREEN;
    color_map["BLUE"] = COLOR_BLUE;
    color_map["CYAN"] = COLOR_CYAN;
    color_map["YELLOW"] = COLOR_YELLOW;
    color_map["MAGENTA"] = COLOR_MAGENTA;
    color_map["WHITE"] = COLOR_WHITE;
    color_map["BLACK"] = COLOR_BLACK;

    map<string, int>::iterator it;
    for (it = color_map.begin(); it != color_map.end(); it++) {
        int color_key = it->second;
        init_pair(color_key, color_key, COLOR_BLACK);
    }
}

void generate_monsters_from_templates(int how_many) {
    for (int i = 0; i < monsters.size(); i++) {
        if (monsters[i]) {
            delete(monsters[i]);
        }
    }
    monsters.clear();
    while (monsters.size() < (size_t) how_many) {
        int i = random_int(0, monster_templates.size() - 1);
        struct Coordinate coordinate;
        MonsterTemplate monster_template = monster_templates[i];
        Monster * monster = monster_template.makeMonster();
        while (true) {
            coordinate = get_random_board_location();
            Board_Cell cell = board[coordinate.y][coordinate.x];
            if (cell.monster || player->x == coordinate.x || player->y == coordinate.y) {
                continue;
            }
            else {
                break;
            }
        }
        monster->x = coordinate.x;
        monster->y = coordinate.y;
        board[monster->y][monster->x].monster = monster;
        monsters.push_back(monster);
        game_queue.insertWithPriority(monster, monsters.size());
    }

}

void generate_objects_from_templates() {
    for (int i = 0; i < objects.size(); i++) {
        Object * obj = objects[i];
        if (obj && !player->hasObject(obj)) {
            delete(objects[i]);
        }
    }
    objects.clear();
    int number_of_objects = random_int(20, 40);
    while(objects.size() < (size_t) number_of_objects) {
        int i = random_int(0, object_templates.size() - 1);
        struct Coordinate coordinate;
        ObjectTemplate object_template = object_templates[i];
        Object * object = object_template.makeObject();
        if (object->name.compare("Healing") == 0 || object->name.compare("Fireball") == 0 || object->name.compare("Teleport") == 0) {
            objects.push_back(object);
            player->addObjectToInventory(object);
            continue;
        }
        while(true) {
            coordinate = get_random_board_location();
            Board_Cell cell = board[coordinate.y][coordinate.x];
            if (!cell.object) {
                break;
            }
        }
        object->x = coordinate.x;
        object->y = coordinate.y;
        board[object->y][object->x].object = object;
        objects.push_back(object);
    }
}

void make_monster_templates() {
    string filename = RLG_DIRECTORY + "monster_desc.txt";
    MonsterDescriptionParser p = MonsterDescriptionParser(filename);
    try {
        p.parseFile();
    }
    catch(const char * e) {
        cout << "Error reading monster file: " << e << "\nExiting program" <<endl;
        exit(1);
    }
    monster_templates = p.getMonsterTemplates();
}

void make_object_templates() {
    string filename = RLG_DIRECTORY + "object_desc.txt";
    ObjectDescriptionParser p = ObjectDescriptionParser(filename);
    try{
        p.parseFile();
    }
    catch(const char * e) {
        cout << "Error reading object file: " << e << "\nExiting program" << endl;
        exit(2);
    }
    object_templates = p.getObjectTemplates();
}

void generate_new_board() {
    initialize_board();
    rooms.clear();
    if (DO_LOAD) {
        load_board();
        DO_LOAD = 0;
    }
    else {
        int num_rooms = random_int(MIN_NUMBER_OF_ROOMS, MAX_NUMBER_OF_ROOMS);
        dig_rooms(num_rooms);
        dig_cooridors();
    }

    game_queue.clear();
    place_player();
    set_placeable_areas();
    set_non_tunneling_distance_to_player();
    set_tunneling_distance_to_player();
    int num_monsters = random_int(MIN_NUMBER_OF_MONSTERS, MAX_NUMBER_OF_MONSTERS);
    generate_monsters_from_templates(num_monsters);
    generate_stairs();
    generate_objects_from_templates();
    int index = get_room_index_player_is_in();
    if (index != -1) {
        rooms[index].has_explored = true;
    }
}

struct Coordinate get_random_unoccupied_location_in_room(struct Room room) {
    struct Coordinate coord;
    coord.x = random_int(room.start_x, room.end_x);
    coord.y = random_int(room.start_y, room.end_y);
    return coord;
}

void generate_stairs() {
    int number_of_stairs_up = rooms.size() / 2;
    for (int i = 0; i < number_of_stairs_up; i++) {
        struct Room room = rooms[i];
        struct Coordinate coord = get_random_unoccupied_location_in_room(room);
        board[coord.y][coord.x].type = TYPE_UPSTAIR;
    }
    for (int i = number_of_stairs_up; i < rooms.size(); i++) {
        struct Room room = rooms[i];
        struct Coordinate coord = get_random_unoccupied_location_in_room(room);
        board[coord.y][coord.x].type = TYPE_DOWNSTAIR;
    }
}

void make_rlg_directory() {
    char * home = getenv("HOME");
    char dir[] = "/.rlg327/";
    RLG_DIRECTORY.append(home);
    RLG_DIRECTORY.append(dir);
    mkdir(RLG_DIRECTORY.c_str(), 0777);
}

void save_board() {
    string filename = "dungeon";
    string filepath = RLG_DIRECTORY + filename;
    cout << "Saving file to: " << filepath << endl;
    FILE * fp = fopen(filepath.c_str(), "wb+");
    if (fp == NULL) {
        cout << "Cannot save file\n";
        return;
    }
    string file_marker = "RLG327-S2017";
    uint32_t version = htonl(0);
    uint32_t file_size = htonl(16820 + (rooms.size() * 4));

    fwrite(file_marker.c_str(), 1, file_marker.length(), fp);
    fwrite(&version, 1, 4, fp);
    fwrite(&file_size, 1, 4, fp);
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            uint8_t num = board[y][x].hardness;
            fwrite(&num, 1, 1, fp);
        }
    }

    for (size_t i = 0; i < rooms.size(); i++) {
        struct Room room = rooms[i];
        uint8_t height = room.end_y - room.start_y + 1;
        uint8_t width = room.end_x - room.start_x + 1;
        fwrite(&room.start_x, 1, 1, fp);
        fwrite(&room.start_y, 1, 1, fp);
        fwrite(&(width), 1, 1, fp);
        fwrite(&(height), 1, 1, fp);
    }
    fclose(fp);
}

void load_board() {
    string filename = "dungeon";
    string filepath = RLG_DIRECTORY + filename;
    cout << "Loading dungeon: " << filepath << endl;
    FILE *fp = fopen(filepath.c_str(), "r");
    if (fp == NULL) {
        cout << "Cannot load " << filepath << endl;
        exit(1);
    }
    char title[13]; // one extra index for the null value at the end
    uint32_t version;
    uint32_t file_size;


    // Get title
    fread(title, 1, 12, fp);

    // Get version
    fread(&version, 4, 1, fp);
    version = ntohl(version);

    // Get file size
    fread(&file_size, 4, 1, fp);
    file_size = ntohl(file_size);

    printf("File Marker: %s :: Version: %d :: File Size: %d bytes\n", title, version, file_size);

    uint8_t num;
    int x = 0;
    int y = 0;
    for (int i = 0; i < 16800; i++) {
        fread(&num, 1, 1, fp);
        Board_Cell cell;
        cell.hardness = num;
        if (num == 0) {
            cell.type = TYPE_CORRIDOR;
        }
        else {
            cell.type = TYPE_ROCK;
        }
        cell.x = x;
        cell.y = y;
        board[y][x] = cell;
        board[y][x].monster = NULL;
        if (x == WIDTH - 1) {
            x = 0;
            y ++;
        }
        else {
            x ++;
        }
    }

    uint8_t start_x;
    uint8_t start_y;
    uint8_t width;
    uint8_t height;
    // NUMBER_OF_ROOMS = (file_size - ftell(fp)) / 4;
    int counter = 0;
    while(ftell(fp) != file_size) {
        fread(&start_x, 1, 1, fp);
        fread(&start_y, 1, 1, fp);
        fread(&width, 1, 1, fp);
        fread(&height, 1, 1, fp);

        struct Room room;
        room.start_x = start_x;
        room.start_y = start_y;
        room.end_x = start_x + width - 1;
        room.end_y = start_y + height - 1;
        rooms.push_back(room);
        counter ++;
    }
    add_rooms_to_board();
    fclose(fp);
}

void print_usage() {
    printf("usage: generate_dungeon [--save] [--load] [--rooms=<number of rooms>] [--player_x=<player x position>] [--player_y=<player y position>] [--nummon=<number of monsters>]\n");
}

void initialize_board() {
    Board_Cell cell;
    cell.type = TYPE_ROCK;
    cell.monster = NULL;
    cell.object = NULL;
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            cell.x = x;
            cell.y = y;
            cell.hardness = random_int(1, 254);
            board[y][x] = cell;
            cell.hardness = IMMUTABLE_ROCK;
            player_board[y][x] = cell;
        }
    }
    initialize_immutable_rock();
}

void initialize_immutable_rock() {
    int y;
    int x;
    int max_x = WIDTH - 1;
    int max_y = HEIGHT - 1;
    Board_Cell cell;
    cell.type = TYPE_ROCK;
    cell.hardness = IMMUTABLE_ROCK;
    cell.monster = NULL;
    cell.object = NULL;
    for (y = 0; y < HEIGHT; y++) {
        cell.y = y;
        cell.x = 0;
        board[y][0] = cell;
        cell.x = max_x;
        board[y][max_x] = cell;
    }
    for (x = 0; x < WIDTH; x++) {
        cell.y = 0;
        cell.x = x;
        board[0][x] = cell;
        cell.y = max_y;
        board[max_y][x] = cell;
    }
}

void place_player() {
    if (!player->x && !player->y) {
        struct Room room = rooms[0];
        int x = random_int(room.start_x, room.end_x);
        int y = random_int(room.start_y, room.end_y);
        player->x = x;
        player->y = y;
    }
    struct Coordinate coord;
    coord.x = player->x;
    coord.y = player->y;
    game_queue.insertWithPriority(player, 0);
}

void set_placeable_areas() {
    placeable_areas.clear();
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            Board_Cell cell = board[y][x];
            if (cell.hardness == 0 && (cell.x != player->x || cell.y != player->y)) {
                struct Coordinate coord;
                coord.x = cell.x;
                coord.y = cell.y;
                placeable_areas.push_back(coord);
            }
        }
    }
}

int get_cell_weight(Board_Cell cell) {
    if (cell.hardness == 0) {
        return 1;
    }
    if (cell.hardness <= 84) {
        return 1;
    }
    if (cell.hardness <= 170) {
        return 2;
    }
    if (cell.hardness <= 254) {
        return 3;
    }
    return 1000;
}

vector<Board_Cell> get_tunneling_neighbors(struct Coordinate coord) {
    vector<Board_Cell> neighbors;
    int can_go_right = coord.x < WIDTH -1;
    int can_go_up = coord.y > 0;
    int can_go_left = coord.x > 0;
    int can_go_down = coord.y < HEIGHT -1;

    if (can_go_right) {
        Board_Cell right = board[coord.y][coord.x + 1];
        if (right.hardness < IMMUTABLE_ROCK) {
            neighbors.push_back(right);
        }
        if (can_go_up) {
            Board_Cell top_right = board[coord.y - 1][coord.x + 1];
            if (top_right.hardness < IMMUTABLE_ROCK) {
                neighbors.push_back(top_right);
            }
        }
        if (can_go_down) {
            Board_Cell bottom_right = board[coord.y + 1][coord.x + 1];
            if (bottom_right.hardness < IMMUTABLE_ROCK) {
                neighbors.push_back(bottom_right);
            }
        }
    }
    if (can_go_left) {
        Board_Cell left = board[coord.y][coord.x - 1];
        if (left.hardness < IMMUTABLE_ROCK) {
            neighbors.push_back(left);
        }
        if (can_go_up) {
            Board_Cell top_left = board[coord.y - 1][coord.x - 1];
            if (top_left.hardness < IMMUTABLE_ROCK) {
                neighbors.push_back(top_left);
            }
        }
        if (can_go_down) {
            Board_Cell bottom_left = board[coord.y + 1][coord.x - 1];
            if (bottom_left.hardness < IMMUTABLE_ROCK) {
                neighbors.push_back(bottom_left);
            }
        }
    }

    if (can_go_up) {
        Board_Cell above = board[coord.y - 1][coord.x];
        if (above.hardness < IMMUTABLE_ROCK) {
            neighbors.push_back(above);
        }
    }
    if (can_go_down) {
        Board_Cell below = board[coord.y + 1][coord.x];
        if (below.hardness < IMMUTABLE_ROCK) {
            neighbors.push_back(below);
        }
    }

    return neighbors;
}


void set_tunneling_distance_to_player() {
    PriorityQueue tunneling_queue = PriorityQueue();
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            struct Coordinate coord;
            coord.x = x;
            coord.y = y;
            if (y == player->y && x == player->x) {
                board[y][x].tunneling_distance = 0;
            }
            else {
                board[y][x].tunneling_distance = INT_MAX;
            }
            if (board[y][x].hardness < IMMUTABLE_ROCK) {
                tunneling_queue.insertCoordWithPriority(coord, board[y][x].tunneling_distance);
            }
        }
    }
    int count = 0;
    while(tunneling_queue.size()) {
        Node min = tunneling_queue.extractMin();
        Board_Cell min_cell = board[min.coord.y][min.coord.x];
        vector<Board_Cell> neighbors = get_tunneling_neighbors(min.coord);
        int min_dist = min_cell.tunneling_distance + get_cell_weight(min_cell);
        for (size_t i = 0; i < neighbors.size(); i++) {
            Board_Cell neighbor_cell = neighbors[i];
            Board_Cell cell = board[neighbor_cell.y][neighbor_cell.x];
            if (min_dist < cell.tunneling_distance) {
                struct Coordinate coord;
                coord.x = cell.x;
                coord.y = cell.y;
                board[cell.y][cell.x].tunneling_distance = min_dist;
                tunneling_queue.decreaseCoordPriority(coord, min_dist);
            }
        }
        count ++;
        neighbors.clear();
    }
};

vector<Board_Cell> get_non_tunneling_neighbors(struct Coordinate coord) {
    vector<Board_Cell> neighbors;
    int can_go_right = coord.x < WIDTH -1;
    int can_go_up = coord.y > 0;
    int can_go_left = coord.x > 0;
    int can_go_down = coord.y < HEIGHT -1;
    if (can_go_right) {
        Board_Cell right = board[coord.y][coord.x + 1];
        if (right.hardness < 1) {
            neighbors.push_back(right);
        }
        if (can_go_up) {
            Board_Cell top_right = board[coord.y - 1][coord.x + 1];
            if (top_right.hardness < 1) {
                neighbors.push_back(top_right);
            }
        }
        if (can_go_down) {
            Board_Cell bottom_right = board[coord.y + 1][coord.x + 1];
            if (bottom_right.hardness < 1) {
                neighbors.push_back(bottom_right);
            }
        }
    }

    if (can_go_left) {
        Board_Cell left = board[coord.y][coord.x - 1];
        if (left.hardness < 1) {
            neighbors.push_back(left);
        }
        if (can_go_up) {
            Board_Cell top_left = board[coord.y - 1][coord.x - 1];
            if (top_left.hardness < 1) {
                neighbors.push_back(top_left);
            }
        }
        if (can_go_down) {
            Board_Cell bottom_left = board[coord.y + 1][coord.x - 1];
            if (bottom_left.hardness < 1) {
                neighbors.push_back(bottom_left);
            }
        }
    }

    if (can_go_up) {
        Board_Cell above = board[coord.y - 1][coord.x];
        if (above.hardness < 1) {
            neighbors.push_back(above);
        }
    }
    if (can_go_down) {
        Board_Cell below = board[coord.y + 1][coord.x];
        if (below.hardness < 1) {
            neighbors.push_back(below);
        }
    }
    return neighbors;
}

void set_non_tunneling_distance_to_player() {
    PriorityQueue non_tunneling_queue = PriorityQueue();
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            struct Coordinate coord;
            coord.x = x;
            coord.y = y;
            if (y == player->y && x == player->x) {
                board[y][x].non_tunneling_distance = 0;
            }
            else {
                board[y][x].non_tunneling_distance = INT_MAX;
            }
            if (board[y][x].hardness < 1) {
                non_tunneling_queue.insertCoordWithPriority(coord, board[y][x].non_tunneling_distance);
            }
        }
    }
    while(non_tunneling_queue.size()) {
        Node min = non_tunneling_queue.extractMin();
        Board_Cell min_cell = board[min.coord.y][min.coord.x];
        vector<Board_Cell> neighbors = get_non_tunneling_neighbors(min.coord);
        int min_dist = min_cell.non_tunneling_distance + 1;
        for (size_t i = 0; i < neighbors.size(); i++) {
            Board_Cell neighbor_cell = neighbors[i];
            Board_Cell cell = board[neighbor_cell.y][neighbor_cell.x];
            if (min_dist < cell.non_tunneling_distance) {
                struct Coordinate coord;
                coord.x = cell.x;
                coord.y = cell.y;
                board[cell.y][cell.x].non_tunneling_distance = min_dist;
                non_tunneling_queue.decreaseCoordPriority(coord, min_dist);
            }
        }
        neighbors.clear();
    }
}

struct Coordinate get_random_board_location() {
    int index = random_int(0, placeable_areas.size() - 1);
    return placeable_areas[index];
}

void print_non_tunneling_board() {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
           Board_Cell cell = board[y][x];
           if(cell.x == player->x && cell.y == player->y) {
               printf("@");
           }
           else {
               if (cell.type.compare(TYPE_ROCK) != 0) {
                   printf("%d", cell.non_tunneling_distance % 10);
               }
               else {
                    printf(" ");
               }
           }
        }
        printf("\n");
    }
}
void print_tunneling_board() {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
           Board_Cell cell = board[y][x];
           if(cell.x == player->x && cell.y == player->y) {
               printf("@");
           }
           else {
               if (cell.hardness == IMMUTABLE_ROCK) {
                   printf(" ");
               }
               else {
                   printf("%d", cell.tunneling_distance % 10);
               }
           }
        }
        printf("\n");
    }

}

void add_message(string message) {
    add_temp_message(message);
    Message * my_message = new Message(message);
    if (all_messages.size() == 0 || all_messages[0]->message.compare(message)) {
        all_messages.insert(all_messages.begin(), my_message);
    }
}

void add_temp_message(string message) {
    move(0,0);
    clrtoeol();
    mvprintw(0, 0, "%s", message.c_str());
    move(ncurses_player_coord.y, ncurses_player_coord.x);
    refresh();
}

void print_on_clear_screen(string message) {
    curs_set(0);
    clear();
    add_temp_message(message);
    getch();
    clear();
    center_board_on_player();
    add_temp_message("It's your turn");
}

void update_player_board() {
    struct Coordinate p_coord;
    p_coord.x = player->x;
    p_coord.y = player->y;

    int light_radius = player->getLightRadius();
    for (int y = player->y - light_radius; y <= player->y + light_radius; y++) {
        for (int x = player->x - light_radius; x <= player->x + light_radius; x++) {
            struct Coordinate cell_coord;
            cell_coord.y = y;
            cell_coord.x = x;
            if (!is_in_line_of_sight(p_coord, cell_coord)) {
                continue;
            }
            player_board[y][x] = board[y][x];
        }
    }
}

bool cell_is_illuminated(Board_Cell cell) {
    struct Coordinate p_coord;
    p_coord.x = player->x;
    p_coord.y = player->y;
    struct Coordinate cell_coord;
    cell_coord.x = cell.x;
    cell_coord.y = cell.y;
    if (!is_in_line_of_sight(p_coord, cell_coord)) {
        return false;
    }
    int light_radius = player->getLightRadius();
    int min_y = player->y - light_radius;
    int max_y = player->y + light_radius;
    int min_x = player->x - light_radius;
    int max_x = player->x + light_radius;
    return (
        min_x <= cell.x && cell.x <= max_x
        &&
        min_y <= cell.y && cell.y <= max_y
    );
}


void update_board_view(int ncurses_start_x, int ncurses_start_y) {
    update_player_board();
    ncurses_start_x = min(ncurses_start_x + NCURSES_WIDTH, WIDTH - 1);
    ncurses_start_y = min(ncurses_start_y + NCURSES_HEIGHT, HEIGHT - 1);
    ncurses_start_x = max(ncurses_start_x - NCURSES_WIDTH, 0);
    ncurses_start_y = max(ncurses_start_y - NCURSES_HEIGHT, 0);
    ncurses_start_coord.x = ncurses_start_x;
    ncurses_start_coord.y = ncurses_start_y;
    int row = 1;
    for (int y = ncurses_start_y; y <= ncurses_start_y + NCURSES_HEIGHT; y++) {
        int col = 0;
        for (int x = ncurses_start_x; x <= ncurses_start_x + NCURSES_WIDTH; x++) {
            Board_Cell cell = player_board[y][x];
            bool is_illuminated = cell_is_illuminated(cell);
            if (is_illuminated) {
                attron(A_BOLD);
            }
            if (player->isAlive() && y == player->y && x == player->x) {
                mvprintw(row, col, "@");
                ncurses_player_coord.x = col;
                ncurses_player_coord.y = row;
            }
            else if (player_board[y][x].monster) {
                Monster *monster = player_board[y][x].monster;
                int color_key = color_map[monster->color];
                attron(COLOR_PAIR(color_key));
                mvprintw(row, col, "%c", monster->symbol);
                attroff(COLOR_PAIR(color_key));
            }
            else if(player_board[y][x].object) {
                Object * object = player_board[y][x].object;
                int color_key = color_map[object->color];
                attron(COLOR_PAIR(color_key));
                mvprintw(row, col,"%c", object->getSymbol());
                attroff(COLOR_PAIR(color_key));
            }
            else {
                Board_Cell cell = player_board[y][x];
                if (cell.type.compare(TYPE_UPSTAIR) == 0) {
                    mvprintw(row, col, "<");
                }
                else if (cell.type.compare(TYPE_DOWNSTAIR) == 0) {
                    mvprintw(row, col, ">");
                }
                else if (cell.type.compare(TYPE_ROCK) == 0) {
                    mvprintw(row, col, " ");
                }
                else if (cell.type.compare(TYPE_ROOM) == 0) {
                    mvprintw(row, col, ".");
                }
                else if (cell.type.compare(TYPE_CORRIDOR) == 0) {
                    mvprintw(row, col, "#");
                }
                else {
                    mvprintw(row, col, "F");
                }
            }
            if (is_illuminated) {
                attroff(A_BOLD);
            }
            col ++;
        }
        row ++;
    }

    display_health_status_at(row);
    row ++;
    row ++;
    row ++;

    // Display Stamina
    display_stamina_status_at(row);
    row++;
    row++;
    row++;

    // Display magic
    display_magic_status_at_row(row);
    row ++;
    row++;
    row++;

    // Display xp
    display_xp_status_at(row);
    row++;
    row++;
    row++;

    move(row, 0);
    clrtoeol();
    mvprintw(row, 0, ("Monsters remaining: " + to_string(monsters.size())).c_str());
    row++;
    move(row, 0);
    clrtoeol();
    mvprintw(row, 0, ("Rooms explored: " + to_string(get_number_of_explored_rooms()) + "/" + to_string(rooms.size())).c_str());
    row++;
}

void display_health_status_at(int row) {
    mvprintw(row, 0, "Health:");
    row++;
    int red = color_map["RED"];
    move(row, 0);
    clrtoeol();
    attron(COLOR_PAIR(red));
    mvprintw(row, 0, (player->getStatusProgressBar(player->hitpoints/(1.0*player->max_hitpoints)).c_str()));
    attroff(COLOR_PAIR(red));
}

void display_stamina_status_at(int row) {
    mvprintw(row, 0, "Stamina:");
    row++;
    move(row, 0);
    clrtoeol();
    int green = color_map["GREEN"];
    attron(COLOR_PAIR(green));
    mvprintw(row, 0, (player->getStatusProgressBar(player->stamina_points/(1.0*player->max_stamina_points))).c_str());
    attroff(COLOR_PAIR(green));
}

void display_magic_status_at_row(int row) {
    mvprintw(row, 0, "Magic:");
    row++;
    move(row, 0);
    clrtoeol();
    int blue = color_map["BLUE"];
    attron(COLOR_PAIR(blue));
    mvprintw(row, 0, (player->getStatusProgressBar(player->magic/(1.0*player->max_magic))).c_str());
    attroff(COLOR_PAIR(blue));
}

void display_xp_status_at(int row) {
    mvprintw(row, 0, "XP:");
    row++;
    move(row, 0);
    clrtoeol();
    int yellow = color_map["YELLOW"];
    attron(COLOR_PAIR(yellow));
    float percentage = player->experience / (1.0*player->getExperienceRequiredForNextLevel());
    if (player->skill_points) {
        percentage = 1;
    }
    mvprintw(row, 0, (player->getStatusProgressBar(percentage)).c_str());
    attroff(COLOR_PAIR(yellow));
}

void handle_user_input_for_look_mode(int key) {
    int new_x = ncurses_start_coord.x;
    int new_y = ncurses_start_coord.y;
    if(key == 107 || key == 8) { // k - one page up
        new_y -= NCURSES_HEIGHT;
    }
    else if (key == 106 || key == 2) { // j - one page down
        new_y += NCURSES_HEIGHT;
    }
    else if (key == 104 || key == 4) { // h - one page left
        new_x -= NCURSES_WIDTH;
    }
    else if(key == 108 || key == 6) { // l - one page right
        new_x += NCURSES_WIDTH;
    }
    else if (key == 27) { // escape - enter control mode
        IS_CONTROL_MODE = 1;
        center_board_on_player();
        add_temp_message("It's your turn");
        return;
    }
    else if (key == 81) { // Q - quit
        DO_QUIT = 1;
    }
    update_board_view(new_x, new_y);
    refresh();
}

void handle_killed_monster(Monster * monster) {
    add_experience_to_player(monster->experience);
    game_queue.removeFromQueue(monster);
    board[monster->y][monster->x].monster = NULL;
    int index = -1;
    for (size_t i = 0; i < monsters.size(); i++) {
        if (!monsters[i]->id.compare(monster->id)) {
            index = i;
            break;
        }
    }
    if (index >= 0) {
        monsters.erase(monsters.begin() + index);
    }
    if (monster) {
        delete(monster);
    }

}

BoardElement * letPlayerSelectBoardElement() {
    struct Coordinate new_coord = ncurses_player_coord;
    int local_x = player->x;
    int local_y = player->y;

    while (true) {
        int key = getch();
        if (key == 107) { // k - one cell up
            if (board[local_y - 1][local_x].hardness > 0 || !cell_is_illuminated(board[local_y - 1][local_x])) {
                continue;
            }
            new_coord.y --;
            local_y --;
        }
        else if (key == 106) { // j - one cell down
            if (board[local_y + 1][local_x].hardness > 0 || !cell_is_illuminated(board[local_y + 1][local_x])) {
                continue;
            }
            new_coord.y ++;
            local_y ++;
        }
        else if (key == 104) { // h - one cell left
            if (board[local_y][local_x - 1].hardness > 0 || !cell_is_illuminated(board[local_y][local_x - 1])) {
                continue;
            }
            new_coord.x --;
            local_x --;
        }
        else if(key == 108) { // l - one cell right
            if (board[local_y][local_x + 1].hardness > 0 || !cell_is_illuminated(board[local_y][local_x + 1])) {
                continue;
            }
            new_coord.x ++;
            local_x ++;
        }
        else if (key == 121) { // y - one cell up-left
            if (board[local_y - 1][local_x - 1].hardness > 0 || !cell_is_illuminated(board[local_y - 1][local_x - 1])) {
                continue;
            }
            new_coord.x --;
            new_coord.y --;
            local_y --;
            local_x --;
        }
        else if (key == 117) { // u - one cell up-right
            if (board[local_y - 1][local_x + 1].hardness > 0 || !cell_is_illuminated(board[local_y - 1][local_x + 1])) {
                continue;
            }
            new_coord.x ++;
            new_coord.y --;
            local_y --;
            local_x ++;
        }
        else if (key == 110) { // n - one cell low-right
            if (board[local_y + 1][local_x + 1].hardness > 0 || !cell_is_illuminated(board[local_y + 1][local_x + 1])) {
                continue;
            }
            new_coord.x ++;
            new_coord.y ++;
            local_x ++;
            local_y ++;
        }
        else if (key == 98) { // b - one cell low-left
            if (board[local_y + 1][local_x - 1].hardness > 0 || !cell_is_illuminated(board[local_y + 1][local_x - 1])) {
                continue;
            }
            new_coord.x --;
            new_coord.y ++;
            local_x --;
            local_y ++;
        }
        else if (key == 27 || key == 81) { // escape - exit
            return NULL;
        }
        else if (key == 13 || key == 10) { // carriage return
            if (board[local_y][local_x].monster) {
                return board[local_y][local_x].monster;
            }
            else if(board[local_y][local_x].object) {
                return board[local_y][local_x].object;
            }
        }
        move(new_coord.y, new_coord.x);
    }
}

int cast_spell(Object * spell) {
    if (spell->name.compare("Fireball") == 0) {
        add_temp_message("Select cell to cast spell");
        BoardElement * element = letPlayerSelectBoardElement();
        if (!element) {
            add_temp_message("Exited cast mode. It's still your turn");
            return 0;
        }
        int damage = player->getDamageForSpell(spell);
        if (!player->hasEnoughMagicForSpell(damage)) {
            add_temp_message("You do not have enough magic for that spell!");
            return 0;
        }
        Monster * m = dynamic_cast<Monster *> (element);
        if (!m) {
            return cast_spell(spell);
        }
        damage_monster_from_magic((Monster *) element, damage);
        return 1;
    }
    else if(spell->name.compare("Healing") == 0) {
        int healing_amount = spell->defense_bonus;
        if (!player->hasEnoughMagicForSpell(healing_amount)) {
           add_message("You do not have enough magic for that spell!");
           return 0;
        }
        add_message("You restore some of your health");
        player->restoreHealth(healing_amount);
        player->reduceMagicFromSpell(healing_amount);
        add_experience_to_player(healing_amount * 0.1);
        return 1;
    }
    else if (spell->name.compare("Teleport") == 0) {
        int teleport_cost = 50;
        if (!player->hasEnoughMagicForSpell(teleport_cost)) {
            add_message("You do not have enough magic for that spell!");
            return 0;
        }
        add_message("You teleport to a room");
        int index = get_room_index_player_is_in();
        int new_room_index = -1;
        for (int i = 0; i < rooms.size(); i++) {
            struct Room room = rooms[i];
            if (room.has_explored && index != i) {
                new_room_index = i;
                break;
            }
        }
        if (new_room_index == -1) {
            add_message("There is no available room to teleport to");
            return 0;
        }
        struct Room new_room = rooms[new_room_index];
        int x = random_int(new_room.start_x, new_room.end_x);
        int y = random_int(new_room.start_y, new_room.end_y);
        while (board[y][x].monster || board[y][x].object) {
            x = random_int(new_room.start_x, new_room.end_x);
            y = random_int(new_room.start_y, new_room.end_y);
        }
        player->x = x;
        player->y = y;
        player->reduceMagicFromSpell(teleport_cost);
        add_experience_to_player(teleport_cost * 0.1);

        center_board_on_player();
    }
    return 0;
}

int handle_cast_mode_input() {
    string pre_message = "Select spell to cast\n\n";
    vector<string> options;
    for (int i = 0; i < player->spells.size(); i++) {
        options.push_back(player->spells[i]->name);
    }
    string post_message = "\n\n(Press esc to exit)";
    string selection = letPlayerSelectOption(pre_message, options, post_message);
    center_board_on_player();
    if (selection.empty()) {
        return 0;
    }
    for (int i = 0; i < player->spells.size(); i++) {
        if (player->spells[i]->name.compare(selection) == 0) {
            return cast_spell(player->spells[i]);
        }
    }
    add_message("Major borkage in handle_cast_mode_input");
    return 0;
}

int handle_ranged_mode_input() {
    BoardElement * element = letPlayerSelectBoardElement();
    if (!element) {
        add_temp_message("Exited ranged mode. It's still your turn");
        return 0;
    }
    Monster * m = dynamic_cast<Monster *> (element);
    if (!m) {
        return handle_ranged_mode_input();
    }
    int damage = player->getRangedAttackDamage();
    if (!player->hasEnoughStaminaForAttack(damage)) {
        add_message("You do not have enough stamina for this attack!");
        return 0;
    }
    damage_monster_from_melee_or_ranged(m, damage);
    return 1;
}

string buildSelectionMessage(string pre_text, vector<string> options, string post_text) {
    string str = pre_text;
    for (int i = 0; i < options.size(); i++) {
        str += options[i] + "\n";
    }
    return str + post_text;
}

map<int, string> getKeyToWordMapForSelectionBody(string body, vector<string> options) {
    map<int, string> key_to_word_map;
    vector<string> lines = split(body, "\n");
    for (int i = 0; i < lines.size(); i++) {
       string line = lines[i];
       if (find(options.begin(), options.end(), line) != options.end()) {
            key_to_word_map[i] = line;
       }
    }
    return key_to_word_map;
}

string letPlayerSelectOption(string pre_text, vector<string> options, string post_text) {
    clear();
    string msg = buildSelectionMessage(pre_text, options, post_text);
    add_temp_message(msg);
    map<int, string> key_to_word_map = getKeyToWordMapForSelectionBody(msg, options);
    vector<int> keys = getKeysFromMap(key_to_word_map);
    sort(keys.begin(), keys.end());
    int current_y = keys[0];
    int prev_y = keys[0];
    while (true) {
        mvprintw(prev_y, 0, key_to_word_map[prev_y].c_str());
        attron(A_REVERSE);
        mvprintw(current_y, 0, key_to_word_map[current_y].c_str());
        attroff(A_REVERSE);

        prev_y = current_y;

        int input = getch();
        if (input == 27) { // Escape
            break;
        }
        else if (input == 106) {  // k - up
           current_y ++;
        }
        else if (input == 107) { // j - down
            current_y --;
        }
        else if (input == 13 || input == 10) { // enter
            return key_to_word_map[current_y];
        }
        current_y = max(current_y, keys[0]);
        current_y = min(current_y, keys[keys.size() - 1]);
    }
    return "";
}

void show_level_up_screen() {
    string additional_message = "";
    while(player->skill_points) {
        string pre_message = "LEVEL UP\n";
        pre_message += "Strength: " + to_string(player->strength_level) + "\tDexterity: ";
        pre_message += to_string(player->dexterity_level) + "\tIntelligence: ";
        pre_message += to_string(player->intelligence_level);
        pre_message += "\n\nSelect skill to level up:\n";
        vector<string> options;
        options.push_back("Strength");
        options.push_back("Dexterity");
        options.push_back("Intelligence");
        string post_message = "\n\nSkill Points: " + to_string(player->skill_points);
        if (!additional_message.empty()) {
            post_message += "\n\n" + additional_message;
        }
        post_message += "\n\n(Press esc to exit)";
        additional_message = "";

        string selection = letPlayerSelectOption(pre_message, options, post_message);
        if (selection.empty()) {
            break;
        }
        try {
            player->levelUpSkill(selection);
        }
        catch(exception &e) {
            clear();
            additional_message = e.what();
            continue;
        }
        clear();
        additional_message = selection + " increased!";
    }
    center_board_on_player();
    add_temp_message("It's your turn");
}

int handle_user_input(int key) {
    struct Coordinate new_coord;
    new_coord.x = player->x;
    new_coord.y = player->y;
    string str = "";
    if (key == 73) { // I - examine inventory
        string title = "Which inventory index? ";
        add_temp_message(title);
        move(0, title.length());
        char arr[80];
        echo();
        getstr(arr);
        string str(arr);
        noecho();
        int index;
        try {
            index = stoi(str);
        }
        catch(...) {
            add_temp_message("Invalid input. It's your turn");
            return 0;
        }
        if (index > player->getNumberOfItemsInInventory() - 1 || index < 0) {
            add_temp_message("No inventory item at that index. It's your turn");
            return 0;
        }
        string message = "Inventory item " + to_string(index) + ":\n";
        message += player->viewInventoryObjectAt(index) + "\n\n";
        message += "(Press any key to return to game view)";
        print_on_clear_screen(message);
        return 0;
    }
    else if (key == 63) { // ? - help
        string message = "HELP\n\n";
        message += "MOVEMENT\n";
        message += "h - one cell left\n";
        message += "j - one cell down\n";
        message += "k - one cell up\n";
        message += "l - one cell left\n";
        message += "y - one cell top-left\n";
        message += "u - one cell top-right\n";
        message += "b - one cell bottom-left\n";
        message += "n - one cell bottom-right\n\n";
        message += "GENERAL OPERATIONS\n";
        message += "M - show messages\n";
        message += "L - enter look mode\n";
        message += "r - enter ranged mode\n";
        message += "H - view HUD\n";
        message += "d - drop item\n";
        message += "x - expunge item from game\n";
        message += "t - take off equiped item and place in inventory if applicable\n";
        message += "w - wear item\n";
        message += "i - view inventory\n";
        message += "I - view invetory item at index\n";
        message += "e - view equipment\n";
        message += "? - view help\n\n";
        message += "(Press any key to return to game view)";
        print_on_clear_screen(message);
        return 0;
    }
    else if (key == 94) { // ^ - level up
        show_level_up_screen();
        return 0;
    }
    else if (key == 83) { // s - view spells
        string message = "SPELLS\n\n";
        for (int i = 0; i < player->spells.size(); i++) {
            Object * spell = player->spells[i];
            message += to_string(i + 1) + ": " + spell->name;
        }
        message += "\n\n";
        message += "(Press any key to return to game view)";
        print_on_clear_screen(message);
    }
    else if (key == 77) { // M - show messages
        string logs = "";
        for(int i = 0; i < all_messages.size(); i++) {
            logs += all_messages[i]->toString() + "\n";
        }
        string message = "MESSAGES\n\n";
        message += logs + "\n\n";
        message += "(Press any key to return to game view)";
        print_on_clear_screen(message);
        return 0;
    }
    else if (key == 120) { // x - expunge
        string title = "Which inventory index? ";
        add_temp_message(title);
        move(0, title.length());
        char arr[80];
        echo();
        getstr(arr);
        string str(arr);
        noecho();
        int index;
        try {
            index = stoi(str);
        }
        catch(...) {
            add_temp_message("Invalid input. It's your turn");
            return 0;
        }
        if (index > player->getNumberOfItemsInInventory() - 1 || index < 0) {
            add_temp_message("No inventory item at that index. It's your turn");
            return 0;
        }
        Object * object = player->getInventoryItemAt(index);
        player->removeInventoryItemAt(index);
        add_temp_message("Expunged " + object->name + " from the game. It's your turn");
        if (object) {
            delete object;
        }
        return 0;

    }
    else if (key == 100) { // d - drop item
        string title = "Which inventory index? ";
        add_temp_message(title);
        move(0, title.length());
        char arr[80];
        echo();
        getstr(arr);
        string str(arr);
        noecho();
        int index;
        try {
            index = stoi(str);
        }
        catch(...) {
            add_temp_message("Invalid input. It's your turn");
            return 0;
        }
        if (index > player->getNumberOfItemsInInventory() - 1 || index < 0) {
            add_temp_message("No inventory item at that index. It's your turn");
            return 0;
        }
        Object * object = player->getInventoryItemAt(index);
        board[player->y][player->x].object = object;
        player_board[player->y][player->x].object = object;
        player->removeInventoryItemAt(index);
        add_message("Dropped " + object->name + ". It's your turn");
        return 0;
    }
    else if (key == 116) { // t - take off item
        string title = "Which equipment index? ";
        add_message(title);
        move(0, title.length());
        char arr[80];
        echo();
        getstr(arr);
        string str(arr);
        noecho();
        int index;
        try {
            index = stoi(str);
        }
        catch(...) {
            add_temp_message("Invalid input. It's your turn");
            return 0;
        }
        if (player->equipmentExistsAt(index)) {
            Object * equipment = player->getEquipmentAt(index);
            if (player->canPickUpObject()) {
                add_message("Moved " + equipment->type + " to inventory");
                player->takeOffEquipment(index);
                update_player_board();
                center_board_on_player();
            }
            else {
                add_message("Cannot take off " + equipment->type + "; no room in inventory. Try dropping it instead.");
            }
        }
        else {
            add_temp_message("No equipment item at that index. It's your turn");
        }
        return 0;
    }
    else if (key == 119) { // w - wear item
        string title = "Which inventory index? ";
        add_temp_message(title);
        move(0, title.length());
        char arr[80];
        echo();
        getstr(arr);
        string str(arr);
        noecho();
        int index;
        try {
            index = stoi(str);
        }
        catch(...) {
            add_temp_message("Invalid input. It's your turn");
            return 0;
        }
        if (index > player->getNumberOfItemsInInventory() - 1 || index < 0) {
            add_temp_message("No inventory item at that index. It's your turn");
            return 0;
        }
        Object * object = player->getInventoryItemAt(index);
        try {
            player->equipObjectAt(index);
        }
        catch(const char * e) {
            add_message(e);
            return 0;
        }
        add_message("Equipped " + object->type + ": " + object->name);
        update_player_board();
        center_board_on_player();
        return 0;
    }
    else if(key == 105) { // i - list inventory
        string message = "INVENTORY\n";
        if (player->getNumberOfItemsInInventory() == 0) {
            message += "No items in inventory\n";
        }
        else {
            for (int i = 0; i < player->getNumberOfItemsInInventory(); i++) {
                Object * item = player->getInventoryItemAt(i);
                message += to_string(i) + ". " + item->name + "\n";
            }
        }
        message += "\n(Press any key to return to game view)";
        print_on_clear_screen(message);
        return 0;
    }
    else if (key == 101) { // e - list equipment
        string message = "EQUIPMENT\n";
        for (int i = 0; i < player->equipmentSlots(); i++) {
            message += to_string(i) + ". " + player->equipmentSlotToString(i) + "\n";
        }
        message += "\n(Press any key to return to game view)";
        print_on_clear_screen(message);
        return 0;

    }
    else if (key == 72) { // H - view health, etc.
        string message = "HUD\n";
        message += player->getHudInfo();
        message += "\n(Pres any key to return to game view)";
        print_on_clear_screen(message);
        return 0;
    }
    else if (key == 114) { // r - ranged combat
        if (!player->hasRangedWeapon()) {
            add_temp_message("You have no ranged weapon. It's still your turn");
            return 0;
        }
        add_temp_message("Entered ranged mode");
        return handle_ranged_mode_input();
    }
    else if (key == 99) { // c - cast spell
        if (player->spells.size() == 0) {
            add_temp_message("You have not learned any spells. It's still your turn");
            return 0;
        }
        add_temp_message("Entered cast mode");
        return handle_cast_mode_input();
    }
    else if (key == 107 || key == 8) { // k - one cell up
        if (board[player->y - 1][player->x].hardness > 0) {
           return 0;
        }
        new_coord.y = player->y - 1;
    }
    else if (key == 106 || key == 2) { // j - one cell down
        if (board[player->y + 1][player->x].hardness > 0) {
            return 0;
        }
        new_coord.y = player->y + 1;
    }
    else if (key == 104 || key == 4) { // h - one cell left
        if (board[player->y][player->x - 1].hardness > 0) {
            return 0;
        }
        new_coord.x = player->x - 1;
    }
    else if(key == 108 || key == 6) { // l - one cell right
        if (board[player->y][player->x + 1].hardness > 0) {
            return 0;
        }
        new_coord.x = player->x + 1;
    }
    else if (key == 121 || key == 7) { // y - one cell up-left
        if (board[player->y - 1][player->x - 1].hardness > 0) {
            return 0;
        }
        new_coord.x = player->x - 1;
        new_coord.y = player->y - 1;
    }
    else if (key == 117 || key == 9) { // u - one cell up-right
        if (board[player->y - 1][player->x + 1].hardness > 0) {
            return 0;
        }
        new_coord.x = player->x + 1;
        new_coord.y = player->y - 1;
    }
    else if (key == 110 || key == 3) { // n - one cell low-right
        if (board[player->y + 1][player->x + 1].hardness > 0) {
            return 0;
        }
        new_coord.x = player->x + 1;
        new_coord.y = player->y + 1;
    }
    else if (key == 98 || key == 1) { // b - one cell low-left
        if (board[player->y + 1][player->x - 1].hardness > 0) {
            return 0;
        }
        new_coord.x = player->x - 1;
        new_coord.y = player->y + 1;
    }
    else if (key == 60 && IS_CONTROL_MODE) {  // upstairs
        if (board[player->y][player->x].type.compare(TYPE_UPSTAIR) != 0) {
           return 0;
        }
        add_message("You travel upstairs");
        player->x = 0;
        player->y = 0;
        player->addExperience(Numeric("0+5d3").roll());
        generate_new_board();
        return 2;
    }
    else if (key == 62) {  // downstairs
        if (board[player->y][player->x].type.compare(TYPE_DOWNSTAIR) != 0) {
            return 0;
        }
        add_message("You travel downstairs");
        player->y = 0;
        player->x = 0;
        player->addExperience(Numeric("0+5d3").roll());
        generate_new_board();
        return 2;
    }
    else if (key == 32 || key == 5) { // space - rest
        add_message("You rest");
    }
    else if (key == 76 && IS_CONTROL_MODE) { // L - enter look mode
        add_temp_message("Entering look mode");
        IS_CONTROL_MODE = 0;
    }
    else if (key == 81) { // Q - quit
        DO_QUIT = 1;
    }
    else {
        return 0;
    }
    if (board[new_coord.y][new_coord.x].monster) {
        Monster * monster = board[new_coord.y][new_coord.x].monster;
        int damage = player->getAttackDamage();
        if (!player->hasEnoughStaminaForAttack(damage)) {
            add_message("You do not have enough stamina for this attack!");
            return 0;
        }
        bool isAlive = damage_monster_from_melee_or_ranged(monster, damage);
        if (isAlive) {
            new_coord.x = player->x;
            new_coord.y = player->y;
        }
        else {
            player->x = new_coord.x;
            player->y = new_coord.y;

        }
    }
    else if (new_coord.x != player->x || new_coord.y != player->y) {
        player->x = new_coord.x;
        player->y = new_coord.y;
    }
    Board_Cell cell = board[new_coord.y][new_coord.x];
    if (cell.object && player->canPickUpObject()) {
        add_message("You picked up an object: " + cell.object->name);
        player->addObjectToInventory(cell.object);
        board[new_coord.y][new_coord.x].object = NULL;
    }
    update_player_board();
    return 1;
}

void center_board_on_player() {
    curs_set(1);
    int new_y = player->y - 10;
    int new_x = player->x - 40;
    update_board_view(new_x, new_y);
    move(ncurses_player_coord.y, ncurses_player_coord.x);
}

void print_board() {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (player->isAlive() && y == player->y && x == player->x) {
                printf("@");
            }
            else if (board[y][x].monster) {
                struct Coordinate coord;
                coord.x = x;
                coord.y = y;
                Monster * m = board[y][x].monster;
                int decimal_type = m->getDecimalType();
                printf("%x", decimal_type);
            }
            else {
                print_cell(board[y][x]);
            }
        }
        printf("\n");
    }
}

void print_cell(Board_Cell cell) {
    if (cell.type.compare(TYPE_ROCK) == 0) {
        printf(" ");
    }
    else if (cell.type.compare(TYPE_ROOM) == 0) {
        printf(".");
    }
    else if (cell.type.compare(TYPE_CORRIDOR) == 0) {
        printf("#");
    }
    else {
        printf("F");
    }
}

void dig_rooms(int number_of_rooms_to_dig) {
    for (int i = 0; i < number_of_rooms_to_dig; i++) {
        dig_room();
    }
    add_rooms_to_board();
}

void dig_room() {
    int start_x = random_int(1, WIDTH - MIN_ROOM_WIDTH - 1);
    int start_y = random_int(1, HEIGHT - MIN_ROOM_HEIGHT - 1);
    int room_height = random_int(MIN_ROOM_HEIGHT, MAX_ROOM_HEIGHT);
    int room_width = random_int(MIN_ROOM_WIDTH, MAX_ROOM_WIDTH);
    int end_y = start_y + room_height;
    if (end_y >= HEIGHT - 1) {
        end_y = HEIGHT - 2;

    }
    int end_x = start_x + room_width;
    if (end_x >= WIDTH - 1) {
        end_x = WIDTH - 2;

    }
    int height = end_y - start_y;
    int height_diff = MIN_ROOM_HEIGHT - height;
    if (height_diff > 0) {
        start_y -= height_diff + 1;
    }

    int width = end_x - start_x;
    int width_diff = MIN_ROOM_WIDTH - width;
    if (width_diff > 0) {
        start_x -= width_diff + 1;
    }
    struct Room room;
    room.start_x = start_x;
    room.start_y = start_y;
    room.end_x = end_x;
    room.end_y = end_y;
    room.has_explored = false;
    if (room_is_valid(room)) {
        rooms.push_back(room);
    }
    else {
        dig_room();
    }
}

int room_is_valid(struct Room room) {
    int width = room.end_x - room.start_x;
    int height = room.end_y - room.start_y;
    if (height < MIN_ROOM_HEIGHT || width < MIN_ROOM_WIDTH) {
        return 0;
    }
    if (room.start_x < 1 || room.start_y < 1 || room.end_x > WIDTH - 2 || room.end_y > HEIGHT - 2) {
        return 0;
    }
    for (size_t i = 0; i < rooms.size(); i++) {
        struct Room current_room = rooms[i];
        int start_x = current_room.start_x - 1;
        int start_y = current_room.start_y - 1;
        int end_x = current_room.end_x + 1;
        int end_y = current_room.end_y + 1;
        if ((room.start_x >= start_x  && room.start_x <= end_x) ||
                (room.end_x >= start_x && room.end_x <= end_x)) {
            if ((room.start_y >= start_y && room.start_y <= end_y) ||
                    (room.end_y >= start_y && room.end_y <= end_y)) {
                return 0;
            }
        }
    }
    return 1;
}

void add_rooms_to_board() {
    Board_Cell cell;
    cell.type = TYPE_ROOM;
    cell.hardness = ROOM;
    cell.monster = NULL;
    cell.object = NULL;
    for(size_t i = 0; i < rooms.size(); i++) {
        struct Room room = rooms[i];
        for (int y = room.start_y; y <= room.end_y; y++) {
            for(int x = room.start_x; x <= room.end_x; x++) {
                cell.x = x;
                cell.y = y;
                board[y][x] = cell;
            }
        }
    }
}

void dig_cooridors() {
    for (size_t i = 0; i < rooms.size(); i++) {
        int next_index = i + 1;
        if ((size_t) next_index == rooms.size()) {
            next_index = 0;
        }
        connect_rooms_at_indexes(i, next_index);
    }
}

void connect_rooms_at_indexes(int index1, int index2) {
    struct Room room1 = rooms[index1];
    struct Room room2 = rooms[index2];
    int start_x = ((room1.end_x - room1.start_x) / 2) + room1.start_x;
    int end_x = ((room2.end_x - room2.start_x) / 2) + room2.start_x;
    int start_y = ((room1.end_y - room1.start_y) / 2) + room1.start_y;
    int end_y = ((room2.end_y - room2.start_y) / 2) + room2.start_y;
    int x_incrementer = 1;
    int y_incrementer = 1;
    if (start_x > end_x) {
        x_incrementer = -1;
    }
    if (start_y > end_y) {
        y_incrementer = -1;
    }
    int cur_x = start_x;
    int cur_y = start_y;
    while(1) {
        int random_num = random_int(0, RAND_MAX);
        int move_y = random_num % 2 == 0;
        if (board[cur_y][cur_x].type.compare(TYPE_ROCK) != 0) {
            if (cur_y != end_y) {
                cur_y += y_incrementer;
            }
            else if(cur_x != end_x) {
                cur_x += x_incrementer;
            }
            else if(cur_y == end_y && cur_x == end_x) {
                break;
            }
            continue;
        }
        Board_Cell corridor_cell;
        corridor_cell.type = TYPE_CORRIDOR;
        corridor_cell.hardness = CORRIDOR;
        corridor_cell.monster = NULL;
        corridor_cell.object = NULL;
        corridor_cell.x = cur_x;
        corridor_cell.y = cur_y;
        board[cur_y][cur_x] = corridor_cell;
        if ((cur_y != end_y && move_y) || (cur_x == end_x)) {
            cur_y += y_incrementer;
        }
        else if ((cur_x != end_x && !move_y) || (cur_y == end_y)) {
            cur_x += x_incrementer;
        }
        else {
            break;
        }
    }
}

vector<struct Coordinate> get_non_tunneling_available_coords_for(struct Coordinate coord) {
    int x = coord.x;
    int y = coord.y;
    struct Coordinate new_coord;
    vector<struct Coordinate> coords;
    if (board[y - 1][x].hardness == 0) {
        new_coord.y = y - 1;
        new_coord.x = x;
        coords.push_back(new_coord);
    }
    if (board[y - 1][x - 1].hardness == 0) {
        new_coord.y = y - 1;
        new_coord.x = x - 1;
        coords.push_back(new_coord);
    }
    if(board[y - 1][x + 1].hardness == 0) {
        new_coord.y = y - 1;
        new_coord.x = x + 1;
        coords.push_back(new_coord);
    }
    if(board[y + 1][x].hardness == 0) {
        new_coord.y = y + 1;
        new_coord.x = x;
        coords.push_back(new_coord);
    }
    if(board[y + 1][x - 1].hardness == 0) {
        new_coord.y = y + 1;
        new_coord.x = x - 1;
        coords.push_back(new_coord);
    }
    if(board[y + 1][x + 1].hardness == 0) {
        new_coord.y = y + 1;
        new_coord.x = x + 1;
        coords.push_back(new_coord);
    }
    if(board[y][x - 1].hardness == 0) {
        new_coord.y = y;
        new_coord.x = x - 1;
        coords.push_back(new_coord);
    }
    if (board[y][x + 1].hardness == 0) {
        new_coord.y = y;
        new_coord.x = x + 1;
        coords.push_back(new_coord);
    }
    return coords;
}

struct Coordinate get_random_new_non_tunneling_location(struct Coordinate coord) {
    struct Coordinate new_coord;
    vector<struct Coordinate> coords = get_non_tunneling_available_coords_for(coord);
    int new_coord_index = random_int(0, coords.size() - 1);
    struct Coordinate temp_coord = coords[new_coord_index];
    new_coord.x = temp_coord.x;
    new_coord.y = temp_coord.y;
    return new_coord;
}

struct Coordinate get_random_new_tunneling_location(struct Coordinate coord) {
    struct Coordinate new_coord;
    new_coord.x = coord.x;
    new_coord.y = coord.y;
    int max_x = coord.x + 1;
    if (coord.x + 1 >= WIDTH - 1) {
        max_x = coord.x;
    }
    int min_x = coord. x - 1;
    if (min_x <= 1) {
        min_x = coord.x;
    }
    int min_y = coord.y - 1;
    if (min_y <= 1) {
        min_y = coord.y;
    }
    int max_y = coord.y + 1;
    if (max_y >= WIDTH - 1) {
        max_y = coord.y;
    }
    while(1) {
        new_coord.x = random_int(min_x, max_x);
        new_coord.y = random_int(min_y, max_y);
        if (coord.x == new_coord.x && coord.y == new_coord.y) {
            continue;
        }
        if (board[new_coord.y][new_coord.x].hardness != IMMUTABLE_ROCK) {
            break;
        }
    }
    return new_coord;
}


vector<Board_Cell> get_surrounding_cells(struct Coordinate c) {
    vector<Board_Cell> cells;
    cells.push_back(board[c.y + 1][c.x]);
    cells.push_back(board[c.y + 1][c.x - 1]);
    cells.push_back(board[c.y + 1][c.x + 1]);
    cells.push_back(board[c.y - 1][c.x]);
    cells.push_back(board[c.y - 1][c.x + 1]);
    cells.push_back(board[c.y - 1][c.x - 1]);
    cells.push_back(board[c.y][c.x + 1]);
    cells.push_back(board[c.y][c.x - 1]);
    return cells;
}

Board_Cell get_cell_on_tunneling_path(struct Coordinate c) {
    vector<Board_Cell> cells = get_surrounding_cells(c);
    Board_Cell cell = board[c.y][c.x];
    for (size_t i = 0; i < cells.size(); i++) {
        Board_Cell current_cell = cells[i];
        if (current_cell.tunneling_distance < cell.tunneling_distance) {
            cell = current_cell;
        }
    }
    return cell;
}


Board_Cell get_cell_on_non_tunneling_path(struct Coordinate c) {
    vector<Board_Cell> cells = get_surrounding_cells(c);
    Board_Cell cell = board[c.y][c.x];
    int min = cell.non_tunneling_distance;
    for (size_t i = 0; i < cells.size(); i++) {
        Board_Cell my_cell = cells[i];
        if (my_cell.non_tunneling_distance < min) {
            cell = my_cell;
            min = my_cell.non_tunneling_distance;
        }
    }
    return cell;
}

int get_room_index_player_is_in() {
    int index = -1;
    for (size_t i = 0; i < rooms.size(); i++) {
        struct Room current_room = rooms[i];
        if (current_room.start_x <= player->x && player->x <= current_room.end_x) {
            if (current_room.start_y <= player->y && player->y <= current_room.end_y) {
                index = i;
                break;
            }
        }
    }
    return index;
}

int should_do_erratic_behavior() {
    return random_int(0, 1);
}

int monster_knows_last_player_location(Monster * m) {
    int last_x = m->last_known_player_x;
    int last_y = m->last_known_player_y;
    return last_x != 0 && last_y != 0;
}

struct Coordinate get_straight_path_to(Monster * m, struct Coordinate coord) {
    int x = m->x;
    int y = m->y;
    struct Coordinate new_coord;
    if (x == coord.x) {
        new_coord.x = x;
    }
    else if (x < coord.x) {
        new_coord.x = x + 1;
    }
    else {
        new_coord.x = x - 1;
    }

    if (y == coord.y) {
        new_coord.y = y;
    }
    else if (y < coord.y) {
        new_coord.y = y + 1;
    }
    else {
        new_coord.y = y - 1;
    }

    return new_coord;
}

void displace_monster(struct Coordinate coord) {
    Monster * monster = board[coord.y][coord.x].monster;
    vector<Board_Cell> cells = get_surrounding_cells(coord);
    Board_Cell potential_cell;
    for (size_t i = 0; i < cells.size(); i++) {
        Board_Cell cell = cells[i];
        if (cell.hardness == 0) {
            if (!cell.monster) {
                monster->x = cell.x;
                monster->y = cell.y;
                board[cell.y][cell.x].monster = board[coord.y][coord.x].monster;
                board[coord.y][coord.x].monster = NULL;
                return;
            }
            potential_cell = cell;
        }
    }
    monster->x = potential_cell.x;
    monster->y = potential_cell.y;
    board[potential_cell.y][potential_cell.x].monster = board[coord.y][coord.x].monster;
    board[coord.y][coord.x].monster = NULL;
}

void move_monster(Monster * monster) {
    int monster_x = monster->x;
    int monster_y = monster->y;
    Board_Cell cell = board[monster_y][monster_x];
    struct Coordinate monster_coord;
    monster_coord.x = monster_x;
    monster_coord.y = monster_y;
    struct Coordinate new_coord;
    new_coord.x = monster_x;
    new_coord.y = monster_y;
    int decimal_type = monster->getDecimalType();
    struct Coordinate last_known_player_location;
    last_known_player_location.x = monster->last_known_player_x;
    last_known_player_location.y = monster->last_known_player_y;
    struct Coordinate player_coord;
    player_coord.x = player->x;
    player_coord.y = player->y;
    switch(decimal_type) {
        case 0: // nothing
            if (is_in_line_of_sight(monster_coord, player_coord)) {
                new_coord = get_straight_path_to(monster, player_coord);
            }
            else {
                new_coord = get_random_new_non_tunneling_location(monster_coord);
            }
            break;
        case 1: // intelligent
            if (is_in_line_of_sight(monster_coord, player_coord)) {
                monster->last_known_player_x = player->x;
                monster->last_known_player_y = player->y;
                new_coord = get_straight_path_to(monster, player_coord);
            }
            else if(monster_knows_last_player_location(monster)) {
                new_coord = get_straight_path_to(monster, last_known_player_location);
                if (new_coord.x == last_known_player_location.x && new_coord.y == last_known_player_location.y) {
                    monster->resetPlayerLocation();
                }
            }
            else {
                new_coord = get_random_new_non_tunneling_location(monster_coord);
            }
            break;
        case 2: // telepathic
            new_coord = get_straight_path_to(monster, player_coord);
            if (board[new_coord.y][new_coord.x].hardness > 0) {
                new_coord.x = monster_coord.x;
                new_coord.y = monster_coord.y;
            }
            break;
        case 3: // telepathic + intelligent
            cell = get_cell_on_non_tunneling_path(new_coord);
            new_coord.x = cell.x;
            new_coord.y = cell.y;
            break;
        case 4: // tunneling
            if (is_in_line_of_sight(monster_coord, player_coord)) {
                new_coord = get_straight_path_to(monster, player_coord);
            }
            else {
                new_coord = get_random_new_tunneling_location(monster_coord);
            }
            cell = board[new_coord.y][new_coord.x];
            if (cell.hardness > 0) {
                board[cell.y][cell.x].hardness -= 85;
                if (board[cell.y][cell.x].hardness <= 0) {
                    board[cell.y][cell.x].hardness = 0;
                    board[cell.y][cell.x].type = TYPE_CORRIDOR;
                }
                else {
                    new_coord.x = monster_x;
                    new_coord.y = monster_y;
                }
            }
            break;
        case 5: // tunneling + intelligent
            if (is_in_line_of_sight(monster_coord, player_coord)) {
                monster->last_known_player_x = player->x;
                monster->last_known_player_y = player->y;
                new_coord = get_straight_path_to(monster, player_coord);
            }
            else if(monster_knows_last_player_location(monster)) {
                new_coord = get_straight_path_to(monster, last_known_player_location);
                if (new_coord.x == last_known_player_location.x && new_coord.y == last_known_player_location.y) {
                    monster->resetPlayerLocation();
                }
            }
            else {
                new_coord = get_random_new_tunneling_location(monster_coord);
                cell = board[new_coord.y][new_coord.x];
                if (cell.hardness > 0) {
                    board[cell.y][cell.x].hardness -= 85;
                    if (board[cell.y][cell.x].hardness <= 0) {
                        board[cell.y][cell.x].hardness = 0;
                        board[cell.y][cell.x].type = TYPE_CORRIDOR;
                    }
                    else {
                        new_coord.x = monster_x;
                        new_coord.y = monster_y;
                    }
                }
            }
            break;
        case 6: // tunneling + telepathic
            new_coord = get_straight_path_to(monster, player_coord);
            cell = board[new_coord.y][new_coord.x];
            if (cell.hardness > 0) {
                board[cell.y][cell.x].hardness -= 85;
                if (board[cell.y][cell.x].hardness <= 0) {
                    board[cell.y][cell.x].hardness = 0;
                    board[cell.y][cell.x].type = TYPE_CORRIDOR;
                }
                else {
                    new_coord.x = monster_x;
                    new_coord.y = monster_y;
                }
            }
            break;
        case 7: // tunneling + telepathic + intelligent
            cell = get_cell_on_tunneling_path(new_coord);
            new_coord.x = cell.x;
            new_coord.y = cell.y;
            if (cell.hardness > 0) {
                board[cell.y][cell.x].hardness -= 85;
                if (board[cell.y][cell.x].hardness <= 0) {
                    board[cell.y][cell.x].hardness = 0;
                    board[cell.y][cell.x].type = TYPE_CORRIDOR;
                }
                else {
                    new_coord.x = monster_x;
                    new_coord.y = monster_y;
                }
            }
            break;
        case 8: // erratic
            if (should_do_erratic_behavior()) {
                new_coord = get_random_new_non_tunneling_location(monster_coord);
            }
            else {
                if (is_in_line_of_sight(monster_coord, player_coord)) {
                    new_coord = get_straight_path_to(monster, player_coord);
                }
                else {
                    new_coord = get_random_new_non_tunneling_location(monster_coord);
                }
            }
            break;
        case 9: // erratic + intelligent
            if (should_do_erratic_behavior()) {
                new_coord = get_random_new_non_tunneling_location(monster_coord);
            }
            else {
                if (is_in_line_of_sight(monster_coord, player_coord)) {
                    monster->last_known_player_x = player->x;
                    monster->last_known_player_y = player->y;
                    new_coord = get_straight_path_to(monster, player_coord);
                }
                else if(monster_knows_last_player_location(monster)) {
                    new_coord = get_straight_path_to(monster, last_known_player_location);
                    if (new_coord.x == last_known_player_location.x && new_coord.y == last_known_player_location.y) {
                        monster->resetPlayerLocation();
                    }
                }
                else {
                    new_coord = get_random_new_non_tunneling_location(monster_coord);
                }
            }
            break;
        case 10: // erratic + telepathic
            if (should_do_erratic_behavior()) {
                new_coord = get_random_new_non_tunneling_location(monster_coord);
            }
            else {
                new_coord = get_straight_path_to(monster, player_coord);
                if (board[new_coord.y][new_coord.x].hardness != 0) {
                    new_coord.x = monster_x;
                    new_coord.y = monster_y;
                }
            }
            break;
        case 11: // erratic + intelligent + telepathic
            if (should_do_erratic_behavior()) {
                new_coord = get_random_new_non_tunneling_location(monster_coord);
            }
            else {
                new_coord = get_straight_path_to(monster, player_coord);
                cell = board[new_coord.y][new_coord.x];
                if (cell.hardness > 0) {
                    board[cell.y][cell.x].hardness -= 85;
                    if (board[cell.y][cell.x].hardness <= 0) {
                        board[cell.y][cell.x].hardness = 0;
                        board[cell.y][cell.x].type = TYPE_CORRIDOR;
                    }
                    else {
                        new_coord.x = monster_x;
                        new_coord.y = monster_y;
                    }
                }
            }
            break;
        case 12: // erratic + tunneling
            if (should_do_erratic_behavior()) {
                new_coord = get_random_new_non_tunneling_location(monster_coord);
            }
            else {
                if (is_in_line_of_sight(monster_coord, player_coord)) {
                    new_coord = get_straight_path_to(monster, player_coord);
                }
                else {
                    new_coord = get_random_new_tunneling_location(monster_coord);
                }
                cell = board[new_coord.y][new_coord.x];
                if (cell.hardness > 0) {
                    board[cell.y][cell.x].hardness -= 85;
                    if (board[cell.y][cell.x].hardness <= 0) {
                        board[cell.y][cell.x].hardness = 0;
                        board[cell.y][cell.x].type = TYPE_CORRIDOR;
                    }
                    else {
                        new_coord.x = monster_x;
                        new_coord.y = monster_y;
                    }
                }
            }
            break;
        case 13: // erratic + tunneling + intelligent
            if (should_do_erratic_behavior()) {
                new_coord = get_random_new_non_tunneling_location(monster_coord);
            }
            else {
                if (is_in_line_of_sight(monster_coord, player_coord)) {
                    monster->last_known_player_x = player->x;
                    monster->last_known_player_y = player->y;
                    new_coord = get_straight_path_to(monster, player_coord);
                }
                else if(monster_knows_last_player_location(monster)) {
                    new_coord = get_straight_path_to(monster, last_known_player_location);
                    if (new_coord.x == last_known_player_location.x && new_coord.y == last_known_player_location.y) {
                        monster->resetPlayerLocation();
                    }
                }
                else {
                    new_coord = get_random_new_tunneling_location(monster_coord);
                    cell = board[new_coord.y][new_coord.x];
                    if (cell.hardness > 0) {
                        board[cell.y][cell.x].hardness -= 85;
                        if (board[cell.y][cell.x].hardness <= 0) {
                            board[cell.y][cell.x].hardness = 0;
                            board[cell.y][cell.x].type = TYPE_CORRIDOR;
                        }
                        else {
                            new_coord.x = monster_x;
                            new_coord.y = monster_y;
                        }
                    }
                }
            }
            break;
        case 14: // erratic + tunneling + telepathic
            if (should_do_erratic_behavior()) {
                new_coord = get_random_new_non_tunneling_location(monster_coord);
            }
            else {
                new_coord = get_straight_path_to(monster, player_coord);
                cell = board[new_coord.y][new_coord.x];
                if (cell.hardness > 0) {
                    board[cell.y][cell.x].hardness -= 85;
                    if (board[cell.y][cell.x].hardness <= 0) {
                        board[cell.y][cell.x].hardness = 0;
                        board[cell.y][cell.x].type = TYPE_CORRIDOR;
                    }
                    else {
                        new_coord.x = monster_x;
                        new_coord.y = monster_y;
                    }
                }
            }
            break;
        case 15: // erratic + tunneling + telepathic + intelligent
            if (should_do_erratic_behavior()) {
                new_coord = get_random_new_non_tunneling_location(monster_coord);
            }
            else {
                cell = get_cell_on_tunneling_path(new_coord);
                new_coord.x = cell.x;
                new_coord.y = cell.y;
                if (cell.hardness > 0) {
                    board[cell.y][cell.x].hardness -= 85;
                    if (board[cell.y][cell.x].hardness <= 0) {
                        board[cell.y][cell.x].hardness = 0;
                        board[cell.y][cell.x].type = TYPE_CORRIDOR;
                    }
                    else {
                        new_coord.x = monster_x;
                        new_coord.y = monster_y;
                    }
                }
            }
            break;
        default:
            printf("Invalid decimal type, %d\n", decimal_type);
            break;
    }

    bool attacked_player = false;
    if (new_coord.y == player->y && new_coord.x == player->x) {
        attacked_player = true;
        if (player->willDodgeAttack()) {
            add_message("You dodge the monster's attack!");
        }
        else if(!player->hitWillConnect()) {
            add_message("The monster fails to hit you!");
        }
        else {
            int damage = monster->getAttackDamage();
            int actual_damage = player->damage(damage);
            add_message("Monster inflicted " + to_string(actual_damage) + " points of damage on you!");
        }
    }
    else if (new_coord.x != monster_x || new_coord.y != monster_y) {
        if (board[new_coord.y][new_coord.x].monster != NULL) {
            displace_monster(new_coord);
        }
    }
    if (!attacked_player) {
        board[monster->y][monster->x].monster = NULL;
        monster->x = new_coord.x;
        monster->y = new_coord.y;
        board[new_coord.y][new_coord.x].monster = monster;
    }
}
