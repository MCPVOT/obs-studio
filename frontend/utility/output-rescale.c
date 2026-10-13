#include "output-rescale.h"

void output_rescale_filter_changed(const struct output_rescale_ops *ops)
{
	ops->set_res_enabled(ops->param, ops->rescales(ops->param));
}

void output_rescale_set_locked(const struct output_rescale_ops *ops, bool locked)
{
	ops->set_filter_enabled(ops->param, !locked);
	ops->set_res_enabled(ops->param, !locked);
}
