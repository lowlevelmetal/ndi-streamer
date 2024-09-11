/**
 * @file asyncndisource.hpp
 * @brief This file includes utilities for working with NDI asynchronously.
 * @date 2024-09-11
 * @author Matthew Todd Geiger
 */

#pragma once

namespace AV::Utils {

/**
 * @brief An asynchronous NDI source.
 * 
 * Ok so let me explain a little behind this..
 * Basically, I want something like the NdiSource class but I want the ability to decode/encode/etc... while
 * compressing and sending the previous frame.
 * 
 * Each set of () is a thread running a loop.
 * The (compress -> send) would be this class.
 * Which means that this class will manage two threads,
 * one for video, and one for audio.
 * 
 * Video
 * (demux -> decode -> encode) -> (compress -> send)
 * 
 * Audio
 * (demux -> deocde -> resample) -> (compress -> send)
 */
class AsyncNdiSource {

};

} // namespace AV::Utils