#include "board_element.h"


BoardElement :: ~BoardElement() {

}

struct Coordinate BoardElement :: getCoord() {
    struct Coordinate coord;
    coord.x = x;
    coord.y = y;
    return coord;
}
