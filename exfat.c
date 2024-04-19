//-----------------------------------------
// NAME: Junyi Lu
// STUDENT NUMBER: 7879161
// COURSE: COMP 3430, SECTION: A01
// INSTRUCTOR: Rob Guderian
// Assignment: A4, QUESTION: question 1
//
// REMARKS: Implementing a File System reader
//
//-----------------------------------------

#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

/**
 * Convert a Unicode-formatted string containing only ASCII characters
 * into a regular ASCII-formatted string (16 bit chars to 8 bit
 * chars).
 *
 * NOTE: this function does a heap allocation for the string it
 *       returns, caller is responsible for `free`-ing the allocation
 *       when necessary.
 *
 * uint16_t *unicode_string: the Unicode-formatted string to be
 *                           converted.
 * uint8_t   length: the length of the Unicode-formatted string (in
 *                   characters).
 *
 * returns: a heap allocated ASCII-formatted string.
 */
static char *unicode2ascii(uint16_t *unicode_string, uint8_t length)
{
    assert(unicode_string != NULL);
    assert(length > 0);

    char *ascii_string = NULL;

    if (unicode_string != NULL && length > 0)
    {
        // +1 for a NULL terminator
        ascii_string = calloc(sizeof(char), length + 1);

        if (ascii_string)
        {
            // strip the top 8 bits from every character in the
            // unicode string
            for (uint8_t i = 0; i < length; i++)
            {
                ascii_string[i] = (char)unicode_string[i];
            }
            // stick a null terminator at the end of the string.
            ascii_string[length] = '\0';
        }
    }

    return ascii_string;
}

#pragma pack(1)
#pragma pack(push)
typedef struct MAIN_BOOT_SECTOR
{
    uint8_t jump_boot[3];
    char fs_name[8];
    uint8_t must_be_zero[53];
    uint64_t partition_offset;
    uint64_t volume_length;
    uint32_t fat_offset;
    uint32_t fat_length;
    uint32_t cluster_heap_offset;
    uint32_t cluster_count;
    uint32_t first_cluster_of_root_directory;
    uint32_t volume_serial_number;
    uint16_t fs_revision;
    uint16_t fs_flags;
    uint8_t bytes_per_sector_shift;
    uint8_t sectors_per_cluster_shift;
    uint8_t number_of_fats;
    uint8_t drive_select;
    uint8_t percent_in_use;
    uint8_t reserved[7];
    uint8_t bootcode[390];
    uint16_t boot_signature;
} main_boot_sector;

typedef struct VOLUME_LABEL_DIRECTORY_ENTRY
{
    uint8_t entry_type;
    uint8_t character_count;
    uint16_t volume_label[22];
    uint8_t reserved[8];

} volume_label_directory_entry;

typedef struct AllOCATION_BITMAP_DIRECTORY_ENTRY
{
    uint8_t entry_type;
    uint8_t bitmap_flag;
    uint8_t reserved[18];
    uint32_t first_cluster;
    uint64_t data_length;

} allocation_bitmap_directory_entry;

typedef struct FILE_DIRECTORY_ENTRY
{
    uint8_t entry_type;
    uint8_t SecondaryCount;
    uint16_t SetChecksum;
    uint16_t FileAttributes;
    uint16_t Reserved1;
    uint32_t CreateTimestamp;
    uint32_t LastModifiedTimestamp;
    uint32_t LastAccessedTimestamp;
    uint8_t Create10msIncrement;
    uint8_t LastModified10msIncrement;
    uint8_t CreateUtcOffset;
    uint8_t LastModifiedUtcOffset;
    uint8_t LastAccessedUtcOffset;
    uint8_t Reserved2[7];

} file;

typedef struct STREAM_EXTENSION_DIRECTORY_ENTRY
{
    uint8_t entry_type;
    char GeneralSecondaryFlags[8];
    uint8_t reserved1;
    uint8_t NameLength;
    uint8_t NameHash;
    uint8_t reserved2[2];
    uint64_t ValidDataLength;
    uint8_t reserved3[4];
    uint32_t FirstCluster;
    uint64_t DataLength;
} stream_extension;

