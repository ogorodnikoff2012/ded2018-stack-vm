#include <object.h>
#include <oosf/input_data_stream.h>
#include <cstdio>

int main(int argc, char* argv[]) {
    if (argc == 1) {
        fprintf(stderr, "Warning: no input files specified\n");
    }
    for (int i = 1; i < argc; ++i) {
        std::FILE* file = std::fopen(argv[i], "rb");
        Object obj;
        InputDataStream dstream(file);
        dstream.RegisterClass<Object>(Object::kTypeName);
        if (dstream.TryRead(&obj) == kStatusOk) {
            printf("Success!\n");
        } else {
            printf("Failure!\n");
        }

        std::fclose(file);
    }
    return 0;
}
