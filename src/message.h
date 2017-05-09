#include <string>
#include <ctime>

using namespace std;

class Message {
    private:

    public:
        struct tm * timestamp;
        string message;
        Message(string message);
        string getFormattedTime();
        string toString();

};
