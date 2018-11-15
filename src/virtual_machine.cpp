#include <virtual_machine.h>

const Object::ProcVersion& VirtualMachine::GetProcessorVersion() const {
    return processor_.GetVersion();
}

void VirtualMachine::Execute(const Object& obj) {
    bool ok = processor_.Execute(obj.bytecode, &ram_);
    if (!ok) {
        processor_.Dump();
        processor_.PrintStackTrace(obj);
    }
}


