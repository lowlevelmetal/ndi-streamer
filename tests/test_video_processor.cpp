/**
 * @file test_video_processor.cpp
 * @brief Pixel conversion, NDI layouts and timing metadata on synthetic frames.
 */

#include "av/video_processor.hpp"

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

extern "C" {
#include <libavutil/pixdesc.h>
}

using namespace ndistreamer;
using av::OutputPixelFormat;
using av::VideoProcessor;
using av::VideoProcessorConfig;

namespace {

av::FramePtr MakeVideoFrame(AVPixelFormat format, int width, int height, int64_t pts = 0) {
    av::FramePtr frame = av::MakeFrame();
    frame->format = format;
    frame->width = width;
    frame->height = height;
    frame->pts = pts;
    av::Check(av_frame_get_buffer(frame.get(), 0), "allocating test frame");
    return frame;
}

int PlaneHeight(const AVFrame *frame, int plane) {
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(frame->format));
    bool chroma = plane == 1 || plane == 2;
    return chroma ? AV_CEIL_RSHIFT(frame->height, desc->log2_chroma_h) : frame->height;
}

void FillPlane(AVFrame *frame, int plane, uint8_t value) {
    std::memset(frame->data[plane], value, static_cast<size_t>(frame->linesize[plane]) * PlaneHeight(frame, plane));
}

void FillNv12(AVFrame *frame, uint8_t y, uint8_t u, uint8_t v) {
    FillPlane(frame, 0, y);
    for (int row = 0; row < PlaneHeight(frame, 1); row++) {
        uint8_t *line = frame->data[1] + static_cast<size_t>(row) * frame->linesize[1];
        for (int x = 0; x < frame->width; x += 2) {
            line[x] = u;
            line[x + 1] = v;
        }
    }
}

std::vector<VideoFrame> Process(VideoProcessor &processor, av::FramePtr frame) {
    std::vector<VideoFrame> out;
    processor.Process(std::move(frame), [&](VideoFrame &&f) {
        out.push_back(std::move(f));
        return true;
    });
    return out;
}

std::vector<VideoFrame> Flush(VideoProcessor &processor) {
    std::vector<VideoFrame> out;
    processor.Flush([&](VideoFrame &&f) {
        out.push_back(std::move(f));
        return true;
    });
    return out;
}

VideoProcessorConfig Config(OutputPixelFormat format = OutputPixelFormat::Auto) {
    VideoProcessorConfig config;
    config.pixel_format = format;
    config.time_base = {1, 25};
    config.frame_rate = {25, 1};
    return config;
}

} // namespace

TEST(VideoProcessor, ConvertsYuv420pToUyvy) {
    VideoProcessor processor(Config(OutputPixelFormat::UYVY));
    auto frame = MakeVideoFrame(AV_PIX_FMT_YUV420P, 64, 48);
    FillPlane(frame.get(), 0, 81);
    FillPlane(frame.get(), 1, 90);
    FillPlane(frame.get(), 2, 240);

    auto out = Process(processor, std::move(frame));
    ASSERT_EQ(out.size(), 1u);
    const VideoFrame &f = out[0];
    EXPECT_EQ(f.fourcc, VideoFourCC::UYVY);
    EXPECT_EQ(f.width, 64);
    EXPECT_EQ(f.height, 48);
    EXPECT_GE(f.stride, 128);
    EXPECT_EQ(f.stride % 64, 0);

    for (int y = 0; y < f.height; y++) {
        const uint8_t *line = f.data + static_cast<size_t>(y) * f.stride;
        for (int x = 0; x < f.width * 2; x += 4) {
            ASSERT_NEAR(line[x + 0], 90, 1) << "U at " << x << "," << y;
            ASSERT_NEAR(line[x + 1], 81, 1) << "Y at " << x << "," << y;
            ASSERT_NEAR(line[x + 2], 240, 1) << "V at " << x << "," << y;
            ASSERT_NEAR(line[x + 3], 81, 1) << "Y at " << x << "," << y;
        }
    }
}

TEST(VideoProcessor, UyvyPassesThroughWithoutCopying) {
    VideoProcessor processor(Config(OutputPixelFormat::UYVY));
    auto frame = MakeVideoFrame(AV_PIX_FMT_UYVY422, 64, 48);
    const uint8_t *original = frame->data[0];

    auto out = Process(processor, std::move(frame));
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].fourcc, VideoFourCC::UYVY);
    EXPECT_EQ(out[0].data, original);
}

