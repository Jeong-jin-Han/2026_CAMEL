---
title: "Direct Access, High-Performance Memory Disaggregation with DirectCXL"
aliases: [DirectCXL, Direct CXL, Direct Access High-Performance Memory Disaggregation]
type: paper
status: read
tags: [paper, cluster/cxl, camel-cxl-lineage]
---

# Direct Access, High-Performance Memory Disaggregation with DirectCXL

> **Source PDF**: [DirectCXL.pdf](DirectCXL.pdf)
> **Authors**: Donghyun Gouk, Sangwon Lee, Miryeong Kwon, Myoungsoo Jung (Computer Architecture and Memory Systems Laboratory, KAIST)
> **Venue / Year**: USENIX ATC 2022 (Carlsbad, CA · July 11–13, 2022)
> **arXiv / DOI**: https://www.usenix.org/conference/atc22/presentation/gouk · ISBN 978-1-939133-29-8
> **Length**: 9 pages (proceedings p.287–294)
> **Read status**: ☐ Skim · ☐ Partial · ☑ Full read (2026-07-04)
> **My reading purpose**: CAMEL Lab CXL 계보의 **뿌리(Phase 1, 2022)** 정독. 박사 방향(CXL memory disaggregation · multi-node coherence · PGAS-over-CXL)이 출발하는 baseline을 verbatim 인용 가능하게 확보하고, 이 논문이 **의도적으로 비운 공간**(HDM sharing 없음)을 내 연구 위치로 못박기 위함.

> 계보 전체 흐름: [CAMEL Lab CXL 연구 계보](../CAMEL Lab CXL 연구 계보.md)

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

DirectCXL은 host processor complex와 remote memory를 **CXL.mem 프로토콜로 직결**해, 응용이 disaggregated memory를 **순수 load/store 명령**으로 직접 접근하게 만든 최초의 실증 시스템이다. 기존 memory disaggregation(RDMA 기반 page-swap / object-KVS)은 remote data를 host로 **복사**하고 page cache·kernel 개입을 거치므로 local DRAM 대비 수 배~수십 배 느린데, DirectCXL은 이 data copy와 software fabric 개입을 **아예 제거**한다. 저자들은 CXL controller(16nm FPGA + 64GB DDR4), CXL RP를 LLC에 구현한 in-house RISC-V host, FM 기반 CXL switch를 **밑바닥부터 직접 제작**하고 Linux 5.13 위에 `cxl-namespace`/`mmap` 기반 software runtime을 올려, CXL 2.0을 실제 시스템으로 가져온 첫 사례를 만들었다. 결과적으로 remote memory가 host cache의 이점을 누릴 수 있을 때 **DRAM급 성능**을 보이며, RDMA 대비 latency 6.2×, 실제 워크로드 3× 우위를 정량 실증했다.

---

## Core thesis

> "In this paper, we propose directly accessible memory disaggregation, DIRECTCXL that straight connects a host processor complex and remote memory resources over CXL's memory protocol (CXL.mem). ... Since DIRECTCXL does not require any data copies between the host memory and remote memory, it can expose the true performance of remote-side disaggregated memory resources to the users." (Abstract, p.287)

추가 설명: memory disaggregation의 진짜 병목은 "원격이라서"가 아니라 **data movement + page cache 관리 + software fabric 개입**이라는 진단. CXL.mem은 remote DRAM을 host system memory에 매핑해 CPU가 load/store flit으로 직접 접근하게 하므로, 이 소프트웨어 계층을 통째로 삭제하면 disaggregation의 "true performance"가 드러난다는 것이 핵심 주장. 그리고 그것을 논증이 아니라 **실제 하드웨어 제작으로 증명**했다.

---

## Why this matters to me

