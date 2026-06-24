---
title: "Pre-Stores: Proactive Software-guided Movement of Data Down the Memory Hierarchy"
aliases: [Pre-Stores]
description: "software pre-store(데이터를 메모리 계층 아래로 비동기 선이동)와 이를 탐지·삽입하는 DirtBuster 도구로 write-heavy 워크로드를 최대 2.3배 가속"
venue: Eurosys
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/cxl
  - topic/tiered-memory
  - topic/data-movement
  - venue/eurosys
  - year/2025
---

# Pre-Stores: Proactive Software-guided Movement of Data Down the Memory Hierarchy

> **EuroSys 2025** · `cluster/cxl` · Source: [Pre-Stores - Proactive Software-guided Movement of Data Down the Memory Hierarchy.pdf](Pre-Stores%20-%20Proactive%20Software-guided%20Movement%20of%20Data%20Down%20the%20Memory%20Hierarchy.pdf)

**저자:** Xiaoxiang Wu (University of Sydney) · Baptiste Lepers (Inria, Grenoble / University of Neuchâtel) · Willy Zwaenepoel (University of Sydney)

## TL;DR
software **pre-store**는 software prefetch의 반대 개념으로, 명령어를 코드에 삽입해 CPU가 데이터를 메모리 계층 *아래로* 비동기적으로 미리 내려보내게 한다 (`cldemote`, `clwb`, `dc cvau` 등 기존 명령어로 구현 가능). PMEM·CXL·cache-coherent FPGA처럼 DRAM과 다른 특성(큰 write granularity, 긴 latency)을 가진 메모리에서 발생하는 write amplification과 fence stall을 줄여, write-heavy 애플리케이션을 최대 2.3배 가속한다. 어디에 어떤 pre-store를 넣을지는 동적 분석 도구 **DirtBuster**가 자동으로 찾아준다.

## 문제 & 동기
현대 CPU 캐시는 전통적 DRAM을 전제로 설계되었지만, 점점 더 다양한 특성을 가진 메모리(HBM, PMEM, CXL-attached storage, cache-coherent FPGA)를 캐싱하게 되었다. 이때 두 가지 문제가 성능을 떨어뜨린다.
1. **write amplification**: CPU 캐시 라인 크기(64B)와 메모리 내부 write 단위(Optane PMEM 256B, CXL SSD 256/512B)가 다르고, 캐시가 데이터를 무작위 순서로 evict하므로, 순차적으로 쓴 데이터도 메모리에는 비순차적으로 기록되어 부분 쓰기가 증폭된다 (최대 ~400%까지 근접).
2. **delayed cache operations**: CPU는 비용이 큰 cache coherence를 미루며 수정을 private하게 유지하다가 fence(또는 atomic)를 만나면 "마지막 순간에" 한꺼번에 공개하느라 파이프라인이 stall된다. 메모리 latency가 길수록(CXL/FPGA) 이 stall이 커진다.

> [!quote]- 📄 원문 표현 (paper)
> "We show that cache performance can be improved by asynchronously moving data down the memory hierarchy 'earlier' than would be necessary because of memory models or resource constraints. We formalize this idea with the concept of software *pre-storing*... A pre-store is the converse of a pre-fetch. While a pre-fetch directs the CPU to move data up the memory hierarchy, a pre-store directs the CPU to move data down the memory hierarchy." (p.1)
>
> "Current cache implementations have been optimized to hide the latency of conventional DRAM, but fail to hide the latency of less conventional memories." (p.1)

## 핵심 통찰

