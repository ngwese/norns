#include <alsa/asoundlib.h>
#include <pthread.h>
#include <stdio.h>

#include "../events.h"

#include "../clocks/clock_midi.h"
#include "device.h"
#include "device_midi.h"

#define DEV_MIDI_RX_BUFFER_SIZE 512

unsigned int dev_midi_port_count(const char *path) {
    int card;
    int alsa_dev;

    if (sscanf(path, "/dev/snd/midiC%dD%d", &card, &alsa_dev) < 0) {
        // TODO: Insert error message here
        return 0;
    }

    // mostly from amidi.c
    snd_ctl_t *ctl;
    char name[32];

    snd_rawmidi_info_t *info;
    int subs = 0;
    int subs_in = 0;
    int subs_out = 0;

    sprintf(name, "hw:%d", card);
    if (snd_ctl_open(&ctl, name, 0) < 0) {
        // TODO: Insert error message here
        return 0;
    }

    snd_rawmidi_info_alloca(&info);
    snd_rawmidi_info_set_device(info, alsa_dev);

    snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_INPUT);
    if (snd_ctl_rawmidi_info(ctl, info) >= 0) {
        subs_in = snd_rawmidi_info_get_subdevices_count(info);
    }

    snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_OUTPUT);
    if (snd_ctl_rawmidi_info(ctl, info) >= 0) {
        subs_out = snd_rawmidi_info_get_subdevices_count(info);
    }
    snd_ctl_close(ctl);

    subs = subs_in > subs_out ? subs_in : subs_out;
    return subs;
}

int dev_midi_init(void *self, unsigned int port_index, bool multiport_device) {
    struct dev_midi *midi = (struct dev_midi *)self;
    struct dev_common *base = (struct dev_common *)self;

    unsigned int alsa_card;
    unsigned int alsa_dev;
    char *alsa_name;

    snd_rawmidi_params_t *params;

    sscanf(base->path, "/dev/snd/midiC%uD%u", &alsa_card, &alsa_dev);

    if (asprintf(&alsa_name, "hw:%u,%u,%u", alsa_card, alsa_dev, port_index) < 0) {
        fprintf(stderr, "failed to create alsa device name for card %d,%d\n", alsa_card, alsa_dev);
        return -1;
    }

    if (snd_rawmidi_open(&midi->handle_in, &midi->handle_out, alsa_name, 0) < 0) {
        fprintf(stderr, "failed to open alsa device %s\n", alsa_name);
        return -1;
    }

    snd_rawmidi_params_malloc(&params);
    snd_rawmidi_params_current(midi->handle_in, params);
    fprintf(stderr, "rawmidi params for: %s\n", base->name);
    fprintf(stderr, "  avail_min: %u\n", snd_rawmidi_params_get_avail_min(params));
    fprintf(stderr, "  buff_size: %u\n", snd_rawmidi_params_get_buffer_size(params));
    fprintf(stderr, " no_sensing: %u\n", snd_rawmidi_params_get_no_active_sensing(params));
    snd_rawmidi_params_free(params);

    char *name_with_port_index;
    if (multiport_device) {
        if (asprintf(&name_with_port_index, "%s %u", base->name, port_index + 1) < 0) {
            fprintf(stderr, "failed to create human-readable device name for card %d,%d,%d\n", alsa_card, alsa_dev,
                    port_index);
            return -1;
        }
        base->name = name_with_port_index;
    }

    base->start = &dev_midi_start;
    base->deinit = &dev_midi_deinit;

    return 0;
}

void dev_midi_deinit(void *self) {
    struct dev_midi *midi = (struct dev_midi *)self;
    snd_rawmidi_close(midi->handle_in);
    snd_rawmidi_close(midi->handle_out);
}

