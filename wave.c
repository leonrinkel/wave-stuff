#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#ifndef NDEBUG
#define LOG(fmt, ...) fprintf(stderr, fmt, ## __VA_ARGS__)
#else
#define LOG(fmt, ...) (void) 0
#endif

struct wav_chunk
{
    char     id[4];
    uint32_t size;
};

struct wav_riff
{
    char     id[4];
    uint32_t size;
    char     format[4];
};

struct wav_fmt
{
    char     id[4];
    uint32_t size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
};

int main(int argc, char* argv[])
{
    int ret = EXIT_SUCCESS; /* return code to exit with */
    const char* filename; /* filename of the wav file to open */
    FILE* file = NULL; /* file descriptor of the wav file */
    off_t off = 0; /* offset in the wav file */
    size_t nread; /* bytes of data read */
    struct wav_chunk chunk; /* chunk read from wav file */
    struct wav_riff riff = {0}; /* riff chunk read from wav file */
    struct wav_fmt fmt; /* fmt chunk read from wav file */
    void* data = NULL; /* contents of data chunk */
    size_t data_size; /* size of data chunk */
    int nsamples; /* number of samples the file contains */
    int nseconds; /* length of file in seconds */

    assert(sizeof(struct wav_chunk) == 8);
    assert(sizeof(struct wav_riff) == 12);
    assert(sizeof(struct wav_fmt) == 24);

    // parse cli args
    if (argc != 2)
    {
        fprintf(stderr, "usage: wave <file>\n");
        ret = EXIT_FAILURE;
        goto cleanup;
    }

    // open the file
    filename = argv[1];
    file = fopen(filename, "r");
    if (file == NULL)
    {
        perror("unable to open file");
        ret = EXIT_FAILURE;
        goto cleanup;
    }

    // read file chunk by chunk
    while (
        riff.size == 0 || /* size of file not known yet or */
        off < riff.size + 8 /* offset not at the end yet */
    )
    {
        // seek in file
        if (fseek(file, off, SEEK_SET) != 0)
        {
            perror("unable to seek file");
            ret = EXIT_FAILURE;
            goto cleanup;   
        }

        // read next chunk from file
        nread = fread(&chunk, sizeof(struct wav_chunk), 1, file);
        if (nread != 1)
        {
            fprintf(stderr, "unable to read chunk");
            ret = EXIT_FAILURE;
            goto cleanup;
        }

        LOG("chunk: id=%.4s, size=%d\n", chunk.id, chunk.size);

        // check for known chunk types
        if (strncmp(chunk.id, "RIFF", sizeof(chunk.id)) == 0)
        {
            // this is a riff type chunk

            // go back to start of riff chunk
            if (fseek(file, off, SEEK_SET) != 0)
            {
                perror("unable to seek file");
                ret = EXIT_FAILURE;
                goto cleanup;   
            }

            // read riff chunk
            nread = fread(&riff, sizeof(struct wav_riff), 1, file);
            if (nread != 1)
            {
                fprintf(stderr, "unable to read riff chunk");
                ret = EXIT_FAILURE;
                goto cleanup;
            }

            LOG("riff.format=%.4s\n", riff.format);

            // increment offset to next chunk
            off += sizeof(struct wav_riff);
        }
        else if (strncmp(chunk.id, "fmt ", sizeof(chunk.id)) == 0)
        {
            // this is a fmt type chunk

            // go back to start of fmt chunk
            if (fseek(file, off, SEEK_SET) != 0)
            {
                perror("unable to seek file");
                ret = EXIT_FAILURE;
                goto cleanup;   
            }

            // read fmt chunk
            nread = fread(&fmt, sizeof(struct wav_fmt), 1, file);
            if (nread != 1)
            {
                fprintf(stderr, "unable to read fmt chunk");
                ret = EXIT_FAILURE;
                goto cleanup;
            }

            LOG(
                "fmt.audio_format=%d\n"
                "fmt.num_channels=%d\n"
                "fmt.sample_rate=%d\n"
                "fmt.byte_rate=%d\n"
                "fmt.block_align=%d\n"
                "fmt.bits_per_sample=%d\n",
                fmt.audio_format,
                fmt.num_channels,
                fmt.sample_rate,
                fmt.byte_rate,
                fmt.block_align,
                fmt.bits_per_sample
            );

            // increment offset to next chunk
            off += chunk.size + 8;
        }
        else if (strncmp(chunk.id, "LIST", sizeof(chunk.id)) == 0)
        {
            // TODO: check what this chunk does
            off += chunk.size + 8;
        }
        else if (strncmp(chunk.id, "data", sizeof(chunk.id)) == 0)
        {
            // this is a data type chunk

            // allocate mem for data
            data = (void*) malloc(chunk.size);
            data_size = chunk.size;

            // read contents of data chunk
            nread = fread(data, chunk.size, 1, file);
            if (nread != 1)
            {
                fprintf(stderr, "unable to read contents of data chunk");
                ret = EXIT_FAILURE;
                goto cleanup;
            }

            off += chunk.size + 8;
        }
        else
        {
            // don't know this chunk type
            break;
        }
    }

    // calculate some useful values
    nsamples = data_size / fmt.num_channels / (fmt.bits_per_sample / 8);
    nseconds = round(((double) nsamples) / ((double) fmt.sample_rate));

    LOG("nsamples=%d, nseconds=%d\n", nsamples, nseconds);

    // TODO: do something with the data

cleanup:
    if (data != NULL)
    {
        free(data);
    }

    if (file != NULL)
    {
        fclose(file);
    }
    
    return ret;
}