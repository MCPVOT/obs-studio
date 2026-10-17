/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.utility, frontend.utility-2, frontend.utility-3. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* utility/AdvancedOutput.cpp */
#include "frontend/utility/AdvancedOutput.hpp"

#include <utility/audio-encoders.hpp>
#include <utility/replay-buffer-naming.h>
#include <utility/service-encoder-settings.h>
#include <utility/StartMultiTrackVideoStreamingGuard.hpp>
#include <widgets/OBSBasic.hpp>

#include <qt-wrappers.hpp>

/* utility/CrashHandler.cpp */
#include "frontend/utility/CrashHandler.hpp"
#include "frontend/utility/crash-sentinel-dir.hpp"
#include <OBSApp.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>

/* utility/FFmpegCodec.cpp */
#include "frontend/utility/FFmpegCodec.hpp"
#include "frontend/utility/FFmpegFormat.hpp"

#include <unordered_map>
#include <unordered_set>

/* utility/GoLiveAPI_CensoredJson.cpp */
#include "frontend/utility/GoLiveAPI_CensoredJson.hpp"

/* utility/GoLiveAPI_Network.cpp */
#include "frontend/utility/GoLiveAPI_Network.hpp"

#include <utility/MultitrackVideoError.hpp>
#include <utility/RemoteTextThread.hpp>

#include <obs.hpp>

#include <QMessageBox>
#include <QThreadPool>
#include <qstring.h>

#include <string>

/* utility/GoLiveAPI_PostData.cpp */
#include "frontend/utility/GoLiveAPI_PostData.hpp"
#include "frontend/utility/models/multitrack-video.hpp"

#include <utility/system-info.hpp>

/* utility/MissingFilesModel.cpp */
#include "frontend/utility/MissingFilesModel.hpp"

#include <QFileInfo>

/* utility/MissingFilesPathItemDelegate.cpp */
#include "frontend/utility/MissingFilesPathItemDelegate.hpp"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QToolButton>

/* utility/MultitrackVideoError.cpp */
#include "frontend/utility/MultitrackVideoError.hpp"

#include <QPushButton>

/* utility/MultitrackVideoOutput.cpp */
#include "frontend/utility/MultitrackVideoOutput.hpp"
#include "frontend/utility/multitrack-encoder-override.h"

#include <bpm.h>
#include <util/dstr.hpp>
#include <libavformat/avformat.h>

#include <QThread>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <cinttypes>

/* utility/OBSCanvas.cpp */
#include "frontend/utility/OBSCanvas.hpp"

#include <utility>

/* utility/OBSProxyStyle.cpp */
#include "frontend/utility/OBSProxyStyle.hpp"

/* utility/OBSTranslator.cpp */
#include "frontend/utility/OBSTranslator.hpp"

/* utility/PreviewProgramSizeObserver.cpp */
#include "frontend/utility/PreviewProgramSizeObserver.hpp"

#include <QEvent>
#include <QLayout>
#include <QResizeEvent>
#include <QTimer>

#include <util/base.h>

/* utility/QuickTransition.cpp */
#include "frontend/utility/QuickTransition.hpp"

/* utility/RemoteTextThread.cpp */
#include "frontend/utility/RemoteTextThread.hpp"

#include <util/curl/curl-helper.h>

#include <iterator>

/* utility/RemuxEntryPathItemDelegate.cpp */
#include "frontend/utility/RemuxEntryPathItemDelegate.hpp"
#include "frontend/utility/RemuxQueueModel.hpp"

/* utility/RemuxQueueModel.cpp */

#include <QDir>
#include <QStyle>

/* utility/RemuxWorker.cpp */
#include "frontend/utility/RemuxWorker.hpp"

#include <media-io/media-remux.h>

/* utility/SceneRenameDelegate.cpp */
#include "frontend/utility/SceneRenameDelegate.hpp"

#include <QKeyEvent>

/* utility/SimpleOutput.cpp */
#include "frontend/utility/SimpleOutput.hpp"

/* utility/ThumbnailItem.cpp */
#include "frontend/utility/ThumbnailItem.hpp"

#include <utility/ScreenshotObj.hpp>
#include <utility/ThumbnailView.hpp>

#include <QIcon>
#include <QPainter>
#include <QPixmap>

/* utility/ThumbnailManager.cpp */
#include "frontend/utility/ThumbnailManager.hpp"

#include "frontend/utility/display-helpers.hpp"

#include <QImageWriter>

/* utility/ThumbnailView.cpp */
#include "frontend/utility/ThumbnailView.hpp"

/* utility/audio-encoders.cpp */
#include "frontend/utility/audio-encoders.hpp"

#include <widgets/OBSMainWindow.hpp>

#include <mutex>

/* utility/item-widget-helpers.cpp */
#include "frontend/utility/item-widget-helpers.hpp"

#include <QListWidget>

/* utility/undo_stack.cpp */
#include "frontend/utility/undo_stack.hpp"
