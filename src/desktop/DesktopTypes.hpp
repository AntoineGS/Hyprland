#pragma once
#include "../helpers/memory/Memory.hpp"

class CWorkspace;
class CMonitor;
class CMonitorGroup;

namespace Desktop::View {
    class CWindow;
    class CLayerSurface;
}

/* Shared pointer to a workspace */
using PHLWORKSPACE = SP<CWorkspace>;
/* Weak pointer to a workspace */
using PHLWORKSPACEREF = WP<CWorkspace>;

/* Shared pointer to a window */
using PHLWINDOW = SP<Desktop::View::CWindow>;
/* Weak pointer to a window */
using PHLWINDOWREF = WP<Desktop::View::CWindow>;

/* Shared pointer to a layer surface */
using PHLLS = SP<Desktop::View::CLayerSurface>;
/* Weak pointer to a layer surface */
using PHLLSREF = WP<Desktop::View::CLayerSurface>;

/* Shared pointer to a monitor */
using PHLMONITOR = SP<CMonitor>;
/* Weak pointer to a monitor */
using PHLMONITORREF = WP<CMonitor>;

/* Shared pointer to a monitor group */
using PHLMONITORGROUP = SP<CMonitorGroup>;
/* Weak pointer to a monitor group */
using PHLMONITORGROUPREF = WP<CMonitorGroup>;
