#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <jack/jack.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <unistd.h>

const char* HW_IN_L_VAR = "RS_PHYS_INPUT_L";
const char* HW_IN_R_VAR = "RS_PHYS_INPUT_R";
const char* HW_OUT_L_VAR = "RS_PHYS_OUTPUT_L";
const char* HW_OUT_R_VAR = "RS_PHYS_OUTPUT_R";

void try_connect(jack_client_t *client, const char *src, const char *dst, FILE *log_file) {
    if (src == NULL) {
        fprintf(log_file, "Source is unset, intentionally skipping connection to %s\n", dst);
        return;
    }

    if (dst == NULL) {
        fprintf(log_file, "Destination is unset, intentionally skipping connection from %s\n", src);
        return;
    }

    if (jack_connect(client, src, dst) != 0) {
        fprintf(log_file, "Failed to connect %s -> %s\n", src, dst);
    }

    else {
        fprintf(log_file, "Connected %s -> %s\n", src, dst);
    }
}

int jack_activate (jack_client_t *client) {
    FILE *log_file = fopen("jack_shim_debug.log", "w");
    if (log_file == NULL) {
        perror("Error opening file");
        return 1;
    }

    fprintf(log_file, "Running shim library as %s\n\n", jack_get_client_name(client));

    // Do the real Jack stuff
    int (*real_jack_activate)(jack_client_t *) = dlsym(RTLD_NEXT, "jack_activate");
    int result = real_jack_activate(client);

    // Get all physical ports
    // Naming is a bit strange
    // These are *node* input and output ports. The physical input node has virtual outputs, the physical output node has virtual inputs
    // First two entries are the system "preference"
    // Regardless, print all nodes for the user
    const char** input_ports = jack_get_ports(client, NULL, NULL, JackPortIsInput | JackPortIsPhysical);
    const char** output_ports = jack_get_ports(client, NULL, NULL, JackPortIsOutput | JackPortIsPhysical);

    const char* physical_input_l = getenv(HW_IN_L_VAR);
    const char* physical_input_r = getenv(HW_IN_R_VAR);
    const char* physical_output_l = getenv(HW_OUT_L_VAR);
    const char* physical_output_r = getenv(HW_OUT_R_VAR);

    const char* rs_in_l = getenv("RS_GAME_IN_L");
    const char* rs_in_r = getenv("RS_GAME_IN_R");
    const char* rs_out_l = getenv("RS_GAME_OUT_L");
    const char* rs_out_r = getenv("RS_GAME_OUT_R");

    // Fallback to default port names
    // If at least one is set, only use the user defined ports
    // The user probably did that for a reason
    if (!physical_input_l && !physical_input_r) {
        physical_input_l = output_ports[0];
        physical_input_r = output_ports[1];
    }

    if (!physical_output_l && !physical_output_r) {
        physical_output_l = input_ports[0];
        physical_output_r = input_ports[1];
    }

    if (!rs_in_l && !rs_in_r && !rs_out_l && !rs_out_r) {
        rs_in_l = "Rocksmith2014:in_1";
        rs_in_r = "Rocksmith2014:in_2";
        rs_out_l = "Rocksmith2014:out_1";
        rs_out_r = "Rocksmith2014:out_2";
    }

    // Wait for Rocksmith to wake up
    sleep(1);

    // Connections
    fprintf(log_file, "Beginning connections\n");
    try_connect(client, physical_input_l, rs_in_l, log_file);
    try_connect(client, physical_input_r, rs_in_r, log_file);
    try_connect(client, rs_out_l, physical_output_l, log_file);
    try_connect(client, rs_out_r, physical_output_r, log_file);

    // Cleanup & debug
    fprintf(log_file, "\nUse the below port names to set environment variables for custom connections to Rocksmith. "
            "The default behaviour is to match the system-wide PipeWire device selection.\n\n");
    fprintf(log_file, "Your current environment variables are set as follows:\n"
            "%s=%s\n"
            "%s=%s\n"
            "%s=%s\n"
            "%s=%s\n",
            HW_IN_L_VAR, getenv(HW_IN_L_VAR) ?: "",
            HW_IN_R_VAR, getenv(HW_IN_R_VAR) ?: "",
            HW_OUT_L_VAR, getenv(HW_OUT_L_VAR) ?: "",
            HW_OUT_R_VAR, getenv(HW_OUT_R_VAR) ?: "");

    fprintf(log_file, "\nFor example, to connect the jack input of a Scarlett Solo to the left input of Rocksmith,"
            "put RS_PHYS_INPUT_L='Scarlett Solo (3rd Gen.) Pro:capture_AUX1' in your Steam launch options.\n");

    if (output_ports) {
        fprintf(log_file, "\nPhysical JACK Input Ports:\n");
        for (int i = 0; output_ports[i]; ++i) {
            fprintf(log_file, "%s\n", output_ports[i]);
        }
        free(output_ports);
    }

    if (input_ports) {
        fprintf(log_file, "\nPhysical JACK Output Ports:\n");
        for (int i = 0; input_ports[i]; ++i) {
            fprintf(log_file, "%s\n", input_ports[i]);
        }

        free(input_ports);
    }

    fclose(log_file);

    return result;
}