> [!note]- 통찰 토글
> - **pre-fetch의 대칭 개념**: prefetch가 데이터를 계층 위로 당겨오듯, pre-store는 데이터를 계층 아래로 미리 내려보낸다. 기존 명령어(`cldemote`/`clwb`/`dc cvau`)가 본래는 ordering·persistence·coherence를 *강제*하는 용도였지만, 이를 비동기 background write를 *유도*하는 데 재활용한다 (p.2).
> - **non-blocking이 핵심**: pre-store는 데이터를 캐시에 그대로 둔 채(invalidate하지 않고) background로만 메모리에 반영하므로 CPU 파이프라인을 막지 않는다. 그래서 fence가 도래하기 *전에* 쓰기를 완료시켜 stall을 흡수한다 (p.2).
> - **두 가지 독립적 이득 원천**: (1) eviction을 순차화해 write amplification 제거, (2) fence 이전에 미리 write를 공개해 fence stall 단축. 전자는 strong memory model의 x86에서도 유효하지만, 후자는 weak memory(ARM/FPGA)에서 특히 크다 (p.4-6).
> - **수동 삽입은 비현실적**: 대규모 코드에서 어디가 write-heavy이고 데이터가 순차적인지/재사용되는지 파악하기는 어렵고 오류가 잦다. 그래서 자동 분석 도구가 필수 (p.6).

## 설계 / 메커니즘

> [!note]- 메커니즘 토글
> **pre-store API** (p.2): `prestore(void *location, size_t size, op_t op)`. `op`는 4종:
> - `demote`: 데이터를 캐시 계층 아래로 이동 (L1→L2 등). private buffer→공유 가능 캐시로. fence stall 완화용.
> - `clean`: dirty 데이터를 캐시→메모리로 writeback (캐시에는 유지). write amplification 완화용.
> - `skip`: 캐시를 우회해 register→메모리 직접 쓰기(non-temporal store). `demote`/`clean`과 달리 코드 구조를 더 복잡하게 수정해야 함.
> - (skip을 제외한) 모든 op는 데이터를 캐시에 유지(non-invalidating), non-blocking.
>
> **하드웨어 매핑** (p.2): Intel `cldemote`(demotion), `clwb`(cleaning); ARM `dc cvau`(point of unification까지 write). 모두 non-blocking이라 background pre-store 구현에 적합.
>
> **DirtBuster 도구** (p.6-7, Fig.6) — 3단계 동적 분석:
> - **Step 1**: `perf`로 load/store를 sampling(<1% overhead)해 write-intensive 함수와 callchain을 식별. memcpy 같은 generic 라이브러리에서 쓰기가 일어나므로 callchain으로 실제 앱 코드를 patch 위치로 역추적.
> - **Step 2**: Intel **PIN**으로 binary instrumentation(최대 25× slowdown, offline 전용)해 모든 read/write·fence를 기록. "sequentiality context"(메모리 영역 + 마지막 write 위치)를 다중으로 추적해 순차 쓰기를 탐지하고, fence/atomic까지의 최소 명령어 거리로 memory ordering 제약을 탐지.
> - **Step 3**: 각 cache line의 **re-write distance**(같은 라인 연속 쓰기 사이 명령어 수)와 **re-read distance**(쓰기 후 다음 읽기까지)를 B-Tree에 저장해 계산.
> - **권고 규칙**: 데이터가 re-written → `demote`(fence 전 공개 + 재쓰기 가속). re-read만 됨 → `clean`(writeback 후 캐시 유지로 재읽기 가속). 재읽기·재쓰기 모두 없음 → `skip`(non-temporal, 단 구현 어려우면 `clean`으로 대체). sampling은 step1(저오버헤드 발견)에, instrumentation은 step2-3(정밀 패턴)에 — 상호보완적.

## 평가

