#pragma once
#include "types.h"

struct Move
{
    uchar to;
    uchar from;
    uchar type;

    Move(uchar from, uchar to) : from(from), to(to), type(MOVE_REGULAR) { }
    Move(uchar from, uchar to, uchar type) : from(from), to(to), type(type) { }
    Move() : to(0), from(0), type(MOVE_REGULAR) { }
};