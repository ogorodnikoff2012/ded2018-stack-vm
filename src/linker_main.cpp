#include <cstdio>
#include <object.h>
#include <vector>
#include <fstream>
#include <oosf/output_data_stream.h>
#include <linker.h>
#include <instruction_set.h>

int main(int argc, char* argv[]) {
    std::vector<Object> objects;
    if (!TryLoadObject(&objects, "_start.vobj")) {
        std::fprintf(stderr, "FATAL ERROR cannot load _start.vobj\n");
        return 1;
    }
    for (int i = 1; i < argc; ++i) {
        if (!TryLoadObject(&objects, argv[i])) {
            std::fprintf(stderr, "ERROR cannot load %s\n", argv[i]);
            return 1;
        }
        if (objects.back().object_type != Object::kObjectStaticLinkable) {
            std::fprintf(stderr, "ERROR file %s is not static linkable\n", argv[i]);
            return 1;
        }
    }

    Object executable;
    executable.proc_version.major = PROC_VERSION_MAJOR;
    executable.proc_version.minor = PROC_VERSION_MINOR;
    executable.proc_version.patch = PROC_VERSION_PATCH;
    executable.object_type = Object::kObjectExecutable;

    std::string error;
    if (!TryLink(objects, &executable, &error)) {
        std::fprintf(stderr, "ERROR %s\n", error.c_str());
        return 1;
    }

    std::ofstream out("a.vexe", std::ios_base::binary);
    OutputDataStream dstream(&out);
    Object::RegisterIn(&dstream);
    dstream.Write(executable);
    out.close();

    return 0;
}
