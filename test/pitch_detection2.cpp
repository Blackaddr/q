/*=============================================================================
   Copyright (c) 2014-2018 Joel de Guzman. All rights reserved.

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <q/support/literals.hpp>
#include <q/pitch/pitch_detector.hpp>
#include <q_io/audio_file.hpp>
#include <q/fx/envelope.hpp>
#include <q/fx/low_pass.hpp>
#include <q/fx/dynamic.hpp>
#include <q/fx/waveshaper.hpp>

#include <vector>
#include <iostream>
#include <fstream>

#include "notes.hpp"

namespace q = cycfi::q;
using namespace q::literals;

void process(
   std::string name
 , q::frequency lowest_freq
 , q::frequency highest_freq)
{
   ////////////////////////////////////////////////////////////////////////////
   // Prepare output file

   std::ofstream csv("results/frequencies_" + name + ".csv");

   ////////////////////////////////////////////////////////////////////////////
   // Read audio file

   q::wav_reader src{"audio_files/" + name + ".wav"};
   std::uint32_t const sps = src.sps();

   std::vector<float> in(src.length());
   src.read(in);

   ////////////////////////////////////////////////////////////////////////////
   // Output
   constexpr auto n_channels = 4;
   std::vector<float> out(src.length() * n_channels);
   std::fill(out.begin(), out.end(), 0);

   ////////////////////////////////////////////////////////////////////////////
   // Process
   q::pitch_detector<>        pd{ lowest_freq, highest_freq, sps, -30_dB };
   q::bacf<> const&           bacf = pd.bacf();
   auto                       size = bacf.size();
   q::edges const&            edges = bacf.edges();
   q::peak_envelope_follower  env{ 30_ms, sps };
   q::one_pole_lowpass        lp{ highest_freq, sps };
   q::one_pole_lowpass        lp2{ lowest_freq, sps };

   constexpr float            slope = 1.0f/4;
   constexpr float            makeup_gain = 4;
   q::compressor              comp{ -18_dB, slope };
   q::clip                    clip;

   float                      onset_threshold = 0.005;
   float                      release_threshold = 0.001;
   float                      threshold = onset_threshold;

   int ii = 0;

   for (auto i = 0; i != in.size(); ++i)
   {
      auto pos = i * n_channels;
      auto ch1 = pos;      // input
      auto ch2 = pos+1;    // zero crossings
      auto ch3 = pos+2;    // bacf
      auto ch4 = pos+3;    // frequency

      auto s = in[i];

      // Bandpass filter
      s = lp(s);
      s -= lp2(s);

      // Envelope
      auto e = env(std::abs(s));

      if (e > threshold)
      {
         // Compressor + makeup-gain + hard clip
         auto gain = float(comp(e)) * makeup_gain;
         s = clip(s * gain);
         threshold = release_threshold;
      }
      else
      {
         s = 0.0f;
         threshold = onset_threshold;
      }

      out[ch1] = s;

      // Pitch Detect
      std::size_t extra;
      bool proc = pd(s, extra);
      out[ch3] = -1;   // placeholder

      // BACF default placeholder
      out[ch2] = -0.8;

      if (proc)
      {
         auto out_i = (&out[ch3] - (((size-1) + extra) * n_channels));
         auto const& info = bacf.result();
         for (auto n : info.correlation)
         {
            *out_i = n / float(info.max_count);
            out_i += n_channels;
         }

         out_i = (&out[ch2] - (((size-1) + extra) * n_channels));
         for (auto i = 0; i != size; ++i)
         {
            *out_i = bacf[i] * 0.8;
            out_i += n_channels;
         }

         csv << pd.frequency() << ", " << pd.periodicity() << std::endl;
      }

      // Frequency
      auto f = pd.frequency() / double(highest_freq);
      auto fi = int(i - bacf.size());
      if (fi >= 0)
         out[(fi * n_channels) + 3] = f;
   }

   csv.close();

   ////////////////////////////////////////////////////////////////////////////
   // Write to a wav file

   q::wav_writer wav{
      "results/pitch_detect_" + name + ".wav", n_channels, sps
   };
   wav.write(out);
}

void process(std::string name, q::frequency lowest_freq)
{
   process(name, lowest_freq * 0.8, lowest_freq * 5);
}

int main()
{
   using namespace notes;

   // process("sin_440", d);
   // process("1-Low E", low_e);
   // process("2-Low E 2th", low_e);
   // process("3-A", a);
   // process("4-A 12th", a);
   // process("5-D", d);
   // process("6-D 12th", d);
   // process("7-G", g);
   // process("8-G 12th", g);
   // process("9-B", b);
   // process("10-B 12th", b);
   // process("11-High E", high_e);
   // process("12-High E 12th", high_e);
   // process("Tapping D", d);
   // process("Hammer-Pull High E", high_e);
   // process("Bend-Slide G", g);

   // process("GLines1", g);
   process("GLines1a", g);
   // process("GLines2", g);
   // process("GLines2a", g);
   // process("GLines3", g);
   // process("Staccato3", g);
   // process("GStaccato", g);

   return 0;
}

