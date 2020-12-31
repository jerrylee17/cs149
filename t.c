#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main() {
	write(1, "hi\n", 3);
}
