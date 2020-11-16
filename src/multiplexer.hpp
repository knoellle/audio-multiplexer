#include <jack/jack.h>
#include <jack/types.h>
#include <vector>

using SampleBlock = std::vector<jack_default_audio_sample_t>;

struct Channel
{
    jack_port_t* port;
    std::vector<SampleBlock> sampleBuffer;
};

class Multiplexer
{
    jack_client_t* client_;
    jack_port_t* outputPort_;
    std::vector<Channel> channels_;

    static int processWrapper(jack_nframes_t nSamples, void* arg);
    int process(jack_nframes_t nSamples);

  public:

    explicit Multiplexer(const int numChannels)
        : client_(nullptr)
        , outputPort_(nullptr)
        , channels_(numChannels)
    {
    }
    ~Multiplexer();
    void initJack();
};
