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
using std::flush;
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

int merrno(int &status);

int mpwd();

int mcd(const char *path);

int mexit(int status);

int mecho(vector<string> texts);

int mexport();

int main(int argc, char **argv) {
    std::string comm;
    int status;
    auto path_ptr = getenv("PATH");
    string path_var;
    if (path_ptr != nullptr)
        path_var = path_ptr;
    path_var += ":.";
    setenv("PATH", path_var.c_str(), 1);
  
    while (1) {
        std::vector<string> args;
        char buff[FILENAME_MAX];
        getcwd( buff, FILENAME_MAX );
        cout << buff << flush;

        comm = readline("$ ");
        add_history(comm.c_str());

        parse_line(args, comm);
        execute(status, args);

    }
    return 0;
//    ps = parse(comm);

}

void execute(int &status, vector<string> args) {
    string program_name = args[0];
    vector<const char *> arg_for_c;
    arg_for_c.reserve(args.size());
    for (const auto &s: args)
        arg_for_c.push_back(s.c_str());
    arg_for_c.push_back(nullptr);

    if (program_name == "merrno") {
        merrno(status);
    } else if (program_name == "mpwd") {
        mpwd();
    } else if (program_name == "mcd") {
        if ( arg_for_c.size() == 3 ) {
            mcd(arg_for_c[1]);
        } else {
            cerr << "Error: mcd() should receive one argument" << endl;
            exit(EXIT_FAILURE);
        }
    } else if(program_name == "mexit") {
        int exit_status = 0;
        if ( arg_for_c.size() > 3 ) {
            cerr << "Error: mcd() should receive one argument" << endl;
            exit(EXIT_FAILURE);
        } else if ( arg_for_c.size() == 3 ) {
            exit_status = atoi(arg_for_c[1]);
        }
        mexit(exit_status);
    } else if(program_name == "mecho") {
        mecho(args);
    } else if(program_name == "mexport") {
        mexport();
    } else {

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
            execvp(program_name.c_str(), const_cast<char *const *>(arg_for_c.data()));
            cerr << "Parent: Failed to execute " << program_name << " \n\tCode: " << errno << endl;
            exit(EXIT_FAILURE);   // exec never returns
        }

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


int merrno(int &status) {

    cout << status << endl;
    return 0;

}

int mpwd() {

    char buff[FILENAME_MAX];
    getcwd( buff, FILENAME_MAX );
    cout << buff << endl;
    return 0;

}


int mcd(const char *path) {

    char *full_path = realpath(path, NULL);;
    int status = chdir(full_path);
    free(full_path);
    return status;

}


int mexit(int status) {

    exit(status);

}


int mecho(vector<string> texts) {

    for (int i=1; i<texts.size(); i++) {
        if (texts[i].at(i) == '$') {
            // print env var
        } else {
            cout << texts[i] << " " << flush;
        }
    }
    cout << endl;
    return 0;
}

int mexport() {
    return 0;
}
