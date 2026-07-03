---
title: "From Block to Byte: Transforming PCIe SSDs with CXL Memory Protocol and Instruction Annotation"
aliases: [From Block to Byte]
type: paper
status: read
tags: [paper, cluster/cxl, camel-cxl-lineage]
---
# From Block to Byte: Transforming PCIe SSDs with CXL Memory Protocol and Instruction Annotation

> **Source PDF**: [From Block to Byte.pdf](From Block to Byte.pdf)
> **Authors**: Miryeong Kwon, Donghyun Gouk, Junhyeok Jang, Jinwoo Baek, Hyunwoo You, Sangyoon Ji, Hongjoo Jung, Junseok Moon, Seungkwan Kang, Seungjoon Lee, Myoungsoo Jung (Panmnesia, Inc. · KAIST)
> **Venue / Year**: IEEE Micro 2025 (extended version, arXiv:2506.15613v1, 18 Jun 2025)
> **arXiv / DOI**: arXiv:2506.15613
> **Length**: 11 pages
> **Read status**: ☐ Skim · ☐ Partial · ☑ Full read (2026-07-04)
> **My reading purpose**: CAMEL Lab CXL 계보 Phase 4 결정판 정독. **block↔byte 전환의 완성형 논거**와 **instruction annotation**이 무엇인지, 그리고 그것이 내 방향(메모리 시스템 아키텍처: CXL-SSD, byte/block 경계, CXL address/programming semantics, feasibility-by-building)과 어디서 만나는지 확보.

계보: [CAMEL Lab CXL 연구 계보](../CAMEL Lab CXL 연구 계보.md) — Phase 4(2025) · CXL-SSD 결정판. Hello Bytes→Cache in Hand→From Block to Byte.

---

## 📋 목차

