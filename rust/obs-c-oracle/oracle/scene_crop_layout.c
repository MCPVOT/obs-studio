/* Measure the real obs.h ABI, not the lean PCH's private duplicate. */
#include <stddef.h>
#include "obs.h"
size_t oracle_scene_crop_layout(unsigned field);
size_t oracle_scene_crop_layout(unsigned field)
{
	switch (field) {
	case 0:
		return sizeof(struct obs_sceneitem_crop);
	case 1:
		return _Alignof(struct obs_sceneitem_crop);
	case 2:
		return offsetof(struct obs_sceneitem_crop, left);
	case 3:
		return offsetof(struct obs_sceneitem_crop, top);
	case 4:
		return offsetof(struct obs_sceneitem_crop, right);
	case 5:
		return offsetof(struct obs_sceneitem_crop, bottom);
	default:
		return (size_t)-1;
	}
}
