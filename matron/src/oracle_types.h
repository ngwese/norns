#pragma once
#include <stdarg.h>
#include <stdbool.h>

// enumerate types of polls
typedef enum {
    POLL_TYPE_VALUE, // returns a single float value
    POLL_TYPE_DATA   // returns an arbitrarily-size binary data blob
} poll_type_t;

// data structure for engine command descriptor/header
struct engine_command {
    char *name;   // name string
    char *format; // format string
};

// data structure for engine poll descriptor/headerx
struct engine_poll {
    char *name;       // name string
    poll_type_t type; // value or data
};

// data structure for engine parameters
struct engine_param {
    char *name;
    int busIdx; // control bus index
};