inline ssize_t dev_midi_post_events(uint8_t *rx_buffer, ssize_t size, uint8_t *status, uint8_t *data_len, struct dev_midi *midi) {
    union event_data *ev;

    uint8_t *byte = rx_buffer;
    uint8_t *end = rx_buffer + size;

    // uint8_t status = 0;
    // uint8_t data_len = 0;
    uint8_t i;

    fprintf(stderr, "buf 0x%p, read %zd begin normalize // head 0x%02x\n", rx_buffer, size, *byte);

    // normalize events, handling running status
    while (byte < end) {
        // system real time
        // if ((*byte >= 0xf8) && (*byte <= 0xff)) {
        if (*byte >= 0xf8) {
            clock_midi_handle_message(*byte++);
            *data_len = 0;
            goto post;
        }

        // channel voice messages
        switch (*byte & 0xf0) {
        case 0x80:
        case 0x90:
        case 0xa0:
        case 0xb0:
        case 0xe0:
            *status = *byte++;
            *data_len = 2;
            goto post;
        case 0xc0:
        case 0xd0:
            *status = *byte++;
            *data_len = 1;
            goto post;
        }

        // channel mode messages
        if (*byte >= 0x79 && *byte <= 0x7f) {
            *status = *byte++;
            *data_len = 1;
            goto post;
        }

        // system common messages
        switch (*byte) {
        case 0xf2:
            *status = *byte++;
            *data_len = 2;
            goto post;
        case 0xf1:
        case 0xf3:
            *status = *byte++;
            *data_len = 1;
            goto post;
        case 0xf6:
            *status = *byte++;
            *data_len = 0;
            goto post;
        }

        // system exclusive, potentially continued from previous rx buffer
        if (*byte == 0xf7 || *status == 0xf7) {
            fprintf(stderr, "eating sysx\n");
            *status = *byte;
            *data_len = 0;
            // consume and discard
            while (*byte != 0xf0 && byte <= end) { ++byte; };
            continue;
        }

        // no status byte, running status applies, prior status and data len
        // applies
        fprintf(stderr, "RMS? byte: 0x%02x -- last s 0x%x n %u\n", *byte, *status, *data_len);
        if (*status == 0) {
            ++byte;
            fprintf(stderr, "corrupt?, not posting");
            continue;
        }

    post:
        ev = event_data_new(EVENT_MIDI_EVENT);
        ev->midi_event.id = midi->dev.id;
        ev->midi_event.nbytes = *data_len + 1;
        ev->midi_event.data[0] = *status;
        for (i = 1; i <= *data_len; i++) {
            ev->midi_event.data[i] = *byte++;
        }
        fprintf(stderr, "POST(%d // 0x%x 0x%x 0x%x)\n", *data_len + 1,
            ev->midi_event.data[0],
            ev->midi_event.data[1],
            ev->midi_event.data[2]
        );
        event_post(ev);
    }

    ssize_t rtn = byte - rx_buffer;
    fprintf(stderr, "buf 0x%p, read %zd, rtn %zd\n", rx_buffer, size, rtn);
    return rtn;
}

static inline bool is_status_byte(uint8_t byte) {
    return (byte & 0xf0) >> 7;
}

static inline bool is_status_real_time(uint8_t byte) {
    return byte >= 0xf8;
}

static inline uint8_t midi_msg_len(uint8_t status) {
    uint8_t upper = status & 0xf0;

    // channel voice messages
    switch (upper) {
    case 0x80:  // note off
    case 0x90:  // note on
    case 0xa0:  // polyphonic key pressure
    case 0xb0:  // control change
    case 0xe0:  // pitch bend
        return 3;
    case 0xc0:  // program change
    case 0xd0:  // channel pressure
        return 2;
    }

    // system real-time messages
    switch (status) {
    case 0xf8:  // timing clock
    case 0xfa:  // start sequence
    case 0xfb:  // continue sequence
    case 0xfc:  // stop sequence
    case 0xfe:  // active sensing
    case 0xff:  // system reset
        return 1;
    }

    // system common messages
    switch (status) {
    case 0xf1:  // midi timing code
    case 0xf3:  // song select
        return 2;
    case 0xf2:  // song position pointer
        return 3;
    case 0xf6:  // tune request
        return 1;
    }

    // system exclusive messages
    switch (status) {
    case 0xf7:  // sysex start
    case 0xf0:  // sysex stop
        return 0; // special case
    }

    // channel mode messages
    return 2;
}

