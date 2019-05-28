#include <object.h>
#include <oosf/input_data_stream.h>
#include <jit_compiler.h>
#include <cstdio>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::fprintf(stderr, "Usage: %s <executable>\n", argv[0]);
        return 1;
    }

    Object executable;
    JITCompiler jit;

    std::FILE* file = std::fopen(argv[1], "rb");
    if (file == nullptr) {
        std::fprintf(stderr, "Failed to open %s\n", argv[1]);
        return 1;
    }

    InputDataStream dstream(file);
    Object::RegisterIn(&dstream);
    ReadStatus read_status = dstream.TryRead(&executable);
    std::fclose(file);

    if (read_status != kStatusOk) {
        std::fprintf(stderr, "Failed to read %s\n", argv[1]);
        return 1;
    }

    if (executable.object_type != Object::kObjectExecutable) {
        std::fprintf(stderr, "Failed to execute %s: object file is not executable\n", argv[1]);
        return 1;
    }

    const Object::ProcVersion& required_version = jit.GetProcessorVersion();
    if (!executable.proc_version.CompatibleWith(required_version)) {
        std::fprintf(stderr, "Failed to execute %s: incompatible processor version (required >=%d.0.0, found %d.%d.%d)\n",
                argv[1], required_version.major, executable.proc_version.major, executable.proc_version.minor, executable.proc_version.patch);
        return 1;
    }

    jit.Compile(executable);
    jit.Execute();

    return 0;
}
