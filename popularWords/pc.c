#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#define BUCKETS (256)

// Node for hash table
typedef struct __node_t
{
    int count;
    char *key;
    struct __node_t *next;
} node_t;

// List in each hash table entry
typedef struct __list_t
{
    node_t *head;
} list_t;

// Hash table
typedef struct __hash_t
{
    list_t lists[BUCKETS];
} hash_t;

typedef struct __queue_t
{
    node_t *head;
    node_t *tail;
    pthread_mutex_t head_lock, tail_lock;
} queue_t;

// Parameters to pass into threads
typedef struct __thread_params
{
    char *filename;
    queue_t **queues;
} thread_params;

typedef struct __occur_vals
{
    queue_t *words;
    int maximum;
} occur_vals;

typedef struct __counter_params
{
    queue_t *queue;
    occur_vals *max_occur;
} counter_params;

void Queue_Init(queue_t *q);
int Queue_Dequeue(queue_t *q, char **value);
void Queue_Enqueue(queue_t *q, char *value);

// Initialize list
void List_Init(list_t *L)
{
    L->head = NULL;
}

// Generate key
int keyGen(char *key)
{
    int count = 0;
    for (int i = 0; i < strlen(key); i++)
    {
        count += (int)key[i];
    }
    return count % BUCKETS;
}

// Get size of list from unique words
// Modified to print out words with max count
int List_Size(list_t *L)
{
    node_t *curr = L->head;
    int listMax = 0;
    while (curr)
    {
        if (curr->count > listMax)
        {
            listMax = curr->count;
        }
        curr = curr->next;
    }
    return listMax;
}

// Lookup words
int List_Lookup(list_t *L, char *key)
{
    node_t *curr = L->head;
    while (curr)
    {
        if (strcmp(curr->key, key) == 0)
        {
            curr->count += 1;
            return 0; // success
        }
        curr = curr->next;
    }
    return -1; // failure
}

// Insert words into list
int List_Insert(list_t *L, char *key)
{
    // Lookup first
    if (List_Lookup(L, key) == 0)
    {
        return -1;
    }
    node_t *new = malloc(sizeof(node_t));
    if (new == NULL)
    {
        perror("malloc");
        return -1; // fail
    }
    new->key = malloc(strlen(key) + 1);
    strcpy(new->key, key);
    new->count = 1;
    new->next = L->head;
    L->head = new;
    return 0; // success
}

// Free space in the list
void List_Free(list_t *L)
{
    node_t *curr = L->head;
    node_t *prev = L->head;
    while (curr)
    {
        curr = curr->next;
        free(prev->key);
        free(prev);
        prev = curr;
    }
}

// Initialize hash table
void Hash_Init(hash_t *H)
{
    int i;
    for (i = 0; i < BUCKETS; i++)
        List_Init(&H->lists[i]);
}

// Insert into hash table
int Hash_Insert(hash_t *H, char *word)
{
    int position = keyGen(word);
    return List_Insert(&H->lists[position], word);
}

// Lookup in hash table
int Hash_Lookup(hash_t *H, char *word)
{
    int position = keyGen(word);
    return List_Lookup(&H->lists[position], word);
}

// Return the words with max occurences and the max occurance
occur_vals *Hash_Occurance(hash_t *H)
{
    occur_vals *values = malloc(sizeof(occur_vals));
    values->maximum = 0;
    values->words = malloc(sizeof(queue_t));
    Queue_Init(values->words);
    int j = 0;
    for (int i = 0; i < BUCKETS; i++)
    {
        node_t *curr = H->lists[i].head;
        while (curr)
        {
            if (curr->count == values->maximum)
            {
                // Add to queue
                Queue_Enqueue(values->words, curr->key);
                j++;
            }
            else if (curr->count > values->maximum)
            {
                char *tmp;
                // clear values queue and add current one
                for (int k = 0; k < j; k++)
                {
                    Queue_Dequeue(values->words, &tmp);
                    free(tmp);
                }
                Queue_Enqueue(values->words, curr->key);
                j = 1;
                values->maximum = curr->count;
            }
            curr = curr->next;
        }
    }
    return values;
}

// Free space in the hash table
void Hash_Free(hash_t *H)
{
    int i;
    for (i = 0; i < BUCKETS; i++)
        List_Free(&H->lists[i]);
}

// Initialize queue
void Queue_Init(queue_t *q)
{
    node_t *tmp = malloc(sizeof(node_t));
    tmp->next = NULL;
    q->head = q->tail = tmp;
    pthread_mutex_init(&q->head_lock, NULL);
    pthread_mutex_init(&q->tail_lock, NULL);
}

