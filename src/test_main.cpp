#include <iostream>
#include <vector>
#include <string>

using namespace std;



void print_msg (string msg[]) {
    cout << msg->size() << "\n";
    for (int i = 0; i < 3; i++)
    {
        if (msg[i] != "") {
            cout << msg[i] << "\n";
        }
    }
    cout << "over" << endl;
}

void split(const string& s, vector<string>& tokens, const string& delimiters = " ") {
    string::size_type lastPos = s.find_first_not_of(delimiters, 0);
    string::size_type pos = s.find_first_of(delimiters, lastPos);
    while (string::npos != pos || string::npos != lastPos) {
        tokens.push_back(s.substr(lastPos, pos - lastPos));
        lastPos = s.find_first_not_of(delimiters, pos);
        pos = s.find_first_of(delimiters, lastPos);
    }
}

int main()
{
    string s = "192.168.100.1";
    vector<string> tokens;
    split(s, tokens, ".");
    for (string s: tokens) {
        cout << s << "\n";
    }
    cout << "over" << endl;
}