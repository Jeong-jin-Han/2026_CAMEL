---
title: "Practical Memory Disaggregation using Compute Express Link"
aliases: [Practical Memory Disaggregation using CXL, Practical Memory Disaggregation using Compute Express Link]
type: paper
status: read
tags: [paper, cluster/cxl, camel-cxl-lineage]
---
# Practical Memory Disaggregation using Compute Express Link

> **Source PDF**: [Practical Memory Disaggregation using CXL.pdf](Practical%20Memory%20Disaggregation%20using%20CXL.pdf)
> **Authors**: Donghyun Gouk, Sangwon Lee, Miryeong Kwon, Myoungsoo Jung (KAIST CAMEL)
> **Venue / Year**: WORDS 2022 (3rd Workshop On Resource Disaggregation and Serverless Computing)
> **arXiv / DOI**: workshop paper (원본 full paper는 USENIX ATC 2022 "Direct Access, High-Performance Memory Disaggregation with DirectCXL")
> **Length**: 4 pages
> **Read status**: ☐ Skim · ☐ Partial · ☑ Full read (2026-07-04)
> **My reading purpose**: CAMEL CXL 계보 Phase 1의 practical 축을 확정하기 위함. DirectCXL(ATC'22)의 축약·보완판으로, "CXL.mem 위에서 load/store만으로 disaggregation을 실제로 만든다"는 **feasibility-by-building** 원형을 내 연구 방향(CXL disaggregation, multi-node coherence, PGAS-over-CXL)의 출발점으로 인용 가능하게 정리.

> **계보 위치**: Phase 1 (2022) · DirectCXL의 practical 보완 · [CAMEL Lab CXL 연구 계보](../CAMEL%20Lab%20CXL%20연구%20계보.md)

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

기존 memory disaggregation은 page-based·object-based 어느 쪽이든 **network 기반 data exchange (RDMA)** 에 의존해, host software 개입과 반복적인 memory copy가 성능을 크게 갉아먹는다. 저자들은 CXL의 memory protocol(`CXL.mem`) 위에서 host processor complex와 remote memory를 **직접** 연결하는 **DirectCXL**을 제안한다. CXL device의 내부 DRAM(HDM)을 host system memory 공간에 mapping해, 응용이 **순수 load/store 명령만으로** disaggregated memory에 접근하게 한다 — data copy가 전혀 없다. CXL을 지원하는 OS가 없으므로 별도의 software runtime/driver를 만들어 HDM 주소공간을 segment(cxl-namespace)로 쪼개 `mmap`으로 노출한다. 실제 시스템(FPGA + in-house RISC-V host)에서 real workload 기준 conventional disaggregation 대비 평균 **3배** 성능을 보인다.

---

## Core thesis

> "We propose directly accessible memory disaggregation, DIRECTCXL that directly connects a host processor complex and remote memory resources over CXL's memory protocol (CXL.mem). ... Since DIRECTCXL does not require any data copies between the host memory and remote memory, it can expose the true performance of memory disaggregation." (Abstract, p.1)

추가 설명: disaggregation의 성능 저하는 개념 자체가 아니라 **network 기반 전송의 software/copy overhead** 때문이라는 진단이 핵심이다. CXL.mem으로 이 layer를 제거하면 disaggregation의 "true performance"가 드러난다 — 즉 문제를 protocol/interconnect 층위에서 재정의한다.

---

## Why this matters to me

내 방향은 CXL 위의 메모리 시스템 아키텍처를 **직접 만들어(feasibility-by-building)** 검증하는 것인데, 이 paper는 그 원형이다. OS가 CXL을 지원하지 않던 시점에 저자들은 host processor(RISC-V), CXL RP/EP/Switch IP, FPGA add-in-card, driver/runtime까지 **전 스택을 밑바닥부터 쌓아** disaggregation을 실제로 동작시켰다. "왜 CXL이 RDMA보다 나은가"를 시뮬레이션이 아니라 실제 하드웨어 latency 분해로 보인 점이, 내가 multi-node coherence·PGAS-over-CXL을 논할 때 성능 논증의 baseline과 방법론(직접 build → latency 분해)을 그대로 제공한다.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| Abstract | — | p.1 | CXL.mem으로 host↔remote 직결, data copy 제거 → disaggregation의 true performance 노출 |
| I | Memory Disaggregation and Its Challenge | p.1 | page-based/object-based 모두 network 기반 exchange 의존 → software+copy가 end-to-end의 66% 소모 |
| II | Direct Accessible Memory Disaggregation | p.1–2 | HDM을 host system memory에 mapping(Fig 1) + runtime이 HDM을 cxl-namespace로 쪼개 mmap 노출(Fig 2) |
| III | Evaluations and Conclusion | p.2 | in-house RISC-V+FPGA로 전부 build. RDMA latency 분해(Fig 3), real workload 평균 3× (Fig 4) |
| IV | Future Work and Acknowledgement | p.2–3 | NUMA subsystem 통합, CXL 3.0(dynamic capacity), SoC silicon fabrication |
| V | Demo and Original Publication | p.3 | demo video + 원본 USENIX ATC 2022 논문 |

---

## Section notes

### §I Memory Disaggregation and Its Challenge (p.1)

기존 disaggregation runtime을 host↔memory server 간 데이터 이동 방식으로 두 부류로 나눈다. **Page-based** [1,3,9,10,13,18,24]는 virtual memory 기법으로 코드 변경 없이 동작하며, page fault 시 host local DRAM과 remote memory 사이로 page cache를 network 너머 swap한다. **Object-based** [7,8,11,17,19,23]는 virtual memory 대신 key-value store 같은 자체 database로 remote-side에서 접근을 처리한다 — address translation 문제(page fault, context switch, write amplification)를 피하지만 대신 **상당한 source-level 수정과 interface 변경**을 요구한다.

핵심 진단: 두 방식 모두 network 기반 data exchange(예: RDMA)에 의존하는데, 이는 NIC/DMA 제어를 위한 host software 개입과 다수의 memory copy를 부르고, network(InfiniBand)와 host interface(PCIe) 간 protocol/interface 변경까지 필요로 한다.

> "Our evaluation shows that the software intervention and copy operations consume 66% of end-to-end network-based data exchange (Section III)." (§I, p.1)

### §II Direct Accessible Memory Disaggregation (p.1–2)

CXL은 host processor·accelerator·I/O device를 잇는 산업 표준 interconnect로, 원래는 heterogeneity 관리 목적이지만 **cache coherence** 능력이 low latency로 memory over-provisioning을 완화할 수 있다는 점에 주목한다 [12,20,21]. DirectCXL은 `CXL.mem`으로 host processor complex와 remote memory를 직결하며, CXL device를 **자체 HW controller를 가진 순수 passive DRAM DIMM 모듈**로 설계한다.

**Integrating CXL devices into system memory (Fig 1, p.1–2):** host system bus의 root port(RP)가 CXL device를 endpoint(EP)로 연결한다. Host-side kernel driver가 PCIe transaction으로 BAR와 내부 메모리(**HDM, host-managed device memory**) 크기를 질의(enumeration)해 host의 reserved system memory 공간에 mapping한다. HDM은 EP 내부 DRAM과 다른 주소에 mapping되므로, CXL controller가 들어오는 주소에서 HDM base를 빼서 translation한 뒤 underlying DRAM controller로 넘긴다. 결과는 CXL switch와 FlexBus를 통해 host로 돌아온다.

> "Note that, since HDM accesses have no software intervention or memory data copies, DIRECTCXL can expose the CXL device's memory resources to the host with low access latency." (§II, p.2)

**Software Runtime for DirectCXL (Fig 2, p.2):** CXL을 지원하는 OS가 없으므로 runtime/driver가 HDM을 응용의 memory 공간에 노출한다. Runtime은 HDM 주소공간을 여러 **segment(cxl-namespace)** 로 쪼개 각 namespace를 memory-mapped file(`mmap`)로 접근하게 한다. PCIe enumeration 시 driver가 entry device(`/dev/directcxl`)를 만들어 `ioctl`로 namespace를 관리하게 하고, 요청이 오면 HDM segment table(offset·size·reference count 보유)을 참조해 물리적으로 연속된 공간을 확인·할당하고 `/dev/cxl-ns0` 같은 device를 만들어 준다. 응용은 이를 `mmap`으로 자기 virtual memory 공간에 붙인다.

### §III Evaluations and Conclusion (p.2)

**Build 전 과정을 직접 구성**: 모든 DirectCXL IP를 밑바닥부터 만들고, customized FPGA add-in-card와 datacenter accelerator card로 CXL network topology를 구현했다. CXL을 지원하는 processor가 없어 host processor도 in-house로 RISC-V ISA 기반, 4개 out-of-order core에 **LLC가 CXL RP를 구현**하도록 만들었다. In-house softcore는 100MHz, CXL/PCIe IP(RP, EP, Switch)는 250MHz로 동작.

**Microbenchmark (Fig 3a, p.2):** RDMA latency를 필수 하드웨어(Memory, Network), software(Library), data copy(Copy)로 분해. Payload가 1KB 미만일 때 Library가 주 병목(평균 53.3%)이고, payload가 커지면 Copy가 늘어 총 실행시간의 37.3%까지 차지한다(사용자가 RNIC의 MR로 데이터를 전부 복사해야 하므로). 전체 평균으로 Library와 Copy가 end-to-end latency의 46.2%·19.8%를 소모한다. 반면 DirectCXL(Fig 3b)은 순수 load/store라 software도 copy overhead도 없다.

> "In contrast, ... as DIRECTCXL allows host to access remote memory resources using sheer load/store instruction, there is neither software nor data copy overhead." (§III, p.2)

**Real workloads (Fig 4, p.2):** DLRM [16], in-memory DB(MemDB [11]), Ligra [22]의 graph 분석 4종. Swap(FastSwap [2]), KVS(HERD [11]), DirectCXL 비교(Ligra는 KV 구조와 안 맞아 KVS 제외). Swap은 workload의 접근 특성을 몰라 최악. KVS는 data placement를 세밀 제어해 page 기반 관리 overhead를 없애지만 (1) 소스 코드 대폭 수정 필요(MIS·BFS·CC·BC는 사실상 불가), (2) memory node에서 hashing 같은 연산이 필요해 비용 증가라는 두 문제가 있다. DirectCXL은 소스 수정·remote-side 연산 없이 Swap 대비 3×, KVS 대비 2.2× 성능.

> "The results of our real system evaluation show that DIRECTCXL exhibits 3× better performance than conventional memory disaggregation, on average, for real-world workloads." (§III, p.2)

### §IV Future Work and Acknowledgement (p.2–3)

software·hardware 양쪽을 확장 중: i) CXL로 노출된 remote memory를 **NUMA subsystem에 통합**해 소스 수정 없이 쓰게 함, ii) in-house CXL IP를 **CXL 3.0**으로 확장해 dynamic capacity [5] 등 신기능 지원, iii) **SoC silicon fabrication**. 교신저자 Myoungsoo Jung.

