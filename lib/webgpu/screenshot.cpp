#include <aurora/screenshot.hpp>

#include "gpu.hpp"
#include "screenshot.hpp"
#include "screenshot_pixels.hpp"

#include <memory>
#include <mutex>
#include <new>
#include <utility>

namespace aurora::screenshot {
namespace {

enum class State { Requested, Encoded, Mapping, Complete };

struct Job {
  State state = State::Requested;
  Result result;
  wgpu::Buffer buffer;
  uint32_t pitch = 0;
  uint64_t bufferSize = 0;
  bool bgra = false;
  bool cancelled = false;
};

std::mutex mutex;
std::shared_ptr<Job> active;

void fail(Job& job, const char* error) {
  job.result.error = error;
  job.state = State::Complete;
}

} // namespace

bool request() {
  std::lock_guard lock{mutex};
  if (active) {
    return false;
  }
  active = std::make_shared<Job>();
  return true;
}

bool poll(Result& result) {
  std::lock_guard lock{mutex};
  if (!active || active->state != State::Complete) {
    return false;
  }
  result = std::move(active->result);
  active.reset();
  return true;
}

void cancel() {
  std::lock_guard lock{mutex};
  if (active && active->state == State::Mapping) {
    active->cancelled = true;
  } else {
    active.reset();
  }
}

void encode(const wgpu::CommandEncoder& encoder, const wgpu::Texture& composed) {
  std::lock_guard lock{mutex};
  if (!active || active->state != State::Requested) {
    return;
  }
  auto& job = *active;
  if (!composed) {
    fail(job, "No composed frame is available");
    return;
  }
  if (!(composed.GetUsage() & wgpu::TextureUsage::CopySrc)) {
    fail(job, "Surface does not support screenshot readback");
    return;
  }
  const auto format = composed.GetFormat();
  if (format != wgpu::TextureFormat::RGBA8Unorm && format != wgpu::TextureFormat::RGBA8UnormSrgb &&
      format != wgpu::TextureFormat::BGRA8Unorm && format != wgpu::TextureFormat::BGRA8UnormSrgb) {
    fail(job, "Screenshot requires an RGBA8 or BGRA8 surface");
    return;
  }
  const uint32_t width = composed.GetWidth();
  const uint32_t height = composed.GetHeight();
  if (width == 0 || height == 0 || width > 4096 || height > 4096) {
    fail(job, "Screenshot dimensions exceed the 4096 by 4096 limit");
    return;
  }
  job.result.width = width;
  job.result.height = height;
  job.bgra = format == wgpu::TextureFormat::BGRA8Unorm || format == wgpu::TextureFormat::BGRA8UnormSrgb;
  job.pitch = (width * 4 + 255) & ~255u;
  job.bufferSize = static_cast<uint64_t>(job.pitch) * height;
  if (job.bufferSize > 64 * 1024 * 1024) {
    fail(job, "Screenshot readback exceeds the 64 MiB limit");
    return;
  }
  const wgpu::BufferDescriptor descriptor{
      .label = "Screenshot readback",
      .usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst,
      .size = job.bufferSize,
  };
  job.buffer = webgpu::g_device.CreateBuffer(&descriptor);
  if (!job.buffer) {
    fail(job, "Could not allocate screenshot readback buffer");
    return;
  }
  const wgpu::TexelCopyTextureInfo source{.texture = composed, .aspect = wgpu::TextureAspect::All};
  const wgpu::TexelCopyBufferInfo destination{
      .layout = {.bytesPerRow = job.pitch, .rowsPerImage = height},
      .buffer = job.buffer,
  };
  const wgpu::Extent3D extent{width, height, 1};
  encoder.CopyTextureToBuffer(&source, &destination, &extent);
  job.state = State::Encoded;
}

void after_submit() {
  std::shared_ptr<Job> job;
  {
    std::lock_guard lock{mutex};
    if (!active || active->state != State::Encoded) {
      return;
    }
    job = active;
    job->state = State::Mapping;
  }
  job->buffer.MapAsync(wgpu::MapMode::Read, 0, job->bufferSize, wgpu::CallbackMode::AllowSpontaneous,
                       [job](wgpu::MapAsyncStatus status, wgpu::StringView) {
                         Result result;
                         result.width = job->result.width;
                         result.height = job->result.height;
                         if (status == wgpu::MapAsyncStatus::Success) {
                           const auto* bytes =
                               static_cast<const uint8_t*>(job->buffer.GetConstMappedRange(0, job->bufferSize));
                           if (bytes) {
                             try {
                               result.rgba =
                                   detail::unpack_rgba(bytes, result.width, result.height, job->pitch, job->bgra);
                             } catch (const std::bad_alloc&) {
                               result.error = "Could not allocate screenshot pixel storage";
                             }
                           } else {
                             result.error = "Screenshot readback returned no pixels";
                           }
                           job->buffer.Unmap();
                         } else {
                           result.error = "Screenshot GPU readback failed or was cancelled";
                         }
                         std::lock_guard lock{mutex};
                         if (active == job) {
                           if (job->cancelled) {
                             active.reset();
                           } else {
                             job->result = std::move(result);
                             job->state = State::Complete;
                           }
                         }
                       });
}

} // namespace aurora::screenshot
