/*
 * osc.h
 *
 * user OSC device, send/receive arbitrary OSC within lua scripts
 *
 */

#pragma once

#include "osc_types.h"

extern void osc_init();
extern void osc_deinit();

extern void osc_send(const char *, const char *, const char *, osc_message);
extern void osc_send_crone(const char *, osc_message);
