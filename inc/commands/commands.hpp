#ifndef COMMANDS
#define COMMANDS

#include <string>
#include <vector>
#include <iostream>
#include <boost/algorithm/string.hpp>
using std::cout;
using std::cerr;
using std::flush;
using std::endl;
using std::string;
using std::vector;

void execute(int &status, vector<string> args);

int merrno(int &status);

int mpwd();

int mcd(const char *path);

int mexit(int status);

int mecho(vector<string> texts);

int mexport(string varname, string value);

#endif //COMMANDS