// Author: Ott-Kaarel Martens
// Date: 25.01.2020

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <locale.h>

// The compressed file is composed of sequential blocks.
// A block is the data structure for encoding one or more sequential characters, according to the lz-77 algorithm.
// A block has the following properties:
// jump - 2 characters
// len - 1 character
// If a character's max value is 128, then the max value for jump is 128x128 and for len 128.
// A block is used in two different ways:
// 1) jump > 0 : jump holds the relative jump to jump back to to, len holds the amount of characters to copy from the jump location
// 2) jump == 0 : jump is zero, len holds the character itself (used on first encountering a character)
struct block
{
    char jump[2];
    char len[1];
};

// A function to get a block from an integer and character
struct block getCharBlock(int jump, char c)
{
    char jumpArr[2];
    if (jump >= 128)
    {
        jumpArr[0] = (int)jump / 128;
    }
    else
    {
        jumpArr[0] = 0;
    }
    jumpArr[1] = jump % 128;

    struct block b = {
        .jump = {jumpArr[0] + '0', jumpArr[1] + '0'},
        .len = {c}};

    return b;
}

// A function to get a block from two integers
struct block getBlock(int jump, int len)
{
    return getCharBlock(jump, len + '0');
}

// Assembler function to increment a value
int increment(int value)
{
    asm("mov %1, %0\n\t"
        "add $1, %0"
        : "=r"(value)
        : "r"(value));

    return value;
}

// Assembler function to decrement a value
int decrement(int value)
{
    asm("mov %1, %0\n\t"
        "sub $1, %0"
        : "=r"(value)
        : "r"(value));

    return value;
}

// Find the last occurrence of segment array in buf array.
// Returns the index of the start of occurrence or 0
int findOffset(char buf[], char segment[])
{

    int bufLen = strlen(buf);
    int segmentLen = strlen(segment);

    for (int i = bufLen - segmentLen; i >= 0; i--)
    {

        for (int j = 0; j < segmentLen; j++)
        {
            if (buf[i + j] != segment[j])
            {
                break;
            }

            if (j == segmentLen - 1)
            {
                return bufLen - i;
            }
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    // Read buffer for reading new data (+1 length for string terminator character)
    char readBuf[128 + 1];

    // Search buffer to search already consumed sequences of data
    char searchBuf[(128 * 128) + 1];

    // An array for characters which we have already seen
    char encounteredChars[128];

    int readChars = 0;

    // source file
    FILE *source = fopen(argv[1], "r");
    if (source == NULL)
    {
        printf("Cannot open file \n");
        exit(EXIT_FAILURE);
    };

    // Destination binary file
    char destFile[128];
    sprintf(destFile, "%s-compressed.bin", argv[1]);
    FILE *dest = fopen(destFile, "wb");

    bool reading = true;
    int r;
    while (reading)
    {
        // Clear the buffers from previous data
        memset(readBuf, 0, sizeof readBuf);
        memset(searchBuf, 0, sizeof searchBuf);

        // Read the amount of already processed data into search buffer
        fread(searchBuf, readChars, 1, source);

        // Read the new data
        r = fread(readBuf, 128, 1, source);

        // String terminating chars
        readBuf[128] = '\0';
        searchBuf[128 * 128] = '\0';

        // No more data to read, exit
        if (strlen(readBuf) == 0)
        {
            reading = false;
            break;
        }

        if (strchr(encounteredChars, readBuf[0]) == NULL)
        {
            // We have not encountered the next character before
            strncat(encounteredChars, &readBuf[0], 1);

            struct block b = getCharBlock(0, readBuf[0]);
            fwrite(&b, sizeof(b), 1, dest);
            memset(&b, 0, sizeof b);

            readChars = increment(readChars);
        }
        else
        {
            // We already have this character somewhere. Find the last occurrence

            int currSegmentLength = 1;
            char currSegment[128] = {readBuf[0]};

            int currOffset = findOffset(searchBuf, currSegment);

            int lastSegmentLength = currSegmentLength;
            int lastOffset = currOffset;

            // Find a previous occurence for the biggest possible sequence of characters.
            while (currOffset > 0)
            {
                lastSegmentLength = currSegmentLength;
                lastOffset = currOffset;

                currSegmentLength = increment(currSegmentLength);
                currSegment[currSegmentLength - 1] = readBuf[currSegmentLength - 1];

                if (strlen(readBuf) < currSegmentLength)
                {
                    break;
                }

                currOffset = findOffset(searchBuf, currSegment);
            }

            // Generate a block from the found segment offset and length
            struct block b = getBlock(lastOffset, lastSegmentLength);

            // printf("will write block jump %d len %d\n", lastOffset, lastSegmentLength);

            fwrite(&b, sizeof(b), 1, dest);
            memset(&b, 0, sizeof b);

            // Move forward in the file for the amount of characters in the found segment
            readChars += lastSegmentLength;
        }

        // Seek back to read the buffers again
        fseek(source, -(strlen(searchBuf) + strlen(readBuf)), SEEK_CUR);
    }

    printf("Compression finished! File name: %s", destFile);

    // Close the files
    fclose(source);
    fclose(dest);
}