### §V Demo and Original Publication (p.3)

Demo video(youtu.be/6a5NSMH-7hY)와 원본 full paper인 USENIX ATC 2022 "Direct Access, High-Performance Memory Disaggregation with DirectCXL" 링크 제공. 즉 이 workshop paper는 ATC'22의 압축·practical 요약판.

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "directly accessible memory disaggregation"
- "expose the true performance of memory disaggregation"
- "without host software intervention or memory data copies"

**Technical concepts:**
- "host-managed device memory (HDM)"
- "root port (RP) / endpoint (EP)" — CXL topology 용어
- "CXL controller translates incoming addresses by deducting HDM's base address"
- "memory-mapped files (mmap)" 기반 disaggregated memory 노출

**Value language:**
- "high memory utilization, transparent elasticity, and resource management efficiency"
- "alleviate memory over-provisioning with low latency"
- "without source modification and remote-side computation"

> ⚠ **피해야 할 어휘** (paper-signature, 직접 echo하면 모방으로 보임):
> - "DirectCXL" / "DIRECTCXL" (이 논문 고유 시스템명)
> - "cxl-namespace"
> - "sheer load/store instruction" (저자 특유 표현)
> - "pure passive modules"

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| §I, p.1 | "software intervention and copy operations consume 66% of end-to-end network-based data exchange" | RDMA 기반 disaggregation의 근본 overhead를 지적하는 motivation |
| §III, p.2 | Library가 payload <1KB에서 평균 **53.3%** 병목 | 작은 접근에서 software stack이 지배적임을 보일 때 |
| §III, p.2 | Copy가 총 실행시간의 최대 **37.3%** | 큰 전송에서 data copy가 병목으로 전환됨 |
| §III, p.2 | Library·Copy가 end-to-end latency의 평균 **46.2% / 19.8%** | network 기반 전송의 순수 overhead 정량화 |
| §III, p.2 | DirectCXL이 Swap 대비 **3×**, KVS 대비 **2.2×** | CXL 직접접근의 real workload 이득 |
| §III, p.2 | conventional disaggregation 대비 평균 **3×** | 한 줄 요약용 대표 수치 |
| §III, p.2 | in-house RISC-V 4-core OoO, LLC가 CXL RP 구현; softcore 100MHz / CXL·PCIe IP 250MHz | feasibility-by-building 방법론의 구체 스펙 인용 |

