#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "bigbag.h"

/**
 * Return the entry at a offset
**/
struct bigbag_entry_s *entry_addr(void *hdr, uint32_t offset)
{
    if (offset == 0)
        return NULL;
    return (struct bigbag_entry_s *)((char *)hdr + offset);
}

/**
 * Return the offset of an entry
**/
uint32_t entry_offset(void *hdr, void *entry)
{
    return (uint32_t)((uint64_t)entry - (uint64_t)hdr);
}

/**
 * Print the error message when an invalid input is entered.
 * @param {char} c - message that was entered
**/
void printCommands(char *c)
{
    printf("%c not used correctly\n", c[0]);
    printf("possible commands:\n");
    printf("a string_to_add\n");
    printf("d string_to_delete\n");
    printf("c string_to_check\n");
    printf("l\n");
}

/**
 * List all elements in the file. 
 * Method:
 * 1. Fetch first entry from hdr->first_element
 * 2. If entry doesn't exist - bag is empty
 * 3. Iterate through the linked list of entries
 *  - Print each entry
 *  - Fetch next entry
 *  - Exit if the next offset is 0 (reached the end)
 * 
 * @param {bigbag_hdr_s} hdr - header of the file
**/
void listElements(struct bigbag_hdr_s *hdr)
{
    struct bigbag_entry_s *entry;
    uint32_t first_element = hdr->first_element;
    // fetch first entry
    entry = entry_addr(hdr, first_element);
    if (!entry)
    {
        printf("empty bag\n");
        return;
    }
    while (entry)
    {
        printf("%s\n", entry->str);
        // fetch next entry
        unsigned int nextOffset = entry->next;
        if (nextOffset == 0)
            return;
        entry = entry_addr(hdr, nextOffset);
    }
}

/**
 * Add an element to the file. 
 * Method:
 * 1. Create a new entry in the file from the free space
 *  - If the entry exceeds the amount of free space, the bag is full
 * 2. Traverse the linked list with 2 runners front and back
 *  - Corner case #1: Bag is empty
 *  - Corner case #2: Element should be inserted at the first position
 *  - Corner case #3: There is a string that matches the element
 *  - Corner case #4: Element should be inserted at the last position
 *  - Note: My if statement covers corner case #3
 * 3. Set the pointer of back to the new entry
 * 4. Set the new entry next to front's offset
 * 
 * 
 * @param {bigbag_hdr_s} hdr - header of the file
 * @param {char} element - element to add to file
**/
void addElement(struct bigbag_hdr_s *hdr, char *element)
{
    struct bigbag_entry_s *newEntry;
    struct bigbag_entry_s *front;
    struct bigbag_entry_s *back;
    uint32_t first_element = hdr->first_element;
    uint32_t first_free = hdr->first_free;
    uint32_t element_entry_size = MIN_ENTRY_SIZE + strlen(element) + 1;
    front = entry_addr(hdr, first_free);
    uint32_t old_length = front->entry_len;
    // if there is enough free space
    if (front->entry_len >= element_entry_size)
    {
        newEntry = entry_addr(hdr, hdr->first_free);
        newEntry->next = 0;
        strcpy(newEntry->str, element);
        newEntry->entry_len = strlen(element) + 1;
        newEntry->entry_magic = BIGBAG_USED_ENTRY_MAGIC;
        // Remove element size
        uint32_t new_first_free = first_free + sizeof(newEntry) + strlen(element) + 1;
        front = entry_addr(hdr, new_first_free);
        front->entry_magic = BIGBAG_FREE_ENTRY_MAGIC;
        front->entry_len = old_length - element_entry_size;
        // Change offset
        hdr->first_free = new_first_free;
    }
    else
    {
        printf("out of space\n");
        return;
    }
    front = entry_addr(hdr, first_element);
    back = front;
    // Resolves corner case #1: Empty bag
    if (!front)
    {
        newEntry->next = hdr->first_element;
        hdr->first_element = first_free;
    }

    while (front)
    {
        // element <= front
        if (strcmp(element, front->str) <= 0)
        {
            // Check if this is first element
            // Corner case #2: Element should be inserted at the first position
            if (back == front)
            {
                newEntry->next = hdr->first_element;
                hdr->first_element = first_free;
            }
            else
            {
                // Set new element next to front
                newEntry->next = back->next;
                // Should point to newly inserted element
                back->next = first_free;
            }
            break;
        }
        // Fetch next entry
        unsigned int nextOffset = front->next;
        back = front;
        front = entry_addr(hdr, nextOffset);
        // Resolves corner case #4: Element is last position
        if (!front)
        {
            newEntry->next = 0;
            back->next = first_free;
            break;
        }
    }
    printf("added %s\n", element);
}

