---
title: "CXL-GPU: Pushing GPU Memory Boundaries with the Integration of CXL Technologies"
aliases: [CXL-GPU, CXL GPU]
type: paper
status: read
tags: [paper, cluster/cxl, camel-cxl-lineage]
---
# CXL-GPU: Pushing GPU Memory Boundaries with the Integration of CXL Technologies

> **Source PDF**: [CXL-GPU.pdf](CXL-GPU.pdf)
> **Authors**: Donghyun Gouk, Seungkwan Kang, Seungjun Lee, Jiseon Kim, Kyungkuk Nam, Eojin Ryu, Sangwon Lee, Dongpyung Kim, Junhyeok Jang, Hanyeoreum Bae, Myoungsoo Jung (Panmnesia, Inc. + KAIST)
> **Venue / Year**: IEEE Micro 2025 (extended version of the accepted paper)
> **arXiv / DOI**: arXiv:2506.15601v1 [cs.AR], 18 Jun 2025
> **Length**: 11 pages
> **Read status**: ☐ Skim · ☐ Partial · ☑ Full read (2026-07-04)
> **My reading purpose**: CAMEL Lab CXL 연구 계보 Phase 4(2025) · **GPU 라인 결정판** 이해. 특히 (1) feasibility-by-building의 모범 사례 — RTL 수준 custom CXL controller를 실제 silicon(7nm AIC)으로 만든 과정, (2) [Breaking Barriers](../Breaking Barriers/Breaking Barriers.md) 대비 무엇이 full paper로 확장됐는지, (3) 내 메모리 시스템 아키텍처 방향(CXL-GPU·multi-node coherence)에서 재사용 가능한 mechanism(SR/DS, DevLoad-기반 QoS 제어).

> 계보: [CAMEL Lab CXL 연구 계보](../CAMEL Lab CXL 연구 계보.md)

---

## 📋 목차