---

## 🎯 Strategic anchor

> "We built all DIRECTCXL IPs from the ground and configure many customized FPGA add-in-cards and high-performance datacenter accelerator cards to implement CXL network topology for memory disaggregation. As yet there is no processor architecture supporting CXL, so we build our own in-house host processor using RISC-V ISAs, which employs four out-of-order cores whose last-level cache (LLC) implements CXL RP." (§III, p.2)

→ **본인 활용**: 면담·자소서에서 "CXL 연구는 시뮬레이션이 아니라 host processor의 LLC까지 직접 만들어 검증한 계보에서 출발한다"는 **feasibility-by-building** 논증의 근거로 인용. 내가 multi-node coherence / PGAS-over-CXL을 "실제 하드웨어로 만들어 latency를 분해해 보이겠다"고 말할 때, 이 문장이 그 방법론의 lab 내 선례임을 짚는 anchor.

---

## Connection to my research direction

| 차원 | 이 paper | 본인 방향 |
|---|---|---|
| Scope | 단일 host ↔ passive CXL memory device 직접 접근 | multi-node/multi-host coherent memory, PGAS-over-CXL |
| Mechanism | HDM을 system memory에 mapping, load/store로 CXL.mem 접근 (data copy 제거) | node 간 coherence protocol·shared address space를 CXL 위에 구축 |
| Workload | DLRM·in-memory DB·graph (single-node 소비) | 분산·병렬 workload가 공유 메모리를 coherent하게 접근 |
| Open space | coherence는 언급만, 실제 multi-node 공유·일관성은 미구현. CXL 3.0/NUMA 통합은 future work | 바로 그 미구현 영역(multi-node coherence, dynamic capacity 공유)을 build로 채움 |