이 논문은 내 박사 방향(메모리 시스템 아키텍처: CXL disaggregation · multi-node coherence · PGAS-over-CXL)의 **출발점이자 baseline**이다. "OS page-fault 경로를 없앤 direct load/store"라는 계보의 뿌리 명제가 어디서 왔는지, 어떤 정량치로 뒷받침되는지를 원문으로 확인해야 이후 branch들(pooling·coherence·SSD 교집합)이 무엇을 상속하고 무엇을 넘어서려는지 말할 수 있다. 특히 이 논문의 **feasibility-by-building 방법론**(CXL IP를 상용품 없이 FPGA·RISC-V로 밑바닥부터 만들어 CXL 2.0을 실측)은 내가 지향하는 "짓는 것으로 타당성을 증명한다"의 교과서적 사례라 방법론 자체를 흡수할 가치가 있다. 결정적으로 DirectCXL은 **single-host → single-device 직결**에 머물고 여러 host가 같은 HDM을 공유하는 것을 명시적으로 배제하는데, 바로 그 빈 공간이 내가 겨냥하는 multi-node coherence / sharing이다. 즉 이 논문은 내 방향의 **전제이자 대비 대상** 둘 다다.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| 1 | Introduction | p.287–288 | Disaggregation 병목 = data copy·page cache·SW fabric. CXL 2.0을 실제 시스템으로 가져온 첫 작업 |
| 2 | Memory Disaggregation and Related Work | p.288 | RDMA / page-swap / object-KVS 세 갈래 모두 RDMA 위에서 data를 복사·캐시함 |
| 3 | Direct Accessible Memory Aggregation | p.288–291 | CXL controller·switch(FM)·RISC-V host·`cxl-namespace` runtime를 직접 설계·제작 |
| 3.1 | Connecting Host and Memory over CXL | p.289 | CXL device = passive DRAM 모듈. HDM을 host system memory에 매핑, base 주소 차감으로 translation |
| 3.2 | Software Runtime for DirectCXL | p.289–290 | HDM을 `cxl-namespace` 세그먼트로 쪼개 `mmap`으로 프로세스 주소공간에 노출 |
| 3.3 | Prototype Implementation | p.290 | 16nm FPGA CXL blade(64GB), RISC-V 4-core host(LLC에 CXL RP), FM 기반 switch |
| 4 | Evaluation | p.290–292 | DirectCXL이 RDMA 대비 latency 6.2×·실워크로드 3× 우위, DRAM급 성능 |
| 4.1 | In-depth Analysis of RDMA and CXL | p.291 | RDMA 병목은 Network(78.7%), DirectCXL 병목은 PCIe/CXL IP(77.8%) |
| 4.2 | Latency Distribution and Scaling Study | p.291–292 | DirectCXL best latency = Local(4~24 cycles), tail만 2.8× |
| 5 | Conclusion | p.292 | Host cache를 누릴 때 DRAM급, 실워크로드 평균 3× |
| 6 | Future Work | p.292 | Kernel의 CXL 메모리 관리 확장, SoC silicon 검토 |

---

## Section notes

### §1 Introduction (p.287–288)

기존 disaggregation runtime을 host–memory server 간 데이터 관리 방식으로 **page-based / object-based** 둘로 나눈다. Page-based(swap)는 page fault 시 데이터를 remote로 스왑하되 코드 변경이 없고, object-based(KVS)는 자체 DB로 다루되 source-level 수정이 필요하다. 두 갈래의 공통 병목을 이렇게 못박는다.

> "the data movement and its accompanying operations (e.g., page cache management) introduce redundant memory copies and software fabric intervention, which makes the latency of disaggregated memory longer than that of local DRAM accesses by multiple orders of magnitude." (§1, p.287)

그리고 이 작업의 위치를 명확히 선언한다.

> "To the best of our knowledge, this is the first work that brings CXL 2.0 into a real system and analyzes the performance characteristics of CXL-enabled disaggregated memory design." (§1, p.288)

핵심 결과를 intro에서 미리 제시: load/store가 CXL network를 거쳐 disaggregated memory에서 서비스될 때 DirectCXL latency는 RDMA best 대비 6.2× 짧고, 실워크로드는 RDMA 대비 3× 빠르다(§1, p.288).

### §2 Memory Disaggregation and Related Work (p.288)

**§2.1 RDMA**: RDMA는 RNIC(RDMA NIC)과 MR(memory region), MTT(memory translation table)로 software stack 개입을 최소화하려 하지만, 여전히 각 side의 application이 데이터를 MR로 **복사(memory copy for DMA)**해야 하며 추가 DRAM 복사를 유발한다(Figure 1, p.288).

**§2.2 Swap (page-based)**: `kswapd`가 incoming block 주소를 memory node의 virtual address로 변환하고 target page를 RNIC MR로 복사해 RDMA로 전송. 투명하고 코드 변경이 없으나 **page fault handling, I/O amplification, context switching**으로 성능이 저하된다(§2.2, p.288).

