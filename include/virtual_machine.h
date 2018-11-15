#pragma once

#include <processor.h>
#include <ram.h>
#include <object.h>

class VirtualMachine {
public:
    const Object::ProcVersion& GetProcessorVersion() const;
    void Execute(const Object& obj);
private:
    Processor processor_;
    RAM ram_;
};
