#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <jack/jack.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <ctype.h>

#define SHIM_VERSION "1.1.1"

const char* HW_IN_L_VAR = "RS_PHYS_INPUT_L";
const char* HW_IN_R_VAR = "RS_PHYS_INPUT_R";
const char* HW_OUT_L_VAR = "RS_PHYS_OUTPUT_L";
const char* HW_OUT_R_VAR = "RS_PHYS_OUTPUT_R";

bool pipewire = false;

void try_connect(jack_client_t *client, const char *src, const char *dst, FILE *log_file) {
    if (src == NULL) {
        fprintf(log_file, "Source is unset, skipping connection to %s\n", dst);
        return;
    }

    if (dst == NULL) {
        fprintf(log_file, "Destination is unset, skipping connection from %s\n", src);
        return;
    }

    int connection_result = jack_connect(client, src, dst);
    if (connection_result != 0) {
        char* error = strerror(connection_result);
        // pw-jack returns EINVAL if it can't find the port, or if they are not compatible
        // jack_get_ports doesn't necessarily return a useful name on pw-jack
        if (connection_result == EINVAL) {
            const char* src_error = (jack_port_by_name(client, src) == NULL) ? src : NULL;
            const char* dst_error = (jack_port_by_name(client, dst) == NULL) ? dst : NULL;
            if (src_error && dst_error) {
                asprintf(&error, "could not find requested ports %s and %s", src_error, dst_error);
            }
            
            else if (src_error || dst_error) {
                asprintf(&error, "could not find requested port %s", src_error ? src_error : dst_error);
            }

            else {
                asprintf(&error, "port types are not compatible");
            }
        }
        
        fprintf(log_file, "Failed to connect %s -> %s: %s\n", src, dst, error);
        return;
    }

    fprintf(log_file, "Connected %s -> %s\n", src, dst);
}

// Remove prefixes and suffixes from client names
char* clean_name(const char* port_name) {
    if (pipewire == false) {
        return strdup(port_name);
    }

    // Format is pw-client-12:port, remove pw-
    port_name += 3;
    
    char *separator = strchr(port_name, ':');
    // Only client name, no separator
    if (!separator) {
        return strdup(port_name);
    }

    char *suffix = separator;
    while (suffix > port_name && *(suffix - 1) != '-') {
        suffix -= 1;
    }

    // No suffix at all
    if (*(suffix - 1) != '-') {
        return strdup(port_name);
    }

    char *p = suffix;
    while (p < separator) {
        // Not numbers, probably part of the client name
        if (!isdigit(*p)) {
            return strdup(port_name);
        }

        p += 1;
    }

    // It's a valid "-number" suffix, remove it
    size_t port_name_len = (suffix - 1) - port_name;
    size_t suffix_len = strlen(separator);
    char *result = malloc(port_name_len + suffix_len + 1);

    memcpy(result, port_name, port_name_len);
    memcpy(result + port_name_len, separator, suffix_len + 1);
    return result;
}

