#pragma once
#include <valarray>
#include <complex>

template <typename value_type>
void fft (std::valarray<std::complex<value_type>>& x)
{	
	typedef std::complex<value_type> complex_type;
	static const auto pi = value_type (3.141592653589793238462643383279502884);
	static const auto _j = complex_type (0, -1); // -j

	const auto N = x.size ();
	assert(__popcnt64(N) == 1u);

	for (auto i = 0u; i < N; ++i) {
		auto j = 0u;
		for (auto v = N | i; v > 1u; v >>= 1u) {
			j = (j << 1u) | (v & 1u);
		}
		if (i < j) { 
			std::swap(x[j], x[i]);			
		}		
	}

	for (auto m = 2u; m <= N; m <<= 1u) {
		const auto h = m >> 1u;
		const auto wm = std::exp(_j * (pi / h));
		complex_type w (1, 0);
		for (auto j = 0u; j < h; ++j) {
			for (auto k = j; k < N; k += m) {
				const auto t = x[k + h] * w;
				const auto u = x[k + 0];
				x[k + 0] = u + t;
				x[k + h] = u - t;
			}
			w *= wm;
		}
	}
}

template <typename value_type>
void ifft (std::valarray<std::complex<value_type>>& x)
{
	for(auto& xi : x) xi = std::conj(xi);
	fft (x);
	for(auto& xi : x) xi = std::conj(xi) * (1.0f/x.size ());	
}