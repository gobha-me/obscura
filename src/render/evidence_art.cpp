#include <obscura/render/evidence_art.hpp>

#include <span>

#include <obscura/render/e02_manifest_seal.hpp>

#include <termforge/core/types.hpp>

namespace obscura::render {

auto e02_manifest_seal() -> termforge::EncodedImage {
  return termforge::EncodedImage{
      .format = termforge::ImageFormat::Png,
      .bytes = std::as_bytes(std::span{detail::kE02ManifestSealPng}),
      .pixels = termforge::Extent{.w = 320, .h = 120},
  };
}

} // namespace obscura::render
