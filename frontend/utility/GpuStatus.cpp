#include "GpuStatus.hpp"

#ifdef OBS_GPU_STATUS
#include "obs-gpu-status.h"
#endif

#include <obs.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace GpuStatus {

bool Available()
{
#ifdef OBS_GPU_STATUS
	return true;
#else
	return false;
#endif
}

#ifdef OBS_GPU_STATUS
static Info Parse(int state, const char *json)
{
	Info info;
	info.state = state == 0 ? State::Green : state == 1 ? State::Yellow : State::Red;
	info.json = json ? json : "null";

	QJsonObject obj = QJsonDocument::fromJson(QByteArray(info.json.c_str())).object();
	info.adapter = obj["adapter"].toString();
	info.summary = obj["summary"].toString();
	for (const QJsonValue &cause : obj["causes"].toArray()) {
		info.causes << cause.toString();
	}
	for (const QJsonValue &value : obj["detectedGpus"].toArray()) {
		QJsonObject gpu = value.toObject();
		QString name = gpu["name"].toString();
		if (name.isEmpty()) {
			name = gpu["vendor"].toString().toUpper() + " " + gpu["deviceId"].toString();
		}
		if (!gpu["renderNode"].toBool()) {
			name += " (no render node)";
		}
		info.gpus << name;
	}
	return info;
}
#endif

Info Query()
{
#ifdef OBS_GPU_STATUS
	std::string adapter;
	bool haveAdapter = false;
	obs_enter_graphics();
	if (const char *name = gs_get_device_name()) {
		adapter = name;
		haveAdapter = true;
	}
	obs_leave_graphics();

	/* libobs keeps an average, not a median; close enough for a light. */
	uint64_t interval = obs_get_frame_interval_ns();
	uint64_t frame = obs_get_average_frame_time_ns();
	if (frame == 0) {
		interval = 0; /* no frames rendered yet */
	}

	int state = 2;
	char *json = obs_gpu_status_report(haveAdapter ? adapter.c_str() : nullptr, frame, interval, &state);
	Info info = Parse(state, json);
	obs_gpu_status_free(json);
	return info;
#else
	return Info();
#endif
}

#ifdef OBS_GPU_STATUS
static void GetGpuStatusProc(void *, calldata_t *cd)
{
	Info info = Query();
	calldata_set_string(cd, "json", info.json.c_str());
}
#endif

void Init()
{
#ifdef OBS_GPU_STATUS
	static bool registered = false;
	if (!registered) {
		proc_handler_add(obs_get_proc_handler(), "void get_gpu_status(out string json)", GetGpuStatusProc,
				 nullptr);
		registered = true;
	}

	Info info = Query();
	if (info.state == State::Green) {
		blog(LOG_INFO, "GPU acceleration: green: %s", info.summary.toUtf8().constData());
		return;
	}

	const char *color = info.state == State::Yellow ? "YELLOW" : "RED";
	blog(LOG_WARNING, "==== GPU acceleration: %s ====", color);
	blog(LOG_WARNING, "%s", info.summary.toUtf8().constData());
	for (const QString &gpu : info.gpus) {
		blog(LOG_WARNING, "  detected GPU: %s", gpu.toUtf8().constData());
	}
	for (const QString &cause : info.causes) {
		blog(LOG_WARNING, "  possible cause: %s", cause.toUtf8().constData());
	}
#endif
}

} // namespace GpuStatus
