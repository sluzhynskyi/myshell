#include <iostream>
#include <cstdio>
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
#include <sys/file.h>

#include <vector>
#include <string>
#include "commands/commands.hpp"

#include <sys/socket.h>
#include <arpa/inet.h>

using std::cout;
using std::cerr;
using std::flush;
using std::endl;
using std::string;
using std::vector;

namespace po = boost::program_options;

using std::perror;
using namespace boost::filesystem;
using namespace boost::algorithm;
using boost::tokenizer;
using boost::escaped_list_separator;
typedef tokenizer<escaped_list_separator<char>> my_tokenizer;

void myshell_execute(int argc, char **argv, int fd);

void parse_commands(vector<string> &comm_args, string &comm, vector<string> &delimiters);

void parse_line(std::vector<string> &args, std::string &comm);

bool is_wildcard(string &s);

void comm_pipe(std::vector<string> &comm_args, vector<string> &delimiters);

int main(int argc, char **argv) {
    try {
        // Declare the supported options.
        po::options_description desc("General options");
        desc.add_options()
                ("help,h", "Show help")
                ("server", "Set shell as remote")
                ("port", po::value<int>(), "Set port number");

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        std::string usage = "Usage:\n  myshell [--server --port <portNumb>] \n";
        if (vm.count("help")) {
            std::cout << usage << desc << std::endl;
            return EXIT_SUCCESS;
        }
        if (vm.count("server")) {
            struct sockaddr_in server{};

            int sd = socket(AF_INET, SOCK_STREAM, 0);
            if (sd == -1) {
                perror("Error: cannot open socket");
            }
            memset(&server, 0, sizeof(server));
            server.sin_family = AF_INET;
            server.sin_addr.s_addr = htonl(INADDR_ANY);
            server.sin_port = htons(vm["port"].as<int>());
            int res = bind(sd, (struct sockaddr *) &server, sizeof(server));
            if (res == -1) {
                perror("Error: cannot bind socket");
            }
            listen(sd, 1);
            int csock, cpid;
            for (;;) {  /* loop, accepting connections */
                if ((csock = accept(sd, NULL, NULL)) == -1) exit(1);
                cpid = fork();
                if (cpid == -1) {
                    cerr << "Failed to fork()" << endl;
                    exit(EXIT_FAILURE);
                } else if (cpid > 0) {
                    // We are parent process
                    close(csock); /* csock is not needed in the parent after the fork */
                } else {
                    dup2(csock, STDOUT_FILENO);  /* duplicate socket on stdout */
                    dup2(csock, STDERR_FILENO);  /* duplicate socket on stderr too */

                    myshell_execute(argc, argv, csock);
                    close(csock);  /* can close the original after it's duplicated */
                }
            }

        } else {
            myshell_execute(argc, argv, 0);
        }
    } catch (const std::exception &ex) {
        std::cerr << ex.what() << std::endl;
    }
    return 0;

}

void myshell_execute(int argc, char **argv, int fd) {
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
        vector<string> comm_args;
        vector<string> delimiters;
        vector<string> main_args(argv + 1, argv + argc);
        if (argc > 1 && std::find(main_args.begin(), main_args.end(), "--server") == main_args.end()) {
            if (!getline(script_input, comm))
                break;
        } else {
            char buff[FILENAME_MAX];
            getcwd(buff, FILENAME_MAX);
            char full_buff[FILENAME_MAX + 2];
            strcpy(full_buff, buff);
            strcat(full_buff, "$ ");
            if (fd == 0) {
                comm = readline(full_buff);
                add_history(comm.c_str());
            } else {
                cout << full_buff << flush;
                char buf[1024];
                int cc = recv(fd, buf, sizeof(buf), 0);
                if (cc == 0) exit(EXIT_SUCCESS);
                buf[cc] = '\0';
                comm = buf;
            }

        }

        parse_commands(comm_args, comm, delimiters);

        if (delimiters.empty()) {
            std::vector<string> args;
            parse_line(args, comm);
            if (!args.empty())
                execute(status, args);
        } else if (comm_args.size() > 1) {
            comm_pipe(comm_args, delimiters);
        }
    }
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

