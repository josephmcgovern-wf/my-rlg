#ifndef BOARD_ELEMENT_H
#define BOARD_ELEMENT_H
#include "util.h"

class BoardElement {
    public:
        int x;
        int y;
        virtual ~BoardElement();
        struct Coordinate getCoord();
};
#endif
