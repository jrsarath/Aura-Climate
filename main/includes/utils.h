#pragma once

#define UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

void AbortAppOnFailure(bool condition, void (*action)(void))

#ifdef __cplusplus
}
#endif