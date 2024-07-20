#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <signal.h>


bool isPowerOfTwo(int num) {
    if (num == 0 || num < 0) {
        return false;
    }
    int check = num - 1;
    if((num & check) == 0) {
        return true;
    }
    return false;
}

typedef struct {
    int cacheSize;
    int blockSize; 
    int RAMSize;
    int threadCount;
    char technique[50];
    int setCount;
    char **data;
    pthread_mutex_t cacheLock;
} Cache;

typedef struct {
    int valid;
    int tag;
    int lastAccessed;
    char *block;
} Line;

typedef struct {
    Cache *cache;
    Line **Lines;
    int threadId;
} ThreadArgs;

void accessCache(Cache *, Line **, int);

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *thread_func(void *args) {
    ThreadArgs *threadArgs = (ThreadArgs *)args;
    Cache *cache = threadArgs->cache;
    Line **Lines = threadArgs->Lines;
    int threadId = threadArgs->threadId;
    pthread_mutex_lock(&mutex);

    while (1) {
        int address;
        printf("Thread %d: Enter address (or -1 to finish): ", threadId);
        scanf("%d", &address);

        if (address == -1) {
            break;
        }

        pthread_mutex_lock(&(cache->cacheLock));
        accessCache(cache, Lines, address);
        pthread_mutex_unlock(&(cache->cacheLock));

    }
    
    pthread_mutex_unlock(&mutex);

    return NULL;
}

bool isValidBlockSize (int cacheSize, int blockSize) {
    if(isPowerOfTwo(blockSize)) {
        if (cacheSize == blockSize) {
            return false;
        }
        return cacheSize % blockSize == 0;
    }
    return false;
}

bool isValidRamSize(int cacheSize, int RamSize) {
    if (isPowerOfTwo(RamSize)) {
        return RamSize > cacheSize;
    }
    return false;
}

bool isValidSetCount (int cacheSize, int blockSize, int setCount) {
    if (isPowerOfTwo(setCount)) {
        if (cacheSize / blockSize == setCount || setCount == 1) {
            return false;
        }
        return !((cacheSize / blockSize) % setCount);
    }
    return false;
}

void displayUserInput (Cache *cache) {
    do {
        printf("Cache Size : ");
        if(scanf("%d", &(cache -> cacheSize)) == 0) {
            cache -> cacheSize = 0;
            fflush(stdin);
        }
    } while (!(isPowerOfTwo(cache -> cacheSize)));

    printf("\n");

    do {
        printf("Block Size : ");
        if(scanf("%d", &(cache -> blockSize)) == 0) {
            cache -> blockSize = 0;
            fflush(stdin);
        }
    } while (!isValidBlockSize(cache -> cacheSize, cache -> blockSize));
    
    printf("\n");

    do {
        printf("There are 3 options for cache techniques - Direct-Mapped, Set-Associative, Full-Associative\n");
        printf("Mapping Technique : ");
        scanf("%s", cache -> technique);
        if (strcmp(cache -> technique, "Direct-Mapped") == 0) {
            cache -> setCount = cache -> cacheSize / cache -> blockSize;
            break;
        } else if (strcmp(cache -> technique, "Set-Associative") == 0) {
            do {   
                printf("Set Count : ");
                scanf("%d", &(cache -> setCount));
            } while (!isValidSetCount(cache -> cacheSize, cache -> blockSize, cache -> setCount));
            break;
        } else if (strcmp(cache -> technique, "Full-Associative") == 0) {
            cache -> setCount = 1;
            break;
        }
    } while (1);
    
    printf("\n");

    do {   
        printf("Size of RAM : ");
        if(scanf("%d", &(cache -> RAMSize)) == 0) {
            cache -> RAMSize = 0;
            fflush(stdin);
        }
    } while (!isValidRamSize(cache -> cacheSize, cache -> RAMSize));
    
    printf("\n");

    do {
        printf("Number of Threads : ");
        if(scanf("%d", &(cache -> threadCount)) == 0) {
            cache -> threadCount = 0;
            fflush(stdin);
        }
    } while ((cache -> threadCount) <= 0);
    printf("\n");
}

void displayContent (Cache *cache) {
    printf("Congrats! You specified cache with these configurations:\n");
    printf("Cache Size : %d\n", cache -> cacheSize);
    printf("Block Size : %d\n", cache -> blockSize);
    printf("Mapping Technique : %s\n", cache -> technique);
    printf("Size of RAM : %d\n", cache -> RAMSize);
    printf("Number of Threads : %d\n", cache -> threadCount);
}

char** createCache (Cache *cache) {
    int numBlocks = cache -> cacheSize / cache -> blockSize;
    char **cacheMemory = (char **)malloc(numBlocks * sizeof(char *));

    if (cacheMemory == NULL) {
        perror("malloc failedx on cacheMemory");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < numBlocks; i++) {
        cacheMemory[i] = (char *)malloc(cache -> blockSize * sizeof(char));
        if(cacheMemory[i] == NULL) {
            perror("malloc failedx on cacheMemory[i]");
            exit(EXIT_FAILURE);
        }
    }
    return cacheMemory;
}

void freeCache(char **cache, int numBlocks) {
    for (int i = 0; i < numBlocks; i++) {
        free(cache[i]);
    }
    free(cache);
}

char **createRam(Cache *cache) {
    int numBlocks = cache -> RAMSize / cache -> blockSize;
    char **ram = (char **)malloc(numBlocks * sizeof(char *));

    if (ram == NULL) {
        perror("malloc failed on ram");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < numBlocks; i++) {
        ram[i] = (char *)malloc(cache -> blockSize * sizeof(char));
        if (ram == NULL) {
            perror("malloc failed on ram[i]");
            exit(EXIT_FAILURE);
        }
        for (int j = 0; j < cache -> blockSize; j++) {
            ram[i][j] = rand() % 256;
        }
    }
    return ram;
}

