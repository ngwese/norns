#pragma once

#include "event_types.h"

// number of bytes in waveform data blob
#define EVENT_WAVE_DISPLAY_BYTES 128

extern bool quit;

typedef void (event_handler_t)(union event_data *ev);

extern void events_init(void);
extern void event_set_handler(event_handler_t *h);
extern void event_loop(void);
extern union event_data *event_data_new(event_t evcode, free_event_data_t cb);
extern void event_data_free(union event_data *ev);
extern void event_post(union event_data *ev);
