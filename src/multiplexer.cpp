#include <algorithm>
#include <bits/ranges_algo.h>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <jack/ringbuffer.h>
#include <memory>
#include <numeric>
#include <sstream>

#include <jack/jack.h>
#include <jack/types.h>
#include <string>
#include <vector>

#include "multiplexer.hpp"

using namespace std::chrono_literals;

void Multiplexer::initJack()
{
    // set up jack client

    client_ = jack_client_open("Audio Multiplexer", JackNullOption, nullptr);
    if (client_ == NULL)
    {
        std::cerr << "Could not create JACK client.\n";
        exit(1);
    }
    outputPort_ =
        jack_port_register(client_, "output", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    if (outputPort_ == NULL)
    {
        std::cerr << "Could not register port.\n";
        exit(1);
    }
    int i = 0;
    for (auto& channel : channels_)
    {
        ++i;
        std::stringstream namestream;
        namestream << i << " left";
        channel.port = jack_port_register(client_, namestream.str().c_str(),
                                          JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        channel.lastPlayed = std::chrono::steady_clock::now();
        // channel.port = jack_port_register(client_, "right", JACK_DEFAULT_AUDIO_TYPE,
        // JackPortIsInput, 0);
    }

    jack_set_process_callback(client_, Multiplexer::processWrapper, this);

    // set up output buffer

    const int bufferSize = jack_get_sample_rate(client_);
    outputBuffer_ = jack_ringbuffer_create(sizeof(jack_default_audio_sample_t) * bufferSize * 32);

    // set up soundtouch object

    const int sampleRate = jack_get_sample_rate(client_);
    soundTouch.setSampleRate(sampleRate);
    soundTouch.setChannels(1);
    soundTouch.setTempo(2.0f);
    soundTouch.setSetting(SETTING_SEQUENCE_MS, 40);
    soundTouch.setSetting(SETTING_SEEKWINDOW_MS, 15);
    soundTouch.setSetting(SETTING_OVERLAP_MS, 8);

    // activate jack client
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

bool isSilent(const Samples& samples)
{
    jack_default_audio_sample_t total = 0.0f;
    for (auto sample : samples)
    {
        total += std::abs(sample);
    }
    // std::cout << "Total: " << total << "\n";
    return total < 10.0f;
}

float Multiplexer::channelAffinity(Channel &channel)
{
    if (channel.sampleBlocks.size() == 0)
        return 0;

    const double timeSinceLastPlayed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - channel.lastPlayed).count() / 100.0f;
    const double currentlyPlayingBonus = channel.port == channels_[currentChannel_].port ? 100 : 0;
    const double silencePenalty = channel.silence_counter * 0;

    return channel.sampleBlocks.size() + timeSinceLastPlayed + currentlyPlayingBonus - silencePenalty;
}

bool Multiplexer::shouldSwitch()
{
    std::cout << "Affinities: \n";
    std::cout << channels_[0].affinity << "\n";
    std::cout << channels_[1].affinity << "\n";
    return channels_[currentChannel_].affinity < channels_[1 - currentChannel_].affinity;
}

int Multiplexer::process(jack_nframes_t nSamples)
{
    auto* out =
        static_cast<jack_default_audio_sample_t*>(jack_port_get_buffer(outputPort_, nSamples));
    std::memset(out, 0, nSamples * sizeof(jack_default_audio_sample_t));
    // store inputs in their respective buffers
    for (auto& channel : channels_)
    {
        auto* input =
            static_cast<jack_default_audio_sample_t*>(jack_port_get_buffer(channel.port, nSamples));
        std::cout << channel.sampleBlocks.size() << "\n";
        Samples buff(nSamples);
        if (!input)
            continue;

        std::memcpy(buff.data(), input, nSamples * sizeof(jack_default_audio_sample_t));

        if (isSilent(buff))
        {
            channel.silence_counter++;
            if (channel.sampleBlocks.size() > 0)
            {
                channel.sampleBlocks.back().following_silence++;
                if (channel.sampleBlocks.back().following_silence > 10)
                    continue;
            }
            else
                continue;
        }
        else
        {
            if (channel.silence_counter > 10)
                channel.sampleBlocks.emplace_back();
            channel.silence_counter = 0;
        }

        if (channel.sampleBlocks.size() == 0)
            channel.sampleBlocks.emplace_back(std::move(buff));
        else
        {
            channel.sampleBlocks.back().samples.reserve(channel.sampleBlocks.back().samples.size() + buff.size());
            channel.sampleBlocks.back().samples.insert(channel.sampleBlocks.back().samples.end(), buff.begin(), buff.end());
        }
        channel.affinity = channelAffinity(channel);
    }
    // select input channel
    if (shouldSwitch())
    {
        /* std::cout << "switching channel\n"; */
        currentChannel_ = 1 - currentChannel_;
    }
    size_t total_buffer = 0;
    for (auto &channel : channels_)
        total_buffer += std::max(0, static_cast<int>(std::accumulate(channel.sampleBlocks.begin(), channel.sampleBlocks.end(), static_cast<size_t>(0),
                        [](const size_t x, const SampleBlock& b) {
                            return x + b.samples.size() - 100;
                        })));

    if (total_buffer < nSamples * 2)
    {
        std::cout << "buffers empty, skipping\n";
        return 0;
    }

    // exponential scaling
    // double tempo = 1.0f + 1.0f / (exp(5.0f - total_buffer / 200.0f));

    // linear scaling
    double tempo = std::ranges::clamp(1.0f + total_buffer / 200.0f / nSamples, 1.0f, 2.0f);
    /* if (channels_[currentChannel_].sampleBlocks.size() < 2) */
    /*     tempo = 1.0f; */

    soundTouch.setTempo(tempo);
    std::cout << "Buff: " << total_buffer << "\n";
    std::cout << "Tempo: " << tempo << "\n";
    std::cout << "Status: " << statusline_ << "\n";
    // return output from the currently selected input channel
    while (channels_[currentChannel_].sampleBlocks.size() > 0 &&
           jack_ringbuffer_read_space(outputBuffer_) <
               nSamples * 4 * sizeof(jack_default_audio_sample_t))
    {
        size_t n;
        auto& buff = channels_[currentChannel_].sampleBlocks.front().samples;
        n = std::min(buff.size(), static_cast<size_t>(nSamples));
        soundTouch.putSamples(buff.data(), n);
        buff.erase(buff.begin(), buff.begin() + n);
        if (buff.size() == 0)
            channels_[currentChannel_].sampleBlocks.pop_front();
        channels_[currentChannel_].lastPlayed = std::chrono::steady_clock::now();
        do
        {
            jack_ringbuffer_data_t data[2];
            jack_ringbuffer_get_write_vector(outputBuffer_, data);
            n = soundTouch.receiveSamples(reinterpret_cast<float*>(data[0].buf),
                                          data[0].len / sizeof(jack_default_audio_sample_t));
            jack_ringbuffer_write_advance(outputBuffer_, n * sizeof(jack_default_audio_sample_t));
        } while (n > 0);
    }
    if (jack_ringbuffer_read_space(outputBuffer_) >= nSamples * sizeof(jack_default_audio_sample_t))
    {
        int n = jack_ringbuffer_read(outputBuffer_, reinterpret_cast<char*>(out),
                                     nSamples * sizeof(jack_default_audio_sample_t));
    }

    return 0;
}

int Multiplexer::processWrapper(jack_nframes_t nSamples, void* arg)
{
    auto* multiplexer = static_cast<Multiplexer*>(arg);
    return multiplexer->process(nSamples);
}
