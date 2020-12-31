#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct Student {
    int id;
    char *name;
};

struct Student *create_student(int id, char *name) {
    struct Student *new_student = malloc(sizeof *new_student);
    new_student->id = id;
    new_student->name = strdup(name);
    return new_student;
}

int main(int argc, char *argv[]) {
    struct Student *s[3];
    s[0] = create_student(1, "ben");
    s[1] = create_student(2, "amy");
    s[2] = create_student(3, "nico");
    for (int i = 0; i < 3; i++){
        printf("%d %s\n", s[i]->id, s[i]->name);
    }
    printf("%ld %p %p\n", sizeof(struct Student), s[0], s[1]->name);
}

