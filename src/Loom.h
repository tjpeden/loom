// ---------------------------------------------------------------------------
// Loom — Main Include
// ---------------------------------------------------------------------------
// Users include only this header:
//
//   #include <Loom.h>
//
// It pulls in all framework types and declares the global `Loom` instance.
// ---------------------------------------------------------------------------

#ifndef LOOM_H
#define LOOM_H

#include "Platform.h"
#include "Event.h"
#include "Actor.h"
#include "Bus.h"
#include "LoomCore.h"

// ---------------------------------------------------------------------------
// Global Loom instance — declared here, defined in LoomCore.cpp
// ---------------------------------------------------------------------------

extern LoomCore Loom;

#endif // LOOM_H
