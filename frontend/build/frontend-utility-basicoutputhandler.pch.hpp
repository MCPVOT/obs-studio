/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.utility-basicoutputhandler. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* utility/BasicOutputHandler.cpp */
#include "frontend/utility/BasicOutputHandler.hpp"
#include "frontend/utility/AdvancedOutput.hpp"
#include "frontend/utility/SimpleOutput.hpp"

#include <utility/MultitrackVideoError.hpp>
#include <utility/output-filename-reserve.h>
#include <utility/StartMultiTrackVideoStreamingGuard.hpp>
#include <utility/VCamConfig.hpp>
#include <widgets/OBSBasic.hpp>

#include <qt-wrappers.hpp>

#include <QThreadPool>
