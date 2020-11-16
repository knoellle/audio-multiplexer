#include <iostream>
#include <memory>
#include <cstring>
#include <sstream>

#include <jack/jack.h>
#include <jack/types.h>
#include <string>

#include "multiplexer.hpp"

void Multiplexer::initJack()
{
    client_ = jack_client_open("Audio Interlacer", JackNullOption, nullptr);
    if (client_ == NULL)
    {
        std::cerr << "Could not create JACK client.\n";
        exit(1);
    }
    outputPort_ = jack_port_register(client_, "output", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    if (outputPort_ == NULL)
    {
        std::cerr << "Could not register port.\n";
        exit(1);
    }
    int i = 0;
    for (auto &channel : channels_)
    {
        ++i;
        std::stringstream namestream;
        namestream << i << " left";
        channel.port = jack_port_register(client_, namestream.str().c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        // channel.port = jack_port_register(client_, "right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    }

    jack_set_process_callback(client_, Multiplexer::processWrapper, this);

    int r = jack_activate(client_);
    if (r != 0)
    {
        std::cerr << "Could not activate client.\n";
        exit(1);
    }
}

Multiplexer::~Multiplexer()
{
    jack_client_close(client_);
}

int Multiplexer::process(jack_nframes_t nSamples)
{
    auto *out = jack_port_get_buffer(outputPort_, nSamples);
    for (auto &channel : channels_)
    {
        auto *input = static_cast<jack_default_audio_sample_t*>(jack_port_get_buffer(channel.port, nSamples));
        std::cout << channel.port << " - " << input << "\n";
        if (input)
            std::memcpy(out, input, nSamples * sizeof(jack_default_audio_sample_t));
        for (int i = 0; i < nSamples; ++i)
            std::cout << input[i] << "\n";
    }
    std::cout << nSamples << "\n";
    return 0;
}

int Multiplexer::processWrapper(jack_nframes_t nSamples, void* arg)
{
    auto* multiplexer = static_cast<Multiplexer*>(arg);
    return multiplexer->process(nSamples);
}