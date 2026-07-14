---
title: "Direct Access, High-Performance Memory Disaggregation with DirectCXL"
aliases: [DirectCXL]
type: paper-ref
venue: USENIX ATC
year: 2022
tags:
  - paper
  - cluster/cxl
  - topic/memory-disaggregation
  - topic/cxl-mem
  - topic/rdma
  - venue/atc
  - year/2022
---

# Direct Access, High-Performance Memory Disaggregation with DirectCXL

> **Source PDF**: [DirectCXL - Direct Access, High-Performance Memory Disaggregation.pdf](<DirectCXL - Direct Access, High-Performance Memory Disaggregation.pdf>)
> 🕸️ NodeGraph: [DirectCXL.html (새 탭에서 렌더링, push 후 유효)](https://raw.githack.com/Jeong-jin-Han/2026_CAMEL/main/papers/SkyByte%20-%20Architecting%20An%20Efficient%20Memory-Semantic%20CXL-based%20SSD%20with%20OS%20and%20Hardware%20Co-design/refs/DirectCXL%20-%20Direct%20Access%2C%20High-Performance%20Memory%20Disaggregation/DirectCXL.html)
> **Authors**: Donghyun Gouk, Sangwon Lee, Miryeong Kwon, **Myoungsoo Jung** — KAIST (CAMELab)
> **Venue / Year**: USENIX ATC 2022 · **Length**: 9 pages
> **Read status**: ☑ Full read (2026-07-13)
> **My reading purpose**: **최초의 실제 CXL.mem 메모리 분해(disaggregation) 프로토타입**. RDMA 대비 CXL.mem의 정량적 우위를 실측한 기준점. [[CXL Overview]]·[[CXL Multi-node Coherence]]의 하드웨어 근거이자, "virtual hierarchy당 HDM은 한 host 전용(no sharing)"이라는 **pooling≠sharing**의 실물 증거.

---

## 📋 목차
- [TL;DR](#tldr)
- [Core thesis](#core-thesis)
- [Why this matters to me](#why-this-matters-to-me)
- [Structure overview](#structure-overview)
- [Section notes](#section-notes)
- [Key vocabulary](#key-vocabulary)
- [Citable quantitative data](#citable-quantitative-data)
- [🎯 Strategic anchor](#-strategic-anchor)
- [Connection to my research direction](#connection-to-my-research-direction)
- [Open questions / gaps](#open-questions--gaps)
- [References worth following up](#references-worth-following-up)
- [Personal annotations](#personal-annotations)

---

## TL;DR
DirectCXL는 **CXL.mem으로 host processor complex와 원격 메모리를 직접 연결**하는 최초의 실제 메모리 분해 프로토타입이다. 기존 분해 방식(RDMA 기반 **page-based swap**·**object-based KVS**)은 원격→host로 데이터를 복사하고 network stack이 개입해 지연이 local DRAM보다 수 자릿수 커지는데, DirectCXL는 **데이터 복사 없이** `load`/`store`가 LLC에서 곧바로 CXL flit으로 변환돼 원격 메모리에 도달한다. 16nm FPGA로 CXL controller·CXL switch·RISC-V host(LLC에 CXL RP 내장)를 직접 구현(상용 CXL 2.0 IP가 없어 밑바닥부터)했고, HDM을 host 시스템 메모리에 매핑해 **cxl-namespace**(mmap)로 노출하는 software runtime을 제공한다. 결과: 64B load 지연이 RDMA 대비 **8.3×**(memory-hierarchy 관점 6.2×) 짧고, 실제 워크로드(DLRM·MemDB·Ligra)에서 RDMA swap 대비 **3×**, KVS 대비 **2.2×** 빠르다.

---

## Core thesis
> "we propose directly accessible memory disaggregation, DIRECTCXL that straight connects a host processor complex and remote memory resources over CXL's memory protocol (CXL.mem). ... DIRECTCXL does not require any data copies between the host memory and remote memory, and therefore, it can expose the true performance of remote-side disaggregated memory resources to the users." (§1)

CXL.mem은 원격 메모리를 host system bus에 붙은 local DRAM처럼 다루게 해준다. RDMA의 근본 병목 — 두 번의 DMA, network 프로토콜 변환, MR로의 데이터 복사 — 을 전부 없애고, CPU cache까지 활용해 원격 메모리의 "진짜 성능"을 드러낸다.

---

## Why this matters to me
DirectCXL는 [[SkyByte]]·[[CXL Overview]]가 전제하는 "CXL.mem으로 device 메모리를 load/store" 가 **실물 하드웨어에서 되고, RDMA를 압도한다**는 것을 최초로 증명한 논문이다. 특히 CXL switch의 **virtual hierarchy(VH)**가 "root(host)→terminal(CXL device)의 단일 경로만 제공하여 **어떤 host도 HDM을 공유하지 않도록** 보장한다"는 대목은, 내가 관심 있는 **multi-node coherence 문제가 왜 CXL 2.0에서 '회피'되는지**(=공유가 아니라 분할이라서)의 실물 근거다. 같은 KAIST 그룹의 [[Hello Bytes, Bye Blocks - PCIe Storage Meets Compute Express Link for Memory Expansion (CXL-SSD)]]와 짝으로, SkyByte류 CXL-SSD 계보의 하드웨어·개념 토대다.

---

## Structure overview
| § | Title | Pages | Key takeaway |
|---|---|---|---|
| 1 | Introduction | p.1-2 | RDMA 기반 분해의 복사·개입 오버헤드 → CXL.mem 직접 연결 제안 |
| 2 | Disaggregation & Related Work | p.2-3 | RDMA / page-based swap(kswapd) / object-based KVS의 한계 |
| 3 | Direct Accessible Memory Aggregation | p.3-5 | HDM을 host 메모리에 매핑, CXL switch VH, cxl-namespace runtime, FPGA 프로토타입 |
| 4 | Evaluation | p.5-7 | RDMA 대비 latency 8.3×↓, 실 워크로드 3×↑ |
| 5-6 | Conclusion & Future | p.7 | DRAM급 성능, 커널 확장·SoC 실리콘 향후 과제 |

---

## Section notes

### §2 RDMA / Swap / KVS의 한계 (p.2-3)
메모리 분해의 두 접근:
- **Page-based (swap)**: disaggregation 드라이버가 kswapd 아래에서 page fault 시 원격 노드로 swap. 투명(코드 변경 없음)하나 **page fault 처리·I/O amplification·context switching** 오버헤드.
- **Object-based (KVS)**: KV store로 원격 메모리를 다룸. page swap 오버헤드는 피하나 **소스 대규모 수정 + 메모리 노드에서 해싱 등 무거운 연산** 필요.
- 공통: 둘 다 **RDMA(또는 유사 network)**로 데이터를 복사하고, 그 network handling 비용을 낸다. RDMA는 양쪽에 MR(memory region) 등록, MTT 주소변환, RNIC 간 데이터 복사가 필수(one-sided RDMA도 DMA 2회).

> "all these approaches use RDMA (or a similar network protocol), which is essential to cache the data and pay the cost of memory operations for network handling." (§2.2)

### §3.1 Host와 memory를 CXL로 연결 (p.4)
- CXL device = **pure passive module**(각자 DRAM DIMM + 자체 HW controller). CXL controller가 들어오는 CXL flit(주소·길이)을 DRAM 요청으로 변환.
- Host system bus에 **CXL root port(RP)**가 있고 CXL device를 endpoint(EP)로 연결. host kernel driver가 BAR·HDM 크기를 PCIe transaction으로 질의해 host **예약 시스템 메모리**에 매핑하고, device에 매핑 위치를 알림. load/store → RP가 CXL flit으로 변환 → CXL controller가 **HDM base를 빼서** 주소 변환 → DRAM controller. SW 개입·데이터 복사 없음 → 낮은 지연.

### §3.1 CXL switch & Virtual Hierarchy (p.4)
- CXL switch: **USP(upstream)/DSP(downstream)** + **fabric manager(FM)**. FM이 crossbar를 재구성해 각 USP를 다른 DSP에 연결 → **virtual hierarchy(VH)** 생성(root host → terminal CXL device). CXL device는 여러 controller/DRAM으로 **multiple logical device**를 정의해 각자 HDM을 host에 노출.
- ★ "each CXL virtual hierarchy only offers the path from one to another **to ensure that no host is sharing an HDM.**" → **분할(pooling)이지 공유(sharing)가 아니다.**

### §3.2-3.3 Software runtime + 프로토타입 (p.4-5)
- 상용 CXL OS가 없어 **DirectCXL runtime** 자작. HDM 주소공간을 segment(=**cxl-namespace**)로 쪼개 app이 **mmap**으로 접근. `/dev/directcxl`를 ioctl로 관리, HDM segment table(offset/size/refcount). PMDK보다 단순·유연.
- 프로토타입: n hosts × m CXL devices(각 4개) + CXL switch. CXL memory blade AIC = 16nm FPGA + **8 DDR4(64GB)**, CXL controller + 8 DRAM controller. Host = 자작 **RISC-V 4-core OoO**, LLC에 **CXL RP** 구현. softcore 100MHz, CXL/PCIe IP 250MHz. **상용 CXL 2.0 IP가 없어 전부 자작.**

### §4 Evaluation (p.5-7)
- Baseline: **RDMA**(Mellanox ConnectX-3 56Gbps InfiniBand, FastSwap=page-based, HERD=object-based), **Local**(DRAM only). 동일 testbed. Workload: DLRM(embedding), MemDB, Ligra(MIS/BFS/CC/BC).
- **64B read 지연**: DirectCXL **328 cycles** vs RDMA **2705 cycles** → **8.3×**. 이유: (1) DirectCXL은 PCIe 직결 vs RDMA는 InfiniBand↔PCIe 변환, (2) DirectCXL은 LLC의 load/store를 CXL flit으로 변환 vs RDMA는 DMA로 메모리 read/write.
- **Memory hierarchy**: Local·DirectCXL는 CPU cache hit 시 4 cycle. RDMA best-case 2027 cycles = DirectCXL 대비 **6.2×**, L1 대비 510.5× 느림. DirectCXL **tail latency는 Local 대비 2.8×** 나쁘지만 곡선은 유사(같은 DRAM, network 없음).
- **지연 분해(Table 2)**: CXL IPs(RP/EP/Switch)가 지배(추정 239ns), FlexBus ~57ns, DRAM controller 105ns → 총 328 cycles. PCIe+CXL IP = 지연의 **77.8%**, 주파수 올리면 개선 여지.
- **실 워크로드**: DirectCXL가 Swap 대비 **3×**, KVS 대비 **2.2×** 빠름. Swap은 exec의 **51.8%**를 kswapd+FastSwap에 소모. 그래프(Ligra)는 4KB page를 옮겨 8B 포인터를 읽어 Swap이 2.2× 손해.

---

## Key vocabulary
**Thesis / framing:**
- "directly accessible memory disaggregation"
- "expose the true performance of remote-side disaggregated memory"
- "no data copies between host memory and remote memory"

**Technical concepts:**
- "CXL.mem" / "host-managed device memory (HDM)"
- "virtual hierarchy (VH)" / "fabric manager (FM)" / "USP/DSP"
- "cxl-namespace" (mmap-exposed HDM segment)
- "CXL root port (RP)" / "CXL flit"

**Value language:**
- "DRAM-like performance when the workload can enjoy the host processor's cache"

> ⚠ **피해야 할 어휘** (DirectCXL-signature):
> - "directly accessible memory disaggregation"
> - "cxl-namespace"

---

## Citable quantitative data
| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| Abstract/§4.1 | DirectCXL 64B load = 328 cycles vs RDMA 2705 → **8.3×** | CXL.mem이 RDMA 압도 |
| §4.1 | RDMA best 2027 cycles = DirectCXL 대비 **6.2×**, L1 대비 510.5× | memory hierarchy 위치 |
| §4.1 | InfiniBand network = RDMA 지연의 **78.7%** (2129 cycles) | RDMA 병목이 network |
| §4.3 | 실 워크로드 Swap 대비 **3×**, KVS 대비 **2.2×** | end-to-end 우위 |
| §4.3 | Swap 실행의 **51.8%**가 kswapd+FastSwap | page-based 오버헤드 |

---

## 🎯 Strategic anchor
> "each CXL virtual hierarchy only offers the path from one to another to ensure that no host is sharing an HDM." (§3.1, p.4)

→ **본인 활용**: CXL 2.0 실물에서도 **HDM은 host당 전용(분할)이지 공유가 아니다**는 결정적 근거. 면담에서 "DirectCXL이 CXL.mem으로 RDMA를 8.3× 이겼지만, 그건 여전히 **한 host가 한 HDM을 보는** 세계다 — 여러 host가 같은 HDM을 coherent하게 공유하는 것은 CXL 3.0 back-invalidate가 필요하고, 거기가 내 연구 지점"으로 연결. [[CXL Overview]]의 pooling↔sharing 분수령을 실물로 뒷받침.

---

## Connection to my research direction
| 차원 | DirectCXL (ATC'22) | 내 방향 |
|---|---|---|
| 인터커넥트 | CXL 2.0 (.mem) 실물 프로토타입 | CXL 3.0 fabric |
| 대상 | 원격 DRAM 분해 | 공유 memory pool |
| host↔HDM | **1:1 (VH당 no sharing)** | **다:1 공유** |
| coherence | host-local만(HDM-H, single host) | **HDM-DB / back-invalidate** |
| SW | cxl-namespace(mmap), 커널 밖 | multi-host 커널/런타임 |

DirectCXL은 "CXL.mem 직접 접근이 RDMA를 이긴다"를 실증했지만 **철저히 pooling(분할)** — VH가 host↔HDM을 1:1로 격리해 coherence 문제를 애초에 발생시키지 않는다. 내 연구는 이 격리를 풀어 **여러 host가 같은 HDM을 공유**할 때 필요한 directory/back-invalidate coherence를 다룬다. DirectCXL의 FPGA testbed(자작 CXL controller/switch/RISC-V RP)는 내가 multi-host coherence를 실험할 **실물 플랫폼의 원형**이 될 수 있다. → [[CXL Multi-node Coherence]]

---

## Open questions / gaps
- [ ] VH가 host↔HDM을 1:1 격리 → **여러 host가 같은 HDM 공유 시 coherence**는 논문 범위 밖(CXL 2.0 한계).
- [ ] CXL IPs가 지연의 77.8% → 실리콘/주파수 개선 전까진 tail latency가 Local 대비 2.8×.
- [ ] fabric manager의 VH 재구성은 있으나, 공유 상태에서의 **cross-host 무효화·directory**는 없음.
- [ ] persistence·GC 등 storage 결합(=CXL-SSD)은 다루지 않음(순수 DRAM 분해).

---

## References worth following up
| 상태 | Paper | 왜 봐야 |
|---|---|---|
| ☐ | Clio (Guo et al., ASPLOS'22) | HW-SW co-design 분해 메모리, 비교 baseline |
| ☐ | Rethinking SW runtimes for disaggregated memory (Calciu et al., ASPLOS'21) | 분해 메모리 런타임 설계 |
| ☐ | FastSwap (Amaro et al., EuroSys'20) | page-based 분해 baseline |
| ☐ | HERD (Kalia et al., SIGCOMM'14) | RDMA KVS baseline |
| ☐ | [[Hello Bytes, Bye Blocks - PCIe Storage Meets Compute Express Link for Memory Expansion (CXL-SSD)]] | 같은 그룹의 CXL-SSD 비전 (Type-3 논거) |

---

## Personal annotations
<!-- 본인 메모 영역 -->