- [TL;DR](#tldr)
- [Core thesis](#core-thesis)
- [Why this matters to me](#why-this-matters-to-me)
- [Structure overview](#structure-overview)
- [Section notes](#section-notes)
- [Key vocabulary (for own writing)](#key-vocabulary-for-own-writing)
- [Citable quantitative data](#citable-quantitative-data)
- [🎯 Strategic anchor](#-strategic-anchor)
- [Connection to my research direction](#connection-to-my-research-direction)
- [Open questions / gaps](#open-questions--gaps)
- [References worth following up](#references-worth-following-up)
- [Personal annotations](#personal-annotations)

---

## TL;DR

PCIe 기반 **block storage**를 CXL memory protocol로 재해석해 **byte-addressable working memory**로 바꾸는 방법을 논한다. 핵심은 SSD의 내부 DRAM을 backend flash에 대한 **cacheable write-back inclusive cache**로 노출시키는 것 — PCIe에서는 SSD의 BAR가 **non-cacheable**로 강제되어 CPU 캐시의 이점을 못 받지만, CXL은 device memory를 host의 **cacheable system memory space**로 매핑해 이 제약을 푼다. 저자들은 이런 storage-integrated memory expander를 **CXL-SSD**라 부르고 **Type 3 endpoint**가 최적이라 주장한다. 나아가 flash 특유의 tail latency와 persistence를 다루기 위해 CXL.mem 메시지의 **10-bit reserved field**에 host semantics를 실어보내는 **instruction annotation** 두 종류 — **Determinism(DT/ND)**, **Bufferability(BF/NB)** — 를 제안한다. 16nm FPGA로 CXL-SSD를 프로토타입하고 gem5+SimpleSSD full-system 시뮬레이션으로 검증: CXL-SSD가 PCIe 기반 memory expander 대비 **10.9× 성능 향상**, annotation을 더하면 추가 **5.4× latency 감소**, high-locality workload에서는 DRAM급 성능에 근접.

---

## Core thesis

> "In this paper, we argue that CXL can effectively transform PCIe-based block storage into a large, scalable working memory by addressing the key questions outlined earlier." (Introduction, p.1)

> "we emphasize that CXL introduces a critical characteristic for inclusion in the memory hierarchy: cacheability." (p.2)

추가 설명: block storage가 memory hierarchy에 들어가지 못한 **근본 원인은 대역폭이 아니라 cacheability**다. x86(Intel/AMD)이 PCIe 매핑 메모리의 CPU 캐싱을 금지하기 때문에 SSD BAR는 non-cacheable일 수밖에 없고, 모든 load/store가 backend media까지 내려간다. CXL의 multi-protocol(CXL.mem)이 device memory를 coherent·cacheable 공간에 놓아 이 벽을 허문다. Type 3(HDM only)이 Type 2보다 나은 이유는 scalability(4,095 devices/RP vs. 16 devices/RP)와 최소 수정, coherence overhead 제거. annotation은 byte 인터페이스로 잃어버린 **storage semantics(persistence, 내부 task 스케줄링)를 host가 되돌려주는** 채널.

---

## Why this matters to me

내 박사 방향은 CXL/coherence 중심 메모리 시스템 아키텍처이고, 그중 **byte↔block 경계**와 **CXL-SSD 교집합**이 핵심 관심이다. 이 논문은 그 경계를 "대역폭 문제가 아니라 **cacheability와 semantics 전달 문제**"로 재정의한다 — 내가 막연히 "SSD를 memory로"라고 말할 때의 논거를 정확한 아키텍처 언어로 못박아준다. 특히 **instruction annotation**은 내가 관심 있는 "CXL address/programming semantics"의 구체적 실현: host가 load/store에 DT/NB 힌트를 붙여 device 내부 GC·persistence를 제어한다는 발상은, byte 인터페이스가 block 인터페이스의 표현력(fsync, flush, priority)을 어떻게 흡수할지에 대한 직접적 답이다. 또한 이들이 실제 16nm FPGA 프로토타입 + full-system 시뮬레이션으로 밀어붙인 점은 내 **feasibility-by-building** 신념과 정확히 align한다 — "된다"를 만들어서 보였다.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| — | Introduction | p.1-2 | CXL이 PCIe block storage를 scalable working memory로 변환 가능 |
| 1 | CXL Memory Protocol (Why CXL Memory for PCIe Storage?) | p.2-3 | non-cacheable BAR가 진짜 병목; cacheability가 enabler |
| 1' | Multi-Protocol & Device Type Classification | p.3 | CXL.io/.cache/.mem ↔ Type 1/2/3; Type 3가 storage에 최적 |
| 2 | Transformation of PCIe SSD (Integrating CXL Protocol) | p.3-5 | Type 3 근거, storage-side 최소 수정, address space sync, Preliminary Performance Model(FPGA) |
| 3 | Instruction Annotation | p.5-7 | DT/ND(latency), BF/NB(persistence) — 10-bit reserved field 활용 |
| 4 | Evaluation | p.7-10 | gem5+SimpleSSD; 10.9×, 5.4×, DRAM-근접 |
| 5 | Disaggregation Discussion | p.10-11 | CXL switch pooling, virtual hierarchy, MLD(최대 16) |
| — | Conclusion | p.11 | block storage를 CXL memory ecosystem에 통합하는 feasibility 입증 |

---

## Section notes

### Introduction (p.1-2)

CXL은 Gen-Z를 흡수하며 최초의 open interconnect로 자리잡았지만 **현재 표준은 block storage를 배제**한다. 저자들의 출발 질문: storage device가 CXL의 이점을 취할 수 있는가, 그 통합이 유의미한가. 답 — CXL은 PCIe storage의 block 의미를 working memory에 필요한 byte-addressable 의미로 잇는 **cost-effective·practical** interconnect다. 단, device type과 protocol 다양성을 신중히 고려해야 한다.

### Why CXL Memory for PCIe Storage? (p.2-3)

**Byte-addressability는 새로운 목표가 아니다** — NVMe 표준과 산업 prototype이 이미 SSD 내부 DRAM/buffer를 PCIe BAR로 노출해왔다. 내부 DRAM은 backend(Z-NAND, Flash, Optane)에 대한 **write-back inclusive cache**로 작동할 수 있다.

> "The best-case scenario highlights the significant advantage of CXL over PCIe-based memory expanders. While most memory requests in this scenario benefit from CPU cache hits, PCIe cannot leverage the host CPU caches, resulting in 129.5× longer latency compared to CXL." (p.5)

핵심 제약: **x86(Intel·AMD) ISA가 PCIe 매핑 메모리의 CPU-level 캐싱을 금지**한다 (system failure/storage disconnection 방지 목적). 결과적으로 PCIe storage expander는 conventional memory hierarchy의 캐싱 이점에서 배제된다. CXL은 CXL.mem으로 device memory를 coherent·cacheable하게 매핑해 이 제약을 정확히 해소한다.

### Multi-Protocol & Device Type Classification (p.3)

CXL 3-subprotocol ↔ 3-device-type 정리. CXL.io(=PCIe 대체, non-coherent load/store I/O, FlexBus 확립), CXL.cache(device→host coherent cache), CXL.mem(host→HDM 직접 접근). CXL RP가 device memory를 host cacheable system memory에 매핑. Type 1(cache, no DRAM), Type 2(cache+HDM, computationally intensive accelerator), Type 3(HDM only, memory expansion, host에 request 개시 불가). 저자 주장: PCIe storage 통합엔 **Type 3가 최적**.

### Transformation of PCIe SSD (p.3-5)

**Type 3 > Type 2 세 가지 근거** (p.4):
1. **Scalability** — Type 2는 cache coherence 유지 시 RP당 16 devices로 제한(CXL 3.0에서도), Type 3는 **4,095 devices/RP**.
2. **Overhead** — full CXL.cache/.mem은 매 load/store가 storage computing complex의 cache state를 검증하게 만들어 I/O당 다중 CXL transaction 유발.
3. **Permission** — Type 2는 memory 접근마다 host 허가 필요(full coherence 유지 위해), device-level I/O 성능 저하.

**Storage-side 수정은 경미**하다 — 기존 PCIe EP logic 확장으로 CXL transaction packet formatting·CXL.io 제어를 구현, NVMe controller의 command parsing/page copy는 단순화. **read/write는 hardware로, firmware는 internal DRAM·backend media 관리에 예약** 권장. **Address space synchronization**(Figure 3, p.4): host boot 시 CXL device 열거→내부 memory를 system memory에 매핑, HDM은 cacheable로. device의 초기 주소와 다르므로 CXL RP가 remapped offset을 device의 CXL capability/config 영역에 써서 통지(❶). application의 load/store → CXL RP가 **CXL flit** 생성 → CXL.mem으로 target storage controller에 전송(❷) → controller가 flit parse(❸) → underlying storage firmware와 조율(❹).

**Preliminary Performance Model (FPGA prototype)** (p.5): 상용 제품이 없어 CPU와 CXL storage를 **두 개의 custom FPGA board**로 프로토타입, tailored PCIe backplane 연결. Host = in-house RISC-V O3 dual-core(128KB L1, 4MB L2)에 CXL.mem/CXL.io agent 통합. Storage = **32GB OpenExpress NVMe, 16nm FPGA**, backend media는 Z-NAND emulation. **Apex-Map** benchmark(512M synthetic instructions, locality parameter α 0.001~1)로 측정 (Figure 4b):
- best case(α=0.001): CXL이 PCIe 대비 **129.5×** latency 우위(캐시 히트 덕).
- average case: CXL이 PCIe 대비 **3.1× 향상**, 단 DRAM보다는 **9.3× 느림**.
- worst case(α=1, no locality): CXL이 DRAM보다 **84.1× 느림**, 그래도 PCIe보다는 **1.6× 빠름**(BAR의 fully-synchronized 처리 회피).

대부분 workload는 high locality라 CXL-SSD의 이점이 넓게 적용된다는 논지. graph processing 등이 예외.

### Instruction Annotation (p.5-7)

Type 3는 memory pooling용이라 block storage로 쓸 때 두 문제: **(i) latency fluctuation**, **(ii) data persistence**. CXL.mem/.io는 엄격한 load/store turnaround를 강제하지 않고 async 처리 가능하지만, 길어진 latency는 host를 저하시킨다. GPF(**Global Persistent Flush**) register는 CXL network·SSD DRAM의 모든 데이터를 즉시 backend로 flush 보장하지만 추가 latency 유발.

두 annotation을 **CXL.mem M2S Req / S2M NDR 메시지의 끝 10-bit reserved field**에 실어 payload 변경·전송비용 없이 전달 (Figure 5a):

- **Determinism** — `DT`(deterministic): host가 Type 3에게 internal task 없이 처리하라 요구, 예측 가능 성능. `ND`(non-deterministic): fire-and-forget, device가 후속 ND 요청/idle 중 internal task(GC 등) 스케줄. → SSD tail latency로 인한 CPU pipeline stall 완화. CPU가 instruction queue/reorder buffer의 load 비율이 임계 초과 시 runtime에 DT 부착 권장.
- **Bufferability** — `BF`(bufferable): SSD 내부 DRAM에 caching/buffering 허용. `NB`(non-bufferable): persistence 우선, block media에 직접 write("**first-class persistence**"). DB의 transaction journaling에 NB store만 골라 쓰면 최소 overhead로 persistence 보장.

조합 사용 가능(BF+DT, BF+ND, NB+DT, NB+ND). 예: `libpmem`/`libpmemobj`의 transaction log는 commit 전엔 persistent 불필요 → BF+ND/NB+ND로 buffering. commit 시 GPF flush + NB+DT로 strict latency 내 persistence. **RocksDB 예시**(Figure 5b): BeginTransaction→ND, Put/Get(query logging)→ND, Commit→NB+DT. spinlock의 compare-and-swap, memory fence/barrier는 persistence 불필요·latency 민감 → BF+DT가 적합.

### Evaluation (p.7-10)

**Methodology** (Table 1a): gem5(CXL RP) + SimpleSSD(CXL-SSD)를 FPGA에서 관측한 실제 cycle로 보정한 full-system 시뮬레이션. Workload: SPEC CPU + RV8 Bench 18종, LLC MPKI로 정렬(>1.2 = high cache miss). 3개 expansion system(PCIe-SSD, CXL-SSD, CXL-ASSD=annotated) vs. 2개 baseline(DRAM, CXL-DRAM). 셋 다 동일 backend media.

- **PCIe-SSD**: DRAM 대비 평균 **406.5× 느림** (Figure 6a). 원인 (1) block 인터페이스의 coarse granularity, (2) 모든 load/store가 PCIe BAR로 → on-chip caching 무력화(MPKI와 무관하게 성능 불변).
- **CXL-SSD**: PCIe-SSD 대비 평균 **10.9× 향상** (Figure 6b). cacheable space 배치로 **storage access frequency 72.1% 감소**(Figure 6c). low cache-miss workload(bwaves/tonto/povray)는 storage access **80.2% 감소 → 12.2× 향상**, high cache-miss는 **62.1% 감소 → 9.6× 향상**.
- **CXL-ASSD**: CXL-SSD 대비 평균 **5.4× 향상** (annotation이 flash long latency를 hide). 성능이 DRAM에 근접, 일부(bzip2)는 CXL-DRAM에 근접.

**Sensitivity** (Figure 6d): 자주 쓰이는 함수의 **25%만 annotate해도 평균 실행시간 50.1% 감소** — 단 8개 함수(`__raw_spin_lock` 등)가 전체 storage access의 **50.5%** 차지. `clear_page_erms` annotate 시 bzip2 실행시간 **77.4% 감소**. **BF annotation이 성능 기여 최대**(SSD 내부 DRAM 활용), **DT는 tail latency 안정성에 필수**(간헐적 수 ms tail) — BF+DT 조합이 균형. Figure 6e(time-series, cactus): CXL-SSD는 random internal task로 tail latency·prolonged pipeline stall, **CXL-DT**는 load 비율 감시로 store-intensive 전환(약 90k-th instruction) 전까지 tail 억제. Figure 6f(STREAM, thread scaling): thread 적을수록 latency 민감 → CXL-ASSD가 CXL-SSD 대비 최대 **14.6×**, 64 thread에서도 **4.2×** 우위. 단 CXL-ASSD는 Z-NAND backend라 CXL-DRAM보다 **4.7× 느림**.

### Disaggregation Discussion (p.10-11)

byte 인터페이스 위에서 storage pooling. **CXL 3.1**의 FlexBus로 switch가 다중 USP/DSP 지원, USP-DSP를 reconfigurable crossbar로 연결(Figure 7a, 스위치당 4~8 high-perf port, max 8 devices). **Multi-level switch**(Figure 7b)로 host memory 확장, 최대 **4PB** address space. **Virtual Hierarchy(VH)**로 arbitrary host CPU 연결·storage를 host에 매핑(Figure 7c). **MLD(Multiple Logical Device)**: 각 endpoint를 **최대 16개 Type 3 device**로 논리 분할, 각 MLD가 자체 HDM을 서로 다른 host memory에 매핑 → fine-grained sharing. 단점: MLD는 backend/internal DRAM partitioning으로 parallelism·bandwidth 저하, single switch 공유 시 fabric congestion.

### Conclusion (p.11)

cacheability 활용 + Type 3 채택으로 block-based PCIe 의미를 memory-compatible 연산으로 잇고, DT/BF annotation으로 DRAM과의 성능 격차를 좁히며 persistence 유지. FPGA prototype + simulation이 CXL-SSD가 PCIe memory expander를 크게 앞서고 high-locality에서 DRAM급에 근접함을 입증, **block storage를 CXL memory ecosystem에 통합하는 feasibility**를 보임. corresponding author: Myoungsoo Jung (mj@panmnesia.com).

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "transform PCIe-based block storage into a large, scalable working memory"
- "from block to byte" — block↔byte 경계 전환의 압축 표어
- "memory-storage convergence"
- "storage-integrated memory expander"

**Technical concepts:**
- "cacheability as a key enabler" — 진짜 병목이 대역폭이 아님을 짚는 어휘
- "non-cacheable BAR" / "non-cacheable behavior" — PCIe storage의 근본 한계
- "instruction annotation" — DT/ND(Determinism), BF/NB(Bufferability)
- "write-back inclusive cache" (SSD 내부 DRAM의 역할)
- "address space synchronization" / "remapped address offsets"
- "Global Persistent Flush (GPF)"
- "10-bit reserved fields" (M2S Req / S2M NDR)
- "fire-and-forget policy" (ND semantics)
- "Type 3 endpoint (HDM only)" / "Multiple Logical Device (MLD)" / "Virtual Hierarchy (VH)"

**Value language:**
- "cost-effective and practical interconnect technology"
- "minimal modifications" (storage-side 수정 경미)
- "approaches DRAM-like performance"
- "foundation for future memory-storage convergence"
- "feasibility of integrating block storage into CXL's ecosystem"

> ⚠ **피해야 할 어휘** (paper-signature 강함, 그대로 echo하면 모방으로 보임):
> - "From Block to Byte" (제목 그 자체 — 표어로 인용 시 반드시 출처 명시)
> - "Determinism and Bufferability" 세트 명명 — 이 두 annotation 이름은 이 논문 고유 브랜딩
> - "CXL-ASSD" (annotated CXL-SSD의 이 논문 전용 약어)
> - "129.5× longer latency" / "406.5×" 같은 시그니처 수치를 근거 없이 재사용

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| Abstract·§4, p.1/p.8 | "CXL-SSD achieves 10.9× better performance than PCIe-based memory expanders" | CXL이 block storage를 memory로 바꿀 때의 이득 규모 |
| Abstract·§4, p.1/p.9 | annotation이 "further reduces latency by 5.4×" (CXL-ASSD vs CXL-SSD) | semantics 힌트만으로 flash tail latency hide 가능 |
| §2, p.5 | "129.5× longer latency compared to CXL" (best case, PCIe non-cacheable) | cacheability가 진짜 enabler라는 정량 근거 |
| §2, p.5 | avg: CXL "3.1× improvement over PCIe", 단 "9.3× slower than DRAM"; worst: DRAM 대비 "84.1× slower"이나 PCIe 대비 "1.6×" 우위 | byte 인터페이스의 이득/한계 균형 |
| §4, p.8 | PCIe-SSD가 DRAM 대비 "406.5× longer execution time" (평균) | block 인터페이스가 memory hierarchy에서 얼마나 불리한지 |
| §4, p.8 | CXL-SSD가 "storage access frequency by 72.1%" 감소 | on-chip caching 효과의 정량화 |
| §4, p.9 | low miss "80.2% → 12.2×", high miss "62.1% → 9.6×" | locality가 CXL-SSD 이득을 좌우 |
| §4, p.9 | "annotating only 25% of commonly used functions reduces average execution time by 50.1%"; 8개 함수가 storage access의 "50.5%" | 최소 annotation으로 큰 효과 — Pareto |
| §2, p.4 | Type 3 = "4,095 devices per RP" vs. Type 2 = "16 per RP" | Type 3 선택의 scalability 근거 |
| §5, p.10 | MLD "up to 16" Type 3 logical devices; switch "4PB" address space | disaggregation scale |
| §2, p.5 (setup) | prototype: "16nm FPGA", "32GB OpenExpress", RISC-V O3 dual-core, 128KB L1/4MB L2 | feasibility-by-building 실증 스펙 |

---

## 🎯 Strategic anchor

> "we emphasize that CXL introduces a critical characteristic for inclusion in the memory hierarchy: cacheability. This feature enables on-chip cache hits to bypass access to the underlying memory, offering a significant advantage over PCIe." (CXL Memory Protocol §, p.2)

→ **본인 활용**: 내가 "SSD를 memory로"라는 방향을 말할 때, 이 문장이 **왜 그것이 지금까지 안 됐고 CXL이 무엇을 정확히 바꾸는지**를 한 줄로 못박는다. 면담·자소서에서 "block↔byte 전환의 병목은 대역폭이 아니라 cacheability였고(§CXL Memory Protocol, p.2), 그래서 CXL-SSD가 성립한다 — 나는 여기서 host가 device 내부 task를 제어하는 semantics 채널(instruction annotation) 쪽을 확장하고 싶다"로 쓸 수 있다. 애매한 "빠르다" 대신 아키텍처적 필연을 짚는 anchor.

---

## Connection to my research direction

| 차원 | 이 paper | 본인 방향 |
|---|---|---|
| Scope | PCIe SSD → CXL Type 3 memory expander 변환 (단일 device + disaggregation) | CXL/coherence 중심 메모리 시스템 아키텍처, byte↔block 경계, multi-node coherence |
| Mechanism | cacheable HDM 매핑 + CXL.mem reserved field에 DT/BF annotation | HW/AT + OS/kernel 계층에서 CXL address/programming semantics 설계 |
| Workload | SPEC CPU/RV8, RocksDB, STREAM (locality 스펙트럼) | 메모리 집약 + persistence 요구가 섞인 실제 시스템 |
| Open space | multi-host MLD의 bandwidth/congestion, GC-latency 완전 제거 불가, coherence는 single-hierarchy 가정 | **multi-node coherence**, annotation의 OS/컴파일러 통합, address translation 계층 |

이 논문은 **single CXL hierarchy 내 host↔storage** 축을 결정판 수준으로 정리하지만, coherence는 여전히 host cache 중심의 단일 계층을 가정한다(Type 3는 host에 request 개시 불가, MLD도 각자 HDM 분할). 내 관심인 **multi-node coherence**는 이 논문이 열어둔 정확한 빈칸이다 — 여러 host가 같은 storage-integrated memory를 공유할 때 coherence·annotation semantics를 어떻게 유지할지는 Disaggregation Discussion(p.10-11)이 "careful network and storage designs 필요"로 남겨둔다. 또한 annotation을 host가 runtime에 붙이는 정책(instruction queue 감시)은 제안 수준이라, **OS/kernel·컴파일러가 언제 DT/NB를 부착할지**의 systematic 결정은 미해결 — 내 HW/AT+OS/kernel 결합 접근이 들어갈 자리다. 방법론적으로는 이들의 FPGA prototype + full-system sim이 내 feasibility-by-building 신념의 모범 사례.

---

## Open questions / gaps

- [ ] Annotation 부착 정책이 **runtime heuristic(instruction queue 임계)** 수준 — OS/컴파일러가 DT/NB/BF를 언제 붙일지의 **systematic·correct-by-construction 결정**은 열림.
- [ ] Coherence는 **single hierarchy·host 중심** 가정 (Type 3는 host에 request 불가). **multi-node/multi-host가 같은 CXL-SSD를 공유할 때의 coherence**는 미해결(내 방향).
- [ ] DT annotation도 SSD internal task를 **근본적으로 제거하지 못함**(store-intensive 전환 시 tail 재출현, Figure 6e). GC/wear-leveling과 latency SLA의 근본 tension.
- [ ] MLD는 backend/internal DRAM partitioning으로 **parallelism·bandwidth 저하** — pooling과 성능의 trade-off 정량 모델 부재.
- [ ] 평가가 **simulation(gem5+SimpleSSD) 중심** — end-to-end 실물 CXL-SSD(상용 IP)에서의 재현은 미검증(상용 제품 부재를 스스로 인정, p.5).
- [ ] Address space synchronization의 **보안/신뢰** 측면(host가 remapped offset을 device config에 write) 미논의.

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [3] | Zhong et al., "Managing memory tiers with CXL in virtualized environments," OSDI/SOSP-계열(Symp. on OS Design and Implementation) 2024 | CXL memory tiering 실사용 — annotation 정책의 상위 맥락 |
| ☐ | [5] | Hermes, Minor, Wu, Patil, Van Hensbergen, "Udon: A case for offloading to general purpose compute on CXL memory," arXiv:2404.02868, 2024 | Type 2/data-processing 대안 — 내 near-memory 관심과 교차 |
| ☐ | [8] | Gouk, Kwon, Zhang, Koh, Choi, Kim, Kandemir, Jung, "Amber: Enabling precise full-system simulation with detailed modeling of all SSD resources," MICRO 2018 | 이 논문 sim 기반(SimpleSSD 계열) — 방법론 정독 |
| ☐ | [6] | Strohmaier & Shan, "Apex-Map: A global data access benchmark...," SC'05 | Preliminary Performance Model의 locality 벤치 — 재현 시 필요 |
| ☐ | [4] | "Compute Express Link specification revision 3.1" | M2S Req/S2M NDR reserved field·MLD·VH 원전 확인 |
| ☐ | [7] | Binkert et al., "The gem5 simulator," 2011 | full-system CXL sim 구축 base |

---

## Personal annotations

<자유 형식 메모 — user 전용. 아래는 워크플로우가 남긴 초기 관찰이며 자유롭게 덮어써도 됨.>

- 이 논문의 진짜 기여는 "CXL-SSD가 빠르다"가 아니라 **"왜 지금까지 안 됐나 = non-cacheable BAR"를 정확히 짚고, byte 인터페이스가 잃는 storage semantics를 reserved field로 되돌려주는 채널을 설계"**한 점. 내가 인용할 때 성능 수치(10.9×)보다 **cacheability 논거(p.2)와 annotation 발상(p.5-7)**을 강조하는 게 차별화됨.
- 내 multi-node coherence 방향과의 접점: 이들이 "host에 request 개시 불가한 Type 3"를 고른 것은 scalability 때문인데, 이는 곧 **multi-host 공유 시 coherence를 누가 관리하나**라는 내 질문을 정확히 남긴다. Phase 4 결정판이 닫은 문 옆의 열린 문.
</content>
</invoke>
