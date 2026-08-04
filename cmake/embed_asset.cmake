# ── obscura_embed_asset ─────────────────────────────────────────────────────
#
# Turn a committed binary asset into a constexpr byte array in the BUILD tree:
#
#   obscura_embed_asset(<asset> <symbol> <out_header> <max_bytes>)
#
# MAX_BYTES is required rather than defaulted, and it belongs to the CALLER.
# Embedding a byte array has no natural size limit; what has a limit is the
# thing the asset is eventually going to do — a plate is capped by what kitty
# charges to transmit it, and the next asset will be capped by something else
# entirely, or by nothing. A budget baked into this function would be the plate's
# budget silently applied to everything, and the first larger asset would be
# "fixed" by raising it for all of them.
#
# Configure time, not build time, and deliberately. The asset is committed and
# changes only when a human re-bakes it, so re-configuring is exactly the event
# that should regenerate the header. A custom command would need a target
# dependency threaded around src/lib/'s source glob to buy the same thing.
#
# Why embed at all rather than read the PNG at startup: a plate that can fail to
# load is a plate that can fail to load in front of a player, which is the same
# argument cases/ makes for compiling authored content in. It also means the
# installed package has no data files to find.

function(obscura_embed_asset ASSET SYMBOL OUT_HEADER MAX_BYTES)
  if (NOT EXISTS "${ASSET}")
    message(FATAL_ERROR
      "embed_asset: ${ASSET} does not exist.\n"
      "  It is a committed asset, not a build product, so there is nothing to\n"
      "  build that would create it. Re-generate it from its own recipe under\n"
      "  tools/, or restore it.")
  endif ()

  # file(READ) records no dependency of its own. Without this the build caches a
  # header generated from the OLD asset and a re-baked plate is silently ignored
  # until someone deletes the build directory — which presents as "my art change
  # did nothing", the worst possible symptom to debug.
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ASSET}")

  file(SIZE "${ASSET}" _size)

  # An empty asset is not a small asset. file(READ ... HEX) would yield an empty
  # string, the generated header would be a valid `std::array<unsigned char, 0>`,
  # and everything downstream would compile without a diagnostic — leaving a
  # library that ships a plate with no bytes and a declared 240x160 extent, which
  # fails at draw time in front of a player. This is the shape a git-lfs pointer
  # left unsmudged, an interrupted write, or a bad restore all arrive in.
  if (_size EQUAL 0)
    message(FATAL_ERROR
      "embed_asset: ${ASSET} is empty. It is a committed asset — restore it or\n"
      "  re-generate it from its recipe under tools/.")
  endif ()

  # The caller's budget, enforced at configure time so it fails with the
  # arithmetic in front of you rather than as a red assertion after a build. It
  # doubles as the cap on how large a braced initializer the compiler has to
  # chew, which is not academic on a runner already capped at --parallel 2.
  if (_size GREATER MAX_BYTES)
    message(FATAL_ERROR
      "embed_asset: ${ASSET} is ${_size} bytes, over its ${MAX_BYTES}-byte\n"
      "  budget. The budget is set where this is called; go and read why it is\n"
      "  that number before changing it, because raising it here is how a\n"
      "  budget stops meaning anything.")
  endif ()

  file(READ "${ASSET}" _hex HEX)

  # Wrap FIRST, on pure hex, 32 characters (16 bytes) to a line. Doing it in
  # this order means a byte pair can never straddle the newline, so the
  # expansion below stays correct. Wrapping afterwards would have to count
  # "0x.., " groups instead, and get it wrong on the last line.
  string(REGEX REPLACE "(................................)" "\\1\n" _hex "${_hex}")
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " _bytes "${_hex}")

  set(OBSCURA_EMBED_SOURCE "${ASSET}")
  set(OBSCURA_EMBED_SYMBOL "${SYMBOL}")
  set(OBSCURA_EMBED_SIZE   "${_size}")
  set(OBSCURA_EMBED_BYTES  "${_bytes}")

  # configure_file, not file(WRITE): it rewrites the output only when the
  # content actually differs, so a plain `cmake -B build` does not touch the
  # header's timestamp and force a recompile of everything that includes it.
  configure_file(
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/embed_asset.hpp.in"
    "${OUT_HEADER}"
    @ONLY
  )
endfunction()
