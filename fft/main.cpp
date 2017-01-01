#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_render.h>
/////////////////////////////////

#include <iostream>
#include <algorithm>
#include <memory>
#include <array>
#include <complex>
#include <cstddef>
#include <cassert>

/////////////////////////////////

#include "fft.hpp"

/////////////////////////////////////////////////////////////////////////////////////

template <typename T>
auto clamp (T v, T a, T b) {
	if (v < a) return a;
	if (v > b) return b;
	return v;
}

void plot_pixel(const SDL_Surface& surface, int x, int y, float value) {
	auto* p = reinterpret_cast<std::uint32_t*>(surface.pixels);

	uint8_t r = (uint8_t)clamp(value, 0.0f, 255.0f); 
	uint8_t g = r; 
	uint8_t b = r;

	if (surface.w > x && x >= 0 && surface.h > y && y > 0) {
		p [y * surface.w + x] = 0xFF000000|(r << 16)|(g << 8)|(b);
	}
}

/////////////////////////////////////////////////////////////////////////////////////

int main (int argc, char** argv) {
	if (argc <= 1) return -1;

	SDL_Init(SDL_INIT_EVERYTHING);
	std::atexit(SDL_Quit);

	SDL_LogSetAllPriority (SDL_LOG_PRIORITY_VERBOSE);

//////////////////////////////////////////
	struct sample_type {
		float left, right;
	};

	static constexpr auto block_points = 2048;
	static constexpr auto block_bytes = sizeof(sample_type)*block_points;


	SDL_AudioSpec wav_spec;
	SDL_zero(wav_spec);

	wav_spec.channels = 2;
	wav_spec.freq = 44100;
	wav_spec.format = AUDIO_F32;

	Uint32 wav_len = 0;
	const sample_type* wav_buf = nullptr;

	const auto* wav_out_spec = SDL_LoadWAV (argv[1], &wav_spec, (Uint8**)&wav_buf, &wav_len);

	assert(wav_out_spec != nullptr);
	assert(wav_out_spec->format == AUDIO_F32);
	assert(wav_out_spec->channels == 2);
	assert(wav_out_spec->freq == 44100);

	wav_spec.userdata = nullptr;
	wav_spec.callback = nullptr;
	wav_spec.samples = block_points;
	
	auto adid = SDL_OpenAudioDevice(nullptr, 0, &wav_spec, &wav_spec, 0);
	
	assert(wav_spec.format == AUDIO_F32);
	assert(wav_spec.channels == 2);
	assert(wav_spec.freq == 44100);	
		
	SDL_PauseAudioDevice(adid, 0);

	
	const auto num_blocks = (wav_len/block_bytes);
	auto cur_block = 0ull;

	//typedef std::array<std::complex<float>, block_points> buffer_type;
	//buffer_type out;

	std::valarray<std::complex<float>> fft_buf (block_points);
	std::array<sample_type, block_points> tmp_buf;


//////////////////////////////////////////

	SDL_Window* p_window = SDL_CreateWindow (nullptr, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080, 0);
	auto p_surface = SDL_GetWindowSurface (p_window);
	SDL_Renderer* p_render = SDL_CreateRenderer(p_window, -1, SDL_RENDERER_ACCELERATED);

//////////////////////////////////////////

	//cur_block = num_blocks / 2;


	const auto bucket_count = std::ceil(std::log2(22010.0f));
	std::valarray<float> buckets (0.0f, (unsigned)bucket_count);
	std::valarray<float> scalefq (1.0f, (unsigned)bucket_count);

	while (!SDL_QuitRequested ()) {
		SDL_Event p_event;
		if (SDL_PollEvent(&p_event)) {
			continue;
		}
		
		if (SDL_GetQueuedAudioSize (adid) < block_bytes) {

			SDL_RenderClear(p_render);

			for (auto i = 0u; i < block_points; ++i) {
				const auto& s = wav_buf[cur_block*block_points + i];
				fft_buf[i] = (s.left + s.right)*0.5f;				
			}

			fft (fft_buf);									

			buckets = 0.0f;
			scalefq = 1.0f;

			for (auto i = 0; i < block_points/2; ++i) {
				const auto m = std::abs(fft_buf [i]);
				const auto b = unsigned (std::round (std::log2 (44100.0f*i/block_points)));
				buckets [b] += m;
				scalefq [b] += 1.0f;
			}

			buckets = buckets * 1.5f / std::sqrt (scalefq);

			for (auto i = 1; i < bucket_count; ++i) {
				const auto m = buckets[i];
				for(auto j = 0; j < m; ++j) {
					static constexpr auto W = 40;
					static constexpr auto O = 5;

					SDL_Rect r;
					r.x = i*(O + W);
					r.y = p_surface->h - 1 - j;
					r.w = W;
					r.h = j;

					SDL_SetRenderDrawColor(p_render, 255, 0, 255, 255);
					SDL_RenderFillRect(p_render, &r);
					SDL_SetRenderDrawColor(p_render, 0, 0, 0, 255);
					//SDL_RenderDrawRect(p_render, &r);
				}
			}

			ifft (fft_buf);

			for (auto i = 0u; i < block_points; ++i) {
				tmp_buf[i].left =  fft_buf[i].real();
				tmp_buf[i].right = fft_buf[i].real();
			}

			SDL_QueueAudio (adid, &tmp_buf, sizeof (tmp_buf));
			cur_block = (cur_block + 1) % num_blocks;

			SDL_RenderPresent(p_render);
			//SDL_UpdateWindowSurface (p_window);		
		}		

	}

	SDL_FreeWAV ((Uint8*)wav_buf);
	SDL_DestroyRenderer (p_render);
	SDL_DestroyWindow(p_window);
	return 0;
}
