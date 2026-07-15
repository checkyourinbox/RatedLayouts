#pragma once
#include <Geode/platform/cplatform.h>

// Various attribute macros

#if defined(GEODE_IS_WINDOWS)
# define RL_NOINLINE __declspec(noinline)
# define RL_ALWAYS_INLINE __forceinline
#else
# define RL_NOINLINE __attribute__((noinline))
# define RL_ALWAYS_INLINE __attribute__((always_inline)) inline
#endif
