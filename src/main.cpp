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
#include <sys/wait.h>

#include <vector>
#include <string>
#include "commands/commands.hpp"

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

bool is_wildcard(string &s);


int main(int argc, char **argv) {
    std::string comm;
    int status;
    auto path_ptr = getenv("PATH");
    string path_var;
    if (path_ptr != nullptr)
        path_var = path_ptr;
    path_var += ":.";
    setenv("PATH", path_var.c_str(), 1);
    std::ifstream script_input(argv[1]);


    while (1) {
        std::vector<string> args;

        if (argc > 1) {
            if (!getline(script_input, comm))
                break;
        } else {
            char buff[FILENAME_MAX];
            getcwd(buff, FILENAME_MAX);
            cout << buff << flush;
            comm = readline("$ ");
            add_history(comm.c_str());
        }
        parse_line(args, comm);
        if (!args.empty())
            execute(status, args);
    }
    return 0;

}

void execute(int &status, vector<string> args) {
    string program_name = args[0];
    vector<const char *> arg_for_c;
    arg_for_c.reserve(args.size());
    for (const auto &s: args)
        arg_for_c.push_back(s.c_str());
    arg_for_c.push_back(nullptr);

    if (program_name == "merrno") {
        if (std::find(args.begin(), args.end(), "-h") != args.end() ||
            std::find(args.begin(), args.end(), "--help") != args.end()) {
            cout << "\nmerrno [-h|--help]  – вивести код завершення останньої програми чи команди\n"
                    "Повертає нуль, якщо ще жодна програма не виконувалася.\n" << endl;
        } else {
            if (args.size() == 1) {
                status = merrno(status);
            } else {
                cerr << "Error: mpwd() should not receive any arguments" << endl;
                status = 1;
            }

        }
    } else if (program_name == "mpwd") {
        if (std::find(args.begin(), args.end(), "-h") != args.end() ||
            std::find(args.begin(), args.end(), "--help") != args.end()) {
            cout << "\nmpwd [-h|--help] – вивести поточний шлях\n" << endl;
        } else {
            if (args.size() == 1) {
                status = mpwd();
            } else {
                cerr << "Error: mpwd() should not receive any arguments" << endl;
                status = 1;
            }
        }
    } else if (program_name == "mcd") {
        if (std::find(args.begin(), args.end(), "-h") != args.end() ||
            std::find(args.begin(), args.end(), "--help") != args.end()) {
            cout << "\nmcd <path> [-h|--help]  -- перейти до шляху <path>\n" << endl;
        } else {
            if (args.size() == 2) {
                status = mcd(arg_for_c[1]);
            } else {
                cerr << "Error: mcd() should receive one argument" << endl;
                status = 1;
            }
        }
    } else if (program_name == "mexit") {
        if (std::find(args.begin(), args.end(), "-h") != args.end() ||
            std::find(args.begin(), args.end(), "--help") != args.end()) {
            cout << "\nmexit [код завершення] [-h|--help]  – вийти із myshell\n" << endl;
        } else {
            int exit_status = 0;
            if (args.size() > 2) {
                cerr << "Error: mexit() should receive one argument" << endl;
                exit(EXIT_FAILURE);
            } else if (args.size() == 2) {
                exit_status = atoi(arg_for_c[1]);
            }
            mexit(exit_status);
        }
    } else if (program_name == "mecho") {
        if (std::find(args.begin(), args.end(), "-h") != args.end() ||
            std::find(args.begin(), args.end(), "--help") != args.end()) {
            cout << "\nmecho [-h|--help] [text|$<var_name>] [text|$<var_name>]  [text|$<var_name>] ...\n"
                    "Якщо аргумент починається не з $ -- просто виводить його на консоль.\n"
                    "Якщо з $ -- шукає відповідну змінну та виводить її вміст. Якщо такої змінної не існує -- не виводить нічого.\n"
                 << endl;
        } else {
            status = mecho(args);
        }
    } else if (program_name == "mexport") {
        if (std::find(args.begin(), args.end(), "-h") != args.end() ||
            std::find(args.begin(), args.end(), "--help") != args.end()) {
            cout << "\nmexport var_name=VAL\n"
                    "Додати глобальну змінну -- поміщається в блоку змінних середовища для дочірніх процесів.\n"
                    "Виклик mexport var_name=    створює порожню змінну -- це не помилка.\n" << endl;
        } else {
            if (args.size() < 3) {
                vector<string> envvar_args;
                boost::split(envvar_args, args[1], boost::is_any_of("="));
                status = mexport(envvar_args[0], envvar_args[1]);
            } else {
                cout << "Error: proper arguments should look like 'varname=val'" << endl;
                status = 1;
            }
        }

    } else if (program_name == ".") {
        std::ifstream script_input(args[1]);

        for (std::string script_line; getline(script_input, script_line);) {
            std::vector<string> script_args;

            parse_line(script_args, script_line);
            if (!script_args.empty()) {
                string script_program_name = script_args[0];
                vector<const char *> script_arg_for_c;
                script_arg_for_c.reserve(script_args.size());
                for (const auto &s: script_args)
                    script_arg_for_c.push_back(s.c_str());
                script_arg_for_c.push_back(nullptr);

//                pid_t parent = getpid();
                pid_t script_pid = fork();
                if (script_pid == -1) {
                    std::cerr << "Failed to fork()" << std::endl;
                    status = -1;
                    exit(EXIT_FAILURE);
                } else if (script_pid > 0) {
                    // We are parent process
                    waitpid(script_pid, &status, 0);
                } else {

                    execvp(script_program_name.c_str(), const_cast<char *const *>(script_arg_for_c.data()));
                    cerr << "Parent: Failed to execute " << script_program_name << " \n\tCode: " << errno << endl;
                    exit(EXIT_FAILURE);   // exec never returns
                }
            }
        }
    } else {

        pid_t pid = fork();
        if (pid == -1) {
            std::cerr << "Failed to fork()" << std::endl;
            status = -1;
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            // We are parent process
            waitpid(pid, &status, 0);
        } else {
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
    std::size_t found = comm.find_first_of("#");
    if (found != std::string::npos) {
        comm = comm.substr(0, found);
    }
    boost::algorithm::trim(comm);
    my_tokenizer tok(comm, escaped_list_separator<char>('\\', ' ', '\"'));
    for (my_tokenizer::iterator beg = tok.begin(); beg != tok.end(); ++beg) {
        tmp.push_back(*beg);
    }
    if (!tmp.empty()) {
        split(arg_with_eq, tmp[0], is_any_of("="));
        if (tmp.size() == 1 && arg_with_eq.size() == 2) {
            // TODO: Створення локальної змінної
        }
        if (starts_with(tmp[0], "./")) {
            args.push_back("myshell");
            args.push_back(tmp[0].substr(2));
        }
        args.push_back(tmp[0]);
        for (size_t i = 1; i < tmp.size(); ++i) {
            if ((!starts_with(tmp[i], "-")) || (starts_with(tmp[i], "--"))) {
                split(arg_with_eq, tmp[i], is_any_of("="));
                bool with_eq = (arg_with_eq.size() == 2);
                p = with_eq ? arg_with_eq[1] : tmp[i];
                dir = p.string().find('/') == 0 ? p.parent_path().string() : ".";
                filename = p.filename().string();
                if (is_wildcard(filename)) {
                    boost::filesystem::directory_iterator end_itr; // Default ctor yields past-the-end
                    bool wildcard_used = false;
                    for (boost::filesystem::directory_iterator iter(dir); iter != end_itr; ++iter) {
                        if (!boost::filesystem::is_regular_file(iter->status())) continue; // Skip if not a file
                        if (fnmatch(filename.c_str(), iter->path().filename().string().c_str(), 0) != 0) continue;
                        string f = iter->path().filename().string();
                        with_eq ? args.push_back(arg_with_eq[0] + "=" + f) : args.push_back(f);
                        wildcard_used = true;
                    }
                    if (!wildcard_used) {
                        // TODO: Якщо таких файлів немає -- повідомити про помилку та не виконувати команду
                        // (merror після цього повинна виводити не нульове значення -- див. далі).
                        break;
                    }
                } else args.push_back(tmp[i]);
            } else args.push_back(tmp[i]);
        }
    }

}


bool is_wildcard(string &s) {
    string p = "?[]()*";
    for (auto c: p) {
        if (s.find(c) != string::npos) return true;
    }
    return false;
}


