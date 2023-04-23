/*
 * -----10--------20--------30--------40--------50--------60--------70-------80
 *
 * P1X DEMO TOOL V2 - SYNTHESIZER
 * by w84death^P1X
 *
 * https://github.com/w84death/p1x-demo-tool
 * (c) 2023.04
 *
 * -----10--------20--------30--------40--------50--------60--------70-------80
 */
#include <random>
#include <vector>
#include <atomic>
#include <cmath>
#include <thread>
#include <chrono>
#include <alsa/asoundlib.h>
#include "music.h"

/*
 * -----10--------20--------30--------40--------50--------60--------70-------80
 */

std::atomic<bool> pause_playback{false};
std::atomic<bool> quit_playback{false};
std::vector<std::thread> player_threads;
int sample_rate = 44100;

/*
 * -----10--------20--------30--------40--------50--------60--------70-------80
 */

float sine_wave(float frequency, float time) {
    const float PI = 3.14159265;
    return sin(2 * PI * frequency * time);
}

float midi_note_to_frequency(int note) {
    return 440.0f * pow(2.0f, (note - 69) / 12.0f);
}

float white_noise() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dist(-1.0, 1.0);
    return dist(gen);
}

/*
 * -----10--------20--------30--------40--------50--------60--------70-------80
 */
float kick(float frequency, float time) {
    float base_freq = 50.0f; // Base frequency for the kick
    float envelope = std::exp(-4.0f * time); // Exponential decay envelope
    float sine_wave1 = std::sin(2.0f * M_PI * base_freq * time);
    float sine_wave2 = std::sin(2.0f * M_PI * frequency * time);

    return envelope * (sine_wave1 + sine_wave2);
}

float hi_hat(float frequency, float time) {
    float envelope = std::exp(-10.0f * time); // Exponential decay envelope
    return envelope * white_noise();
}

float arpeggiator_synth(float frequency, float time) {
    return sine_wave(frequency, time);
}

struct ADSR {
    float attack;
    float decay;
    float sustain;
    float release;
    float sustain_level;
};

float adsr_envelope(const ADSR &adsr, float time, float key_release_time) {
    if (time < adsr.attack) {
        return time / adsr.attack;
    } else if (time < adsr.attack + adsr.decay) {
        return 1.0f - (time - adsr.attack) / adsr.decay * (1.0f - adsr.sustain_level);
    } else if (key_release_time < 0 || time < key_release_time) {
        return adsr.sustain_level;
    } else if (time < key_release_time + adsr.release) {
        return adsr.sustain_level * (1.0f - (time - key_release_time) / adsr.release);
    } else {
        return 0.0f;
    }
}

float electric_piano(float frequency, float time, float key_release_time) {
    ADSR envelope = {0.01f, 0.3f, 0.6f, 1.0f, 0.7f};
    float amplitude_envelope = adsr_envelope(envelope, time, key_release_time);

    float sine_wave1 = std::sin(2.0f * M_PI * frequency * time);
    float sine_wave2 = 0.5f * std::sin(2.0f * M_PI * 2 * frequency * time);
    float sine_wave3 = 0.25f * std::sin(2.0f * M_PI * 4 * frequency * time);

    return amplitude_envelope * (sine_wave1 + sine_wave2 + sine_wave3);
}


/*
 * -----10--------20--------30--------40--------50--------60--------70-------80
 */
void play_note(Note note, snd_pcm_t *handle, int sample_rate) {
    int buffer_size = sample_rate * note.duration;
    float buffer[buffer_size];

    for (int i = 0; i < buffer_size; ++i) {
        switch (note.instrument) {
            case 0:
                buffer[i] = kick(midi_note_to_frequency(note.pitch), static_cast<float>(i) / sample_rate);
                break;
            case 1:
                buffer[i] = hi_hat(midi_note_to_frequency(note.pitch), static_cast<float>(i) / sample_rate);
                break;
            case 2:
                buffer[i] = arpeggiator_synth(midi_note_to_frequency(note.pitch), static_cast<float>(i) / sample_rate);
                break;
            case 3:
                buffer[i] = electric_piano(midi_note_to_frequency(note.pitch), static_cast<float>(i) / sample_rate, note.release);
                break;
            default:
                buffer[i] = sine_wave(midi_note_to_frequency(note.pitch), static_cast<float>(i) / sample_rate);
        }
    }

    snd_pcm_writei(handle, buffer, buffer_size);
}

/*
 * -----10--------20--------30--------40--------50--------60--------70-------80
 */
void playback_thread(int track_id, std::vector<Note> &notes, int sample_rate) {
    snd_pcm_t *handle;
    snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    snd_pcm_set_params(handle, SND_PCM_FORMAT_FLOAT, SND_PCM_ACCESS_RW_INTERLEAVED, 1, sample_rate, 1, 100000);

    size_t current_note = 0;
    bool is_paused = false;

    while (!quit_playback) {
        if (!pause_playback) {
            if (is_paused) {
                snd_pcm_pause(handle, 0);
                is_paused = false;
            }

            if (current_note < notes.size()) {
                play_note(notes[current_note], handle, sample_rate);
                current_note++;
            } else {
                current_note = 0;
            }
        } else {
            if (!is_paused) {
                snd_pcm_pause(handle, 1);
                is_paused = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    snd_pcm_close(handle);
}

/*
 * -----10--------20--------30--------40--------50--------60--------70-------80
 */
void synthStart(){
    for (int i = 0; i < tracks.size(); ++i) {
        player_threads.emplace_back(playback_thread, i, std::ref(tracks[i]), sample_rate);
    }
}

/*
 * -----10--------20--------30--------40--------50--------60--------70-------80
 */
void synthQuit(){
    for (auto &t : player_threads) {
        t.join();
    }
}
