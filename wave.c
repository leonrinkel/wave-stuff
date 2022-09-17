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
    const char* in_filename; /* filename of the input wav file */
    const char* out_filename; /* filename of the output wav file */
    FILE* in_file = NULL; /* file descriptor of the input wav file */
    off_t off = 0; /* offset in the input wav file */
    size_t nread; /* bytes of data read */
    struct wav_chunk chunk; /* chunk read/written from/to wav file */
    struct wav_riff riff = {0}; /* riff chunk read/written from/to wav file */
    struct wav_fmt fmt; /* fmt chunk read/written from/to wav file */
    void* data = NULL; /* contents of data chunk */
    size_t data_size; /* size of data chunk */
    int nsamples; /* number of samples the file contains */
    int nseconds; /* length of file in seconds */
    FILE* out_file = NULL; /* file descriptor of the output wav file */

    assert(sizeof(struct wav_chunk) == 8);
    assert(sizeof(struct wav_riff) == 12);
    assert(sizeof(struct wav_fmt) == 24);

    // parse cli args
    if (argc != 3)
    {
        fprintf(stderr, "usage: wave <out> <in>\n");
        ret = EXIT_FAILURE;
        goto cleanup;
    }
    out_filename = argv[1];
    in_filename = argv[2];

    // open the input file
    in_file = fopen(in_filename, "r");
    if (in_file == NULL)
    {
        perror("unable to open file");
        ret = EXIT_FAILURE;
        goto cleanup;
    }

    // read input file chunk by chunk
    while (
        riff.size == 0 || /* size of file not known yet or */
        off < riff.size + 8 /* offset not at the end yet */
    )
    {
        // seek in file
        if (fseek(in_file, off, SEEK_SET) != 0)
        {
            perror("unable to seek file");
            ret = EXIT_FAILURE;
            goto cleanup;   
        }

        // read next chunk from file
        nread = fread(&chunk, sizeof(struct wav_chunk), 1, in_file);
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
            if (fseek(in_file, off, SEEK_SET) != 0)
            {
                perror("unable to seek file");
                ret = EXIT_FAILURE;
                goto cleanup;   
            }

            // read riff chunk
            nread = fread(&riff, sizeof(struct wav_riff), 1, in_file);
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
            if (fseek(in_file, off, SEEK_SET) != 0)
            {
                perror("unable to seek file");
                ret = EXIT_FAILURE;
                goto cleanup;   
            }

            // read fmt chunk
            nread = fread(&fmt, sizeof(struct wav_fmt), 1, in_file);
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
            nread = fread(data, chunk.size, 1, in_file);
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

    // the following stuff is only implemented for 16 bit stereo pcm
    if (
        fmt.bits_per_sample != 16 ||
        fmt.audio_format != 1 ||
        fmt.num_channels != 2
    )
    {
        fprintf(stderr, "unsupported format\n");
        ret = EXIT_FAILURE;
        goto cleanup;
    }

    /* do something with the data, in this example: apply sin() to left channel
     * and cos() to right channel, so it sounds as if the audio source moves
     * back and forth between the left and right channel */
    for (int sample = 0; sample < nsamples; sample++)
    {
        // left and right channel are interleaved
        int16_t* l = (((int16_t*) data) + 0 + sample * fmt.num_channels);
        int16_t* r = (((int16_t*) data) + 1 + sample * fmt.num_channels);

        // period is 2 seconds
        double x = ((double) sample) / ((double) fmt.sample_rate * 2);
        *l = ((double) *l) * sin(x * 2 * M_PI);
        *r = ((double) *r) * cos(x * 2 * M_PI);
    }

    // open the output file
    out_file = fopen(out_filename, "w");
    if (out_file == NULL)
    {
        perror("unable to open file");
        ret = EXIT_FAILURE;
        goto cleanup;
    }

    // reuse chunk variable to mark data chunk
    strncpy(chunk.id, "data", 4);
    chunk.size = data_size;

    // write the chunks to output file
    fwrite(&riff, sizeof(struct wav_riff), 1, out_file);
    fwrite(&fmt, sizeof(struct wav_fmt), 1, out_file);
    fwrite(&chunk, sizeof(struct wav_chunk), 1, out_file);
    fwrite(data, data_size, 1, out_file);

cleanup:
    if (out_file != NULL)
    {
        fclose(out_file);
    }

    if (data != NULL)
    {
        free(data);
    }

    if (in_file != NULL)
    {
        fclose(in_file);
    }
    
    return ret;
}