> [!note]- 평가 토글
> **머신** (p.3): Machine A = 2-node NUMA, 40-core Intel Xeon Gold 6230 @2.1GHz, 128GB DRAM + 8×128GB Optane NV-DIMM(내부 256B write). Machine B = Enzian, 48-core ArmV8 ThunderX-1 + Xilinx XCVU9P FPGA(cache-coherent). B-Fast = FPGA 60 cycle/10GB/s(high-end CXL 대표), B-Slow = 200 cycle/1.5GB/s(medium-tier CXL 대표).
>
> **벤치마크** (p.8, Table 2): Phoronix subset, TensorFlow(CNN), NAS(9개), KV store 2종(CLHT/Masstree index), X9 message passing.
>
> **마이크로벤치 — write amplification** (p.4, Fig.3): 1 thread 비순차 eviction 시 PMEM에 180% amplification(64B writeback→115B), 2+ thread는 330%(→211B, 최대 400%에 근접). pre-store는 amplification을 *완전히 제거*. 성능은 2 thread 2.2×, 5 thread 최대 3×.
>
> **마이크로벤치 — fence stall** (p.5, Fig.5): `demote`로 dirty 데이터를 fence 전 공개 → 최대 65% 빠름. FPGA latency가 클수록(B-Slow) overlap 가능 window가 커짐. 단 fence 직전에 demote하거나 read가 너무 많으면 이득 0으로 수렴.
>
> **TensorFlow** (p.9-10, Fig.7-8): Eigen `scalar_sum_op`(`TensorExecutor.h` line272)에 `clean` pre-store 1줄 추가. batch size 1에서 최대 47% 향상, 큰 batch는 20%. write amplification 3.7×→2.7×. (DirtBuster 권고와 반대로) `skip`은 오히려 20% 저하.
>
> **NAS HPC** (p.10-11, Fig.9): MG/FT/SP/UA/BT(쓰기 시간 >10%)에 패치, 최대 40% 빠름. MG의 `psinv`(`mg.f90` line614, 2.1MB chunk, re-read inf) → `skip` 권고, `resid`(line544) → `clean`. Fortran 무지식 상태에서도 DirtBuster가 정확히 행렬·위치 지목, 패치는 반나절 미만.
>
> **KV store** (p.11-12, Fig.10-13): YCSB A(50%PUT). CLHT `skip` 최대 2.9×(`clean` 2.3×), Masstree 2.5×(`clean` 1.9×). 128B value에서 amplification 절반, CLHT +21.4%/Masstree +7.0%. Machine B-Fast: `clean`으로 CLHT 52%·Masstree 25% 빠름(lock의 fence/CAS 시간 -74%).
>
> **X9 message passing** (p.13, Fig.14): `fill_msg` 후 `demote` 추가 → 전송 latency B-Fast 62%·B-Slow 40% 단축(compare-and-swap 시간 감소).
>
> **overhead** (p.13): DirtBuster가 부적합 판정한 곳(이득 없는 경우)에서도 pre-store overhead 최대 0.3%. FT의 `fftz2`를 수동으로 잘못 패치 시 `clean`으로 3× slowdown — DirtBuster는 이를 권고하지 않아 회피. 극단 케이스(Listing3, 같은 라인 계속 재쓰기)는 75× slowdown(메모리 vs 캐시 쓰기 latency 비율).

## 섹션 노트
- **§1 Introduction**: pre-store 개념 도입, 두 시나리오(fence 전 write 보류, 비순차 eviction). 기여 4가지: 캐시-비전통메모리 상호작용 문제 식별, software pre-storing 개념, DirtBuster 도구, pre-store 평가.
- **§2 Pre-stores**: API와 4 op, 하드웨어 명령어 매핑.
- **§3 Diversity of memory architectures**: 두 특성(write granularity 불일치, weak memory + long latency), Machine A/B 소개, Table1(Intel 64B / ThunderX 128B / Optane 256B / CXL SSD 256-512B).
- **§4 Example uses**: Problem#1 비순차 eviction(Machine A, Fig.2-3), Problem#2 delayed cache op(Machine B, Fig.4-5).
- **§5 Pitfalls**: 부적절 사용(Listing3 75× slowdown), skip vs clean 트레이드오프(skip은 캐시 비오염이나 재읽기 시 2× 느림).
- **§6 DirtBuster**: 3-step 설계, sampling+instrumentation 병용 근거, 권고 로직.
- **§7 Evaluation**: §7.2 sequentiality(Machine A), §7.3 latency(Machine B), §7.4 overhead.
- **§8 Related Work**: directing the cache(80년대~), persistence bug 탐지, PMEM latency 은닉(Shin/Ribbon/Xu), cache cleaning(timing attack 방어로도 사용), 문헌상 "pre-store" 용어의 다른 의미들(Chen et al., Kim et al.).
- **§9 Conclusion**: 두 아키텍처에서 이득 실증, DirtBuster로 위치 식별 + pitfall 회피, regression 없이 최대 2.3× 향상.

