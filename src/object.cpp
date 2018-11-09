#include <object.h>
#include <oosf/output_data_stream.h>
#include <oosf/input_data_stream.h>

Symbol::Symbol(int64_t pos, SymbolType t) : position(pos), type(t) {}

bool Object::ProcVersion::CompatibleWith(const Object::ProcVersion& target) const {
    return major == target.major && minor <= target.minor;
}

void Symbol::WriteValue(OutputDataStream* out) const {
    out->WriteMinimal(position);
    out->WriteMinimal(type);
}

ReadStatus Symbol::TryRead(InputDataStream* in) {
    ReadStatus read_status = kStatusOk;
    int64_t type_buf = 0;
    if (
        (read_status = in->TryReadMinimal(&position)) == kStatusOk &&
        (read_status = in->TryReadMinimal(&type_buf)) == kStatusOk
    ) {
        type = static_cast<SymbolType>(type_buf);
    }
    return read_status;
}

void Object::WriteValue(OutputDataStream* out) const {
    out->WriteMinimal(proc_version.major);
    out->WriteMinimal(proc_version.minor);
    out->WriteMinimal(proc_version.patch);
    out->Write(bytecode);
    out->WriteAsMap(defined_symbols.begin(), defined_symbols.end(), defined_symbols.size());
    out->WriteAsMap(required_symbols.begin(), required_symbols.end(), required_symbols.size());
    out->WriteMinimal(bss_size);
}

ReadStatus Object::TryRead(InputDataStream* in) {
    ReadStatus read_status = kStatusOk;
    int64_t major_buf = 0, minor_buf = 0, patch_buf = 0;
    if (
        ((read_status = in->TryReadMinimal(&major_buf)) == kStatusOk) &&
        ((read_status = in->TryReadMinimal(&minor_buf)) == kStatusOk) &&
        ((read_status = in->TryReadMinimal(&patch_buf)) == kStatusOk) &&
        ((read_status = in->TryRead(&bytecode)) == kStatusOk) &&
        ((read_status = in->TryRead(&defined_symbols)) == kStatusOk) &&
        ((read_status = in->TryRead(&required_symbols)) == kStatusOk) &&
        ((read_status = in->TryReadMinimal(&bss_size)) == kStatusOk)
    ) {
        proc_version.major = major_buf;
        proc_version.minor = minor_buf;
        proc_version.patch = patch_buf;
    }
    return read_status;
}