// Add to queue
void Queue_Enqueue(queue_t *q, char *value)
{
    pthread_mutex_lock(&q->tail_lock);
    node_t *tmp = malloc(sizeof(node_t));
    if (tmp == NULL)
    {
        pthread_mutex_unlock(&q->tail_lock);
        return;
    }
    tmp->key = malloc(strlen(value) + 1);
    strcpy(tmp->key, value);

    tmp->next = NULL;

    q->tail->next = tmp;
    q->tail = tmp;
    pthread_mutex_unlock(&q->tail_lock);
}

// Pop from queue
int Queue_Dequeue(queue_t *q, char **value)
{
    pthread_mutex_lock(&q->head_lock);
    node_t *tmp = q->head;
    node_t *new_head = tmp->next;

    if (new_head == NULL)
    {
        pthread_mutex_unlock(&q->head_lock);
        return -1; // queue was empty
    }
    // take tmp->key and pass to hash table
    *value = malloc(strlen(new_head->key) + 1);
    strcpy(*value, new_head->key);
    q->head = new_head;
    if (tmp->key != NULL)
    {
        free(tmp->key);
    }
    free(tmp);
    pthread_mutex_unlock(&q->head_lock);
    return 0;
}

// Free the remaining elements in the queue
void Queue_Free(queue_t *q)
{
    node_t *curr = q->head;
    node_t *prev = q->head;
    while (curr)
    {
        curr = curr->next;
        free(prev->key);
        free(prev);
        prev = curr;
    }
}

// Thread to handle each file
void *fileHandler(void *arg)
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
        int val = (int)ptr[0] & 0x03;
        Queue_Enqueue(parameters->queues[val], ptr);
        free(ptr);
        fscanf(fh, "%ms", &ptr);
    }

    free(ptr);
    fclose(fh);
    return 0;
}

// Thread to count the queues
void *counter(void *arg)
{
    counter_params *parameters = arg;
    hash_t *hashtable = malloc(sizeof(hash_t));
    Hash_Init(hashtable);

    // Deque value
    char *value;
    while (Queue_Dequeue(parameters->queue, &value) == 0)
    {
        // Put it in the hashtable
        Hash_Insert(hashtable, value);
        free(value);
    }
    parameters->max_occur = Hash_Occurance(hashtable);
    Hash_Free(hashtable);
    free(hashtable);
    return 0;
}

int main(int argc, char **argv)
{
    pthread_t *fileThreads;
    pthread_t *counterThreads;
    thread_params *fileParams;
    counter_params *counterParams;
    queue_t **queues;
    queues = malloc(sizeof(queues) * 4);
    fileThreads = malloc(sizeof(pthread_t) * argc);
    fileParams = malloc(sizeof(thread_params) * argc);

    counterThreads = malloc(sizeof(pthread_t) * 4);
    counterParams = malloc(sizeof(counter_params) * 4);

    // Initialize the queues
    for (int i = 0; i < 4; i++)
    {
        queues[i] = malloc(sizeof(queue_t));
        Queue_Init(queues[i]);
    }

    // Threads to read from files
    for (int i = 1; i < argc; i++)
    {
        fileParams[i].filename = argv[i];
        fileParams[i].queues = queues;
        pthread_create(&fileThreads[i], NULL, &fileHandler, &fileParams[i]);
    }

    // Finish putting stuff into the queues from the files
    for (int i = 1; i < argc; i++)
    {
        pthread_join(fileThreads[i], NULL);
    }

    // Create counter threads
    for (int i = 0; i < 4; i++)
    {
        counterParams[i].queue = queues[i];
        pthread_create(&counterThreads[i], NULL, &counter, &counterParams[i]);
    }

    // Finish counting
    for (int i = 0; i < 4; i++)
    {
        pthread_join(counterThreads[i], NULL);
    }

    // Determine the maximum occurances
    int maximum = 0;
    for (int i = 0; i < 4; i++)
    {
        if (counterParams[i].max_occur->maximum > maximum)
        {
            // Set maximum
            maximum = counterParams[i].max_occur->maximum;
        }
    }

    // Print out the max occurences and free the queues
    for (int i = 0; i < 4; i++)
    {
        if (counterParams[i].max_occur->maximum == maximum)
        {
            char *tmp;
            while (Queue_Dequeue(counterParams[i].max_occur->words, &tmp) == 0)
            {
                printf("%s %i\n", tmp, maximum);
                free(tmp);
            }
        }
        Queue_Free(counterParams[i].max_occur->words);
        free(counterParams[i].max_occur->words);
        free(counterParams[i].max_occur);
    }

    // Free everything else
    for (int i = 0; i < 4; i++)
    {
        Queue_Free(queues[i]);
        free(queues[i]);
    }
    free(queues);
    free(fileThreads);
    free(fileParams);
    free(counterThreads);
    free(counterParams);
    return 0;
}
