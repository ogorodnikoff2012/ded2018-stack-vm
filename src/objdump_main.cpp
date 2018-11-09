#include <object.h>
#include <oosf/input_data_stream.h>
#include <cstdio>
#include <objdump.h>

int main(int argc, char* argv[]) {
    if (argc == 1) {
        std::fprintf(stderr, "Warning: no input files specified\n");
    }
    for (int i = 1; i < argc; ++i) {
        if (argc > 2) {
            std::printf("File %s\n", argv[i]);
        }
        std::FILE* file = std::fopen(argv[i], "rb");
        Object obj;
        InputDataStream dstream(file);
        Object::RegisterIn(&dstream);
        if (dstream.TryRead(&obj) == kStatusOk) {
            PrintObject(obj);
        } else {
            std::printf("Failed to parse %s\n", argv[i]);
        }

        std::fclose(file);
    }
    return 0;
}