**§2.3 KVS (object-based)**: KV hash-table 기반으로 RDMA를 직접 다뤄 page cache를 안 쓰지만, legacy 응용에 대해 **상당한 source-level 수정**이 필요하다는 한계. 결국 세 갈래 모두 RDMA(또는 유사 network protocol) 위에서 데이터를 캐시하고 network handling의 memory operation 비용을 지불한다(§2.3, p.288).

### §3 Direct Accessible Memory Aggregation (p.288–291)

전제: caching과 network-based data exchange가 기존 기술의 핵심이지만 바로 그것이 성능을 크게 깎는다. DirectCXL은 대신 remote memory를 host computing complex에 **직결**하고 순수 load/store로 접근한다(§3, p.288).

### §3.1 Connecting Host and Memory over CXL (p.289)

CXL device를 **완전한 passive DRAM 모듈**로 설계·구현한다. 여러 DRAM controller가 DDR interface로 DIMM을 물고, CXL controller가 PCIe 기반 CXL flit(주소·길이)을 파싱해 DRAM request로 변환한다.

> "CXL.mem in contrast allows the host computing resources directly access the underlying memory through PCIe buses (FlexBus); it works similar to local DRAM, connected to their system buses." (§3.1, p.289)

Host-side kernel driver가 CXL device의 BAR과 HDM(host-managed device memory) 크기를 PCIe transaction으로 조회해 host의 reserved system memory에 매핑한다. CPU가 HDM에 load/store하면 RP가 CXL flit으로 변환하고, CXL controller는 **HDM base 주소를 빼는 것만으로** translation을 수행해 DRAM에 서비스한다 — software 개입도, 데이터 복사도 없다(§3.1, p.289, Figure 2).

**CXL network switch (Figure 3)**: host의 CXL RP가 switch의 USP(upstream port) 또는 CXL device에 직결되고, FM(fabric manager)이 switch의 crossbar를 재구성해 각 USP를 서로 다른 DSP에 연결, root(host)에서 terminal(CXL device)로 이어지는 **virtual hierarchy**를 만든다. 여기에 격리 원칙이 명시된다.

> "Note that each CXL virtual hierarchy only offers the path from one to another to ensure that no host is sharing an HDM." (§3.1, p.289)

### §3.2 Software Runtime for DirectCXL (p.289–290)

RDMA와 달리 virtual hierarchy가 서면 응용은 HDM 메모리 공간을 직접 접근할 수 있으나, 이를 응용 주소공간에 노출할 runtime/driver가 필요하다. DirectCXL runtime은 HDM 주소공간을 **`cxl-namespace`**라 부르는 여러 segment로 쪼개고, 각 namespace를 **memory-mapped file(`mmap`)** 처럼 접근하게 한다. Driver는 `/dev/directcxl` device를 만들고 `ioctl`로 namespace를 관리하며, **HDM segment table**(offset·size·reference count·spinlock·read/write lock)로 다중 프로세스 접근을 조율한다. `mmap` 시 `/dev/cxl-ns0` 같은 device를 만들어 `vm_area_struct`로 프로세스 virtual memory에 매핑한다(§3.2, p.289–290, Figure 4). PMDK와 유사하나 file system 없이 conventional memory segment처럼 직접 노출되어 더 단순·유연하다고 주장한다.

### §3.3 Prototype Implementation (p.290)

상용 CXL 2.0 IP가 없으므로 **전부 밑바닥부터 제작**했다는 것이 이 절의 핵심.

> "our CXL memory prototype is built on our customized add-in-card (AIC) CXL memory blade that employs 16nm FPGA and 8 different DDR4 DRAM modules (64GB)." (§3.3, p.290)

Host도 자체 제작: RISC-V ISA 기반 in-house processor, **LLC(last-level cache)에 CXL RP를 구현**, four out-of-order core, 각 host가 Linux 5.13과 DirectCXL runtime을 구동. 4 host를 PCIe backplane으로 4 CXL device에 연결하고, backplane에 accelerator card 하나를 더 얹어 FM 포함 CXL switch를 구현(Figure 5). ACPI 부재를 device tree(`cxl-reserved-area` 필드 추가)로 우회. 동작 주파수:

