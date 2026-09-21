#pragma once
//==============================================================================
//  Minimal JUCE stand-in for offline measurement.
//
//  WavetableOscillator.h is a header-only DSP class whose only JUCE dependency
//  is a handful of numeric helpers. Providing them here lets the *actual*
//  shipping header be compiled and measured outside a plugin host, so the
//  numbers in the write-up come from the code that runs in the product rather
//  than from a re-implementation that might quietly differ from it.
//
//  This file is measurement tooling. It is never part of the plugin build.
//==============================================================================
#include <algorithm>
#include <cmath>

namespace juce
{
    template <typename T>
    struct MathConstants
    {
        static constexpr T pi    = static_cast<T> (3.141592653589793238L);
        static constexpr T twoPi = static_cast<T> (6.283185307179586477L);
    };

    template <typename T> inline T jmax (T a, T b) { return a > b ? a : b; }
    template <typename T> inline T jmin (T a, T b) { return a < b ? a : b; }

    template <typename T>
    inline T jlimit (T lower, T upper, T value)
    {
        return value < lower ? lower : (value > upper ? upper : value);
    }
}