TEST(VideoProcessor, Nv12IsRepackedIntoOneContiguousBuffer) {
    VideoProcessor processor(Config(OutputPixelFormat::NV12));
    // 50 rows: FFmpeg pads the luma plane, leaving a gap before the chroma plane.
    auto frame = MakeVideoFrame(AV_PIX_FMT_NV12, 64, 50);
    FillNv12(frame.get(), 100, 110, 120);

    auto out = Process(processor, std::move(frame));
    ASSERT_EQ(out.size(), 1u);
    const VideoFrame &f = out[0];
    EXPECT_EQ(f.fourcc, VideoFourCC::NV12);

    const uint8_t *uv = f.data + static_cast<size_t>(f.stride) * f.height;
    EXPECT_EQ(f.storage->data[1], uv) << "chroma must follow luma immediately";
    for (int y = 0; y < f.height; y++) ASSERT_EQ(f.data[static_cast<size_t>(y) * f.stride + 10], 100);
    for (int y = 0; y < 25; y++) {
        ASSERT_EQ(uv[static_cast<size_t>(y) * f.stride + 0], 110);
        ASSERT_EQ(uv[static_cast<size_t>(y) * f.stride + 1], 120);
    }
}

TEST(VideoProcessor, I420UsesHalfStrideChromaPlanes) {
    VideoProcessor processor(Config(OutputPixelFormat::I420));
    auto frame = MakeVideoFrame(AV_PIX_FMT_YUV420P, 64, 50);
    FillPlane(frame.get(), 0, 50);
    FillPlane(frame.get(), 1, 60);
    FillPlane(frame.get(), 2, 70);

    auto out = Process(processor, std::move(frame));
    ASSERT_EQ(out.size(), 1u);
    const VideoFrame &f = out[0];
    EXPECT_EQ(f.fourcc, VideoFourCC::I420);

    const AVFrame *storage = f.storage.get();
    EXPECT_EQ(storage->linesize[1], f.stride / 2);
    EXPECT_EQ(storage->linesize[2], f.stride / 2);
    EXPECT_EQ(storage->data[1], f.data + static_cast<size_t>(f.stride) * 50);
    EXPECT_EQ(storage->data[2], storage->data[1] + static_cast<size_t>(f.stride / 2) * 25);
    EXPECT_EQ(storage->data[0][0], 50);
    EXPECT_EQ(storage->data[1][0], 60);
    EXPECT_EQ(storage->data[2][0], 70);
}

TEST(VideoProcessor, AutoConvertsFullRangeYuvToLimitedRange) {
    VideoProcessor processor(Config(OutputPixelFormat::Auto));
    auto frame = MakeVideoFrame(AV_PIX_FMT_YUV420P, 64, 48);
    frame->color_range = AVCOL_RANGE_JPEG;
    FillPlane(frame.get(), 0, 0); // full-range black
    FillPlane(frame.get(), 1, 128);
    FillPlane(frame.get(), 2, 128);

    auto out = Process(processor, std::move(frame));
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].fourcc, VideoFourCC::UYVY);
    EXPECT_NEAR(out[0].data[1], 16, 1); // limited-range black
}

TEST(VideoProcessor, AutoKeepsFormatsNdiAccepts) {
    VideoProcessor processor(Config(OutputPixelFormat::Auto));
    auto frame = MakeVideoFrame(AV_PIX_FMT_BGRA, 32, 16);
    const uint8_t *original = frame->data[0];

    auto out = Process(processor, std::move(frame));
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].fourcc, VideoFourCC::BGRA);
    EXPECT_EQ(out[0].data, original);

    auto nv12 = MakeVideoFrame(AV_PIX_FMT_NV12, 32, 16);
    out = Process(processor, std::move(nv12));
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].fourcc, VideoFourCC::NV12);
}

TEST(VideoProcessor, ConvertsYuvToRgba) {
    VideoProcessor processor(Config(OutputPixelFormat::RGBA));
    auto frame = MakeVideoFrame(AV_PIX_FMT_YUV420P, 64, 48);
    frame->colorspace = AVCOL_SPC_SMPTE170M;
    FillPlane(frame.get(), 0, 81); // BT.601 limited-range red
    FillPlane(frame.get(), 1, 90);
    FillPlane(frame.get(), 2, 240);

    auto out = Process(processor, std::move(frame));
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].fourcc, VideoFourCC::RGBA);
    const uint8_t *pixel = out[0].data + out[0].stride * 10 + 4 * 10;
    EXPECT_NEAR(pixel[0], 255, 4);
    EXPECT_NEAR(pixel[1], 0, 4);
    EXPECT_NEAR(pixel[2], 0, 4);
    EXPECT_EQ(pixel[3], 255);
}

