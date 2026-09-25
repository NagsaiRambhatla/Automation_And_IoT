#pragma once

#define BLYNK_TEMPLATE_ID "TMPL67Jur1hEj"
#define BLYNK_TEMPLATE_NAME "3707ICT Smart Room"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

// Periodic batches use about 86,400 messages per 30 days of continuous running.
// Actuator, occupancy, target and system-status changes add extra batches.
// Local automation runs independently of this cloud reporting interval.
constexpr unsigned long CLOUD_UPLOAD_INTERVAL_MS = 30000;
constexpr unsigned long CLOUD_CONTROL_INTERVAL_MS = 5000;
