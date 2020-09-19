#include <iostream>
#include <boost/program_options.hpp>
#include <readline/readline.h>
#include <readline/history.h>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>
#include <boost/algorithm/string.hpp>

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;
extern char **environ;

int main(int argc, char **argv) {
    char *comm;
    auto path_ptr = getenv("PATH");
    string path_var;
    if (path_ptr != nullptr)
        path_var = path_ptr;
    path_var += ":.";
    setenv("PATH", path_var.c_str(), 1);
    do {
        comm = readline("myshell$ ");
        add_history(comm);
        std::vector<string> args;
        boost::split(args, comm, boost::is_any_of(" "));
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
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            // We are parent process
            int status;
            waitpid(pid, &status, 0);
        } else {
            // We are the child
            execvp(victim_name.c_str(), const_cast<char *const *>(arg_for_c.data()));
            cerr << "Parent: Failed to execute " << comm << " \n\tCode: " << errno << endl;
            exit(EXIT_FAILURE);   // exec never returns
        }
    } while (comm != nullptr);
    return 0;
//    ps = parse(comm);

}
