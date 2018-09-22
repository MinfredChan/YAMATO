

#ifndef SS_MANAGER_CONSTANTS_H
#define SS_MANAGER_CONSTANTS_H

#include <vector>
#include <string>
#include <array>
#include <cstdlib>
#include <sstream>

#define SOFTWARE_VERSION "1.1.0"
#define DEBUG false

using namespace std;

struct Help {
    string option;
    string description;
};

struct YAML {
	int level;
	string left;
	string right;
};

struct PIDInfo {
	string pid;
	string port;
};

#endif //SS_MANAGER_CONSTANTS_H