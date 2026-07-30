// pf_bench.c — Intel HW prefetcher 영향 측정용 마이크로벤치마크
//
// build: gcc -O2 -march=native -o pf_bench pf_bench.c
// run:   ./pf_bench <seq|line|page|chase> [passes]
//
// 네 패턴은 PF-LLM Figure 1에 대응:
//   seq   = (d) Streaming Access   — 8B 간격, 라인당 8원소
//   line  = (b) Strided Access     — 64B 간격, 캐시라인 하나당 한 번
//   page  = (b)의 극단             — 4096B 간격, 매 접근이 페이지를 넘음
//   chase = 예측 불가              — 무작위 순열 포인터 추적, 의존 사슬

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// xorshift64 — rand()보다 빠르고 32M 원소 셔플에 충분
static uint64_t rng_state = 88172645463325252ULL;
static inline uint64_t rng(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <seq|line|page|chase> [passes]\n", argv[0]);
        return 1;
    }
    const char *pat = argv[1];

    size_t bytes;
    size_t stride = 1;      // 원소 단위
    int    chase  = 0;
    int    passes;

    if      (!strcmp(pat, "seq"))   { bytes = 2UL<<30; stride = 1;   passes = 3;   }
    else if (!strcmp(pat, "line"))  { bytes = 2UL<<30; stride = 8;   passes = 10;  }
    else if (!strcmp(pat, "page"))  { bytes = 2UL<<30; stride = 512; passes = 500; }
    else if (!strcmp(pat, "chase")) { bytes = 256UL<<20; chase = 1;  passes = 3;   }
    else { fprintf(stderr, "unknown pattern: %s\n", pat); return 1; }

    if (argc > 2) passes = atoi(argv[2]);

    size_t n = bytes / sizeof(uint64_t);
    uint64_t *a = aligned_alloc(4096, bytes);
    if (!a) { perror("aligned_alloc"); return 1; }

    // ---- setup (측정 구간 밖) ----
    if (chase) {
        uint64_t *perm = a;
        for (size_t i = 0; i < n; i++) perm[i] = i;
        for (size_t i = n - 1; i > 0; i--) {          // Fisher-Yates
            size_t j = rng() % (i + 1);
            uint64_t t = perm[i]; perm[i] = perm[j]; perm[j] = t;
        }
        uint64_t *next = aligned_alloc(4096, bytes);   // perm -> 단일 순환 사슬
        if (!next) { perror("aligned_alloc"); return 1; }
        for (size_t i = 0; i < n; i++) next[perm[i]] = perm[(i + 1) % n];
        free(a);
        a = next;
    } else {
        for (size_t i = 0; i < n; i++) a[i] = i;        // page-in + 초기화
    }

    // ---- 측정 구간 ----
    double t0 = now();
    uint64_t sum = 0, idx = 0;
    for (int p = 0; p < passes; p++) {
        if (chase) {
            for (size_t k = 0; k < n; k++) { idx = a[idx]; sum += idx; }   // 의존 사슬
        } else {
            for (size_t i = 0; i < n; i += stride) sum += a[i];
        }
    }
    double t1 = now();

    size_t per_pass = chase ? n : (n + stride - 1) / stride;
    size_t acc = (size_t)passes * per_pass;
    double sec = t1 - t0;

    printf("%-6s  passes=%-4d  accesses=%-12zu  loop=%8.3f s  %8.2f ns/acc   (sum=%llu)\n",
           pat, passes, acc, sec, sec * 1e9 / acc, (unsigned long long)sum);

    free(a);
    return 0;
}