typedef struct {
    uint8_t buffer[DEV_MIDI_RX_BUFFER_SIZE];

    uint8_t prior_status;
    uint8_t prior_len;

    uint8_t msg_buf[3];
    uint8_t msg_pos;
    uint8_t msg_len;
    bool    msg_started;
    bool    msg_sysex;
} midi_input_state_t;

static inline void midi_input_msg_post(midi_input_state_t *state, struct dev_midi *midi) {
    union event_data *ev = event_data_new(EVENT_MIDI_EVENT);
    ev->midi_event.id = midi->dev.id;
    ev->midi_event.nbytes = state->msg_len;
    for (uint8_t n = 0; n < state->msg_len; n++) {
        ev->midi_event.data[n] = state->msg_buf[n];
    }
    // fprintf(stderr, "POST[%d](%d // 0x%x 0x%x 0x%x)\n", midi->dev.id, state->msg_len,
    //     ev->midi_event.data[0],
    //     ev->midi_event.data[1],
    //     ev->midi_event.data[2]
    // );
    event_post(ev);
}

static inline void midi_input_update_status(midi_input_state_t *state, uint8_t new_status) {
    // update (running_ statusr if not system rt status byte, assumes that
    // msg_buf 0 is a valid status byte.
    if (is_status_byte(new_status) && !is_status_real_time(new_status)) {
        state->prior_status = state->msg_buf[0];
        state->prior_len = state->msg_len;
    }
}

static inline void midi_input_msg_start(midi_input_state_t *state, uint8_t status) {
    state->msg_pos = 0;
    state->msg_len = midi_msg_len(status);
    state->msg_started = true;
    state->msg_buf[state->msg_pos++] = status;
    state->msg_sysex = status == 0xf7; 
}

static inline void midi_input_msg_end(midi_input_state_t *state) {
    state->msg_started = false;
    state->msg_pos = 0;
}

static inline void midi_input_msg_acc(midi_input_state_t *state, uint8_t byte) {
    if (!state->msg_started) {
        // running status
        // fprintf(stderr, "RMS byte: 0x%02x -- last s 0x%x n %u\n", byte, state->prior_status, state->prior_len);
        state->msg_started = true;
        state->msg_buf[0] = state->prior_status;
        state->msg_len = state->prior_len;
        state->msg_pos++;
    }
    state->msg_buf[state->msg_pos++] = byte;
}

static inline bool midi_input_msg_is_complete(midi_input_state_t *state) {
    // fprintf(stderr, "complete? started %d, pos %d, len %d\n", state->msg_started, state->msg_pos, state->msg_len);
    return state->msg_started && (state->msg_len == state->msg_pos);
}

static inline ssize_t dev_midi_consume_buffer(midi_input_state_t *state, ssize_t size, struct dev_midi *midi) {
    uint8_t i = 0;
    uint8_t byte = 0;

    // fprintf(stderr, "START read %zd // head 0x%02x\n", size, *(state->buffer));

    for (i = 0; i < size; i++) {
        byte = state->buffer[i];

        if (is_status_byte(byte)) {
            midi_input_msg_start(state, byte);
        } else {
            midi_input_msg_acc(state, byte);
        }

        // send completed message
        if (midi_input_msg_is_complete(state)) {
            midi_input_msg_post(state, midi);
            midi_input_update_status(state, byte);
            midi_input_msg_end(state);
        }
    }

    // fprintf(stderr, "  END read %zd, rtn %zd\n", size, i);

    return i;
}