TEST(VideoProcessor, CarriesTimingAspectAndInterlacing) {
    VideoProcessorConfig config = Config();
    config.time_base = {1, 1000};
    VideoProcessor processor(config);

    auto frame = MakeVideoFrame(AV_PIX_FMT_YUV420P, 720, 576, 100);
    frame->sample_aspect_ratio = {16, 15};
#ifdef AV_FRAME_FLAG_INTERLACED
    frame->flags |= AV_FRAME_FLAG_INTERLACED;
#else
    frame->interlaced_frame = 1;
#endif

    auto out = Process(processor, std::move(frame));
    ASSERT_EQ(out.size(), 1u);
    const VideoFrame &f = out[0];
    EXPECT_EQ(f.pts_us, 100000);
    EXPECT_EQ(f.duration_us, 40000);
    EXPECT_EQ(f.frame_rate_num, 25);
    EXPECT_EQ(f.frame_rate_den, 1);
    EXPECT_NEAR(f.aspect_ratio, 4.0 / 3.0, 1e-4);
    EXPECT_TRUE(f.interlaced);
}

TEST(VideoProcessor, FramesWithoutTimestampsAreSequential) {
    VideoProcessor processor(Config());
    std::vector<int64_t> pts;
    for (int i = 0; i < 3; i++) {
        auto frame = MakeVideoFrame(AV_PIX_FMT_UYVY422, 16, 16, AV_NOPTS_VALUE);
        for (auto &f : Process(processor, std::move(frame))) pts.push_back(f.pts_us);
    }
    EXPECT_EQ(pts, (std::vector<int64_t>{0, 40000, 80000}));
}

TEST(VideoProcessor, FiltergraphScalesAndChangesFrameRate) {
    VideoProcessorConfig config = Config();
    config.filter = "scale=32:24,fps=50";
    VideoProcessor processor(config);

    std::vector<VideoFrame> out;
    for (int i = 0; i < 10; i++) {
        for (auto &f : Process(processor, MakeVideoFrame(AV_PIX_FMT_YUV420P, 64, 48, i))) out.push_back(std::move(f));
    }
    for (auto &f : Flush(processor)) out.push_back(std::move(f));

    // Twice the input frames, give or take how the fps filter rounds at the end of the stream.
    ASSERT_GE(out.size(), 18u);
    ASSERT_LE(out.size(), 20u);
    EXPECT_EQ(out[0].width, 32);
    EXPECT_EQ(out[0].height, 24);
    EXPECT_EQ(out[0].frame_rate_num, 50);
    EXPECT_EQ(out[0].frame_rate_den, 1);
    EXPECT_EQ(out[1].pts_us - out[0].pts_us, 20000);
}

TEST(VideoProcessor, InvalidFiltergraphIsReported) {
    VideoProcessorConfig config = Config();
    config.filter = "definitely_not_a_filter";
    VideoProcessor processor(config);
    try {
        Process(processor, MakeVideoFrame(AV_PIX_FMT_YUV420P, 64, 48));
        FAIL() << "expected an error";
    } catch (const av::Error &error) {
        EXPECT_NE(std::string(error.what()).find("parsing video filter"), std::string::npos) << error.what();
    }
}

TEST(VideoProcessor, HandlesResolutionChanges) {
    VideoProcessor processor(Config());
    auto first = Process(processor, MakeVideoFrame(AV_PIX_FMT_YUV420P, 64, 48, 0));
    auto second = Process(processor, MakeVideoFrame(AV_PIX_FMT_YUV420P, 32, 16, 1));
    ASSERT_EQ(first.size(), 1u);
    ASSERT_EQ(second.size(), 1u);
    EXPECT_EQ(first[0].width, 64);
    EXPECT_EQ(second[0].width, 32);
    EXPECT_EQ(second[0].height, 16);
}

TEST(OutputPixelFormat, ParsesNames) {
    EXPECT_EQ(av::ParseOutputPixelFormat("uyvy"), OutputPixelFormat::UYVY);
    EXPECT_EQ(av::ParseOutputPixelFormat("auto"), OutputPixelFormat::Auto);
    EXPECT_EQ(av::ParseOutputPixelFormat("native"), std::nullopt);
    EXPECT_EQ(av::ParseOutputPixelFormat("UYVY"), std::nullopt);
    for (auto format : {OutputPixelFormat::UYVY, OutputPixelFormat::NV12, OutputPixelFormat::I420,
                        OutputPixelFormat::BGRA, OutputPixelFormat::RGBA, OutputPixelFormat::Auto}) {
        EXPECT_EQ(av::ParseOutputPixelFormat(av::ToString(format)), format);
    }
}