이 paper는 **1 host가 passive memory pool을 저지연으로 쓰는** 문제를 CXL.mem으로 실증했지만, 여러 host가 **동시에 coherent하게** 공유하는 문제는 열어 두었다(coherence를 CXL의 장점으로 언급하되 §II/§III에서 실제로 활용·측정하진 않음). 내 방향은 이 계보의 build 방법론(전 스택 직접 제작 + latency 분해)을 그대로 계승하되, **단일 host 접근 → multi-node coherence / PGAS-over-CXL**로 축을 옮겨, 저자들이 future work로 남긴 NUMA 통합과 CXL 3.0 dynamic capacity를 공유·일관성 관점에서 확장하는 것이다.

---

## Open questions / gaps

- [ ] Coherence를 CXL의 강점으로 내세우지만, 실제 **multi-host 공유 시 일관성 프로토콜**은 이 paper 범위 밖 — Phase 1은 single-host 직접 접근에 그침.
- [ ] In-house host가 100MHz softcore라 절대 latency 수치가 실제 CPU와 다름 — 상대 비교(3×)는 유효하나 절대 latency 인용은 주의.
- [ ] CXL switch를 통한 **fabric 확장 시 latency 스케일링**(hop 수, congestion) 미측정.
- [ ] Dynamic capacity(CXL 3.0)·NUMA 통합은 future work로만 언급, 실측 없음.
- [ ] Write amplification/consistency는 object-based 비판 맥락에서만 등장, DirectCXL 자체의 write 일관성 모델은 논의 없음.

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | 원본 | Gouk et al., "Direct Access, High-Performance Memory Disaggregation with DirectCXL", USENIX ATC 2022 | 이 workshop paper의 full 버전 — 상세 설계·평가는 여기 |
| ☐ | [4],[5] | CXL Consortium, Spec Rev 2.0 / 3.0 | dynamic capacity 등 내가 확장하려는 3.0 기능 근거 |
| ☐ | [13] | S.-s. Lee et al., "Mind: In-network memory management for disaggregated data centers", SOSP 2021 | disaggregation의 in-network 관리 대안 — 비교축 |
| ☐ | [2] | Amaro et al., "Can far memory improve job throughput?", EuroSys 2020 (FastSwap) | 이 paper의 Swap baseline 원본 |
| ☐ | [11] | Kalia et al., "Using RDMA efficiently for key-value services", SIGCOMM (HERD) | KVS baseline 원본 |
| ☐ | [14] | Lim et al., "System-level implications of disaggregated memory", HPCA 2012 | disaggregation motivation 고전 |
| ☐ | [19] | Ruan et al., "Aifm: High-performance, application-integrated far memory", OSDI 2020 | object-based far memory의 대표 — source 수정 trade-off 비교 |

---

## Personal annotations

<자유 형식 메모 — user 전용 영역.>

- (초기 노트) 이 paper의 진짜 가치는 수치가 아니라 **"OS도 CPU도 없으면 직접 만든다"는 build 태도**. 내 feasibility-by-building 서사의 lab 내 원류로 못박아 둘 것.
- 계보 메모: Phase 1 = DirectCXL(ATC'22, full) + 본 workshop paper(practical 축약). 이후 Phase에서 coherence/pooling으로 확장되는 출발점.