int jack_activate (jack_client_t *client) {
    FILE *log_file = fopen("jack_shim_debug.log", "w");
    if (log_file == NULL) {
        perror("Error opening file");
        return 1;
    }

    fprintf(log_file, "Library version %s\n", SHIM_VERSION);
    fprintf(log_file, "Running shim library as %s\n", jack_get_client_name(client));

    // Do the real Jack stuff
    int (*real_jack_activate)(jack_client_t *) = dlsym(RTLD_NEXT, "jack_activate");
    int result = real_jack_activate(client);

    // Get all physical ports
    // Naming is a bit strange
    // These are *node* input and output ports. The physical input node has virtual outputs, the physical output node has virtual inputs
    // First two entries are the system "preference"
    // Regardless, print all nodes for the user
    const char** phys_output_ports = jack_get_ports(client, NULL, NULL, JackPortIsInput | JackPortIsPhysical);
    const char** phys_input_ports = jack_get_ports(client, NULL, NULL, JackPortIsOutput | JackPortIsPhysical);
    int phys_input_port_count = 0;
    int phys_output_port_count = 0;

    while (phys_input_ports[phys_input_port_count] != NULL) {
        phys_input_port_count += 1;
    }

    while (phys_output_ports[phys_output_port_count] != NULL) {
        phys_output_port_count += 1;
    }

    const char* physical_input_l = getenv(HW_IN_L_VAR);
    const char* physical_input_r = getenv(HW_IN_R_VAR);
    const char* physical_output_l = getenv(HW_OUT_L_VAR);
    const char* physical_output_r = getenv(HW_OUT_R_VAR);

    // Get all application ports
    // If we are on PipeWire, then the client gets a pw- prefix we can't connect to
    const char prefix[] = "pw-";
    const char* client_name = jack_get_client_name(client);
    if (strncmp(prefix, client_name, 3) == 0) {
        // PipeWire, offset the pointer
        pipewire = true;
        fprintf(log_file, "Running on PipeWire, searching for client %s\n", clean_name(client_name));
    }

    const char** game_input_ports = jack_get_ports(client, clean_name(client_name), NULL, JackPortIsInput);
    const char** game_output_ports = jack_get_ports(client, clean_name(client_name), NULL, JackPortIsOutput);
    int game_input_port_count = 0;
    int game_output_port_count = 0;

    while (game_input_ports[game_input_port_count] != NULL) {
        game_input_port_count += 1;
    }

    while (game_output_ports[game_output_port_count] != NULL) {
        game_output_port_count += 1;
    }

    const char* rs_in_l = getenv("RS_GAME_IN_L");
    const char* rs_in_r = getenv("RS_GAME_IN_R");
    const char* rs_out_l = getenv("RS_GAME_OUT_L");
    const char* rs_out_r = getenv("RS_GAME_OUT_R");

    // Fallback to default port names
    // If at least one is set, only use the user defined ports
    // The user probably did that for a reason
    if (!physical_input_l && !physical_input_r) {
        if (phys_input_port_count > 0) {
            physical_input_l = clean_name(phys_input_ports[0]);

            if (phys_input_port_count > 1) {
                physical_input_r = clean_name(phys_input_ports[1]);
            }
        }

    }

    if (!physical_output_l && !physical_output_r) {
        if (phys_output_port_count > 0) {
            physical_output_l = clean_name(phys_output_ports[0]);
            
            if (phys_output_port_count > 1) {
                physical_output_r = clean_name(phys_output_ports[1]);
            }
        }
    }

    if (!rs_in_l && !rs_in_r && !rs_out_l && !rs_out_r) {
        if (game_input_port_count > 0) {
            rs_in_l = clean_name(game_input_ports[0]);

            if (game_input_port_count > 1) {
                rs_in_r = clean_name(game_input_ports[1]);
            }
        }

        if (game_output_port_count > 0) {
            rs_out_l = clean_name(game_output_ports[0]);

            if (game_output_port_count > 1) {
                rs_out_r = clean_name(game_output_ports[1]);
            }
        }
    }

    // Wait for Rocksmith to wake up
    // Left input is always set
    while (jack_port_by_name(client, rs_in_l) == NULL) {
        usleep(1000*250);
    }

    // Connections
    fprintf(log_file, "\nBeginning connections\n");
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

    fprintf(log_file, "\nFor example, to connect the jack input of a Scarlett Solo to the left input of Rocksmith, "
            "put %s='Scarlett Solo (3rd Gen.) Pro:capture_AUX1' in your Steam launch options.\n", HW_IN_L_VAR);

    fprintf(log_file, "\nFound %d physical input ports and %d physical output ports\n", phys_input_port_count, phys_output_port_count);

    if (phys_input_port_count) {
        fprintf(log_file, "\nPhysical JACK Input Ports:\n");
        for (int i = 0; i < phys_input_port_count; ++i) {
            fprintf(log_file, "%s\n", clean_name(phys_input_ports[i]));
        }

        free(phys_input_ports);
    }

    if (phys_output_port_count) {
        fprintf(log_file, "\nPhysical JACK Output Ports:\n");
        for (int i = 0; i < phys_output_port_count; ++i) {
            fprintf(log_file, "%s\n", clean_name(phys_output_ports[i]));
        }

        free(phys_output_ports);
    }

    fprintf(log_file, "\nFound %d game input ports and %d game output ports\n", game_input_port_count, game_output_port_count);

    if (game_input_port_count) {
        fprintf(log_file, "\nGame JACK Input Ports:\n");
        for (int i = 0; i < game_input_port_count; ++i) {
            fprintf(log_file, "%s\n", clean_name(game_input_ports[i]));
        }

        free(game_input_ports);
    }

    if (game_output_port_count) {
        fprintf(log_file, "\nGame JACK Output Ports:\n");
        for (int i = 0; i < game_output_port_count; ++i) {
            fprintf(log_file, "%s\n", clean_name(game_output_ports[i]));
        }

        free(game_output_ports);
    }

    fclose(log_file);

    return result;
}