typedef struct FILE_NAME_DIRECTORY_ENTRY
{
    uint8_t entry_type;
    uint8_t GeneralSecondaryFlags;
    uint16_t name[30];

} file_name;

#pragma pack(pop)

typedef enum bool
{
    zero = 0,
    one = 1
} boolean;

boolean true = one;
boolean false = zero;

//------------------------------------------------------
// getFatPosition
//
// PURPOSE:
// Locating next cluster index in fat region
// INPUT PARAMETERS:
// currCluster:curr cluster index
// boot:main boot region
//------------------------------------------------------
uint32_t getFatPosition(uint32_t currCluster, main_boot_sector *boot)
{
    uint64_t sectorSize = 1 << boot->bytes_per_sector_shift;
    uint32_t position = boot->fat_offset * sectorSize + currCluster * (sizeof(uint32_t));
    return position;
}

//------------------------------------------------------
// getClusterPosition
//
// PURPOSE:
// Locating curr cluster position in cluster heap
// INPUT PARAMETERS:
// start:curr cluster index
// boot:main boot region
//------------------------------------------------------
uint32_t getClusterPosition(uint32_t start, main_boot_sector *boot)
{
    uint64_t sectorSize = 1 << boot->bytes_per_sector_shift;
    uint64_t clusterSize = (1 << boot->sectors_per_cluster_shift) * sectorSize;
    uint32_t position = (boot->cluster_heap_offset * sectorSize) + (start - 2) * clusterSize;
    return position;
}

//------------------------------------------------------
// printName
//
// PURPOSE:
// according to list command, print sample output
// INPUT PARAMETERS:
// filename: speific file name
// streamExtension: stream extension entry
// isFile: boolean for file attributes
// isDirectory:boolean for file attributes
// num: where are we in the subdirectory
//------------------------------------------------------
void printName(char *filename, stream_extension *streamExtension, boolean isFile, boolean isDirectory, int nums)
{
    int i;
    for (i = 0; i < nums; i++)
    {
        printf("-");
    }
    if (isFile && streamExtension->NameLength != 0)
    {
        printf("File: %s\n", filename);
    }
    else if (isDirectory && streamExtension->NameLength != 0)
    {
        printf("Directory: %s\n", filename);
    }
}

void handleInfo(main_boot_sector *boot, int handle);
void handleList(main_boot_sector *boot, file *files, stream_extension *streamExtension, file_name *filename, int handle);
void listrecursion(main_boot_sector *boot, file *files, stream_extension *streamExtension, file_name *filename, uint32_t cluster, int handle, int count);
void handleGet(main_boot_sector *boot, file *files, stream_extension *streamExtension, file_name *filename, char *path, int handle);
void getrecursion(main_boot_sector *boot, file *files, stream_extension *streamExtension, file_name *filename, uint32_t cluster, int handle, char *path[], int count);

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        if (argc != 4)
        {
            perror("Error for running the program\n");
            exit(1);
        }
    }

    char *fileName = argv[1];
    char *command = argv[2];
    main_boot_sector boot;
    file files;
    stream_extension streamExtension;
    file_name filename;

    int fd = open(fileName, O_RDONLY);

    if (fd == -1)
    {
        perror("where the input file?\n");
    }

    read(fd, &boot, sizeof(main_boot_sector));

    if (strcmp(command, "info") == 0)
    {
        handleInfo(&boot, fd);
    }

    if (strcmp(command, "list") == 0)
    {
        handleList(&boot, &files, &streamExtension, &filename, fd);
    }

    if (strcmp(command, "get") == 0)
    {
        handleGet(&boot, &files, &streamExtension, &filename, argv[3], fd);
    }

    return EXIT_SUCCESS;
}

