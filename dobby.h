#pragma once
#ifdef __cplusplus
extern "C" {
#endif
int DobbyHook(void *address, void *replace, void **origin);
int DobbyDestroy(void *address);
#ifdef __cplusplus
}
#endif
