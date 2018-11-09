#pragma once

#include <object.h>
#include <vector>

bool TryLoadObject(std::vector<Object>* obj, const char* filename);
bool TryLink(const std::vector<Object>& objects, Object* executable, std::string* error);