//------------------------------------------------------
// handleInfo
//
// PURPOSE:
// handle the info command
// INPUT PARAMETERS:
// handle:file content
// boot:main boot sector
//------------------------------------------------------
void handleInfo(main_boot_sector *boot, int handle)
{
    assert(boot != NULL);
    assert(handle > 0);
    volume_label_directory_entry *volumeLabel = malloc(sizeof(volume_label_directory_entry));
    allocation_bitmap_directory_entry *bitmap = malloc(sizeof(allocation_bitmap_directory_entry));
    int used = 0; // same as the lab4 to check the use status
    uint8_t temp;
    uint8_t data_content;
    int sectorSize = 1 << boot->bytes_per_sector_shift;
    int clusterSize = (1 << boot->sectors_per_cluster_shift) * sectorSize;

    uint32_t cluster = getClusterPosition(boot->first_cluster_of_root_directory, boot);
    lseek(handle, cluster, SEEK_SET);

    while (true)
    {
        read(handle, &temp, 1);
        if (temp == 0x83)
        {
            lseek(handle, -1, SEEK_CUR);
            read(handle, &volumeLabel->entry_type, 1);
            read(handle, &volumeLabel->character_count, 1);
            read(handle, &volumeLabel->volume_label, 22);
            read(handle, &volumeLabel->reserved, 8);
            break;
        }
        lseek(handle, 31, SEEK_CUR);
    }


    lseek(handle, cluster, SEEK_SET);

    while (true)
    {
        read(handle, &temp, 1);

        if (temp == 0x81)
        {
            lseek(handle, -1, SEEK_CUR);
            read(handle, &bitmap->entry_type, 1);
            read(handle, &bitmap->bitmap_flag, 1);
            read(handle, &bitmap->reserved, 18);
            read(handle, &bitmap->first_cluster, 4);
            read(handle, &bitmap->data_length, 8);
            break;
        }

        lseek(handle, 31, SEEK_CUR);
    }

    cluster = getClusterPosition(bitmap->first_cluster, boot); // jump to fist cluster index position
    lseek(handle, cluster, SEEK_SET);

    uint64_t i;
    for (i = 0; i < bitmap->data_length; i++)
    {
        read(handle, &data_content, 1);
        used += __builtin_popcount(data_content);
    }

    printf("-----INFO PART-----\n");
    printf("Volume label: %s\n", unicode2ascii(volumeLabel->volume_label, 22));
    printf("Volume serial number: %u\n", boot->volume_serial_number);
    printf("Free space on the volume in %u KB\n", ((boot->cluster_count - used) * clusterSize) / 1024);
    printf("The cluster size: in %d sectors and in %d bytes\n", 1 << boot->sectors_per_cluster_shift, clusterSize);
}

