#include "autodrop_gcc.h"
#include "global_lock.h"

// Comment out the next line and replace 'SCOPED_GLOBAL_LOCK_FUNC' with a name of your choice.
// #define SCOPED_GLOBAL_LOCK_FUNC() AUTODROP(AUTO_LOCK_FUNC_RENAME(GLOBAL_UNLOCK_FUNC_NAME)) int _lock_guard = GLOBAL_LOCK_FUNC_NAME();

#define _AUTO_LOCK_FUNC_RENAME(name) auto_##name
#define AUTO_LOCK_FUNC_RENAME(name) _AUTO_LOCK_FUNC_RENAME(name)

static inline void AUTO_LOCK_FUNC_RENAME(GLOBAL_UNLOCK_FUNC_NAME)(int* dummy) {
	(void)dummy;
	GLOBAL_UNLOCK_FUNC_NAME();
}
