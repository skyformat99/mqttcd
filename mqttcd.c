/* vim: set et fenc=utf-8 ff=unix ts=4 sw=4 sts=0 : */

#include "mqttcd.h"

int main(int argc, char** argv) {
    int ret;
    mqttcd_context_t context;

    // parse command line arguments
    ret = parse_arguments(&context, argc, argv);
    if (ret != MQTTCD_SUCCEEDED) {
        free_arguments(&context);
        return ret;
    }

    // daemonize
    if (context.option.daemonize == 1) {
        ret = fork();
        // fork is failed
        if (ret == -1) {
            free_arguments(&context);
            return MQTTCD_FORK_FAILED;
        }

        // exit parent process with child process number
        if (ret != 0) {
            printf("%d\n", ret);
            free_arguments(&context);
            return MQTTCD_SUCCEEDED;
        }

        // child process
        // close stdin, stdout, stderr
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        // create new session (disconnect from current terminal)
        setsid();
        // change working directory
        chdir("/");
        // reset umask
        umask(0);
        // ignore SIGCHLD for avoiding zombie process
        signal(SIGCHLD, SIG_IGN);
    }

    // setup signal handler
    if (signal(SIGINT, signal_handler) == SIG_ERR) {
        free_arguments(&context);
        return MQTTCD_SETUP_SIGNAL_FAILED;
    }
    if (signal(SIGTERM, signal_handler) == SIG_ERR) {
        free_arguments(&context);
        return MQTTCD_SETUP_SIGNAL_FAILED;
    }

    ret = logger_open(&context);
    if (ret != MQTTCD_SUCCEEDED) {
        free_arguments(&context);
        return ret;
    }

    // connect to mqtt broker
    ret = mqtt_connect(&context);
    if (ret != MQTTCD_SUCCEEDED) {
        logger_close(&context);
        free_arguments(&context);
        return ret;
    }

    // initialize connection and subscribe mqtt topic
    ret = mqtt_initialize_connection(&context);
    if (ret == MQTTCD_SUCCEEDED) {
        // receive loop
        int count = 0;
        while (MQTTCD_INTERRUPTED_SIGNAL == 0) {
            char* payload = NULL;
            ret = mqtt_read_publish(&context, &payload);
            if (ret == MQTTCD_SUCCEEDED && payload != NULL) {
                free(payload);
            }

            if (count++ > 30) {
                ret = mqtt_send_ping(&context);
                if (ret != MQTTCD_SUCCEEDED) {
                    break;
                }
                count = 0;
            }
        }

        // send disconnect packet
        ret = mqtt_finalize_connection(&context);
    }

    // disconnect from mqtt broker
    mqtt_disconnect(&context);

    // cleanup
    logger_close(&context);
    free_arguments(&context);

    return MQTTCD_SUCCEEDED;
}

void signal_handler(int signum) {
    MQTTCD_INTERRUPTED_SIGNAL = 1;
}

