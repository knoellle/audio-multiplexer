#include <cstddef>
#include <ctime>
#include <vector>
#include <list>
#include <chrono>

#include <jack/jack.h>
#include <jack/types.h>
#include <jack/ringbuffer.h>
#include <soundtouch/SoundTouch.h>

using SampleBlock = std::vector<jack_default_audio_sample_t>;

struct Channel
{
    jack_port_t* port;
    int silence_counter;
    std::chrono::steady_clock::time_point lastPlayed;
    std::list<SampleBlock> sampleBuffer;
};

class Multiplexer
{
    jack_client_t* client_;
    jack_port_t* outputPort_;
    jack_ringbuffer_t *outputBuffer_;
    std::vector<Channel> channels_;
    size_t currentChannel_;

    soundtouch::SoundTouch soundTouch;

    static int processWrapper(jack_nframes_t nSamples, void* arg);
    int process(jack_nframes_t nSamples);

  public:

    explicit Multiplexer(const int numChannels)
        : client_(nullptr)
        , outputPort_(nullptr)
        , channels_(numChannels)
        , currentChannel_(0)
    {
    }
    ~Multiplexer();
    void initJack();
};
