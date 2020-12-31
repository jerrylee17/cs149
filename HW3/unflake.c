#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>

void executeTest(char *test_command, char **args);
void killChild();

pid_t child_pid = -1;

int main(int argc, char *argv[]) {
    // Not enough arguments
    if (argc < 4) {
        printf("USAGE: ./unflake max_tries max_timeout test_command args...\n");
        printf("max_tries - must be greater than or equal to 1\n");
        printf("max_timeout - must be greater than or equal to 1\n");
        return 1;
    }

    int max_tries = atoi(argv[1]);
    int max_timeout = atoi(argv[2]);
    char *test_command = argv[3];
    char *args[argc - 2];
    for (int i = 3; i < argc; i++) {
        args[i - 3] = argv[i];
    }
    // Last element of the array needs to be null
    args[argc-3] = NULL;

    // Check if max_tries/max_timeout is valid
    if (max_tries < 1) {
        printf("max_tries - must be greater than or equal to 1\n");
    }
    if (max_timeout < 1) {
        printf("max_timeout - must be greater than or equal to 1\n");
    }
    if (max_tries < 1 || max_timeout < 1) {
        return 1;
    }

    int status, retval;
    char buf[16];
    int runs = 0;
    int stdout_id = dup(1);
    for (int i = 0; i < max_tries; i++) {
        // Increment runs
        runs += 1;
        // File to write all output to
        snprintf(buf, sizeof(buf), "test_output.%i", i + 1);
        int fd = open(buf, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        fflush(stdout);
        dup2(fd, 1);
        // Signal for when child hangs
        signal(SIGALRM, killChild);
        // Fork process
        child_pid = fork();
        if (child_pid == 0) {
            // Child process -- execute test
            executeTest(test_command, args);
        } else {
            // Parent process
            // Wait for child to finish
            alarm(max_timeout);
            wait(&status);
            if (WEXITSTATUS(status) == 127) {
                // Command doesn't exist
                printf("Run #%d: could not exec %s\n", i+1, test_command);
                retval = 2;
            } else if (!WIFEXITED(status)) {
                // Command was killed
                printf("Run #%d: Killed with signal %d\n", i+1, WTERMSIG(status));
                retval = 255;
            } else if (WEXITSTATUS(status) == 0) {
                // Exists successfully
                printf("Run #%d: successfully exited\n", i+1);
                retval = 0;
                break;
            } else {
                // Failed to exit
                printf("Run #%d: Exit status -- %d\n", i+1, WEXITSTATUS(status));
                retval = WEXITSTATUS(status);
            }
            if (WEXITSTATUS(status) == 255) {
                retval = 255;
                break;
            }
        }
    }
    fflush(stdout);
    dup2(stdout_id, 1);
    printf("%d run(s)\n", runs);
    printf("Exit code %i\n", retval);
    return retval;
}

/**
 * Kills child process
 */
void killChild() {
//    printf("Child #%d killed\n", child_pid);
    kill(child_pid, SIGKILL);
}

/**
 * Executes a single test
 * @param {test_command} The name of the test file to run
 * @param {args} The arguments to pass into the test file
 * @return
 */
void executeTest(char *test_command, char **args) {
    int status = execvp(test_command, &args[0]);
    if (status == -1) {
        printf("Could not exec %s\n", test_command);
        exit(127);
    }
}
