#include <string>
#include <ctime>
#include "message.h"

Message::Message(string message) {
    this->message = message;
    time_t t = time(0);
    this->timestamp = localtime(& t);
}

string Message::getFormattedTime() {
    string year = to_string(timestamp->tm_year + 1900);
    string month = to_string(timestamp->tm_mon);
    if (timestamp->tm_mon < 10) {
        month = "0" + month;
    }
    string day = to_string(timestamp->tm_mday);
    if (timestamp->tm_mday < 10) {
        day = "0" + day;
    }
    string hour = to_string(timestamp->tm_hour);
    if (timestamp->tm_hour < 10) {
        hour = "0" + hour;
    }
    string minute = to_string(timestamp->tm_min);
    if (timestamp->tm_min < 10) {
        minute = "0" + minute;
    }
    string second = to_string(timestamp->tm_sec);
    if (timestamp->tm_sec < 10) {
        second = "0" + second;
    }
    return year + "-" + month + "-" + day + " at " + hour + ":" + minute + ":" + second;
}

string Message::toString() {
    return getFormattedTime() + "\t" + message;
}