//------------------------------------------------------
// handleList
//
// PURPOSE:
// handle the List command
// INPUT PARAMETERS:
// handle:file content
// boot:main boot sector
// streamExtension: stream extension entry
// files:file and directory entry
// filename: file name entry
//------------------------------------------------------
void handleList(main_boot_sector *boot, file *files, stream_extension *streamExtension, file_name *filename, int handle)
{
    uint64_t sectorSize = 1 << boot->bytes_per_sector_shift;
    uint64_t clusterSize = (1 << boot->sectors_per_cluster_shift) * sectorSize;
    boolean isDirectory = false;
    boolean isFile = false;
    uint8_t temp;
    uint64_t countSize = 0;
    uint32_t cluster = boot->first_cluster_of_root_directory;
    uint32_t nextCluster = cluster;

    do
    {

        uint32_t fatPos = getFatPosition(cluster, boot);
        uint32_t pos = getClusterPosition(cluster, boot);
        lseek(handle, fatPos, SEEK_SET);
        read(handle, &nextCluster, sizeof(uint32_t));
        lseek(handle, pos, SEEK_SET);

        while (countSize < clusterSize)
        {
            isDirectory = false;
            isFile = false;
            read(handle, &temp, 1);
            if (temp == 0x85)
            {
                lseek(handle, -1, SEEK_CUR);
                read(handle, &files->entry_type, 1);
                read(handle, &files->SecondaryCount, 1);
                read(handle, &files->SetChecksum, 2);
                read(handle, &files->FileAttributes, 2);
                read(handle, &files->Reserved1, 2);
                read(handle, &files->CreateTimestamp, 4);
                read(handle, &files->LastModifiedTimestamp, 4);
                read(handle, &files->LastAccessedTimestamp, 4);
                read(handle, &files->Create10msIncrement, 1);
                read(handle, &files->LastModified10msIncrement, 1);
                read(handle, &files->CreateUtcOffset, 1);
                read(handle, &files->LastModifiedUtcOffset, 1);
                read(handle, &files->LastAccessedUtcOffset, 1);
                read(handle, &files->Reserved2, 7);
                countSize += 32;
            }

            else if (temp == 0xC0)
            {
                lseek(handle, -1, SEEK_CUR);
                read(handle, &streamExtension->entry_type, 1);
                read(handle, &streamExtension->GeneralSecondaryFlags, 1);
                read(handle, &streamExtension->reserved1, 1);
                read(handle, &streamExtension->NameLength, 1);
                read(handle, &streamExtension->NameHash, 2);
                read(handle, &streamExtension->reserved2, 2);
                read(handle, &streamExtension->ValidDataLength, 8);
                read(handle, &streamExtension->reserved3, 4);
                read(handle, &streamExtension->FirstCluster, 4);
                read(handle, &streamExtension->DataLength, 8);
                countSize += 32;
            }

            else if (temp == 0xC1)
            {
                lseek(handle, -1, SEEK_CUR);
                int i;
                uint16_t name[255];
                int nameIndex = 0;
                char *realName;

                // since can only contain 15 unicode characters but maxmimum is 255
                for (i = 0; i < files->SecondaryCount - 1; ++i)
                {
                    read(handle, &filename->entry_type, 1);
                    read(handle, &filename->GeneralSecondaryFlags, 1);
                    read(handle, &filename->name, 30);
                    int j;
                    for (j = 0; j < 15; ++j)
                    {
                        name[nameIndex++] = filename->name[j];
                    }
                }
                realName = unicode2ascii(name, nameIndex);

                countSize += 32;

                if ((files->FileAttributes & (1 << 4)) >> 4 == 1)
                {
                    isDirectory = true;
                }
                else if ((files->FileAttributes & (1 << 4)) >> 4 == 0)
                {
                    isFile = true;
                }

                printName(realName, streamExtension, isFile, isDirectory, 0);
            }

            else
            {
                lseek(handle, 95, SEEK_CUR);
                countSize += 96;
            }

            // if we encouter a directory in root dirctory,we need to do recursion and find the file in the deep.
            if (isDirectory)
            {
                uint32_t originalPlace = lseek(handle, 0, SEEK_CUR);
                listrecursion(boot, files, streamExtension, filename, streamExtension->FirstCluster, handle, 1);
                lseek(handle, originalPlace, SEEK_SET);
            }
        }
        countSize = 0;
        cluster = nextCluster;
    } while (cluster != 0xffffffff && cluster != 0); // 0xffffffff is null
}
//------------------------------------------------------
// listrecursion
//
// PURPOSE:
// handle the List command
// INPUT PARAMETERS:
// handle:file content
// boot:main boot sector
// streamExtension: stream extension entry
// files:file and directory entry
// filename: file name entry
// cluster: first cluster belongs to directory
// count: count the depth
//------------------------------------------------------
void listrecursion(main_boot_sector *boot, file *files, stream_extension *streamExtension, file_name *fileName, uint32_t cluster, int handle, int count)
{
    boolean isDirectory = false;
    boolean isFile = false;
    uint64_t sectorSize = 1 << boot->bytes_per_sector_shift;
    uint64_t clusterSize = (1 << boot->sectors_per_cluster_shift) * sectorSize;
    uint8_t temp;
    uint64_t countSize = 0;
    uint32_t nextCluster = cluster;

    do
    {
        uint32_t fatPos = getFatPosition(cluster, boot);
        uint32_t pos = getClusterPosition(cluster, boot);
        lseek(handle, fatPos, SEEK_SET);
        read(handle, &nextCluster, sizeof(uint32_t));

        lseek(handle, pos, SEEK_SET);

        boolean isFirst = false;
        boolean isSecond = false;
        boolean isThird = false;

        while (countSize < clusterSize)
        {
            isDirectory = false;
            isFile = false;
            char *realName;

            read(handle, &temp, 1);
            if (temp == 0x85)
            {
                lseek(handle, -1, SEEK_CUR);
                read(handle, &files->entry_type, 1);
                read(handle, &files->SecondaryCount, 1);
                read(handle, &files->SetChecksum, 2);
                read(handle, &files->FileAttributes, 2);
                read(handle, &files->Reserved1, 2);
                read(handle, &files->CreateTimestamp, 4);
                read(handle, &files->LastModifiedTimestamp, 4);
                read(handle, &files->LastAccessedTimestamp, 4);
                read(handle, &files->Create10msIncrement, 1);
                read(handle, &files->LastModified10msIncrement, 1);
                read(handle, &files->CreateUtcOffset, 1);
                read(handle, &files->LastModifiedUtcOffset, 1);
                read(handle, &files->LastAccessedUtcOffset, 1);
                read(handle, &files->Reserved2, 7);
                isFirst = true;
                countSize += 32;
            }

            else if (temp == 0xC0)
            {
                lseek(handle, -1, SEEK_CUR);
                read(handle, &streamExtension->entry_type, 1);
                read(handle, &streamExtension->GeneralSecondaryFlags, 1);
                read(handle, &streamExtension->reserved1, 1);
                read(handle, &streamExtension->NameLength, 1);
                read(handle, &streamExtension->NameHash, 2);
                read(handle, &streamExtension->reserved2, 2);
                read(handle, &streamExtension->ValidDataLength, 8);
                read(handle, &streamExtension->reserved3, 4);
                read(handle, &streamExtension->FirstCluster, 4);
                read(handle, &streamExtension->DataLength, 8);
                isSecond = true;
                countSize += 32;
            }

            else if (temp == 0xC1)
            {

                lseek(handle, -1, SEEK_CUR);
                int i;
                uint16_t name[255];
                int nameIndex = 0;

                // since can only contain 15 unicode characters but maxmimum is 255
                for (i = 0; i < files->SecondaryCount - 1; ++i)
                {
                    read(handle, &fileName->entry_type, 1);
                    read(handle, &fileName->GeneralSecondaryFlags, 1);
                    read(handle, &fileName->name, 30);
                    int j;
                    for (j = 0; j < 15; ++j)
                    {
                        name[nameIndex++] = fileName->name[j];
                    }
                }
                realName = unicode2ascii(name, nameIndex);

                isThird = true;
                countSize += 32;
            }

            else
            {
                lseek(handle, 95, SEEK_CUR);
                countSize += 96;
            }

            // since while only check one entry once, when we have 3 complete entry,we start processing.
            if (isFirst && isSecond && isThird)
            {
                if ((files->FileAttributes & (1 << 4)) >> 4 == 1)
                {
                    isDirectory = true;
                }
                else if ((files->FileAttributes & (1 << 4)) >> 4 == 0)
                {
                    isFile = true;
                }
                else
                {
                    printf("impossible here\n");
                }

                printName(realName, streamExtension, isFile, isDirectory, count);

                isFirst = false;
                isSecond = false;
                isThird = false;
            }

            // if we encounter directory,we still need to go inside.
            if (isDirectory)
            {
                listrecursion(boot, files, streamExtension, fileName, streamExtension->FirstCluster, handle, count + 1);
            }
        }
        cluster = nextCluster;
        countSize = 0;
    } while (cluster != 0xfffffff8); // 0xfffffff8 is bad cluster
}
//------------------------------------------------------
// handleGet
//
// PURPOSE:
// handle the get command
// INPUT PARAMETERS:
// handle:file content
// boot:main boot sector
// streamExtension: stream extension entry
// files:file and directory entry
// filename: file name entry
// path: from command, it is the path we need to find
//------------------------------------------------------
void handleGet(main_boot_sector *boot, file *files, stream_extension *streamExtension, file_name *filename, char *path, int handle)
{
    char *pathArr[strlen(path)];
    char *partPath;
    boolean hasDirectory = false;
    int count = 0;
    uint64_t sectorSize = 1 << boot->bytes_per_sector_shift;
    uint64_t clusterSize = (1 << boot->sectors_per_cluster_shift) * sectorSize;
    boolean isDirectory = false;
    boolean isFile = false;
    uint8_t temp;
    uint64_t needToRead;
    uint64_t countSize = 0;
    uint32_t cluster = boot->first_cluster_of_root_directory;
    uint32_t nextCluster = cluster;
    char *buffer = (char *)malloc(clusterSize); // buffer is content for output file

    if (path[0] == 0)
    {
        perror("path is error\n");
        exit(1);
    }

    // if first char is '/'
    if (path[0] == '/')
    {
        path++;
    }

    int i;
    int length = strlen(path);
    for (i = 0; i < length; i++)
    {
        if (path[i] == '/')
        {
            hasDirectory = true;
            break;
        }
    }

    // if we have a dictory in path
    partPath = strtok(path, "/");

    while (partPath != NULL)
    {
        pathArr[count] = partPath;
        partPath = strtok(NULL, "/");
        count++;
    }

    do
    {

        uint32_t fatPos = getFatPosition(cluster, boot);
        uint32_t pos = getClusterPosition(cluster, boot);
        lseek(handle, fatPos, SEEK_SET);
        read(handle, &nextCluster, sizeof(uint32_t));
        lseek(handle, pos, SEEK_SET);

        while (countSize < clusterSize)
        {
            char *realName;
            isDirectory = false;
            isFile = false;
            read(handle, &temp, 1);
            if (temp == 0x85)
            {
                lseek(handle, -1, SEEK_CUR);
                read(handle, &files->entry_type, 1);
                read(handle, &files->SecondaryCount, 1);
                read(handle, &files->SetChecksum, 2);
                read(handle, &files->FileAttributes, 2);
                read(handle, &files->Reserved1, 2);
                read(handle, &files->CreateTimestamp, 4);
                read(handle, &files->LastModifiedTimestamp, 4);
                read(handle, &files->LastAccessedTimestamp, 4);
                read(handle, &files->Create10msIncrement, 1);
                read(handle, &files->LastModified10msIncrement, 1);
                read(handle, &files->CreateUtcOffset, 1);
                read(handle, &files->LastModifiedUtcOffset, 1);
                read(handle, &files->LastAccessedUtcOffset, 1);
                read(handle, &files->Reserved2, 7);
                countSize += 32;
            }

            else if (temp == 0xC0)
            {
                lseek(handle, -1, SEEK_CUR);
                read(handle, &streamExtension->entry_type, 1);
                read(handle, &streamExtension->GeneralSecondaryFlags, 1);
                read(handle, &streamExtension->reserved1, 1);
                read(handle, &streamExtension->NameLength, 1);
                read(handle, &streamExtension->NameHash, 2);
                read(handle, &streamExtension->reserved2, 2);
                read(handle, &streamExtension->ValidDataLength, 8);
                read(handle, &streamExtension->reserved3, 4);
                read(handle, &streamExtension->FirstCluster, 4);
                read(handle, &streamExtension->DataLength, 8);
                countSize += 32;
            }

            else if (temp == 0xC1)
            {
                lseek(handle, -1, SEEK_CUR);
                int i;
                uint16_t name[255];
                int nameIndex = 0;
                for (i = 0; i < files->SecondaryCount - 1; ++i)
                {
                    read(handle, &filename->entry_type, 1);
                    read(handle, &filename->GeneralSecondaryFlags, 1);
                    read(handle, &filename->name, 30);
                    int j;
                    for (j = 0; j < 15; ++j)
                    {
                        name[nameIndex++] = filename->name[j];
                    }
                }
                realName = unicode2ascii(name, nameIndex);

                countSize += 32;

                if ((files->FileAttributes & (1 << 4)) >> 4 == 1)
                {
                    isDirectory = true;
                }
                else if ((files->FileAttributes & (1 << 4)) >> 4 == 0)
                {
                    isFile = true;
                }

                if (isFile)
                {
                    // if we encounter a file, also it is the file we want to find
                    if (strcmp(path, realName) == 0)
                    {
                        int out = open(realName, O_TRUNC | O_CREAT | O_WRONLY | O_APPEND, S_IRWXU);
                        uint32_t firstClusterIndex = streamExtension->FirstCluster;
                        needToRead = streamExtension->DataLength;
                        uint32_t tempPos = getClusterPosition(firstClusterIndex, boot);
                        lseek(handle, tempPos, SEEK_SET);
                        do
                        {

                            int length = read(handle, buffer, 1);
                            needToRead -= length;
                            write(out, buffer, length);

                        } while (needToRead > 0);
                        printf("FOUND PATH: %s \n", path);
                        close(out);
                        return;
                    }
                }
            }

            else
            {
                lseek(handle, 95, SEEK_CUR);
                countSize += 96;
            }

            if (isDirectory)
            {
                // we only do recusion when we have a directory in our path
                if (strcmp(pathArr[0], realName) == 0 && hasDirectory)
                {
                    uint32_t originalPlace = lseek(handle, 0, SEEK_CUR);
                    getrecursion(boot, files, streamExtension, filename, streamExtension->FirstCluster, handle, pathArr, count);
                    lseek(handle, originalPlace, SEEK_SET);
                }
            }
        }
        countSize = 0;
        cluster = nextCluster;
    } while (cluster != 0xffffffff && cluster != 0);
}