## 핵심 용어
- **pre-store (software pre-storing)**: 명령어를 삽입해 CPU가 데이터를 메모리 계층 아래로 비동기·non-blocking으로 미리 내려보내게 하는 기법. prefetch의 반대.
- **demote**: 데이터를 캐시 계층 아래(예 L1→L2)나 private buffer→공유 캐시로 이동시키는 pre-store. fence stall 완화.
- **clean**: dirty 데이터를 캐시에 유지한 채 메모리로 writeback하는 pre-store. write amplification 완화.
- **skip**: 캐시를 우회해 register→메모리 직접 쓰기(non-temporal store). 캐시 비오염, 단 구현 복잡·재읽기에 불리.
- **write amplification**: 캐시 라인(64B)과 메모리 내부 write 단위(256B 등) 불일치 + 비순차 eviction으로 실제 쓰기량이 증폭되는 현상.
- **re-write / re-read distance**: 같은 cache line의 연속 쓰기(또는 쓰기 후 읽기) 사이 명령어 수. DirtBuster가 op 선택에 사용.
- **sequentiality context**: DirtBuster가 추적하는 메모리 영역 범위 + 마지막 write 위치 기록. 순차 쓰기 탐지 단위.
- **DirtBuster**: perf sampling + Intel PIN instrumentation으로 pre-store 삽입 위치와 종류를 자동 추천하는 동적 분석 도구.
- **cache-coherent FPGA / Enzian**: CPU가 FPGA 메모리를 투명하게 캐싱하는 비대칭 NUMA 시스템(Machine B). CXL-attached memory 대용 실험 플랫폼.

## 강점 · 한계 · 열린 질문
- **강점**: 새 하드웨어 없이 기존 명령어만으로 구현; 1줄 패치로 큰 이득(TF 47%, KV 2.9×); DirtBuster가 위치·종류 자동 추천 + 무익한 곳은 거부해 regression 회피(overhead ≤0.3%); x86/ARM·PMEM/FPGA 양쪽 검증.
- **한계**: `skip`(non-temporal)은 architecture-specific이고 ARM엔 표준 라이브러리 부재 → Machine B에서 skip 미구현; DirtBuster instrumentation이 최대 25× slowdown(offline 전용); sequentiality context를 소수 객체만 추적(few large arrays 가정); 부적절 사용 시 최대 75× slowdown 위험.
- **열린 질문**: 실제 CXL Type-3 메모리(FPGA 에뮬레이션 아님)에서 동일 이득이 나는가? read-mostly·소규모 쓰기 워크로드로 적용 범위 확장 가능한가? DirtBuster 권고를 컴파일러/런타임이 자동 삽입할 수 있는가?

## ❓ Q&A (자가 점검)

> [!question]- Q1. pre-store는 prefetch와 어떻게 다른가?
> > prefetch는 CPU에게 데이터를 메모리 계층 *위로*(메모리→캐시) 당겨오라고 지시한다. pre-store는 정반대로 데이터를 계층 *아래로*(캐시→메모리, 또는 L1→L2) 비동기·non-blocking으로 내려보내라고 지시한다. 쓰기 중심 워크로드를 위한 prefetch의 대칭 개념이다.

> [!question]- Q2. pre-store가 줄이는 두 가지 성능 문제는 무엇인가?
> > (1) write amplification: 캐시 라인(64B)과 메모리 내부 write 단위(예 256B) 불일치 + 무작위 eviction 순서로 부분 쓰기가 증폭되는 문제. (2) delayed cache operation: CPU가 수정을 private하게 미루다 fence/atomic에서 한꺼번에 공개하며 파이프라인이 stall되는 문제.

> [!question]- Q3. clean과 demote와 skip은 각각 언제 쓰나?
> > 데이터가 재읽기만 됨 → `clean`(writeback 후 캐시 유지로 재읽기 가속). 재쓰기됨 → `demote`(fence 전 미리 공개 + 재쓰기 가속). 재읽기·재쓰기 모두 없음 → `skip`(non-temporal store로 캐시 비오염), 단 구현이 어려우면 `clean`으로 대체.

