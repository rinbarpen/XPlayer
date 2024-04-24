#pragma once

#include <cassert>

#define ASSERT(cond) assert(cond)
#define CHECK(cond, msg) static_assert(cond, msg)

