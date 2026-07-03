---
title: "CXL Coherence — cache coherence와 헷갈리는 용어"
aliases: [CXL Coherence, CXL.cache, cache coherence, coherence, translation vs caching, 헷갈리는 용어]
type: concept
tags:
  - concept
  - concept/cxl
  - topic/cxl
---

# CXL Coherence — 일관성과 "이름에 속지 않기"

> [!abstract] 이 노트
> CXL이 디바이스를 **coherence 참여자**로 만드는 메커니즘과, 그 과정에서 겹치는 용어(cache / CXL.cache / coherence / translation)를 분리. 변환의 "어디서"는 [[CXL Distributed Translation]], 주소 기초는 [[CXL Address Translation]].

## ⚠️ 헷갈리는 용어 — 같은 단어, 다른 뜻
**"cache" — 저장장치 vs 프로토콜**
| | 보통의 cache | **CXL.cache** |
|---|---|---|
| 정체 | 저장 장치 (SRAM HW) | **통신 프로토콜** (규약) |
| 하는 일 | 데이터 임시 보관 | device↔host 일관성 조율 |
| "cache"인 이유 | 데이터를 캐싱하니까 | **cache coherence를 다루니까** |

**"cache coherence" — 대상은 사실 memory**
| | cache coherence (좁음) | system-wide coherence (넓음, CXL) |
|---|---|---|
| 대상 | CPU 캐시(SRAM)들 사이 | **DRAM을 공유하는 모든 주체** (CPU 코어 + device들) |

- CXL이 하는 건 **넓은 쪽** = 공유 메모리(DRAM)에 일관된 뷰. 이름이 "cache"인 건 **대상이 아니라 방법론**에서 옴 — 다루는 대상=DRAM, 다루는 방법=cache line(64B) 단위 추적 + MESI류 상태(M/S/I). **이름은 방법을 가리킴.**
- (block 용어 충돌 — cache block 64B vs storage block 4KB — 는 [[CXL Address Translation]] 참고)

## coherence가 "왜" 필요한가 — 복사본 때문 (접근자 수 아님)
```
device A가 host 메모리 X를 읽어 자기 안에 복사(캐싱)
host CPU(또는 device B)가 X를 고침
→ device A의 복사본은 옛값 = 불일치
```
- 핵심 = **접근자 수가 아니라 "복사본의 존재"**. → device 하나 + host만 있어도 device가 캐싱하면 coherence 필요. **single-node에서도 CXL.cache가 의미 있음.**

## coherence vs synchronization — 역할이 다르다
| | cache coherence | synchronization |
|---|---|---|
| 보장 | "같은 주소를 봐도 다들 **최신 값**" (값 일관성) | "여럿이 같은 데이터를 **순서대로·안 겹치게**" (원자성/순서) |
| 누가 | **하드웨어 자동** | **소프트웨어 명시** (lock/atomic) |
| 예 | core A가 X=5로 바꾸면 core B 캐시 갱신/무효화 | `count++` 동시 실행 시 race 방지 |

- **coherence가 있어도 sync는 따로 필요.** coherence는 "읽을 때 최신값"만 보장하고, "읽고-고치고-쓰는 사이 남이 못 끼게"는 lock(sync)이 한다. 하부(HW)가 값을 맞추고 상부(SW)가 순서를 정함. (multi-node에서 *둘 다* 비싸지는 문제는 [[CXL Multi-node Coherence]])

## 방향 — host→device vs device→host
| 타입 | 방향 |
|---|---|
| **Type-3 (메모리 확장기)** | host → device (device가 메모리 제공) |
| **Type-2 (가속기·GPU)** | **양방향** (둘 다 메모리 가짐) |

| 프로토콜 | 방향 | 누가 |
|---|---|---|
| **CXL.mem** | host → device 메모리 | host가 device 메모리에 load/store |
| **CXL.cache** | **device → host 메모리** | device가 host 메모리를 coherent 접근 |
| **CXL.io** | 양방향 (설정·관리) | PCIe 호환, 디바이스 발견/초기화 |

- **CXL.cache가 정확히 device→host 방향** — 일반 PCIe엔 없는 CXL의 핵심 차별점. (Type 1/2/3 정의는 [[CXL Overview]])

## ★ translation ≠ caching — 두 번 마주치는 함정
| | 주소 변환 (HPA↔DPA) | 캐시 (L1↔DRAM) |
|---|---|---|
| 데이터 개수 | **하나** (이름만 둘) | **둘** (원본 + 복사본) |
| 관계 | 번역(translation) | 사본(caching) |
| 바뀌면 | **변환표 갱신** | **invalidation / update** |

- HPA·DPA = 같은 device DRAM 데이터의 **두 이름** → 복사본이 아니라 invalidation 개념 자체가 없음.
- 진짜 cache 관계 = L1/L2/L3 ↔ DRAM (복사본 존재). tiered memory(local DRAM ↔ CXL pool)도 캐시처럼.
- 함정: "변환표 갱신"과 "캐시 invalidate"가 둘 다 *데이터 안 옮기고 메타데이터만 건드림*이라 닮아 보임 — **하나는 복사본 관리, 하나는 이름 번역.**

**write-invalidate vs write-update** (복사본이 있을 때만 성립):
- **write-invalidate**: 변경자가 다른 캐시 복사본을 "무효"로만 표시 (상태/tag만). 싸다 → 대부분 채택(MESI).
- **write-update**: 변경자가 새 값을 다른 캐시에 전파. bandwidth 많이 먹음.
- HPA↔DPA엔 복사본이 없으니 이 선택지 자체가 없음(= 변환표 갱신일 뿐).

## ★ device가 coherence "참여자"가 되는 이유 (CXL.cache의 본질)
- **cache coherence 원칙 = "변경자(writer)가 알린다"** — 멀티코어에서 core1이 X를 고치면 core1이 "너희 복사본 invalid해" 통보. 별도 중앙 심판 없음.
- device가 변경자면? → **device가 무효화를 통보하는 주체.**

| | 일반 PCIe device | CXL device |
|---|---|---|
| 메모리 건드리면 | host 캐시에 **못 알림** | **알릴 수 있음** (CXL.cache) |
| 결과 | coherence에서 **배제**(격리, "손님") | coherence **참여**("가족") |

→ **CXL.cache의 본질 = device를 coherence 정식 참여자로 만든 것.** 일반 디바이스가 격리됐던 이유 = 이 통보를 못 해서.

**두 사건 분리 (안 섞기):**
- **① 위치 변경 (HPA→DPA_v2)**: device 내부 재배치 → **변환표만 갱신**, 값 그대로 → coherence 무관.
- **② 값 변경**: device가 값 변경 → host 복사본 무효화 필요 → CXL.cache가 device→host 통보.

## 한 줄 요약
> cache(저장장치) ≠ CXL.cache(프로토콜). coherence는 대상이 memory(DRAM), 이름은 방법(cache line 추적)에서 옴. coherence는 "복사본 존재" 때문에 필요 → single-node도 의미. translation(HPA↔DPA, 복사본 없는 두 이름) ≠ caching(복사본, invalidate). CXL.cache의 본질 = device를 coherence 참여자로 만든 것.

---
**관련**: [[CXL Address Translation]] · [[CXL Distributed Translation]] · [[CXL Overview]] · [[CXL Glossary]]
