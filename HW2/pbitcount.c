#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
//#include <stdlib.h>
//#include <stdbool.h>
#define MSGSIZE 4

int intBitCounter(int i);
int bitCounter(FILE *fh);

int main(int argc, char *argv[])
{
    int fds[2];
    pipe(fds);
    int total = 0;
    // Check for right amount of arguments
    if (argc < 2)
    {
        printf("USAGE: ./bitcount filenames\n");
        return 1;
    }
    for (int i = 1; i < argc; i++)
    {
        FILE *fh = fopen(argv[i], "r");
        // Validate child before forking
        if (fh == NULL)
        {
            perror(argv[i]);
            return 2;
        }
        int pid = fork();
        // Child - determine bits in a file
        if (pid == 0)
        {
            int bits = bitCounter(fh);
            // Write file bits
            // Here, I used MSGSIZE rather than sizeof bits
            write(fds[1], &bits, MSGSIZE);
            // Delete child process
            _exit(0);
        }
        // Parent - add result from child to total bits
        else
        {
            // Wait for child to finish
            wait(NULL);
            int filebits;
            // Read in file bits
            read(fds[0], &filebits, MSGSIZE);
            // Add file bits to total
            total += filebits;
        }
    }
    // Total number of bits
    printf("Total bits of everything: %i\n", total);
}

// Bit counter for a file
int bitCounter(FILE *fh)
{
    // Total bits collected
    int bits = 0;
    int i = fgetc(fh);
    // Read 1 char at a time. Pass it through intBitCounter to see how many
    // bits the integer contains and add it total bits
    while (i != EOF)
    {
        int tmp = intBitCounter(i);
        bits += tmp;
        i = fgetc(fh);
    }
    return bits;
}

// Counts the number of bits in an integer
int intBitCounter(int i)
{
    int count = 0;
    while (i)
    {
        if (i % 2 == 1)
            count++;
        i = i / 2;
    }
    return count;
}
