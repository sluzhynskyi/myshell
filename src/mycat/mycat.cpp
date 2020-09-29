#include <iostream>
#include <boost/program_options.hpp>
#include "operations_with_files/operations_with_files.hpp"
#include <sys/stat.h>        /* for stat */
#include <sys/file.h> // for flags

void cout_error(const std::string &f, const int *st);

namespace po = boost::program_options;


int main(int argc, char **argv) {
    std::vector<std::string> files;
    int a_flag = 0;
    try {
        po::options_description visible("General options");
        visible.add_options()
                ("help,h", "Show help")
                ("show-all,A",
                 "Show all invisible (except whitespaces) in hexadecimal form");

        po::options_description hidden("Hidden options");
        hidden.add_options()
                ("files", po::value<std::vector<std::string>>(), "files");
        po::positional_options_description p;
        p.add("files", -1);

        po::options_description all("All options");
        all.add(visible).add(hidden);

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(all).positional(p).run(), vm);
        po::notify(vm);

        std::string usage = "Usage:\n  mycat [-h|--help] [-A|--show-all] [--files] <file1> <file2> ... <fileN> \n";
        if (vm.count("help")) {
            std::cout << usage << visible << std::endl;
            return EXIT_SUCCESS;
        }
        if (vm.count("show-all")) {
            a_flag = 1;
        }
        if (vm.count("files")) {
            files = vm["files"].as<std::vector<std::string> >();
        } else {
            std::cout << usage << std::endl;
            return EXIT_FAILURE;
        }
    } catch (const std::exception &ex) {
        std::cerr << ex.what() << std::endl;
    }

    int fOut;
    int status;
    struct stat stat_buff{};
    std::vector<int> opened_files;
    for (size_t i = 0; i < files.size(); ++i) {
        fOut = openfile(files[i].c_str(), O_RDONLY, &status);
        if (fOut == -1) {
            cout_error(files[i], &status);
            return EXIT_FAILURE;
        }
        opened_files.push_back(fOut);
    }
    for (size_t i = 0; i < files.size(); ++i) {
        stat(files[i].c_str(), &stat_buff);        /* 1 syscall */
        size_t buffer_size = stat_buff.st_size;
        char *buffer = new char[buffer_size + 1];
        fOut = readbuffer(opened_files[i], buffer, buffer_size, &status);
        if (fOut == -1) {
            cout_error(files[i], &status);
            return EXIT_FAILURE;
        }
        fOut = closefile(opened_files[i], &status);
        if (fOut == -1) {
            cout_error(files[i], &status);
            return EXIT_FAILURE;
        }
        if (a_flag == 1) {
            std::string f;
            char c[5];
            for (size_t j = 0; j < buffer_size; ++j) {
                if (!(isprint(buffer[j])) && !(isspace(buffer[j]))) {
                    sprintf(c, "\\x%02X", static_cast<unsigned char>(buffer[j]));
                    f += std::string(c);
                } else { f += buffer[j]; }
            }
            delete[] buffer;
            buffer = strcpy(new char[f.size() + 1], f.c_str());
            buffer_size = f.size();
        }
        fOut = writebuffer(1, buffer, buffer_size, &status);
        if (fOut == -1) {
            cout_error(files[i], &status);
            return EXIT_FAILURE;
        }
        delete[] buffer;
    }


    return EXIT_SUCCESS;
}


void cout_error(const std::string &f, const int *st) {
    std::string e = strerror(*st);
    std::cerr << "'" << f << "': " << e << std::endl;
}