#include <object.h>
#include <oosf/output_data_stream.h>
#include <oosf/input_data_stream.h>

void Object::WriteValue(OutputDataStream* out) const {
    out->Write(bytecode);
    out->WriteAsMap(defined_labels.begin(), defined_labels.end(), defined_labels.size());
    out->WriteAsMap(required_labels.begin(), required_labels.end(), required_labels.size());
    out->WriteMinimal(bss_size);
}

ReadStatus Object::TryRead(InputDataStream* in) {
    ReadStatus read_status = kStatusOk;
    if (
        ((read_status = in->TryRead(&bytecode)) == kStatusOk) &&
        ((read_status = in->TryRead(&defined_labels)) == kStatusOk) &&
        ((read_status = in->TryRead(&required_labels)) == kStatusOk) &&
        ((read_status = in->TryReadMinimal(&bss_size)) == kStatusOk)
    ) {}
    return read_status;
}