void parse_commands(std::vector<string> &comm_args, std::string &comm, std::vector<string> &delimiters) {
    vector<string> const delims{"|", "<", ">", "2>", "&>", "2>&1"};
    boost::algorithm::trim(comm);
    vector<string> temp_args;
    boost::split(temp_args, comm, boost::is_any_of(" \t"));
    string temp_str;
    for (int i = 0; unsigned(i) < temp_args.size(); i++) {
        if (unsigned(i) == temp_args.size() - 1) {
            if (std::find(delims.begin(), delims.end(), temp_args[i]) != delims.end()) {
                delimiters.push_back(temp_args[i]);

            } else {
                temp_str += temp_args[i];
                comm_args.push_back(temp_str);
            }
        } else if (std::find(delims.begin(), delims.end(), temp_args[i]) != delims.end()) {
            boost::algorithm::trim(temp_str);
            comm_args.push_back(temp_str);
            delimiters.push_back(temp_args[i]);
            temp_str = "";
        } else {
            temp_str += temp_args[i];
            temp_str += " ";
        }
    }

    if (comm_args[comm_args.size() - 1].find('&') != string::npos && comm_args[comm_args.size() - 1] != "&") {
        vector<string> temp_vec;
        boost::split(temp_vec, comm_args[comm_args.size() - 1], boost::is_any_of("&"));
        comm_args[comm_args.size() - 1] = temp_vec[0];
        comm_args.emplace_back("&");
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

void comm_pipe(vector<string> &comm_args, vector<string> &delimiters) {

    const int comm_count = comm_args[comm_args.size() - 1] == "&" ? comm_args.size() - 1 : comm_args.size();

    const int pipe_count = comm_count - 1;

    int **pfd = new int *[pipe_count];
    for (int i = 0; i < pipe_count; ++i)
        pfd[i] = new int[2];


    for (int j = 0; j < pipe_count; j++) {
        if (pipe(pfd[j]) == -1) {
            cerr << "Error: Failed to create pipe" << endl;
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < pipe_count + 1; i++) {
        std::vector<string> args;
        int status;
        switch (fork()) {

            case -1:
                cerr << "Error: Failed to fork process" << endl;
                exit(EXIT_FAILURE);
            case 0:
                if (!(i == pipe_count && delimiters[i - 1] != "|")) {
                    if (i != 0) {
                        if (close(pfd[i - 1][1]) == -1) {
                            cerr << "Error: Failed to close odd file descriptor" << endl;
                            exit(EXIT_FAILURE);
                        }
                        if (pfd[i - 1][0] != STDIN_FILENO) {
                            if (dup2(pfd[i - 1][0], STDIN_FILENO) == -1) {
                                perror("Failed to duplicate file descriptor");
                                cerr << "Error: Failed to duplicate file descriptor" << endl;
                                exit(EXIT_FAILURE);
                            }
                        }
                        if (close(pfd[i - 1][0]) == -1) {
                            cerr << "Error: Failed to close odd file descriptor" << endl;
                            exit(EXIT_FAILURE);
                        }
                    }

                    if (i != pipe_count) {
                        if (delimiters[i] == "|") {
                            if (close(pfd[i][0]) == -1) {
                                cerr << "Error: Failed to close odd file descriptor" << endl;
                                exit(EXIT_FAILURE);
                            }
                            if (pfd[i][1] != STDOUT_FILENO) {
                                if (dup2(pfd[i][1], STDOUT_FILENO) == -1) {
                                    cerr << "Error: Failed to duplicate file descriptor" << endl;
                                    exit(EXIT_FAILURE);
                                }
                            }
                            if (close(pfd[i][1]) == -1) {
                                cerr << "Error: Failed to close odd file descriptor" << endl;
                                exit(EXIT_FAILURE);
                            }
                        } else {
                            int fd = open(comm_args[i + 1].c_str(), O_RDWR);
                            if (fd == -1) {
                                cerr << "Error: Failed to open file" << endl;
                                exit(EXIT_FAILURE);
                            }
                            if (delimiters[i] == "<") {
                                if (fd != STDIN_FILENO) {
                                    if (dup2(fd, STDIN_FILENO) == -1) {
                                        cerr << "Error: Failed to duplicate file descriptor" << endl;
                                        exit(EXIT_FAILURE);
                                    }
                                }
                            }
                            if (delimiters[i] == ">" || delimiters[i] == "&>") {
                                if (fd != STDOUT_FILENO) {
                                    if (dup2(fd, STDOUT_FILENO) == -1) {
                                        cerr << "Error: Failed to duplicate file descriptor" << endl;
                                        exit(EXIT_FAILURE);
                                    }
                                }
                            }
                            if (delimiters[i] == "2>" || delimiters[i] == "&>" ||
                                (delimiters[i] == ">" && unsigned(i + 1) == delimiters.size() - 1 &&
                                 delimiters[i + 1] == "2>&1")) {
                                if (fd != STDERR_FILENO) {
                                    if (dup2(fd, STDERR_FILENO) == -1) {
                                        cerr << "Error: Failed to duplicate file descriptor" << endl;
                                        exit(EXIT_FAILURE);
                                    }
                                }
                            }

                            close(fd);
                        }
                    }

                    parse_line(args, comm_args[i]);
                    if (!args.empty())
                        execute(status, args);
                }
                exit(0);
            default:
                if (i != 0) {
                    if (close(pfd[i - 1][0]) == -1) {
                        cerr << "Error: Failed to close odd file descriptor on parent process" << endl;
                        exit(EXIT_FAILURE);
                    }
                    if (close(pfd[i - 1][1]) == -1) {
                        cerr << "Error: Failed to close odd file descriptor on parent process" << endl;
                        exit(EXIT_FAILURE);
                    }
                }
                break;
        }
    }

    if (comm_args[comm_args.size() - 1] != "&") {
        for (int i = 0; i < pipe_count + 1; i++) {
            signal(SIGCHLD, SIG_IGN);
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