- [TL;DR](#tldr)
- [Core thesis](#core-thesis)
- [Why this matters to me](#why-this-matters-to-me)
- [Structure overview](#structure-overview)
- [Section notes](#section-notes)
- [Breaking Barriers 대비 확장점](#breaking-barriers-대비-확장점)
- [Key vocabulary (for own writing)](#key-vocabulary-for-own-writing)
- [Citable quantitative data](#citable-quantitative-data)
- [🎯 Strategic anchor](#-strategic-anchor)
- [Connection to my research direction](#connection-to-my-research-direction)
- [Open questions / gaps](#open-questions--gaps)
- [References worth following up](#references-worth-following-up)
- [Personal annotations](#personal-annotations)

---

## TL;DR

GPU 메모리 용량 한계(LLM/MoE의 폭증하는 memory demand)를 **CXL로 storage(DRAM 및/또는 SSD)를 GPU에 직접 붙여** 확장하는 시스템을 제안한다. 핵심은 host 개입 없이 GPU가 endpoint(EP) 메모리에 직접 접근하도록 **CXL root port를 GPU 아키텍처 내부에 통합**하고, custom CXL controller를 **RTL 수준에서 설계·siliconize(7nm FPGA-based AIC)** 한 것 — 저자 주장으로 "two-digit nanosecond roundtrip latency, the first in the field". 여기에 backend media(특히 SSD)의 느린/변동하는 latency를 숨기기 위한 두 mechanism, **speculative read(SR)** 와 **deterministic store(DS)** 를 얹었다. 평가에서 UVM 대비 44.2×, ideal GPU-DRAM에 근접(2.3~19.7% 이내)하는 성능을 보인다.

---

## Core thesis

> "We developed and siliconized a custom CXL controller integrated at the hardware RTL level, achieving two-digit nanosecond roundtrip latency, the first in the field." (Abstract, p.1)

GPU에 native CXL logic fabric이 없다는 근본 장벽을, GPU 시스템 버스에 **CXL root complex(multiple root ports + host bridge + HDM decoder)를 직접 삽입**하는 아키텍처 개조로 해결한다. 그리고 이를 이론이 아니라 **실제 실리콘으로 구현해 tens-of-ns round-trip latency를 실측**함으로써, CXL 기반 GPU memory expansion이 실전에서 동작함을 증명한다. SR/DS는 이 하드웨어 위에서 SSD backend의 latency variation을 소프트웨어 개입 없이 흡수하는 보완 mechanism이다.

---

## Why this matters to me

내 박사 방향은 **메모리 시스템 아키텍처(CXL/coherence)이고, "feasibility-by-building"** 를 선호한다. 이 논문은 바로 그 방법론의 교과서적 사례다 — 개념(CXL-GPU)을 슬라이드가 아니라 RTL→silicon(7nm AIC)까지 내려서 latency를 실측했다. 내가 CXL-GPU나 multi-node coherence를 다룰 때 "왜 굳이 하드웨어까지 만드느냐"에 대한 강력한 레퍼런스가 된다. 또한 SR/DS의 **DevLoad(CXL flit의 QoS telemetry) 기반 동적 제어**는 multi-node/coherence 환경에서 traffic regulation을 설계할 때 재사용할 수 있는 primitive다. Breaking Barriers(원논문) 대비 이 full paper가 mechanism·evaluation을 어디까지 열어놨는지가, 내가 그 위에 무엇을 더 얹을 수 있는지(open space)를 결정한다.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| Abstract + Intro | 문제·기여 | p.1–2 | UVM/GPUDirect 한계 → CXL root port 통합 GPU, siliconized controller, SR/DS. UVM 대비 2.36×, 상용 EP prototype 대비 1.36× |
| Memory Management in GPU and CXL | 배경 | p.2–4 | copy-then-execute, UVM, GPUDirect/NVMMU의 host-intervention 병목; CXL sub-protocol(.cache/.io/.mem); CXL end-to-end latency 분해; CXL-SSD |
| Design of CXL-integrated GPU | 아키텍처 | p.4–6 | CXL hardware layer stack(Flex Bus PCS로 PCIe/CXL 공용), arbitrator state machine; CXL root complex + host bridge + HDM decoder를 Vortex GPU 시스템 버스에 통합; 7nm AIC |
| Optimization of CXL Controller | mechanism | p.6–8 | SR(MemSpecRd 재활용, SR/memory queue 각 32-entry, DevLoad 4-state 제어, address window control); DS(reserved space 활용, stack 구조, GC tail latency 은닉) |
| Evaluation | 평가 | p.8–11 | 6개 config(UVM/GDS/CXL/CXL-SR/CXL-DS/GPU-DRAM ideal), Rodinia 11 + real(gnn,mri), Optane/Z-NAND/NAND. CXL이 UVM 대비 44.2×, GPU-DRAM 근접 |
| Conclusion | 결론 | p.11 | CXL multiple root port로 GPU storage 확장, HW-native controller가 fast response 달성 |

---

## Section notes

### Intro — 문제 정의 (p.1–2)

LLM/MoE의 memory demand가 GPU 용량(수십~수백 GB)을 크게 초과. 기존 해법의 한계를 명확히 정리한다: **GPUDirect Storage**는 SSD를 GPU BAR에 직접 매핑하지만 file system·I/O granularity mismatch·`cuFileWrite` 같은 수동 개입으로 "copy-then-execute" 모델을 복잡하게 만들어 채택이 제한됨. **UVM**은 shared virtual memory로 편의성은 좋지만 page fault 해결에 **host runtime intervention** 이 개입해 큰 latency 병목. 근본 원인은 GPU에 **native CXL logic fabric/subsystem이 없다**는 것 — 그래서 저자들은 CXL hardware layer stack을 직접 개발해 custom CXL controller(RTL)로 구현하고, 이를 갖춘 multiple CXL root port 아키텍처를 제안한다.

> "our GPU storage expansion approach significantly outperforms the UVM strategy and a commercial EP prototype controller, achieving 2.36× and 1.36× higher performance, respectively" (p.2)

핵심 기여 3가지: (1) **CXL-integrated GPU 설계**(CXL root port + internal modification으로 host 개입 없이 memory expander 직접 접근), (2) **silicon-based CXL controller 실증**(low-latency CXL silicon stack을 GPU에 RTL 통합), (3) **SR/DS mechanism**.

### Memory Management in GPU and CXL (p.2–4)

**copy-then-execute** 모델(Figure 2a)에서 parameter를 tile로 나눠 layer output vector를 swap하는데, 이 잦은 데이터 이동이 성능 오버헤드·사용성 저하의 원인. GPU memory 요구량은 "**about eight times larger than the memory needed to store their parameters alone**"(p.2). UVM은 page fault 시 host interrupt→runtime이 page 할당·전송(Figure 2b)하는데 이 **host runtime intervention latency**가 핵심 병목. GPUDirect/NVMMU도 storage DMA engine에만 의존해 on-demand page fault 시 결국 host가 개입.

**CXL sub-protocol** 3종 정리: CXL.cache(coherence 유지), CXL.mem(packetized flit로 PCIe transport 위에서 asynchronous memory op — JEDEC의 rigid timing 제약 탈피), CXL.io(PCIe-유사, device enumeration/bulk I/O). 대표 use case가 **CXL memory expander**. **CXL end-to-end latency**(Figure 3a)를 host CPU→EP 전 layer(transaction/link/Flex Bus PHY)로 분해. Samsung/Meta prototype 실측이 **250ns**(p.4). CXL-SSD 통합은 Intel의 CXL-attached Optane SSD, Samsung CMM-H 언급 — SSD expander는 내부 DRAM cache 관리와 write(GC/wear-leveling) 관리가 관건임을 예고(→ DS의 동기).

### Design of CXL-integrated GPU (p.4–6)

publicly accessible한 CXL hardware stack이 없어서, 3개 sub-protocol을 지원하는 **통합 controller를 직접 개발**(Figure 4 silicon layout). **CXL 3.1 호환 + 2.0/1.1 backward**. Flex Bus physical layer를 자체 **PCS(physical coding sublayer)** 와 통합해 PCIe/CXL stack을 elastic buffer 위에서 공용. power management/administrative op를 위한 **arbitrator state machine**으로 PCIe/CXL task 자원 할당.

> "The controller has undergone extensive testing, achieving a round-trip latency in the range of tens of nanoseconds, including the overhead associated with protocol conversion between standard memory operations and CXL flit-based transmissions." (p.5)

SMT[4]·TPP[5]와 비교해 **latency가 3배 이상 빠름** — 저자 가설로 SMT/TPP는 PCIe 아키텍처에 의존하는 반면 자기들은 physical/link/transaction 전 layer를 CXL에 fully optimize. **GPU 통합**(Figure 5): Vortex(RISC-V GPGPU) 시스템 버스에 CXL root complex + simplified core(EP 초기화) + host bridge HDM decoder + root port별 HDM을 삽입. firmware가 CXL EP의 config space/PCIe BAR/HDM capability register를 읽어 각 root port의 HDM base/size를 host bridge HDM decoder에 기록. GPU가 backend storage를 **host-managed device memory(HDM)** 로 노출. 실제 시스템은 **7nm FPGA-based custom AIC**로 구현.

### Optimization: Speculative Read (SR) (p.6–7)

round-trip latency는 two-digit range지만 backend media(DRAM/SSD)에 따라 변동 → SR은 read latency 최소화용. CXL 2.0의 **MemSpecRd**(speculative read) 재활용. root port 아래 queue logic이 **SR queue + memory queue(각 32-entry)** 로 구성. SR reader가 load address를 ring buffer에 기록, 이전 SR과 주소가 매칭되면 EP-side controller가 실제 request 도착 전에 데이터를 prefetch.

**Implementation 디테일**: MemSpecRd address format을 변형 — 하위 2 bit를 length(1~4)로 재활용해 64B가 아닌 **256B offset**으로, 여러 memory request를 하나의 MemSpecRd로 aggregate. **DevLoad(2-bit) 4-state 제어**: light(ll)/optimal(ol)/moderate overload(mo)/severe overload(so). ll이면 granularity를 256B→1024B로 키워 efficient prefetch, ol은 유지, mo는 축소, so는 SR 일시 중단. **Address window control**(Figure 7): memory queue/SR queue를 분석해 SR granularity 단위로 window를 상향 shift·rounding, 불필요한 internal DRAM pollution(64B 실접근인데 wrong-direction으로 large-granular read 시) 방지.

### Optimization: Deterministic Store (DS) (p.7–8)

SSD write는 GC 등으로 tail latency 발생. DS는 GPU memory의 **reserved space** 활용(Figure 8). write를 GPU memory와 SSD에 **동시 발행** → request 즉시 release. SSD write 지연 시 데이터를 GPU memory reserved address에 임시 저장(**stack 구조**, tail/slow write 감지 시 collapse), 문제없으면 GPU memory 갱신 후 background로 flush. 각 stack entry 위치는 시스템 버스 내부 SRAM의 **address list(red-black tree)** 로 관리. SM/LLC 관점에선 store가 **deterministic**하게 보여 write latency variation을 차폐. 내부 task(GC 등)에 대해서도 DevLoad를 write용으로 활용해 throughput 저하 시 write를 dynamically throttle하고 read는 GPU memory buffer에서 우선 서빙.

### Evaluation (p.8–11)

**Methodology**: 하드웨어 prototype은 정밀 latency는 주지만 design space 탐색이 어려워, prototype 동작을 모델링한 **simulator**(RTL behavior + real workload waveform, 메모리 latency는 DRAMSim3[10]) 사용. PCIe/CXL bus 값은 자사 ASIC 실측. UVM/GPUDirect host runtime overhead는 **~500μs**(Allen&Ge[11] 기반)로 반영.

**Config 6종**: UVM, GDS(GPUDirect), CXL(본 논문 optimized controller expander), CXL-SR, CXL-DS, 그리고 ideal **GPU-DRAM**(충분한 on-device 메모리 가정, 모든 결과의 normalization 기준). SR/DS는 Optane/Z-NAND/NAND backend로 평가. **Workload**: Rodinia 11개 + real-world 2개(gnn=bfs+vadd+gemm, mri=sort+conv3). compute/load/store-intensive로 분류.

**DRAM-based(Figure 9a)**: UVM은 GPU-DRAM 대비 **52.7× 느림**(gemm/vadd/saxpy 같은 sequential once-read 패턴에서 page fault 폭증). CXL은 host 개입 없이 확장 DRAM 직접 접근으로 **UVM 대비 44.2× 개선**, GPU-DRAM에 근접("slower by only 2.3%, 19.7%, and 6.8% for compute-intensive, load-intensive, and store-intensive", p.9).

**SSD-based(Figure 9b, log scale)**: **CXL-SR은 CXL 대비 평균 7.4× 개선**. vadd/saxpy(1D vector)가 최대 **15.6×**, graph(path/bfs)는 irregular access로 평균 **67.6%**. **CXL-DS는 CXL-SR 대비 20.9%/8.7%/62.8%(compute/load/store) 추가 개선** — store-intensive에서 GC tail latency 은닉 효과가 큼.

**Backend media sensitivity(Figure 9c)**: SR 평균 개선 **Optane 7.1×, Z-NAND 8.8×, NAND 10.1×**. DS는 bfs에서 **최대 4× gain**. **SR 분해(Figure 9d)**: CXL-NAIVE(무조건 64B MemSpecRd)는 CXL 대비 1.9×지만 GPU-DRAM보다 여전히 6.6× 느림; CXL-DYN(DevLoad로 granularity 조절)은 Seq에서 추가 4.5×, SSD DRAM hit rate를 99% 이상으로; CXL-SR(address window 추가)은 Around에서 추가 2.1×(hit rate 75.8%). **DS 분해(Figure 9e)**: CXL-SR은 GC 후 ingress queue가 delayed write로 차서 load/store latency 급등하나, CXL-DS는 GC 동안 store를 GPU local memory로만 forward해 tail latency와 GC 재발을 함께 억제.

### Conclusion (p.11)

CXL multiple root port로 DRAM/SSD를 GPU에 통합, **hardware-native custom controller** 가 fast response 달성. SR/DS로 read/write handling까지 개선해 GPU storage 용량·효율의 유의미한 진전.

---

## Breaking Barriers 대비 확장점

[Breaking Barriers](../Breaking Barriers/Breaking Barriers.md)가 원논문(short)이고, 본 논문은 그 **extended full version**(IEEE Micro accepted paper의 확장, 표지 각주 명시). 확장된 지점(내가 인용 시 구분해야 할 것):

- **Mechanism 상세화**: SR의 MemSpecRd address format 변형(256B offset, length 2-bit), SR/memory queue 32-entry, DevLoad 4-state(ll/ol/mo/so) 제어, address window control이 full-paper 수준으로 열림. DS의 stack + red-black tree address list, GC 중 write throttling까지 구체화.
- **Design 상세화**: Flex Bus PCS 통합, arbitrator state machine, HDM decoder를 통한 firmware 초기화 흐름, Vortex 시스템 버스 통합 구조(Figure 5) 구체 서술.
- **Evaluation 확대**: 6 config × Rodinia 11 + real-world 2, Optane/Z-NAND/NAND별 SR/DS 민감도(Figure 9c–e), CXL-NAIVE/CXL-DYN/CXL-SR ablation 등 정량 근거 대폭 추가.

(정확한 삭제/축약 대응은 Breaking Barriers 노트와 교차 확인 필요 — 위는 full paper에서 확실히 확장된 축.)

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "GPU storage expansion solution utilizing CXL"
- "novel GPU system design with multiple CXL root ports"
- "CXL-integrated GPU"
- "host-managed device memory (HDM)"

**Technical concepts:**
- "CXL hardware layer stack" / "CXL logic fabric"
- "CXL root complex / CXL root port + host bridge + HDM decoder"
- "Flex Bus physical layer with physical coding sublayer (PCS)"
- "arbitrator state machine"
- "DevLoad (QoS telemetry) field" — light/optimal/moderate/severe load 4-state
- "backend media latency variation"
- "address window control" / "internal DRAM pollution"

**Value language:**
- "without host intervention" / "host runtime intervention"
- "siliconized ... at the hardware RTL level"
- "round-trip latency in the range of tens of nanoseconds"
- "hide the endpoint's backend media latency variation"

> ⚠ **피해야 할 어휘** (paper-signature — 그대로 echo 금지):
> - "two-digit nanosecond roundtrip latency, the first in the field"
> - "Pushing GPU Memory Boundaries"
> - "speculative read and deterministic store" (SR/DS 브랜드 페어)
> - "fire-and-forget" (store 전략 별칭)
> - "copy-then-execute model"

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| Intro, p.1 | "models with 1 billion parameters require approximately 16~24 GB of GPU memory ... models exceeding 100 billion parameters are increasingly commonplace" | GPU memory wall / CXL 확장 motivation |
| p.2 | GPU memory 요구량이 "about eight times larger than the memory needed to store their parameters alone" | LLM training memory footprint 근거 |
| Abstract, p.1 | "two-digit nanosecond roundtrip latency, the first in the field" | CXL controller feasibility (직접 인용 시 출처 명시) |
| §Design, p.5 | "round-trip latency in the range of tens of nanoseconds, including ... protocol conversion" | HW-native CXL controller 실측 latency |
| p.4 | Samsung/Meta CXL prototype "latency of 250ns" | 기존 CXL expander latency baseline |
| p.5 | 자사 controller가 SMT/TPP 대비 "over three times faster" | CXL full-optimization 이득 |
| p.6 | "7 nm FPGA-based custom add-in card (AIC)"; SR/memory queue "each with a capacity of 32 entries" | 구현 feasibility 근거 |
| p.9 | "UVM performs 52.7× worse than GPU-DRAM" | host-intervention 병목 크기 |
| p.9 | CXL "performance improvement of 44.2× compared to UVM"; GPU-DRAM 대비 "slower by only 2.3%, 19.7%, and 6.8%" | CXL expander가 ideal 근접 |
| p.9 | "CXL-SR achieves an average performance improvement of 7.4× compared to CXL" | SR의 SSD backend 이득 |
| p.10 | SR 평균 "7.1×, 8.8×, and 10.1× for Optane, Z-NAND, and standard NAND" | media별 SR 효과 |
| p.9–10 | CXL-DS가 CXL-SR 대비 "20.9%, 8.7%, and 62.8%"(compute/load/store); DS "up to 4× ... for bfs" | DS의 write/tail-latency 은닉 |
| p.2 | "2.36× and 1.36× higher performance" (vs UVM / 상용 EP prototype) | 종합 성능 우위 |

---

## 🎯 Strategic anchor

> "We developed and siliconized a custom CXL controller integrated at the hardware RTL level, achieving two-digit nanosecond roundtrip latency, the first in the field." (Abstract, p.1) — 뒷받침: "achieving a round-trip latency in the range of tens of nanoseconds, including the overhead associated with protocol conversion" (§Design, p.5), 구현체 "7 nm FPGA-based custom add-in card (AIC)" (p.6)

→ **본인 활용**: 면담/자소서에서 "**feasibility-by-building**" 방법론의 레퍼런스로. 개념을 RTL→silicon(7nm AIC)까지 내려 latency를 실측해야 CXL-GPU의 실전 성립을 논증할 수 있다는 근거. 내가 CXL-GPU/multi-node coherence를 "만들어서 증명" 하겠다고 말할 때, 이 논문의 tens-of-ns 실측을 "이 수준의 HW-native 구현이 이미 가능함"의 출발점으로 인용.

---

## Connection to my research direction

| 차원 | 이 paper | 본인 방향 |
|---|---|---|
| Scope | 단일 GPU ↔ CXL storage(DRAM/SSD) expansion | CXL-GPU를 넘어 **multi-node coherence**(여러 노드/가속기 간 shared memory) |
| Mechanism | CXL root port GPU 통합 + SR/DS로 single-EP backend latency 은닉 | coherence protocol · distributed translation · node 간 traffic regulation |
| Workload | LLM/MoE memory footprint, Rodinia 벤치 | 동일 memory-bound 워크로드 + cross-node sharing/coherence 패턴 |
| Open space | multi-root-port지만 **coherence·multi-node 미다룸**; SR/DS는 single node 내 QoS | multi-node에서 DevLoad류 telemetry로 coherence traffic 제어 = 확장 여지 |

이 논문은 **single-node GPU의 memory boundary 확장**에 집중한다(CXL.mem 중심, CXL.cache coherence는 배경 설명에 그침). 내 방향은 여기서 한 축 올라간 **multi-node/coherence** — 즉 이 논문의 CXL root port·HDM·DevLoad 제어를 building block으로 재사용하되, 여러 노드가 memory를 공유할 때의 coherence와 traffic regulation을 새로 설계한다. 특히 DS가 GPU reserved space + address list로 write를 deterministic하게 만든 아이디어는, multi-node에서 write ordering/coherence를 하드웨어로 보장하는 설계로 확장할 여지가 크다. feasibility-by-building 관점에서 이 논문은 "여기까지 실리콘으로 됐다"는 baseline이고, 나는 그 위에 coherence layer를 얹는 셈.

---

## Open questions / gaps

- [ ] **Multi-node / coherence 부재**: multiple root port지만 여러 host/GPU 간 shared-memory coherence는 다루지 않음. CXL.cache 활용은 배경 수준.
- [ ] **평가가 simulator 중심**: 실 silicon은 latency 실측만, 성능 비교는 prototype-모델 simulator. 실제 end-to-end throughput 실측 부재.
- [ ] **DS reserved space 오버헤드**: GPU memory reserved space를 write buffer로 쓰는데, 그 용량/capacity pressure가 memory-bound 워크로드에 주는 영향 정량화 부족.
- [ ] **DevLoad 제어의 fairness**: multi-tenant/multi-EP에서 SR traffic QoS가 EP 간 공정성에 미치는 영향 미논의.
- [ ] **CXL-SSD write 신뢰성**: GC/wear-leveling과 DS의 상호작용을 mechanism으로 언급하나 endurance/reliability 정량 평가는 없음.

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [3] | M. Jung, "Hello bytes, bye blocks: PCIe storage meets CXL for memory expansion," HotStorage 2022 | 랩 CXL-SSD 계보의 출발점(Phase 2). 이 논문 storage-expansion 사상의 뿌리 |
| ☐ | [4] | K. Kim et al., "SMT: Software-defined memory tiering ... with CXL memory expander," IEEE Micro 2023 | 본 논문 latency 비교 baseline. 같은 계보의 tiering 접근 |
| ☐ | [5] | H. A. Maruf et al., "TPP: Transparent page placement for CXL-enabled tiered-memory," ASPLOS 2023 | Meta의 대표 CXL tiering, 비교 대상. multi-node 확장 시 참고 |
| ☐ | [6] | B. Tine et al., "Vortex: Extending the RISC-V ISA for GPGPU and 3D-graphics," MICRO 2021 | 본 논문 base GPU. RTL 통합의 토대 이해에 필수 |
| ☐ | [8] | J. Zhang et al., "NVMMU: A non-volatile memory management unit for heterogeneous GPU-SSD architectures," PACT 2015 | GPU-SSD 직접 데이터 이동의 선행. GPUDirect 한계 근거 |
| ☐ | [9] | CXL Consortium, "CXL specification rev 3.1," 2023 | MemSpecRd/DevLoad/HDM 정의 원출처. mechanism 재사용 시 필독 |
| ☐ | [11] | T. Allen, R. Ge, "Demystifying GPU UVM cost with deep runtime and workload analysis," IPDPS 2021 | UVM host-intervention ~500μs 근거. UVM 병목 정량 인용원 |

---

## Personal annotations

<자유 형식 메모 — user 전용 영역.>
