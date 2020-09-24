#include <iostream>
// Scripts
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
// Parsing
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include "fnmatch.h"
// History of input commands
#include <readline/readline.h>
#include <readline/history.h>
// Syscalls
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <vector>
#include <string>

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;

using namespace boost::filesystem;
using namespace boost::algorithm;
using boost::tokenizer;
using boost::escaped_list_separator;
typedef tokenizer<escaped_list_separator<char> > my_tokenizer;

void parse_line(std::vector<string> &args, std::string &comm);

void execute(int &status, vector<string> args);

bool is_wildcard(string &s);

int main(int argc, char **argv) {
    int status;
    std::string comm;
    auto path_ptr = getenv("PATH");
    string path_var;
    if (path_ptr != nullptr)
        path_var = path_ptr;
    path_var += ":.";
    setenv("PATH", path_var.c_str(), 1);
    do {
        std::vector<string> args;
        comm = readline("myshell$ ");
        add_history(comm.c_str());
        parse_line(args, comm);
        execute(status, args);
    } while (status == 0);
    return 0;
//    ps = parse(comm);

}

void execute(int &status, vector<string> args) {
    string victim_name = args[0];
    vector<const char *> arg_for_c;
    arg_for_c.reserve(args.size());
    for (const auto &s: args)
        arg_for_c.push_back(s.c_str());
    arg_for_c.push_back(nullptr);

    pid_t parent = getpid();
    pid_t pid = fork();
    if (pid == -1) {
        std::cerr << "Failed to fork()" << std::endl;
        status = -1;
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // We are parent process
        waitpid(pid, &status, 0);
    } else {
        // We are the child
        execvp(victim_name.c_str(), const_cast<char *const *>(arg_for_c.data()));
        cerr << "Parent: Failed to execute " << victim_name << " \n\tCode: " << errno << endl;
        exit(EXIT_FAILURE);   // exec never returns
    }
}

void parse_line(std::vector<string> &args, std::string &comm) {
    vector<string> tmp;
    vector<string> arg_with_eq;
    path p;
    std::string filename;
    std::string dir;

    boost::algorithm::trim(comm);
    my_tokenizer tok(comm, escaped_list_separator<char>('\\', ' ', '\"'));
    for (my_tokenizer::iterator beg = tok.begin(); beg != tok.end(); ++beg) {
        tmp.push_back(*beg);
    }

    split(arg_with_eq, tmp[0], is_any_of("="));
    if (tmp.size() == 1 && arg_with_eq.size() == 2) {
        // Створення змінної
    }
    args.push_back(tmp[0]);
    for (size_t i = 1; i < tmp.size(); ++i) {
        if (tmp[i].rfind('-', 0) != 0) {
            split(arg_with_eq, tmp[i], is_any_of("="));
            p = (arg_with_eq.size() == 2) ? arg_with_eq[1] : tmp[i];
            dir = p.parent_path().string();
            filename = p.filename().string();
            try {
                boost::filesystem::directory_iterator test_iter(dir);
            } catch (filesystem_error &e) {
                dir = ".";
                filename = tmp[i];
            }
            if (is_wildcard(filename)) {
                boost::filesystem::directory_iterator end_itr; // Default ctor yields past-the-end
                for (boost::filesystem::directory_iterator iter(dir); iter != end_itr; ++iter) {
                    if (!boost::filesystem::is_regular_file(iter->status())) continue; // Skip if not a file
                    if (fnmatch(filename.c_str(), iter->path().filename().string().c_str(), 0) != 0) continue;
                    args.push_back(iter->path().filename().string());
                }
            } else args.push_back(tmp[i]);
        } else args.push_back(tmp[i]);
    }
}


bool is_wildcard(string &s) {
    string p = "?[]()*";
    for (auto c: p) {
        if (s.find(c) != string::npos) return true;
    }
    return false;
}