void freeRam(char **ram, int numBlocks) {
    for (int i = 0; i < numBlocks; i++) {
        free(ram[i]);
    }
    free(ram);
}

void accessCache(Cache *cache, Line **Lines, int address) {

    if (address < 0 || address > cache -> RAMSize) {
        printf("the passed address is not a valid\n");
        return;
    }

    int blockNumber = address / cache -> blockSize;
    int setIndex = blockNumber % cache -> setCount;
    int tag = blockNumber / cache -> setCount;
    int offset = address % cache -> blockSize;

    Line *set = Lines[setIndex];

    for (int i = 0; i < cache -> cacheSize / (cache -> blockSize * cache -> setCount); i++) {
        if (set[i].valid && set[i].tag == tag) {
            printf("Cache Hit! Data: %d\n", set[i].block[offset]);
            set[i].lastAccessed++; // clock function doesn't work 
            return;
        }
    }

    printf("Cache Miss! Loading from RAM...\n");
    int ramBlockIndex = blockNumber % (cache -> RAMSize / cache -> blockSize);
    for (int i = 0; i < cache -> cacheSize / (cache -> blockSize * cache -> setCount); i++) {
        if (!set[i].valid) {
            set[i].valid = 1;
            set[i].tag = tag;
            set[i].lastAccessed = 0; // clock function ?? doesn't work
            strncpy(set[i].block, cache -> data[ramBlockIndex], cache -> blockSize);
            printf("Loaded Data: %d\n", set[i].block[offset]);
            return;
        }
    }
    int index = 0;
    for(int i = 0; i < cache -> cacheSize / (cache -> blockSize * cache -> setCount); i++) {
        if (set[i].lastAccessed < set[index].lastAccessed) {
            index = i;
        }
    }

    set[index].valid = 1;
    set[index].tag = tag;
    set[index].lastAccessed = 0;
    strncpy(set[index].block, cache -> data[ramBlockIndex], cache -> blockSize);
    printf("Loaded Data: %d\n", set[index].block[offset]);
    return;
}

Line** initializeLines(Cache *cache) {
    int numSets = cache -> setCount;
    int linesPerSet = cache -> cacheSize / (cache -> blockSize * numSets);
    Line **Lines = (Line **)malloc(numSets * sizeof(Line *));

    if (Lines == NULL) {
        perror("malloc on Lines failed");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < numSets; i++) {
        Lines[i] = (Line *)malloc(linesPerSet * sizeof(Line));

        if (Lines[i] == NULL) {
            perror("malloc on Lines[i] failed");
            exit(EXIT_FAILURE);
        }

        for (int j = 0; j < linesPerSet; j++) {
            Lines[i][j].valid = 0;
            Lines[i][j].tag = -1;
            Lines[i][j].lastAccessed = 0;
            Lines[i][j].block = (char *)malloc(cache -> blockSize * sizeof(char));
            if (Lines[i][j].block == NULL) {
                perror("malloc failed on Lines[i][j]");
                exit(EXIT_FAILURE);
            }
        }
    }
    return Lines;
}

void freeLines(Line **Lines, int numSets, int linesPerSet) {
    for (int i = 0; i < numSets; i++) {
        for (int j = 0; j < linesPerSet; j++) {
            free(Lines[i][j].block);
        }
        free(Lines[i]);
    }
    free(Lines);
}

void createThreads(Cache *cache, Line **Lines) {
    pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t) * cache -> threadCount);

    if (threads == NULL) {
        perror("malloc failed on threads");
        exit(EXIT_FAILURE);
    }

    ThreadArgs *threadArgs = (ThreadArgs *)malloc(sizeof(ThreadArgs) * cache -> threadCount);

    if (threadArgs == NULL) {
        perror("malloc failed on threadArgs");
    }

    for (int i = 0; i < cache -> threadCount; ++i) {
        threadArgs[i].cache = cache;
        threadArgs[i].Lines = Lines;
        threadArgs[i].threadId = i + 1;
        pthread_create(&threads[i], NULL, thread_func, &threadArgs[i]);
    }

    for (int i = 0; i < cache -> threadCount; ++i) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(threadArgs);
}

void displayCacheInfo(Line **lines, int numSets, int linesPerSet, int blockSize) {
    for (int i = 0; i < numSets; i++) {
        for (int j = 0; j < linesPerSet; j++) {
            printf("Set %d, Line %d - Valid: %d, Tag: %d, Block: ", i, j, lines[i][j].valid, lines[i][j].tag);
            for (int k = 0; k < blockSize; k++) {
                printf("%02x ", (unsigned char)lines[i][j].block[k]);
            }
            printf("\n");
        }
    }
}


int main () {
    srand(time(NULL)); 
    printf("Welcome to the Cache simulation program :)\n");
    Cache cache;
    memset((void*)&cache, 0, sizeof(cache));
    pthread_mutex_init(&(cache.cacheLock), NULL);
    displayUserInput(&cache);
    displayContent(&cache);

    int numCacheBlocks = cache.cacheSize / cache.blockSize;
    int numRamBlocks = cache.RAMSize / cache.blockSize;
    cache.data = createRam(&cache);

    Line **Lines = initializeLines(&cache);
    
    createThreads(&cache, Lines);

    displayCacheInfo(Lines, cache.setCount, numCacheBlocks / cache.setCount, cache.blockSize);

    freeRam(cache.data, numRamBlocks);
    freeLines(Lines, cache.setCount, numCacheBlocks / cache.setCount);

    pthread_mutex_destroy(&(cache.cacheLock));


    printf("Cache simulation completed.\n");
    return 0;
}