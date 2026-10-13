#include "saved-transition.h"

#include <stddef.h>

void *saved_transition_find(const struct saved_transition_ops *ops, const char *name, const char *id)
{
	void *transition = NULL;

	(void)id;

	if (name && *name)
		transition = ops->find_by_name(ops->param, name);
	return transition ? transition : ops->fallback;
}