> "our in-house softcore processors work at 100MHz while CXL and PCIe IPs (RP, EP, and Switch) operate at 250MHz." (§3.3, p.290)

### §4 Evaluation (p.290–292)

비교 대상: RDMA(Mellanox ConnectX-3 56Gbps + FastSwap=Swap, HERD=KVS를 RISC-V Linux 5.13.19에 포팅), Local(CXL 노드 비활성화). 워크로드: DLRM, MemDB(HERD의 in-memory DB), Ligra 그래프 4종(MIS/BFS/CC/BC). Table 1이 per-node·remote memory usage 요약.

### §4.1 In-depth Analysis of RDMA and CXL (p.291)

64B load 기준 latency 분해(Figure 6).

> "DIRECTCXL only takes 328 cycles for memory load request, which is 8.3× faster than RDMA." (§4.1, p.291)

RDMA는 총 2705 cycles이고 그 중 InfiniBand network가 78.7%(2129 cycles). 차이의 두 이유: (1) DirectCXL은 compute–memory node를 PCIe로 직결해 InfiniBand↔PCIe 프로토콜 변환이 없음, (2) DirectCXL은 LLC에서 CXL flit으로 바로 변환하는 반면 RDMA는 DMA로 메모리를 read/write해야 함. Sensitivity test(Figure 7): RDMA 병목은 payload<1KB에서 Library, 커지면 Copy(28.9%); DirectCXL 병목은 오직 **LLC(CPU cache)** — software도 data copy 오버헤드도 없음.

Memory hierarchy(Figure 8):

> "the latency of RDMA was 2027 cycles, which is 6.2× and 510.5× slower than that of DirectCXL and L1 cache, respectively. DirectCXL requires 328 cycles whereas Local requires only 60 cycles in the case of L2 misses." (§4.1, p.291)

> "the performance bottleneck of DirectCXL is PCIe including CXL IPs (77.8% of the total latency). This can be accelerated by increasing the working frequency." (§4.1, p.291)

### §4.2 Latency Distribution and Scaling Study (p.291–292)

Latency CDF(Figure 9): RDMA는 1790~4006 cycles로 넓게 퍼짐(RNIC MTT buffer·CPU cache 편차). DirectCXL은 Local과 유사.

> "its best performance is the same as Local (4∼24 cycles). ... The tail latency is 2.8× worse than Local, but its latency curve is similar to that of Local. This is because both DirectCXL and Local use the same DRAM (and there is no network access overhead)." (§4.2, p.292)

Speed scaling estimation(Table 2): CPU와 CXL IP를 각각 1.2GHz/1GHz로 올린다고 가정하면 64B load가 328 → (추정) cycles로 감소, FlexBus 시간지연은 ~60ns 수준이나 CXL IP는 더 높은 주파수로 개선 여지 있음.

### §4.3 Performance of Real Workloads (p.292)

Figure 10a(실행시간, Swap 기준 정규화). KVS는 Swap의 page 단위 I/O 오버헤드를 줄이지만 (1) source 대폭 수정 필요, (2) memory node에 hashing 등 연산 요구로 비용 상승.

> "DIRECTCXL without having a source modification and remote-side resource exhibits 3× and 2.2× better performance than Swap and even KVS, respectively." (§4.3, p.292)

실행시간 분해(Figure 10b): Swap이 전체를 깎는 이유는

> "51.8% of the execution time is consumed by kernel swap daemon (kswapd) and FastSwap driver, on average." (§4.3, p.292)

그래프 워크로드: Swap은 4KB page를 교환하지만 그래프 traversal은 8B pointer만 필요 → Swap이 DirectCXL 대비 2.2× 느림(§4.3, p.292). KVS가 DLRM/MemDB에서 Swap보다 나은 건 page 대신 정확한 크기(예: DLRM embedding)를 로드하기 때문(data transfer 6.9× 절감).

### §5 Conclusion & §6 Future Work (p.292)

Host processor의 cache를 누릴 수 있을 때 disaggregated memory가 DRAM급 성능을 내고, 실워크로드 평균 3× RDMA 대비 우위. Future work로 **efficient CXL memory management를 위한 kernel 확장**과 **SoC silicon** 형태의 DirectCXL을 언급(§6, p.292).

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "directly accessible memory disaggregation"
- "straight connects a host processor complex and remote memory resources"
- "expose the true performance of remote-side disaggregated memory resources"
- "brings CXL 2.0 into a real system"

