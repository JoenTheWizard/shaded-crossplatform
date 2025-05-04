#include <stdio.h>
#include <portaudio.h>
#include <mpg123.h>

#define SAMPLE_RATE 44100
#define CHANNELS 2
#define FORMAT MPG123_ENC_SIGNED_16  // 16-bit PCM
#define BUFFER_SIZE 4096

// PortAudio callback function
static int audioCallback(
    const void *input, void *output,
    unsigned long frameCount,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void *userData
) {
    mpg123_handle *mh = (mpg123_handle *)userData;
    size_t bytes_read;
    int err = mpg123_read(mh, output, frameCount * CHANNELS * sizeof(short), &bytes_read);

    if (err == MPG123_DONE) {
        return paComplete;  // End of file
    } else if (err != MPG123_OK) {
        fprintf(stderr, "mpg123_read error: %s\n", mpg123_strerror(mh));
        return paAbort;
    }

    return paContinue;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <mp3_file>\n", argv[0]);
        return 1;
    }

    // Initialize mpg123
    mpg123_init();
    mpg123_handle *mh = mpg123_new(NULL, NULL);
    if (!mh) {
        fprintf(stderr, "Failed to create mpg123 handle\n");
        return 1;
    }

    // Open MP3 file
    if (mpg123_open(mh, argv[1]) != MPG123_OK) {
        fprintf(stderr, "Failed to open MP3 file: %s\n", mpg123_strerror(mh));
        mpg123_delete(mh);
        mpg123_exit();
        return 1;
    }

    // Ensure output format is 16-bit stereo PCM
    long rate;
    int channels, encoding;
    mpg123_getformat(mh, &rate, &channels, &encoding);
    mpg123_format_none(mh);
    mpg123_format(mh, rate, channels, FORMAT);

    // Initialize PortAudio
    PaError pa_err = Pa_Initialize();
    if (pa_err != paNoError) {
        fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(pa_err));
        mpg123_close(mh);
        mpg123_delete(mh);
        mpg123_exit();
        return 1;
    }

    // Open PortAudio stream
    PaStream *stream;
    pa_err = Pa_OpenDefaultStream(
        &stream,
        0,          // No input
        CHANNELS,   // Stereo output
        paInt16,    // 16-bit PCM
        rate,       // Sample rate
        BUFFER_SIZE, // Frames per buffer
        audioCallback,
        mh          // User data (mpg123 handle)
    );

    if (pa_err != paNoError) {
        fprintf(stderr, "PortAudio stream error: %s\n", Pa_GetErrorText(pa_err));
        mpg123_close(mh);
        mpg123_delete(mh);
        mpg123_exit();
        Pa_Terminate();
        return 1;
    }

    // Start playback
    pa_err = Pa_StartStream(stream);
    if (pa_err != paNoError) {
        fprintf(stderr, "PortAudio start error: %s\n", Pa_GetErrorText(pa_err));
        Pa_CloseStream(stream);
        mpg123_close(mh);
        mpg123_delete(mh);
        mpg123_exit();
        Pa_Terminate();
        return 1;
    }

    printf("Playing MP3 file: %s\nPress Enter to stop...\n", argv[1]);
    getchar();  // Wait for user to press Enter

    // Cleanup
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();

    return 0;
}