> [!question]- Q4. pre-store는 새로운 하드웨어를 요구하는가?
> > 아니다. 기존 명령어를 재활용한다. Intel은 `cldemote`(demotion)와 `clwb`(cleaning), ARM은 `dc cvau`(point of unification까지 write)를 제공한다. 이들은 본래 ordering/persistence/coherence 강제용이지만 모두 non-blocking이라 background pre-store로 쓸 수 있다.

> [!question]- Q5. DirtBuster가 sampling과 binary instrumentation을 둘 다 쓰는 이유는?
> > sampling(perf)은 <1% overhead로 write-intensive 함수를 찾는 데 좋지만(Step1) 너무 거칠어 순차 stride나 re-read/re-write distance를 정확히 못 잡는다. instrumentation(PIN)은 최대 25× slowdown으로 Step1엔 부적합하고 cache thrashing으로 오탐을 유발하지만, 모든 read/write의 정확한 view를 줘 Step2-3 패턴 분석에 필수다. 둘은 상호보완적이다.

> [!question]- Q6. weak memory(ARM/FPGA)와 strong memory(x86)에서 이득 원천이 어떻게 다른가?
> > strong memory model(x86)은 쓰기를 순서대로 공개하도록 강제하므로 fence 이전 데이터를 미리 캐시에 보류하는 이득(demote)이 거의 없다. 대신 비순차 eviction 순차화(clean)로 write amplification을 줄이는 이득이 크다. weak memory(ARM/FPGA)는 fence stall 완화(demote) 이득이 크며, FPGA latency가 길수록 overlap window가 커져 더 유리하다.

> [!question]- Q7. pre-store를 잘못 쓰면 어떤 일이 생기나?
> > 같은 cache line을 계속 재쓰기하는 코드에 clean을 넣으면(원래는 캐시에서 덮어쓰일 데이터를 매번 메모리에 불필요하게 writeback) 최대 75× slowdown이 난다 — 이는 메모리 쓰기 vs 캐시 쓰기 latency 비율과 같다. DirtBuster는 re-write distance를 보고 이런 위치를 권고하지 않아 회피한다.

> [!question]- Q8. 실험에서 CXL 메모리를 어떻게 대표했나?
> > 실제 CXL 장치 대신 Enzian 플랫폼의 cache-coherent FPGA(Machine B)를 사용했다. FPGA latency/bandwidth를 조절해 B-Fast(60 cycle/10GB/s, high-end CXL 대표)와 B-Slow(200 cycle/1.5GB/s, medium-tier CXL 대표) 두 구성으로 실험했다. 또 Machine A의 Optane PMEM(256B write)으로 CXL SSD(256-512B)와 유사한 write granularity를 대표했다.

## 🔗 Connections
[[CXL]] · [[EuroSys]] · [[2025]]

## References worth following
- Lepers & Zwaenepoel, "Johnny cache: the end of DRAM cache conflicts (in tiered main memory systems)", OSDI 2023 — 같은 저자, tiered memory 캐시 충돌 (ref [28]).
- Cock et al., "Enzian: an open, general, cpu/fpga platform for systems software research", ASPLOS 2020 — Machine B 플랫폼 (ref [9]).
- Shin et al., "Hiding the long latency of persist barriers using speculative execution", ISCA 2017 — PMEM persist latency 은닉 대안 (ref [42]).
- Xu et al., "Asymmetry & locality-aware cache bypass and flush for nvm-based unified persistent memory", ISPA 2019 — 불필요 writeback 회피 (ref [49]).
- Kaiyrakhmet et al., "SLM-DB: Single-Level Key-Value store with persistent memory", FAST 2019 — PMEM KV store 비교점 (ref [23]).
- Das Sharma et al., "An introduction to the compute express link (cxl) interconnect", arXiv 2306.11227 — CXL 배경 (ref [41]).

## Personal annotations
<본인 메모 영역>