/**
 * Check if an element is in the file.
 * Method:
 * 1. Fetch first entry from hdr->first_element
 * 2. Iterate through the linked list of entries
 *  - If the entry is equal to element, it has been found
 *  - Exit if the next offset is 0
 * 3. Print out "found" or "not found" based on the loop result
 * 
 * @param {bigbag_hdr_s} hdr - header of the file
 * @param {char} element - element to add to file
**/
void checkElement(struct bigbag_hdr_s *hdr, char *element)
{
    bool found = false;
    struct bigbag_entry_s *entry;
    uint32_t first_element = hdr->first_element;
    // fetch first entry
    entry = entry_addr(hdr, first_element);
    while (entry)
    {
        if (strcmp(entry->str, element) == 0)
        {
            found = true;
            break;
        }
        // fetch next entry
        unsigned int nextOffset = entry->next;
        if (nextOffset == 0)
            break;
        entry = entry_addr(hdr, nextOffset);
    }

    if (found)
        printf("found\n");
    else
        printf("not found\n");
}

/**
 * Remove an element from the file.
 * Method:
 * 1. Use the 2 runner algorithm to determine the position of the element
 * 2. If front equals element, we have found the element to remove
 *  - Corner case #1: Remove the first element
 *  - Corner case #2: Remove the last element
 *  - Corner case #3: Element not in list
 *  - Note: My loop covers corner case #2
 * 3. Remove the element
 *  - Point back->next to front->next
 *  - Reset front->entry_magic to free
 *  - Reset front->next
 * 
 * 
 * @param {bigbag_hdr_s} hdr - header of the file
 * @param {char} element - element to add to file
**/
void deleteElement(struct bigbag_hdr_s *hdr, char *element)
{
    struct bigbag_entry_s *front, *back;
    front = entry_addr(hdr, hdr->first_element);
    back = front;
    while (front)
    {
        // Front and element are equlivalent
        if (strcmp(element, front->str) == 0)
        {
            // Resolves corner case #1: Remove first element
            if (back == front)
            {
                // point the first free space to the next element
                hdr->first_element = front->next;
                // Clear front
                front->entry_magic = BIGBAG_FREE_ENTRY_MAGIC;
                front->next = 0;
            }
            else
            {
                back->next = front->next;
                front->entry_magic = BIGBAG_FREE_ENTRY_MAGIC;
                front->next = 0;
            }
            break;
        }
        // fetch next entry
        unsigned int nextOffset = front->next;
        back = front;
        front = entry_addr(hdr, nextOffset);
    }
    // Resolves corner case #3: element not in list
    if (!front)
    {
        printf("no %s\n", element);
        return;
    }
    printf("deleted %s\n", element);
}

/**
 * Main method.
 * 
 * @param {bigbag_hdr_s} hdr - header of the file
 * @param {char} element - element to add to file
**/
int main(int argc, char **argv)
{
    // Check for correct number of arguments
    if (argc <= 1)
    {
        printf("USAGE: ./bigbag [-t] filename\n");
        return 1;
    }

    // Open the file depending on the number of parameteres
    int fd;
    if (strcmp(argv[1], "-t") == 0)
    {
        fd = open(argv[2], O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    }
    else
    {
        fd = open(argv[1], O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    }
    // Error opening file
    if (fd == -1)
    {
        perror(argv[1]);
        return 2;
    }

    // Determine mmap method (private/shared) based on the flag
    void *file_base;
    if (strcmp(argv[1], "-t") == 0)
    {
        file_base = mmap(0, BIGBAG_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    }
    else
    {
        file_base = mmap(0, BIGBAG_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    }
    // Memory map failed
    if (file_base == MAP_FAILED)
    {
        perror("mmap");
        return 3;
    }

    struct stat stat;
    fstat(fd, &stat);
    struct bigbag_hdr_s *hdr = file_base;
    char *buffer = NULL;
    size_t bufsize = 0;
    // If file is size 0, make it the desired format
    if (stat.st_size == 0)
    {
        // Make file 64K
        ftruncate(fd, 65536);
        struct bigbag_hdr_s *header = file_base;
        // create header
        header->first_element = 0;
        header->magic = BIGBAG_MAGIC;
        header->first_free = sizeof(*header);
        // Set up first entry of free space
        struct bigbag_entry_s *entry = entry_addr(hdr, header->first_free);
        entry->next = 0;
        entry->entry_magic = BIGBAG_FREE_ENTRY_MAGIC;
        entry->entry_len = 65536 - sizeof(*header) - sizeof(*entry);
    }
    // Command line interface
    while (getline(&buffer, &bufsize, stdin))
    {
        // Remove line break
        buffer[strlen(buffer) - 1] = 0;

        // Element to be added/deleted/checked
        char *element = malloc(strlen(buffer));
        strncpy(element, buffer + 2, strlen(buffer));

        // List
        if (buffer[0] == 'l')
        {
            listElements(hdr);
        }
        // Add
        else if (buffer[0] == 'a')
        {
            addElement(hdr, element);
        }
        // Delete
        else if (buffer[0] == 'd')
        {

            deleteElement(hdr, element);
        }
        // Check
        else if (buffer[0] == 'c')
        {

            checkElement(hdr, element);
        }
        // Invalid command
        else
        {
            printCommands(buffer);
        }
        // Free memory allocated for element
        free(element);
    }
    // Free buffer space
    free(buffer);
    return 0;
}
