#include <string>
#include <vector>
#include <cstdlib>
#include "util.h"
#include "numeric.h"

using namespace std;


Numeric :: Numeric(string numericString) {
    vector<string> split1 = split(numericString, "+");
    if (split1.size() > 0) {
        base = atoi(split1[0].c_str());
        vector<string> split2 = split(split1[1], "d");
        if (split2.size() > 0) {
            dice = atoi(split2[0].c_str());
            sides = atoi(split2[1].c_str());
        }
        else {
            base = -1;
            sides = -1;
            dice = -1;
        }
    }
    else {
        base = -1;
        dice = -1;
        sides = -1;
    }
}

Numeric :: Numeric() {
    base = -1;
    dice = -1;
    sides = -1;
}

bool Numeric :: isValid() {
    return base >= 0 && dice >= 0 && sides >= 0;
}

int Numeric :: roll() {
    return random_int(base + dice, base + (dice * sides));
}

string Numeric :: toString() {
    return to_string(base) + "+" + to_string(dice) + "d" + to_string(sides);
}


