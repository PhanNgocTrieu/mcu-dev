#include "usbmod.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define AOA_GET_PROTOCOL 51
#define AOA_SEND_STRING 52
#define AOA_START 53
#define AOA_VENDOR_IN 0xc0
#define AOA_VENDOR_OUT 0x40

static void set_err(char *error, size_t error_len, const char *text)
{
    if (error != NULL && error_len > 0) {
        snprintf(error, error_len, "%s", text);
    }
}

static void add_step(usbmod_aoa_plan_t *plan, uint8_t request_type, uint8_t request, uint16_t index,
                     const char *payload, int data_in, size_t in_len)
{
    usbmod_aoa_step_t *step;
    if (plan->count >= USBMOD_AOA_STEPS) {
        return;
    }
    step = &plan->steps[plan->count++];
    memset(step, 0, sizeof *step);
    step->setup.bm_request_type = request_type;
    step->setup.b_request = request;
    step->setup.w_index = index;
    step->data_in = data_in;
    if (data_in) {
        step->setup.w_length = (uint16_t)in_len;
        step->payload_len = in_len;
        return;
    }
    if (payload != NULL) {
        step->payload_len = strlen(payload);
        if (step->payload_len >= sizeof step->payload) {
            step->payload_len = sizeof step->payload - 1;
        }
        memcpy(step->payload, payload, step->payload_len);
        step->setup.w_length = (uint16_t)step->payload_len;
    }
}

int usbmod_aoa_build(const usbmod_aoa_strings_t *strings, usbmod_aoa_plan_t *plan)
{
    static const usbmod_aoa_strings_t defaults = {
        "MCU", "UsbModule", "Android Open Accessory", "1.0", "http://localhost/usb", "usb-module"};
    const char *fields[6];
    size_t i;
    if (plan == NULL) {
        return -1;
    }
    if (strings == NULL) {
        strings = &defaults;
    }
    memset(plan, 0, sizeof *plan);
    add_step(plan, AOA_VENDOR_IN, AOA_GET_PROTOCOL, 0, NULL, 1, 2);
    fields[0] = strings->manufacturer != NULL ? strings->manufacturer : defaults.manufacturer;
    fields[1] = strings->model != NULL ? strings->model : defaults.model;
    fields[2] = strings->description != NULL ? strings->description : defaults.description;
    fields[3] = strings->version != NULL ? strings->version : defaults.version;
    fields[4] = strings->uri != NULL ? strings->uri : defaults.uri;
    fields[5] = strings->serial != NULL ? strings->serial : defaults.serial;
    for (i = 0; i < 6; i++) {
        add_step(plan, AOA_VENDOR_OUT, AOA_SEND_STRING, (uint16_t)i, fields[i], 0, 0);
    }
    add_step(plan, AOA_VENDOR_OUT, AOA_START, 0, NULL, 0, 0);
    return plan->count == USBMOD_AOA_STEPS ? 0 : -1;
}

int usbmod_aoa_run(const char *devnode, const usbmod_aoa_strings_t *strings, int *protocol_out,
                   char *error, size_t error_len)
{
    usbmod_aoa_plan_t plan;
    int fd;
    size_t i;
    int protocol = 0;
    if (protocol_out != NULL) {
        *protocol_out = -1;
    }
    if (usbmod_aoa_build(strings, &plan) != 0) {
        set_err(error, error_len, "AOA plan is incomplete");
        return -1;
    }
    fd = usbdrv_usbfs_open(devnode);
    if (fd < 0) {
        set_err(error, error_len, "cannot open usbfs node");
        return -1;
    }
    for (i = 0; i < plan.count; i++) {
        usbmod_aoa_step_t *step = &plan.steps[i];
        unsigned char inbuf[2];
        void *data = NULL;
        int rc;
        if (step->data_in) {
            data = inbuf;
        } else if (step->payload_len > 0) {
            data = step->payload;
        }
        rc = usbdrv_usbfs_control(fd, &step->setup, data, 1000);
        if (rc < 0) {
            close(fd);
            if (error != NULL && error_len > 0) {
                snprintf(error, error_len, "AOA control request %u failed", step->setup.b_request);
            }
            return -1;
        }
        if (i == 0) {
            if (rc < 2) {
                close(fd);
                set_err(error, error_len, "AOA protocol reply is short");
                return -1;
            }
            protocol = inbuf[0] | (inbuf[1] << 8);
            if (protocol_out != NULL) {
                *protocol_out = protocol;
            }
            if (protocol < 1) {
                close(fd);
                set_err(error, error_len, "AOA protocol is not supported");
                return -EPROTONOSUPPORT;
            }
        }
    }
    close(fd);
    if (protocol_out != NULL) {
        *protocol_out = protocol;
    }
    set_err(error, error_len, "AOA start sent");
    return 0;
}