**Technical concepts:**
- "CXL.mem memory protocol over PCIe (FlexBus)"
- "host-managed device memory (HDM)" / "HDM segment table"
- "cxl-namespace" (mmap-based segment)
- "CXL virtual hierarchy" / "fabric manager (FM)"
- "sheer load/store instructions"
- "passive DRAM modules" (CXL device as pure memory expander)

**Value language:**
- "no data copies between host memory and remote memory"
- "no software (fabric) intervention"
- "DRAM-like performance"
- "without source modification"

> ⚠ **피해야 할 어휘** (paper-signature, 그대로 echo하면 모방으로 보임):
> - "directly accessible memory disaggregation, DirectCXL" (제품명 결합 구호)
> - "straight connects" (이 논문 특유의 표현)
> - "expose the true performance of remote-side disaggregated memory" (Abstract 서명 문장)

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| §1/§4.1, p.288, p.291 | "DIRECTCXL only takes 328 cycles for memory load request, which is 8.3× faster than RDMA" | CXL 직접접근이 RDMA 대비 load latency에서 압도적임을 한 줄로 |
| §4.1, p.291 | RDMA 2705 cycles 중 network가 "78.7% (2129 cycles)" | 기존 disaggregation 병목이 network/software 계층임을 입증 |
| §1/§4.1, p.288, p.291 | RDMA latency가 DirectCXL 대비 "6.2×", L1 대비 "510.5×" slow | disaggregation latency 계층 위치 인용 |
| §4.1, p.291 | "the performance bottleneck of DirectCXL is PCIe including CXL IPs (77.8% of the total latency)" | CXL의 남은 최적화 여지(주파수/IP)를 지적할 때 |
| §4.2, p.292 | DirectCXL best latency "the same as Local (4∼24 cycles)", tail만 "2.8×" | CXL이 DRAM급 성능에 근접함을 인용 |
| §4.3, p.292 | "3× and 2.2× better performance than Swap and even KVS" | 실워크로드에서 direct access의 실용 우위 |
| §4.3, p.292 | Swap 실행시간의 "51.8% ... consumed by kernel swap daemon (kswapd) and FastSwap driver" | page-fault 경로 제거의 동기로 인용 |
| §3.3, p.290 | prototype = "16nm FPGA and 8 different DDR4 DRAM modules (64GB)", RISC-V 4-core host, IP 100/250MHz | feasibility-by-building 실증 규모를 인용 |

---

## 🎯 Strategic anchor

> "Note that each CXL virtual hierarchy only offers the path from one to another to ensure that no host is sharing an HDM." (§3.1, p.289)

→ **본인 활용**: 면담·자소서에서 "DirectCXL은 CXL disaggregation을 실증했지만 **p.289에서 어떤 host도 HDM을 공유하지 않도록 명시적으로 격리**합니다 — 즉 single-host 직접접근까지가 이 논문의 경계이고, 여러 host가 같은 CXL memory를 공유·coherent하게 접근하는 **multi-node coherence / PGAS-over-CXL이 정확히 비워진 다음 칸**입니다. 제 박사 방향이 바로 그 지점을 feasibility-by-building으로 잇는 것"이라고 계보 뿌리 대비 내 위치를 한 문장으로 못박는 데 사용.

---

## Connection to my research direction

| 차원 | 이 paper (DirectCXL) | 본인 방향 |
|---|---|---|
| Scope | Single-host → single CXL device 직접접근. HDM sharing 명시적 배제 | Multi-node가 같은 CXL memory를 공유·coherent 접근 (PGAS-over-CXL) |
| Mechanism | HDM base 차감 translation + `cxl-namespace` mmap, **coherence 없음(비공유)** | Cross-node coherence protocol / directory over CXL fabric |
| Workload | 단일 노드 응용(DLRM·MemDB·Ligra)을 remote DRAM으로 확장 | 다중 노드 공유 데이터 구조·분산 응용 |
| Method | **Feasibility-by-building** (FPGA CXL IP·RISC-V host·switch 직접 제작) | 동일 방법론 상속 — 짓는 것으로 multi-node 타당성 증명 |
| Open space | 공유·coherence·pooling scale은 future work로 남김 | 바로 그 공간이 내 연구 위치 |