void *dev_midi_start(void *self) {
    struct dev_midi *midi = (struct dev_midi *)self;
    struct dev_common *base = (struct dev_common *)self;

    snd_rawmidi_status_t *status = NULL;

    midi_input_state_t state;
    ssize_t read = 0;
    ssize_t xruns;

    if (snd_rawmidi_status_malloc(&status) != 0) {
        fprintf(stderr, "failed allocating rawmidi status, stopping device: %s\n", base->name);
        return NULL;
    }

    do {
        read = snd_rawmidi_read(midi->handle_in, state.buffer, DEV_MIDI_RX_BUFFER_SIZE);
        if (dev_midi_consume_buffer(&state, read, midi) != read) {
            fprintf(stderr, "midi inconsistency for device: %s\n", base->name);
        }

        // TODO: verify this is not overly expensive to do
        if (snd_rawmidi_status(midi->handle_in, status) == 0) {
            xruns = snd_rawmidi_status_get_xruns(status);
            if (xruns > 0) {
                fprintf(stderr, "xruns (%d) for midi device: %s\n", xruns, base->name);
                // TODO: validate that flushing the ring buffer gets us back in a good state
                snd_rawmidi_drop(midi->handle_in);
            }
        }

    } while (read > 0);

    if (status) {
        snd_rawmidi_status_free(status);
    }

    return NULL;
}

void *dev_midi_start_old(void *self) {
    struct dev_midi *midi = (struct dev_midi *)self;
    union event_data *ev;

    ssize_t read = 0;
    uint8_t byte = 0;
    uint8_t msg_buf[256];
    uint8_t msg_pos = 0;
    uint8_t msg_len = 0;

    do {
        read = snd_rawmidi_read(midi->handle_in, &byte, 1);

        if (byte >= 0xf8) {
            clock_midi_handle_message(byte);

            ev = event_data_new(EVENT_MIDI_EVENT);
            ev->midi_event.id = midi->dev.id;
            ev->midi_event.data[0] = byte;
            ev->midi_event.nbytes = 1;
            event_post(ev);
        } else {
            if (byte >= 0x80) {
                msg_buf[0] = byte;
                msg_pos = 1;

                switch (byte & 0xf0) {
                case 0x80:
                case 0x90:
                case 0xa0:
                case 0xb0:
                case 0xe0:
                case 0xf2:
                    msg_len = 3;
                    break;
                case 0xc0:
                case 0xd0:
                    msg_len = 2;
                    break;
                case 0xf0:
                    switch (byte & 0x0f) {
                    case 0x01:
                    case 0x03:
                        msg_len = 2;
                        break;
                    case 0x07:
                        // TODO: properly handle sysex length
                        msg_len = msg_pos; // sysex end
                        break;
                    case 0x00:
                        msg_len = 0; // sysex start
                        break;
                    default:
                        msg_len = 2;
                        break;
                    }
                    break;
                default:
                    msg_len = 2;
                    break;
                }
            } else {
                msg_buf[msg_pos] = byte;
                msg_pos += 1;
            }

            if (msg_pos == msg_len) {
                ev = event_data_new(EVENT_MIDI_EVENT);
                ev->midi_event.id = midi->dev.id;
                ev->midi_event.data[0] = msg_buf[0];
                ev->midi_event.data[1] = msg_len > 1 ? msg_buf[1] : 0;
                ev->midi_event.data[2] = msg_len > 2 ? msg_buf[2] : 0;
                ev->midi_event.nbytes = msg_len;
                event_post(ev);

                msg_pos = 0;
                msg_len = 0;
            }
        }
    } while (read > 0);

    return NULL;
}

ssize_t dev_midi_send(void *self, uint8_t *data, size_t n) {
    struct dev_midi *midi = (struct dev_midi *)self;
    return snd_rawmidi_write(midi->handle_out, data, n);
}
