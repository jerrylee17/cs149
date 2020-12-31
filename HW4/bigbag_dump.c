//
// Created by bcr33d on 10/4/20.
//

#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include "bigbag.h"
struct bigbag_entry_s *entry_addr(void *hdr, uint32_t offset) {
    if (offset == 0) return NULL;
    return (struct bigbag_entry_s *)((char*)hdr + offset);
}

uint32_t entry_offset(void *hdr, void *entry) {
    return (uint32_t)((uint64_t)entry - (uint64_t)hdr);
}


int main(int argc, char **argv) {
    int fd = open(argv[1], O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror(argv[1]);
        return 2;
    }

    void *file_base = mmap(0, BIGBAG_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (file_base == MAP_FAILED) {
        perror("mmap");
        return 3;
    }

    struct bigbag_hdr_s *hdr = file_base;

    struct stat stat;
    fstat(fd, &stat);
    printf("size = %ld\n", stat.st_size);
    printf("magic = %08x\n", htonl(hdr->magic));
    printf("first_free = %d\n", hdr->first_free);
    printf("first_element = %d\n", hdr->first_element);
    struct bigbag_entry_s *entry;
    int offset = sizeof(*hdr);
    while (offset + sizeof(*entry) < stat.st_size) {
        entry = entry_addr(hdr, offset);
        if (entry == NULL) {
            printf("bad entry at offset %d\n", offset);
            break;
        }
        printf("----------------\n");
        printf("entry offset: %d\n", offset);
        printf("entry magic: %x\n", (int)entry->entry_magic);
        printf("entry len: %d\n", entry->entry_len);
        printf("entry next offset: %d\n", entry->next);
        if (entry->entry_magic == BIGBAG_USED_ENTRY_MAGIC) {
            printf("entry data: %s\n", entry->str);
        }
        offset += sizeof(*entry) + entry->entry_len;
    }
}