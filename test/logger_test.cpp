//
// Created by hww1996 on 2019/10/20.
//

#include "logger.h"

int main() {
    ToyRaft::LOGERROR("This is error.");
    ToyRaft::LOGERROR("This is error.%s %d", "OK", 1);
    ToyRaft::LOGWARING("This is warning.");
    ToyRaft::LOGWARING("This is warning.%s", "OK");
    ToyRaft::LOGNOTICE("This is notice.");
    ToyRaft::LOGNOTICE("This is notice.%s", "OK");
    ToyRaft::LOGDEBUG("This is debug.");
    ToyRaft::LOGDEBUG("This is debug.%s", "OK");
    ToyRaft::LOGINFO("This is info.");
    ToyRaft::LOGINFO("This is info.%s", "OK");
    return 0;
}

