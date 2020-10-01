#include "commands/commands.hpp"

int merrno(int &status) {

    cout << status << endl;
    return 0;

}

int mpwd() {

    char buff[FILENAME_MAX];
    getcwd(buff, FILENAME_MAX);
    cout << buff << endl;
    return 0;

}


int mcd(const char *path) {

    char *full_path = realpath(path, NULL);;
    int status = chdir(full_path);
    free(full_path);
    if (status != 0) {
        cout << "Error: no such directory" << endl;
    }
    return status;

}


int mexit(int status) {

    exit(status);

}


int mecho(vector<string> texts) {

    for (size_t i = 1; i < texts.size(); i++) {
        if (texts[i].find("$") != string::npos) {
            vector<string> env_var_arg_parts;
            boost::split(env_var_arg_parts, texts[i], boost::is_any_of("$"));
            string env_var_name = env_var_arg_parts[1];
            cout << env_var_arg_parts[0] << flush;
            char *env_var_value = getenv(env_var_name.c_str());
            if (env_var_value != 0) {
                cout << env_var_value << " " << flush;
            }
        } else {
            cout << texts[i] << " " << flush;
        }
    }
    cout << endl;
    return 0;

}

int mexport(string varname, string value) {

    int status = setenv(varname.c_str(), value.c_str(), 1);
    return status;

}