DirectCXL은 내 방향의 **전제이자 대조군**이다. 상속하는 것: "software/page-fault 경로를 없앤 direct load/store가 disaggregation의 진짜 성능을 드러낸다"는 명제와, 상용 IP 없이 FPGA·RISC-V로 CXL을 실측하는 **feasibility-by-building 방법론**. 넘어서는 것: DirectCXL은 격리된 virtual hierarchy로 "no host is sharing an HDM"을 보장하며 coherence 문제를 아예 회피한다. 내 연구는 그 격리를 풀어 **여러 host가 하나의 CXL memory를 coherent하게 공유**할 때의 protocol·directory·consistency를 다룬다. 즉 DirectCXL이 증명한 "직결의 성능 이점"을 유지하면서 "공유의 정확성"을 추가하는 것이 차별점이며, 방법론(직접 제작·실측)은 그대로 이어받아 correct-by-construction으로 밀고 간다.

---

## Open questions / gaps

- [ ] **Multi-node coherence 부재**: virtual hierarchy가 host 간 HDM 공유를 원천 차단(§3.1, p.289). 여러 host가 같은 CXL memory를 coherent하게 쓰는 protocol은 미해결 → 내 핵심 위치.
- [ ] **Pooling/스케일 부족**: prototype은 4 host × 4 device(§3.3, p.290). 대규모 memory pool의 FM 스케줄링·fragmentation·QoS는 미평가.
- [ ] **CXL IP 주파수 병목**: DirectCXL latency의 77.8%가 PCIe/CXL IP(§4.1, p.291). softcore 100MHz·IP 250MHz 한계 — ASIC/SoC(§6)로 갈 때 실제 수치는 추정(Table 2)뿐.
- [ ] **Consistency/durability 미논의**: `cxl-namespace`가 memory-mapped지만 persistence·failure atomicity·crash consistency 논의 없음(CXL SSD 교집합과 맞닿는 gap).
- [ ] **Write/coherent traffic 미분석**: 평가가 주로 read(64B load) 중심. write-heavy·sharing 트래픽의 CXL.cache 동작은 범위 밖.
- [ ] **보안/격리**: HDM segment table 기반 다중 프로세스 접근 조율은 있으나 cross-tenant isolation·attack surface 논의 없음.

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [9] | Guo et al., *Clio: A hardware-software co-designed disaggregated memory system*, ASPLOS 2022 | HW/SW co-design disaggregation — 내 feasibility-by-building 방법론 비교 |
| ☐ | [8] | Lee et al., *Mind: In-network memory management for disaggregated data centers*, SOSP 2021 | In-network memory mgmt — multi-node pooling/coherence 인접 |
| ☐ | [10] | Calciu et al., *Rethinking software runtimes for disaggregated memory*, ASPLOS 2021 | Disaggregated memory용 software runtime 재설계 — cxl-namespace 대비 |
| ☐ | [6] | Wang et al., *Semeru: A memory-disaggregated managed runtime*, OSDI 2020 | Managed runtime 관점 disaggregation |
| ☐ | [11] | Tsai et al., *Disaggregating persistent memory ... passive disaggregated KV stores*, ATC 2020 | Passive memory + persistence → CXL SSD 교집합 gap |
| ☐ | [18] | CXL Consortium, *Compute Express Link Specification Revision 2.0* | 계보 기반 스펙 원문 — coherence/pooling 정의 확인 |
| ☐ | [24] | Frey & Alonso, *Minimizing the hidden cost of RDMA*, ICDCS 2009 | RDMA data copy 비용의 근거 — motivation 인용 |

---

## Personal annotations

<자유 형식 메모 — 읽으며 떠오른 생각·이견·후속 아이디어. user 전용 영역.>

- (2026-07-04 최초 작성) 계보 뿌리 정독 완료. 이 논문의 진짜 무기는 성능 수치가 아니라 **"상용 IP 0에서 CXL 2.0 실측 시스템을 세웠다"**는 사실 그 자체. 내 방법론 서사(feasibility-by-building)의 최상위 롤모델로 인용.
- Anchor 문장("no host is sharing an HDM")이 내 연구 정당화의 단일 최강 지점. multi-node coherence를 "빈 칸 잇기"로 프레이밍할 때 반드시 §3.1 p.289로 지목.
