#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#define BUCKETS (256)

typedef struct __node_t
{
    int key;
    struct __node_t *next;
} node_t;

typedef struct __list_t
{
    node_t *head;
    pthread_mutex_t lock;
} list_t;

typedef struct __hash_t
{
    list_t lists[BUCKETS];
} hash_t;

typedef struct __thread_params
{
    char *filename;
    hash_t *hashtable;
} thread_params;

void List_Init(list_t *L)
{
    L->head = NULL;
    pthread_mutex_init(&L->lock, NULL);
}

int keyGen(char *key)
{
    int count = 0;
    for (int i = 0; i < strlen(key); i++)
    {
        count += (int)key[i];
    }
    return count % BUCKETS;
}

// hash the string
// Taken from: https://stackoverflow.com/questions/7666509/hash-function-for-string
unsigned int hash(char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

int List_Size(list_t *L)
{
    pthread_mutex_lock(&L->lock);
    node_t *curr = L->head;
    int count = 0;
    while (curr)
    {
        curr = curr->next;
        count++;
    }
    pthread_mutex_unlock(&L->lock);
    return count;
}

int List_Lookup(list_t *L, int key)
{
    // pthread_mutex_lock(&L->lock);
    node_t *curr = L->head;
    while (curr)
    {
        if (curr->key == key)
        {
            // pthread_mutex_unlock(&L->lock);
            return 0; // success
        }
        curr = curr->next;
    }
    // pthread_mutex_unlock(&L->lock);
    return -1; // failure
}

int List_Insert(list_t *L, int key)
{
    pthread_mutex_lock(&L->lock);
    // Lookup first
    if (List_Lookup(L, key) == 0)
    {
        pthread_mutex_unlock(&L->lock);
        return -1;
    }

    node_t *new = malloc(sizeof(node_t));
    if (new == NULL)
    {
        perror("malloc");
        pthread_mutex_unlock(&L->lock);
        return -1; // fail
    }
    new->key = key;
    new->next = L->head;
    L->head = new;
    pthread_mutex_unlock(&L->lock);
    return 0; // success
}

void List_Free(list_t *L)
{
    node_t *curr = L->head;
    node_t *prev = L->head;
    while (curr)
    {
        curr = curr->next;
        free(prev);
        prev = curr;
    }
}

void Hash_Init(hash_t *H)
{
    int i;
    for (i = 0; i < BUCKETS; i++)
        List_Init(&H->lists[i]);
}

int Hash_Insert(hash_t *H, char *word)
{
    int position = keyGen(word);
    unsigned int key = hash(word);
    // int lookup = List_Lookup(&H->lists[position], key);
    // if (lookup == -1)
    // {
    return List_Insert(&H->lists[position], key);
    // }
    // return -1;
}

int Hash_Lookup(hash_t *H, char *word)
{
    int position = keyGen(word);
    unsigned int key = hash(word);
    return List_Lookup(&H->lists[position], key);
}

int Hash_Size(hash_t *H)
{
    int i;
    int count = 0;
    for (i = 0; i < BUCKETS; i++)
    {
        count += List_Size(&H->lists[i]);
    }
    return count;
}

void Hash_Free(hash_t *H)
{
    int i;
    for (i = 0; i < BUCKETS; i++)
        List_Free(&H->lists[i]);
}

void *wordCounter(void *arg)
{
    thread_params *parameters = arg;
    // Open file
    FILE *fh = fopen(parameters->filename, "r");
    // Validate file exists
    if (fh == NULL)
    {
        perror(parameters->filename);
        pthread_exit((void *)pthread_self());
    }
    // Store all words into *ptr
    char *ptr;
    fscanf(fh, "%ms", &ptr);
    while (ptr != NULL)
    {
        Hash_Insert(parameters->hashtable, ptr);
        free(ptr);
        fscanf(fh, "%ms", &ptr);
    }
    free(ptr);
    fclose(fh);
    return 0;
}

int main(int argc, char **argv)
{
    hash_t *hashtable = malloc(sizeof(hash_t));
    Hash_Init(hashtable);
    pthread_t *threads;
    thread_params *parameters;
    threads = malloc(sizeof(pthread_t) * argc);
    parameters = malloc(sizeof(thread_params) * argc);
    for (int i = 1; i < argc; i++)
    {
        parameters[i].filename = argv[i];
        parameters[i].hashtable = hashtable;
        pthread_create(&threads[i], NULL, &wordCounter, &parameters[i]);
    }
    for (int i = 1; i < argc; i++)
    {
        pthread_join(threads[i], NULL);
    }
    int size = Hash_Size(hashtable);
    printf("%i\n", size);
    Hash_Free(hashtable);
    free(hashtable);
    free(parameters);
    free(threads);
    return 0;
}
