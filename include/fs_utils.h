#pragma once

#include <string>

#define PATH_DIR_SEP '/'
#define PATH_EXT_SEP '.'

static void ReplaceExtension(std::string* path, const char* new_extension) {
    auto last_dir_sep_pos = path->find_last_of(PATH_DIR_SEP);
    auto ext_sep_pos = path->find_last_of(PATH_EXT_SEP);
    if (last_dir_sep_pos != std::string::npos && ext_sep_pos != std::string::npos && ext_sep_pos < last_dir_sep_pos) {
        ext_sep_pos = std::string::npos;
    }

    if (ext_sep_pos != std::string::npos) {
        path->resize(ext_sep_pos);
    }

    path->push_back(PATH_EXT_SEP);
    *path += new_extension;
}
