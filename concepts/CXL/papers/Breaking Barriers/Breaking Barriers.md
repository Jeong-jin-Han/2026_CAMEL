---
title: "Breaking Barriers: Expanding GPU Memory with Sub-Two Digit Nanosecond Latency CXL Controller"
aliases: [Breaking Barriers, CXL-GPU HotStorage]
type: paper
status: read
tags: [paper, cluster/cxl, camel-cxl-lineage]
---
# Breaking Barriers: Expanding GPU Memory with Sub-Two Digit Nanosecond Latency CXL Controller

> **Source PDF**: [Breaking Barriers.pdf](Breaking%20Barriers.pdf)
> **Authors**: Donghyun Gouk, Seungkwan Kang, Hanyeoreum Bae, Eojin Ryu, Sangwon Lee, Dongpyung Kim, Junhyeok Jang, Myoungsoo Jung (Panmnesia, Inc. · CAMEL Lab, KAIST)
> **Venue / Year**: 16th ACM Workshop on Hot Topics in Storage and File Systems (HotStorage '24), July 8–9, 2024, Santa Clara, CA
> **DOI**: 10.1145/3655038.3665953
> **Length**: 8 pages (p.108–115)
> **Read status**: ☐ Skim · ☐ Partial · ☑ Full read (2026-07-04)
> **My reading purpose**: CAMEL CXL 계보에서 **CXL-GPU 라인의 출발점**을 확정. '메모리 시스템 아키텍처(CXL-GPU·multi-node)' 방향의 배경 논문이자, **feasibility-by-building**(RTL을 실제 실리콘/FPGA로 구현) 방법론의 전형 사례로 인용하기 위함.

계보: [CAMEL Lab CXL 연구 계보](../CAMEL%20Lab%20CXL%20연구%20계보.md) — Phase 3(2024) · CXL-GPU 첫 등장.

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

대규모 딥러닝 모델의 GPU 메모리 부족 문제를, **CXL로 GPU에 DRAM/SSD endpoint(EP)를 직접 붙여** 확장하는 해법을 제안한다. 핵심은 저자들이 **직접 RTL로 설계·실리콘화한 custom CXL controller**로, **two-digit nanosecond round-trip latency**(=수십 ns)를 달성해 "CXL은 latency가 커서 GPU에는 무의미하다"는 통념을 반박한다. GPU 내부에 multiple CXL root port + host bridge를 두어 host 개입 없이 EP에 load/store로 접근하게 하고, backend media(특히 SSD)의 느린/tail latency를 숨기기 위해 **speculative read(SR)**·**deterministic store(DS)** 두 메커니즘을 추가한다. 7nm FPGA 기반 custom AIC와 RISC-V GPU(Vortex)에 통합해 실제로 동작을 보였고, 시뮬레이터 평가에서 UVM 및 상용 EP prototype 대비 각각 **2.36×·1.36×** 높은 성능을 보고한다.

---

## Core thesis

> "This effort marks the first demonstration of a real, silicon-based CXL controller achieving two-digit nanosecond round-trip latency, showcasing significant advancements in high-speed memory expansion technology." (§1, p.109)

추가 설명: GPU에 CXL을 붙이는 것 자체가 새로운 게 아니라, **GPU가 원래 갖고 있지 않은 CXL logic fabric/subsystem을 하드웨어 계층(physical→link→transaction)으로 직접 만들어** 실제 실리콘에서 수십 ns round-trip을 달성했다는 점이 thesis다. 즉 "CXL-for-GPU는 latency 때문에 불가능하다"는 전제를 **구현으로** 깨뜨렸다.

---

## Why this matters to me

내 박사 방향(메모리 시스템 아키텍처: CXL·coherence·CXL-GPU·multi-node)에서 이 논문은 **CXL-GPU 라인의 원점**이다. GPU를 CXL fabric의 1급 시민으로 만들려면 root port·host bridge·HDM decoder를 GPU 내부에 넣어야 한다는 구체적 구현 요구를 보여주며, 이는 곧 **multi-node/accelerator가 공유 메모리 공간에 참여**하는 그림의 전제 조건이다. 또 저자들이 개념 증명을 넘어 **RTL→7nm FPGA AIC**까지 내려간 점은 내가 지향하는 *feasibility-by-building*의 교과서적 예시다. "GPU memory 확장"이라는 좁아 보이는 문제가 실은 CXL coherence·multi-node 참여 문제와 같은 하드웨어 스택 위에 서 있음을 확인하는 근거로 쓸 수 있다.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| Abstract | — | p.108 | Custom CXL controller를 RTL에서 실리콘화, two-digit ns round-trip 최초 달성 + SR/DS |
| 1 | Introduction | p.108–109 | GPUDirect·UVM의 한계 → CXL을 GPU storage 확장에 도입, 기여 3가지 |
| 2 | Data Movement and CXL | p.109–110 | GPU copy-then-execute·UVM page-fault 오버헤드; CXL 프로토콜과 end-to-end latency(250ns 선행) |
| 3 | GPU Storage Expansion over CXL | p.110–112 | CXL-integrated GPU 설계(root port/host bridge/HDM) + GPU 맞춤 SR·DS |
| 4 | Evaluation | p.112–113 | 시뮬레이터로 UVM/CXL-Proto/CXL-Opt/CXL-SR/CXL-SSD/CXL-DS 비교 |
| 5 | Conclusion | p.113 | Custom CXL controller로 GPU storage 확장의 유의미한 진전 |

---

## Section notes

### §1 Introduction (p.108–109)

대규모 모델의 memory footprint가 GPU 용량을 압도한다: 10억(1 billion) 파라미터 모델도 16∼24GB를 요구하고 1000억(100 billion+) 파라미터 모델도 흔하다(§1, p.108). 기존 해법 두 갈래를 비판한다. (1) **GPUDirect storage** [15]: GPU의 PCIe BAR를 SSD에 직접 매핑하지만 SSD를 block device로 다뤄 file system 관리·I/O granularity 불일치·수동 memory operation이 필요해 복잡하다. (2) **UVM** [16]: CPU/GPU 공유 virtual address space로 단순하지만 page fault 시 host runtime 개입 오버헤드가 크다. 대안으로 **CXL**을 도입 — EP를 cacheable memory space로 매핑해 compute unit이 standard memory request로 직접 접근, JEDEC DDR의 synchronous 제약과 달리 asynchronous라 다양한 media(NVM SSD, DRAM) 수용 가능. GPU에 CXL logic fabric이 없다는 난제를 RTL 하드웨어 스택 설계로 극복.

기여 3가지(§1, p.109):
1. **CXL-integrated GPU 설계** — GPU 내부에 CXL root port를 넣어 host 개입 없이 memory expander 직접 접근.
2. **실제 실리콘 기반 CXL controller 시연** — low-latency CXL silicon stack을 GPU HW에 RTL 수준에서 통합.
3. **Speculative read·deterministic store** — SR는 target address를 예측해 EP가 미리 prefetch, DS는 GPU memory와 SSD EP에 concurrent write.

### §2 Data Movement and CXL (p.109–110)

**2.1 Data movement in GPUs.** GPU memory는 파라미터뿐 아니라 metadata·gradient·intermediate buffer도 담아, 대규모 모델의 memory 수요는 파라미터 저장에 필요한 양의 약 **8배**에 이르며 현 GPU 용량(80GB로 제한 [33])을 넘어선다(§2.1, p.109). copy-then-execute 모델(Fig 2)에서 파라미터를 tile로 나눠 layer별로 swap한다. **UVM**은 page fault 시 PCIe interrupt→host runtime이 page 할당·transfer→GPU 갱신하는데, **cacheline이 아니라 page 단위로 migration**해 실제 필요보다 많은 data를 옮기는 over-fetch가 병목이 된다.

**2.2 CXL Memory Expander.** CXL은 CXL.cache·CXL.io·CXL.mem 세 sub-protocol로 구성(§2.2, p.110). Fig 3은 host→EP의 full round-trip hardware layer stack(transaction→link→Flex Bus physical→EP)을 보인다. 저자들은 **상용/공개 실측 end-to-end latency 정보가 희소**함을 지적하며, Samsung [25]·Meta [26]의 prototype이 **250ns** round-trip을 보고했다고 인용(§2.2, p.110).

### §3 GPU Storage Expansion over CXL (p.110–112)

**3.1 Designing CXL-Integrated GPU.** 공개된 포괄적 CXL HW 스택이 없어 physical→link→transaction 계층을 직접 만들어 하나의 controller로 통합(Fig 4 silicon layout). **CXL 3.1 호환, CXL 2.0/1.1 하위 호환**이며 Flex Bus physical layer를 PCIe PCS와 통합해 PCIe/CXL 스택을 elastic buffer로 매끄럽게 지원. PCIe/CXL의 이중 요구(전력·관리)를 조율하는 **arbiter state machine** 내장. round-trip latency가 **SMT [25]·TPP [26]보다 3× 이상 빠름**을 보이며(Fig 5), 두 선행 대비 자신들은 physical~transaction까지 CXL 전용 최적화임을 강조. GPU 통합을 위해 CXL controller를 memory/SSD controller 기능과 결합해 backend storage를 **host-managed device memory(HDM)** 로 확장(Fig 3 round-trip datapath). EP를 GPU가 인식하게 하려고 **multiple root port를 가진 host bridge**를 별도 설계(Fig 6). GPU는 **Vortex**(RISC-V GPGPU [29])에 통합, SM/cluster가 LLC를 통해 system bus에 연결되고 CXL root complex가 EP 초기화·HDM decoder·HPA 범위를 담당. GPU memory map(Fig 8): SM 요청을 HDM decoder가 CXL flit으로 변환해 root port/controller로 dispatch. 전체 시스템은 **7nm FPGA 기반 custom AIC**로 구현(Fig 1b, RTL Fig 7).

**3.2 Tailoring CXL Controller for GPUs.** two-digit ns round-trip이라도 backend media(DRAM vs SSD)에 따라 latency가 달라, 두 optional 전략 도입.
- **Speculative read(SR).** CXL 2.0의 `MemSpecRd` feature 활용. root port 아래 **SR queue·memory queue 각각 32 entry**의 queue logic(Fig 9). load 요청이 SR queue에 들어가면 reader module이 SR 요청을 dispatch, EP가 target page를 미리 prefetch. `MemSpecRd` address format을 재해석해 하위 2비트로 length를 표현하고 나머지로 **256B granular offset**을 표현, memory request 1∼4개를 하나의 `MemSpecRd`로 amalgamate. QoS telemetry의 `DevLoad` 필드로 EP 혼잡도를 보고 SR 빈도 조절(부하 균형).
- **Deterministic store(DS).** 느린 write media(NVM SSD; Intel Optane [45], Samsung CMM-H [46])의 tail write latency 은폐용(Fig 10). write를 GPU memory와 SSD에 **concurrent** 전송해 compute 관점에서 즉시 완료(fire-and-forget). SSD 지연이 감지되면 GPU memory의 reserved address에 **stack 구조**로 임시 저장, 각 stack의 address는 system bus 내부 SRAM에 기록, 지연 없으면 background로 flush. 결과적으로 SM/LLC 관점에서 store가 결정적으로 보여 write latency 변동을 차폐.

### §4 Evaluation (p.112–113)

**Methodology.** HW prototype은 latency 특성은 정확하나 design space 탐색이 어려워, RTL 동작을 모사하는 시뮬레이터를 구축(실제 workload 실행·waveform dump 기반). memory latency는 **DRAMSim3** [47], PCIe/CXL bus latency는 자체 ASIC 실측값 사용(Table 1: Vortex 4 cores/2 clusters, PCIe 5.0 32GT/s x8, sync-header bypass).

비교 대상 6종: **UVM**, **CXL-Proto**(250ns, Samsung/Meta 보고 기준), **CXL-Opt**(자사 two-digit ns controller), **CXL-SR**(CXL-Opt+SR), **CXL-SSD**(Intel Optane EP), **CXL-DS**(CXL-SSD+DS). Workload 5종(Table 2): BFS(35K cyc, store 46%), SpMV(32K, 9%), GEMM(226K, 5%), Conv(2M, 97%), VecAdd(29K, 94%).

**결과.**
- UVM이 전 workload에서 최악(host runtime 개입 + page 단위 over-fetch; BFS 같은 random access에서 특히 심함).
- **CXL-Proto**가 UVM 대비 **1.8× 짧은 execution time**(§4, p.113).
- **CXL-Opt**가 CXL-Proto 대비 추가로 **1.34×** 단축. Conv·VecAdd는 memory stall이 적어 gain이 작음(Fig 12 IPC). BFS는 IPC 최저로 **execution time의 40%를 memory stall로 idle**.
- **CXL-SR**가 CXL-Opt 대비 **1.08×** 빠름(SR가 실제 요청보다 앞서 발행돼 memory latency를 compute time 뒤에 숨김).
- **CXL-DS**(Fig 13): write 위주(≥93%)인 VecAdd·Conv에서 CXL-SSD 대비 **1.65×** gain; write 비중 낮은(≤46%) 나머지는 **7.36%** gain. Optane 대신 Samsung Z-NAND [50] 같은 NAND flash면 gain이 더 클 것으로 전망.

### §5 Conclusion (p.113)

Custom-designed CXL controller를 하드웨어에 직접 구현해 **이 분야 최초의 빠른 응답 시간**을 달성하고, read/write 처리를 개선하는 기능으로 다듬어 GPU storage 용량·효율에서 유의미한 진전을 이뤘다고 정리.

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "real, silicon-based CXL controller" (개념 증명이 아닌 실제 구현 강조)
- "CXL-integrated GPU" (GPU를 CXL fabric의 1급 시민으로)
- "host-managed device memory (HDM)"

**Technical concepts:**
- "CXL root port / root complex", "host bridge with multiple root ports"
- "HDM decoder", "host physical address (HPA)"
- "Flex Bus physical layer" integrated with "PCIe PCS"
- "speculative read (`MemSpecRd`)", "deterministic store"
- "QoS telemetry (`DevLoad`)"

**Value language:**
- "round-trip latency" (JEDEC DDR의 timing spec이 아닌 full round-trip path로 CXL latency를 재정의하는 프레이밍)
- "hide the endpoint's backend media latency variation"

> ⚠ **피해야 할 어휘** (paper-signature, 그대로 echo 금지):
> - "Breaking Barriers"
> - "Sub-Two Digit Nanosecond Latency" / "two-digit nanosecond round-trip latency, the first in the field"
> - "fire-and-forget"

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| Abstract, p.108 | "achieving two-digit nanosecond roundtrip latency, the first in the field" | CXL-GPU controller latency 최초 달성 주장 |
| Title, p.108 | "Sub-Two Digit Nanosecond Latency" | ⚠ 제목은 sub-two-digit(<10ns), 본문·abstract는 two-digit(수십 ns) — 인용 시 본문 표현 권장 |
| §1, p.108 | 1 billion 파라미터 모델 "16∼24GB"; 100 billion+ 흔함 | GPU memory 부족 motivation |
| §2.1, p.109 | GPU 용량 "limited to 80 GB [33]"; memory 수요 ≈ 파라미터 저장량의 "eight times" | 용량 벽 정량화 |
| §2.2, p.110 | Samsung [25]·Meta [26] prototype round-trip "250ns" | CXL 선행 latency 기준선 |
| §3.1, p.111 (Fig 5) | 자사 controller가 SMT[25]·TPP[26]보다 "more than 3× faster" | 자사 latency 우위 |
| §1, p.109 | UVM·상용 EP prototype[25] 대비 "2.36× and 1.36× higher performance" | 종합 성능 우위 |
| §4, p.113 | CXL-Proto가 UVM 대비 "1.8× shorter"; CXL-Opt는 그 위 "1.34×"; CXL-SR는 "1.08×" | 단계별 기여 분해 |
| §4, p.113 | BFS "wastes 40% of its execution time idling due to the memory stall" | memory-bound workload 특성 |
| §4, p.113 (Fig 13) | CXL-DS: write ≥93% workload에서 "1.65×", 나머지(≤46%)는 "7.36%" | write latency 은폐 효과 |

---

## 🎯 Strategic anchor

> "This effort marks the first demonstration of a real, silicon-based CXL controller achieving two-digit nanosecond round-trip latency, showcasing significant advancements in high-speed memory expansion technology." (§1, p.109)

→ **본인 활용**: 면담·자소서에서 "CXL-GPU가 latency 때문에 불가능하다는 통념을 **개념이 아니라 실제 실리콘 구현으로** 반박한 지점(§1, p.109)이 제가 지향하는 feasibility-by-building의 원형"이라고 인용. 여기서 출발해 "single-node GPU memory 확장 → multi-node에서 여러 GPU가 CXL 공유 공간에 coherent하게 참여"로 내 방향을 확장하는 서사의 앵커로 사용.

---

## Connection to my research direction

| 차원 | 이 paper | 본인 방향 |
|---|---|---|
| Scope | single GPU + local EP(DRAM/SSD) storage 확장 | multi-node에서 여러 GPU/accelerator의 공유 메모리 참여·coherence |
| Mechanism | CXL root port/host bridge + SR·DS로 latency 은폐 | node 간 coherence·address translation·일관성 프로토콜 |
| Workload | DL 모델 파라미터 swap (copy-then-execute) | 위와 유사하나 cross-node 공유·동기화까지 |
| Open space | multi-root/multi-node coherence, 실 NAND media, host bridge 확장성 | 바로 이 open space를 내 연구 위치로 |

이 논문은 **한 GPU 안에서** CXL EP를 붙여 용량을 늘리는 데 집중하고, multi-node·여러 GPU 간 coherence는 다루지 않는다(single host bridge 관점). 내 방향은 같은 하드웨어 스택(root complex·HDM decoder·Flex Bus) 위에서 **여러 노드가 하나의 CXL 공유 메모리 공간에 coherent하게 참여**하는 문제로, 이 논문이 만든 "GPU가 CXL fabric의 1급 시민이 될 수 있다"는 전제를 그대로 물려받아 확장한다. 즉 이 paper의 종점(single-node GPU memory expansion)이 내 연구의 출발선이다.

---

## Open questions / gaps

- [ ] **Multi-node / multi-GPU coherence**: 여러 GPU가 하나의 CXL 공유 공간을 볼 때 CXL.cache coherence를 어떻게 유지하나? (이 paper는 CXL.mem 위주, single host bridge)
- [ ] **실 media 평가 부재**: DS 효과가 Optane 기준이라 과소평가됐다고 저자도 인정(Z-NAND면 더 클 것). 실제 NAND에서의 tail latency 은폐 정량치 미제시.
- [ ] **Host bridge 확장성**: multiple root port host bridge의 root port 수·HDM decoder 용량 한계는?
- [ ] **SR의 mis-speculation 비용**: 예측 실패 시 낭비된 PCIe traffic·전력의 정량 분석 없음.
- [ ] **실측 vs 시뮬레이터**: latency는 실 silicon, 성능은 시뮬레이터 — end-to-end 실측 application 성능은 미제시.

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [28] | Gouk, Kwon, Bae, Jung. "Memory pooling with CXL." *IEEE Micro* 43(2), 2023 | 같은 그룹의 CXL pooling — multi-node 메모리 공유 배경 |
| ☐ | [25] | Kim et al. "SMT: Software-defined memory tiering for heterogeneous computing systems with CXL memory expander." *IEEE Micro* 43(2), 2023 | 본 논문이 latency 비교 대상으로 삼은 선행(3× 언급) |
| ☐ | [26] | Sun et al. "Demystifying CXL memory with genuine CXL-ready systems and devices." MICRO-56, 2023 | 실 CXL 장치 latency 실측 — 내 feasibility 감각 |
| ☐ | [27] | Maruf et al. "TPP: Transparent page placement for CXL-enabled tiered-memory." ASPLOS 2023 | tiering/page placement 비교 기준 |
| ☐ | [24] | Li et al. "Pond: CXL-based memory pooling systems for cloud platforms." ASPLOS 2023 | multi-node CXL pooling 대표작 |
| ☐ | [29] | Tine et al. "Vortex: Extending the RISC-V ISA for GPGPU and 3D-graphics." MICRO-54, 2021 | 이 논문의 GPU 기반 — feasibility-by-building GPU 플랫폼 |
| ☐ | [40] | CXL Consortium. "Compute Express Link specification revision 3.1." 2023 | CXL 3.1 스펙 원문 (coherence 포함) |
| ☐ | [46] | Samsung. "CMM-H (CXL memory module, H: Hybrid)." | CXL hybrid SSD — DS 대상 실 media |

---

## Personal annotations

<자유 형식 메모. 본인이 paper 읽으며 떠올린 생각·이견·후속 아이디어. user가 직접 추가하는 영역.>

- (초기 메모) 제목 "Sub-Two Digit"과 본문 "two-digit"의 표현 불일치는 인용 시 반드시 본문(§ Abstract, p.108) 기준으로 쓸 것. 면담에서 잘못 말하면 신뢰도 손상.
