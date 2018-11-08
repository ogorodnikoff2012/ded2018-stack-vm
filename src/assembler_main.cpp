#include <fstream>
#include <iostream>
#include <string>
#include <fs_utils.h>
#include <assembler.h>

int main(int argc, char* argv[]) {
    if (argc == 1) {
        std::cerr << "Warning! No input files specified" << std::endl;
    }

    for (int i = 1; i < argc; ++i) {
        std::string in_path(argv[i]);
        std::ifstream in(in_path);

        auto out_path = in_path;
        ReplaceExtension(&out_path, "vobj");
        std::ofstream out(out_path, std::ios_base::binary);

        if (!in.good()) {
            std::cerr << "Failed to open " << in_path << ", skipping..." << std::endl;
            continue;
        }

        if (!out.good()) {
            std::cerr << "Failed to open " << out_path << ", skipping..." << std::endl;
            continue;
        }
        Assemble(&in, &out);
    }
    return 0;
}
