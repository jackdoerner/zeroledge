#ifndef ZLUTIL_H
#define ZLUTIL_H

#include "zeroledge.h"
#include <fstream>

#define TAG_VALID   "\e[32m[VALID]     \e[0m"
#define TAG_INVALID "\e[31m[INVALID]   \e[0m"
#define TAG_WORKING "\e[33m[WORKING]   \e[0m"
#define TAG_DONE          "[DONE]      "
#define TAG_FAIL    "\e[31m[FAILURE]   \e[0m"
#define TAG_ERASE         "\b\b\b\b\b\b\b\b\b\b\b\b"

#define SECTION_SEPARATOR "===================="
#define SUBSECTION_SEPARATOR "--------------------"
#define ENTRIES_EXPORT_FIELD_SEPARATOR ' '

unsigned int fetchRandomSeed();

Big zlhash(const char* data, int bytes);

#endif