//------------------------------------------------------
// getrecursion
//
// PURPOSE:
// handle the get command
// INPUT PARAMETERS:
// handle:file content
// boot:main boot sector
// streamExtension: stream extension entry
// files:file and directory entry
// filename: file name entry
// cluster: first cluster of directory
// count:size of our path char pointer array
// path: from command, it is the path we need to find
//------------------------------------------------------
void getrecursion(main_boot_sector *boot, file *files, stream_extension *streamExtension, file_name *filename, uint32_t cluster, int handle, char *path[], int count)
{
    boolean isDirectory = false;
    boolean isFile = false;
    uint64_t sectorSize = 1 << boot->bytes_per_sector_shift;
    uint64_t clusterSize = (1 << boot->sectors_per_cluster_shift) * sectorSize;
    uint8_t temp;
    uint64_t countSize = 0;
    uint64_t needToRead;
    uint32_t nextCluster = cluster;
    char *buffer = (char *)malloc(sizeof(char)); // buffer is content for output file

    if (path[0] == 0)
    {
        perror("path is error\n");
        exit(1);
    }

    do
    {
        uint32_t fatPos = getFatPosition(cluster, boot);
        uint32_t pos = getClusterPosition(cluster, boot);
        lseek(handle, fatPos, SEEK_SET);
        read(handle, &nextCluster, sizeof(uint32_t));

        lseek(handle, pos, SEEK_SET);

        boolean isFirst = false;
        boolean isSecond = false;
        boolean isThird = false;

        while (countSize < clusterSize)
        {
            isDirectory = false;
            isFile = false;
            char *realName;
            read(handle, &temp, 1);
            if (temp == 0x85)
            {
                lseek(handle, -1, SEEK_CUR);
                read(handle, &files->entry_type, 1);
                read(handle, &files->SecondaryCount, 1);
                read(handle, &files->SetChecksum, 2);
                read(handle, &files->FileAttributes, 2);
                read(handle, &files->Reserved1, 2);
                read(handle, &files->CreateTimestamp, 4);
                read(handle, &files->LastModifiedTimestamp, 4);
                read(handle, &files->LastAccessedTimestamp, 4);
                read(handle, &files->Create10msIncrement, 1);
                read(handle, &files->LastModified10msIncrement, 1);
                read(handle, &files->CreateUtcOffset, 1);
                read(handle, &files->LastModifiedUtcOffset, 1);
                read(handle, &files->LastAccessedUtcOffset, 1);
                read(handle, &files->Reserved2, 7);
                isFirst = true;
                countSize += 32;
            }
            else if (temp == 0xC0)
            {
                lseek(handle, -1, SEEK_CUR);
                read(handle, &streamExtension->entry_type, 1);
                read(handle, &streamExtension->GeneralSecondaryFlags, 1);
                read(handle, &streamExtension->reserved1, 1);
                read(handle, &streamExtension->NameLength, 1);
                read(handle, &streamExtension->NameHash, 2);
                read(handle, &streamExtension->reserved2, 2);
                read(handle, &streamExtension->ValidDataLength, 8);
                read(handle, &streamExtension->reserved3, 4);
                read(handle, &streamExtension->FirstCluster, 4);
                read(handle, &streamExtension->DataLength, 8);
                isSecond = true;
                countSize += 32;
            }

            else if (temp == 0xC1)
            {

                lseek(handle, -1, SEEK_CUR);
                int i;
                uint16_t name[255];
                int nameIndex = 0;
                for (i = 0; i < files->SecondaryCount - 1; ++i)
                {
                    read(handle, &filename->entry_type, 1);
                    read(handle, &filename->GeneralSecondaryFlags, 1);
                    read(handle, &filename->name, 30);
                    int j;
                    for (j = 0; j < 15; ++j)
                    {
                        name[nameIndex++] = filename->name[j];
                    }
                }
                realName = unicode2ascii(name, nameIndex);

                isThird = true;
                countSize += 32;
            }

            else
            {
                lseek(handle, 95, SEEK_CUR);
                countSize += 96;
            }

            if (isFirst && isSecond && isThird)
            {
                if ((files->FileAttributes & (1 << 4)) >> 4 == 1)
                {
                    isDirectory = true;
                }
                else if ((files->FileAttributes & (1 << 4)) >> 4 == 0)
                {
                    isFile = true;
                }
                else
                {
                    printf("impossible here\n");
                }

                isFirst = false;
                isSecond = false;
                isThird = false;

                if (isFile)
                {
                    // if we encounter a file, also it is the file we want to find
                    if (strcmp(path[count - 1], realName) == 0)
                    {
                        int out = open(realName, O_TRUNC | O_CREAT | O_WRONLY | O_APPEND, S_IRWXU);
                        uint32_t firstClusterIndex = streamExtension->FirstCluster;
                        needToRead = streamExtension->DataLength;
                        uint32_t tempPos = getClusterPosition(firstClusterIndex, boot);
                        lseek(handle, tempPos, SEEK_SET);
                        do
                        {
                            int length = read(handle, buffer, 1);
                            needToRead -= length;
                            write(out, buffer, length);
                        } while (needToRead > 0);

                        printf("FOUND PATH: %s \n", realName);
                        close(out);
                        return;
                    }
                }
            }

            // if we encounter directory,we still need to go inside.
            if (isDirectory)
            {
                getrecursion(boot, files, streamExtension, filename, streamExtension->FirstCluster, handle, path, count--);
            }
        }
        cluster = nextCluster;
        countSize = 0;
    } while (cluster != 0xfffffff8);
}
