#pragma once

typedef enum {
    ST_INIT,
    ST_CONNECTING,
    ST_READING,
    ST_PUBLISHING,
    ST_ERROR
} device_state_t;
