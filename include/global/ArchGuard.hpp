// SPDX-License-Identifier: GPL-2.0-or-later
// ═══════════════════════════════════════════════════════════════════════════════
// include/global/ArchGuard.hpp — Compile-time amd64 architecture enforcement
// ═══════════════════════════════════════════════════════════════════════════════
// This header is force-included or included from main.cpp.
// It triggers a compile-time #error if the target is not x86_64/amd64.
// Two checks: one for MSVC (_M_AMD64) and one for GCC/Clang (__x86_64__).

#pragma once

// ─── MSVC check ──────────────────────────────────────────────────────────────
#if defined(THRONE_MSVC_AMD64_CHECK)
    #if !defined(_M_AMD64)
        #error "FATAL: Throne requires x86_64 (amd64). This build targets a non-amd64 architecture under MSVC."
    #endif
#endif

// ─── GCC / Clang check ──────────────────────────────────────────────────────
#if defined(THRONE_GCC_AMD64_CHECK)
    #if !defined(__x86_64__) && !defined(__amd64__)
        #error "FATAL: Throne requires x86_64 (amd64). This build targets a non-amd64 architecture under GCC/Clang."
    #endif
#endif
