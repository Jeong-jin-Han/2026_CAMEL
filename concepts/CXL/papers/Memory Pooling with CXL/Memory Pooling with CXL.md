---
title: "Memory Pooling with CXL"
aliases: [Memory Pooling with CXL, CXL Memory Pooling]
type: paper
status: read
tags: [paper, cluster/cxl, camel-cxl-lineage]
---

# Memory Pooling with CXL

> **Source PDF**: [Memory Pooling with CXL.pdf](Memory Pooling with CXL.pdf)
> **Authors**: Donghyun Gouk, Miryeong Kwon, Hanyeoreum Bae (KAIST); Sangwon Lee, Myoungsoo Jung (KAIST & Panmnesia)
> **Venue / Year**: IEEE Micro, vol. 43, Theme: Emerging System Interconnects, 2023 (published online 16 Jan 2023, pp. 48–57)
> **arXiv / DOI**: 10.1109/MM.2023.3237491
> **Length**: 10 pages
> **Read status**: ☐ Skim · ☐ Partial · ☑ Full read (2026-07-04)
> **My reading purpose**: CAMEL Lab CXL 연구 계보 이해 — DirectCXL(disaggregation) 다음 단계인 **pooling**이 실제로 host 간 memory **공유(coherence)**를 지원하는지 확인. 내 방향(multi-node coherence / PGAS-over-CXL)의 정확한 출발점·gap을 이 논문에서 확정하는 것이 목적.

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

> 계보 맥락: [CAMEL Lab CXL 연구 계보](../CAMEL Lab CXL 연구 계보.md) — Phase 1 · pooling.

---

## TL;DR

이 IEEE Micro 논문은 CAMEL Lab의 **DirectCXL**을 full-length로 정리한 글이다. 제목은 "Memory Pooling with CXL"이지만 실제 시스템 기여는 DirectCXL — remote memory를 RDMA/DMA copy 없이 host processor complex에 **CXL.mem 프로토콜로 직접 연결**하고, 애플리케이션이 순수 load/store instruction으로 접근하게 만든 disaggregated memory 설계다. 16-nm FPGA로 CXL controller·switch·RISC-V host processor까지 **바닥부터(from the ground) 실제 하드웨어로 prototype**했고(CXL 2.0 기반), real-world workload(DLRM, MemDB, Ligra)에서 RDMA 대비 평균 **약 7배** 성능을 보인다. 후반부 "Toward Composable Server Architecture"에서 CXL 2.0의 한계(host 간 HDM 공유 불가)를 짚고, 진행 중인 CXL 3.0 기반 작업(back-invalidation snoop, MH-LD, DCD)으로 fine-control memory sharing을 확장하려는 로드맵을 제시한다.

---

## Core thesis

> "We propose DirectCXL that connects host processor complex and remote memory resources over CXL's memory protocol (CXL.mem). The results of our real system evaluation show that the disaggregated memory resources of DirectCXL can exhibit DRAM-like performance when the workload can enjoy the host-processor's cache. For real-world applications, it exhibits 7× better performance than RDMA-based memory disaggregation, on average." (Conclusion, p.57)

추가 설명: 핵심 주장은 "network/software intervention과 memory data copy를 없애면(no software intervention or memory data copies) disaggregated memory가 local DRAM에 준하는 성능을 낼 수 있다"는 것. RDMA의 두 번의 DMA·MR copy·InfiniBand↔PCIe 프로토콜 변환 오버헤드를 CXL.mem의 LLC→CXL flit 직결 경로로 제거한 것이 성능 원천이다. "Pooling"은 CXL switch의 virtual hierarchy로 여러 host가 각자 다른 CXL device를 붙이는 수준이며, **동일 HDM을 여러 host가 공유하는 진짜 sharing은 CXL 2.0에서 불가능**하다고 명시한다.

---

## Why this matters to me

내 박사 방향은 메모리 시스템 아키텍처(CXL disaggregation → multi-node coherence → PGAS-over-CXL)를 feasibility-by-building으로 밀고 가는 것이다. 이 논문은 그 계보의 **Phase 1 baseline**을 확정해 준다. 특히 이 글의 가장 큰 가치는 "pooling"이라는 제목과 달리 **CXL 2.0 pooling은 sharing이 아니다**를 저자 스스로 못박은 점이다 — "CXL virtual hierarchy only offers the path from one to another to ensure that no host is sharing an HDM" (p.51). 즉 여러 host가 같은 memory를 **coherent하게 공유**하려면 CXL 3.0의 back-invalidation snoop이 필요하고, 이건 이 논문 시점에 on-going/future work이다. 내 연구의 정확한 진입점(multi-node coherence gap)이 이 문장에서 열린다. 또한 이 랩이 IP를 "from the ground"로 FPGA에 실제 구현했다는 점은 내 feasibility-by-building 철학과 직접 align된다.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| — | Abstract / Intro | p.48–49 | Memory pooling을 page-based vs object-based로 분류. 둘 다 RDMA로 data move 필요 → DirectCXL 제안 |
| — | Memory Pooling and Related Work | p.49–50 | RDMA(두 DMA + MR copy), page-based Swap(kswapd), object-based KVS(SQ/CQ, source 수정 필요) 한계 |
| — | Direct-Access Memory Disaggregation | p.50–52 | CXL device를 passive DIMM으로 설계, HDM을 host system memory에 매핑(Fig 2), CXL switch virtual hierarchy(Fig 3), DirectCXL runtime·cxl-namespace·mmap(Fig 4), FPGA prototype(Fig 5) |
| — | Evaluation | p.52–55 | RDMA vs DirectCXL in-depth(Fig 6,7), real workload DLRM/MemDB/Ligra(Fig 8), embedding lookup(Fig 9), CXL 버전 비교(Table 3) |
| — | Toward Composable Server Architecture | p.55–56 | CXL 3.0 on-going: multiple cache devices, back-invalidation snoop, MH-LD/DCD로 fine-control sharing(Fig 10), fabric extension(PBR, multilevel switch) |
| — | Discussion / Conclusion | p.56–57 | Pond·TPP 관련연구, DirectCXL 7× 결론 |

---

## Section notes

### Abstract & Introduction (p.48–49)

Memory pooling(=disaggregation) runtime을 두 갈래로 분류한다: **page-based**(virtual memory technique로 code 수정 없이 page cache를 remote와 swap)와 **object-based**(KVS 같은 자체 DB 사용, address translation 문제는 풀지만 significant source-level modification 필요). 두 접근 모두 결국 remote→host로 데이터를 **RDMA(또는 fine-grain network interface)로 옮겨야** 하며, 이 data movement와 부수 연산(page cache management 등)이 redundant memory copy·software fabric intervention을 낳아 disaggregated memory latency를 local DRAM 대비 여러 자릿수(multiple orders of magnitude) 크게 만든다. DirectCXL은 이 복사 자체를 없애 "true performance of remote-side disaggregated memory resources"를 노출한다.

Contribution 4가지(p.49): (1) CXL network infrastructure를 처음부터 끝까지 실현, (2) memory pooling용 software runtime 설계·구현, (3) memory expander prototype 및 real-world application으로 pooling 분석, (4) CXL-based composable system architecture 분석·논의.

### Memory Pooling and Related Work (p.49–50)

**RDMA (Fig 1, p.49)**: host·memory node 양쪽에 RNIC 필요. memory region(MR)을 RNIC에 등록하고 MTT(memory translation table)에 물리주소 매핑. 쓰기 시 host가 destination virtual address와 data를 보내면 remote node가 MTT로 translate 후 MR에 copy. 핵심 비용: DMA 연산 자체 외에 **각 side의 application이 MR로 data를 prepare/retrieve하는 추가 local DRAM copy**가 발생.

**Swap: page-based (p.49–50)**: page fault 시 disaggregation driver 밑의 kswapd가 block address를 memory node의 virtual address로 변환, target page를 RNIC MR로 copy 후 RDMA 발행. 모든 것이 kswapd 아래에서 관리돼 transparent·easy-to-adopt하지만 **page fault handling, I/O amplification, context switching** 오버헤드로 성능 저하.

**KVS: object-based (p.50)**: host·node 양쪽에 두 MR(buffer + SQ/CQ). Put/Get을 KV hash-table로 처리, node가 SQ MR을 polling. Local page cache를 못 써서 성능이 app semantics에 크게 의존하고 **legacy application에 대규모 source 수정** 필요.

### Direct-Access Memory Disaggregation (p.50–52)

**CXL device를 passive로 설계**: 기존 disaggregation은 remote node side에 computing resource가 필요(DRAM은 passive peripheral이라). CXL.mem은 host가 PCIe bus(FlexBus)로 remote memory를 local DRAM처럼 직접 접근하게 하므로, CXL device를 **여러 DRAM DIMM + 자체 hardware controller**만 가진 pure passive module로 만든다. CXL controller가 들어오는 PCIe-based CXL packet(**CXL flit**)을 파싱해 address·length를 DRAM request로 변환.

**HDM을 system memory에 통합 (Fig 2, p.50)**: host-side kernel driver가 CXL device를 enumerate하며 BAR와 내부 memory(host-managed device memory, **HDM**) 크기를 PCIe로 질의, host reserved system memory에 BAR·HDM base를 매핑. host가 HDM에 load/store하면 request가 해당 root port(RP)로 전달되고 RP가 CXL flit으로 변환. HDM의 memory address space는 EP 내부 DRAM과 다르므로 CXL controller가 **HDM base address를 빼서(deducting) DRAM 주소로 translate** — page fault도, software도 개입 없음.

**CXL switch virtual hierarchy (Fig 3, p.51)**: switch의 fabric manager(FM)가 internal routing table을 설정해 각 USP를 서로 다른 DSP에 연결, root(host)에서 terminal(CXL device)까지 virtual hierarchy를 만든다. CXL device는 여러 logical device를 정의해 각각 HDM을 다른 host에 노출할 수 있다. **결정적 문장**: "CXL virtual hierarchy only offers the path from one to another to ensure that no host is sharing an HDM." (p.51) → pooling은 되지만 sharing은 안 됨.

**DirectCXL runtime (Fig 4, p.51)**: HDM address space를 **cxl-namespace**라는 segment들로 분할. CXL device가 PCIe enumeration 시 감지되면 driver가 entry device(`/dev/directcxl`)를 만들고 ioctl로 cxl-namespace를 관리. HDM segment table(offset, size, ref count)을 참조해 (physically) contiguous space를 할당하고 `/dev/cxl-ns0` 같은 device를 만들어, application이 `mmap` + `vm_area_struct`로 자기 process virtual memory에 매핑. → **순수 memory-mapped file(load/store)로 disaggregated memory 사용**.

**Prototype (Fig 5, p.51–52)**: 4개 compute host가 CXL switch로 4개 CXL device에 연결(switch 늘리면 scale). 각 CXL device는 커스텀 memory blade에 16-nm FPGA + 8개 DDR4 module(64 GB), FPGA 안에 CXL controller + 8 DRAM controller. Host processor는 **자체 RISC-V ISA 4-out-of-order-core**, LLC가 CXL RP 구현. In-house softcore 100 MHz, CXL/PCIe IP(RP, EP, Switch) 250 MHz. "there are no commercialized CXL 2.0 IPs for the processor side's CXL engines and CXL switch. Thus, we built DirectCXL IPs from the ground." (p.52)

### Evaluation (p.52–55)

**In-depth RDMA vs CXL (Fig 6, 7, p.52–53)**: 64B read 기준. RDMA는 두 DMA로 PCIe transfer·memory access latency가 두 배, InfiniBand 통신 오버헤드가 total latency(2,705 cycles)의 **78.7% (2,129 cycles)**. DirectCXL은 memory load request에 **328 cycles**로 RDMA보다 **8.3× faster**. 이유 두 가지: (1) CXL은 PCIe로 직결(RDMA는 InfiniBand↔PCIe 프로토콜/인터페이스 변환), (2) DirectCXL은 LLC의 load/store request를 CXL flit으로 translate(RDMA는 DMA로 data 이동). Payload < 1KB일 때 RDMA의 primary bottleneck은 **Library(software, 4,158 cycles avg)**; payload 커지면 Copy가 total의 28.9%. DirectCXL은 software·copy 오버헤드가 전혀 없고 4-KB payload에서 **LLC(CPU Cache)가 67% dominant**, PCIe physical bus(FlexBus)가 **28%**.

**Memory hierarchy (Fig 7, p.53)**: RDMA best-case 2,027 cycles로 DirectCXL·L1 cache보다 각각 **6.2×, 510.5× 느림**. DirectCXL은 328 cycles, Local은 L2 miss 시 60 cycles. DirectCXL의 bottleneck은 **CXL IP 포함 PCIe가 total latency의 77.8%**.

**Real workloads (Fig 8, Table 1, p.52–54)**: DLRM(remote 17 GB embedding tables), MemDB(4 GB KV), Ligra 4 graph(MIS/BFS/CC/BC, 7 GB), local per-node < 100 MB. Swap 대비 normalize. DirectCXL은 Swap·KVS 대비 각각 **3×, 2.2× better** (p.53). Swap은 exec time의 **51.8%**를 kswapd+FastSwap driver가 소비(LRU 기반 잦은 page exchange). DLRM에서 KVS는 page 대신 정확한 embedding 크기만 load해 Swap의 transfer 오버헤드를 **6.9×** 줄임. MemDB에서 KVS는 exec time의 55.3%(RDMA)·24.9%(Software)를 remote DRAM 처리에 소비.

**Geometrical deep learning (Fig 9, Table 2, p.54)**: GNN embedding lookup. KVS가 Swap보다 평균 **3.9× better**(local memory 효율적 사용), DirectCXL은 KVS보다 평균 **2.4× better**(RNIC 제어·data management 없음). Swap은 어떤 data를 local/remote에 둘지 정책이 없어 4-KB page로 8-byte pointer를 읽는 graph traverse에서 DirectCXL 대비 **2.2× worse**.

**CXL version 비교 (Table 3, p.54)**: CXL 1.1/2.0/3.0 feature 비교. Back invalidation·multilogical/multiheaded device·DCD·port-based routing·multi-level switch·fabric-attached memory는 모두 **CXL 3.0에서만** ✓. 즉 fine-control memory sharing은 CXL 3.0 영역.

### Toward Composable Server Architecture (p.55–56)

CXL 2.0의 한계를 CXL 3.0으로 확장하는 on-going/future work. 세 축:

1. **Multiple cache devices (Fig 10a)**: CXL 3.0은 CXL flit(66B)을 256-byte flit으로 확장하고 cache identifier(4 bits) 등을 추가해, 한 RP에 type-1/type-2 device 16개를 같은 coherent domain에 둘 수 있게 함 → heterogeneous computing scalable.
2. **Back-invalidation snoop**: type-3 device(memory expander)를 CXL cache coherent domain에 넣기 위해 256-byte flit에 back invalidation opcode 도입.
   > "Each type-3 device can invalidate the host-side processor cache through back invalidation, which allows the underlying devices to control all cache states of the corresponding host processor(s) or type-1/2 devices in an active manner. The back-invalidation snoop thus makes all types of CXL devices coherent, which can tightly integrate computing and memory domains into a single cache coherent domain." (p.55)
3. **Memory expansion control (Fig 10b)**: **MH-LD(multiheaded logical device)** — type-3 device가 여러 head를 갖고 각각 독립 logical device로 partition, bandwidth degradation 없이 여러 host에 노출. **DCD(dynamic capacity device)** — HDM 물리 공간을 작은 **block** 단위로 관리, block을 host/accelerator 간 dynamic reallocation. DCD extent list의 tag(10 bytes)에 host identifier를 넣어 어느 host가 소유하는지 구분 → **load balancing + data sharing(back-invalidation snoop 보조)** 가능. (CXL 2.0 대비: CXL 2.0은 HDM을 static partition, 각 HDM이 single PCIe path(head)로 노출돼 bandwidth 제약 + exclusive binding.)

**Fabric extension (p.56)**: CXL 2.0은 PCIe의 8-bit device identifier로 최대 253개 device 제한. CXL 3.0의 **port-based routing(PBR)**으로 flit을 PBR flit으로 확장, port number 기반 routing → multilevel switch로 최대 **4,096 device/host** 통합, heterogeneous·sharable·scalable.

### Discussion & Conclusion (p.56–57)

관련연구: **Pond**(CXL-attached memory pooling, ML로 offload 결정, ASPLOS 2023)와 **TPP**(NUMA balancing으로 CXL memory를 second tier로, arXiv 2022)를 비교. Figure 11: DirectCXL은 switch 때문에 memory hierarchy test에서 **No Switch 대비 1.54× slower**(flit routing + serialization/deserialization), **Local 대비 5.5× slower**이지만 real-world workload에선 **Local 대비 1.45× slower**(locality·prefetch 덕). TPP는 emulation 기준 DirectCXL 대비 **2.03× lower performance**. 결론: DirectCXL은 workload가 host cache 혜택을 받으면 DRAM-like, real app에서 RDMA 대비 평균 7×.

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "directly accessible memory disaggregation"
- "connects host processor complex and remote memory resources over CXL's memory protocol"
- "no software intervention or memory data copies"
- "composable server architecture"

**Technical concepts:**
- "host-managed device memory (HDM)" / "HDM segment table"
- "CXL virtual hierarchy" / "fabric manager (FM)"
- "cxl-namespace" (runtime의 segment 추상화)
- "back-invalidation snoop" (type-3 device가 host cache를 invalidate)
- "multiheaded logical device (MH-LD)" / "dynamic capacity device (DCD)" / "DCD extent list"
- "single cache coherent domain"
- "port-based routing (PBR)" / "multilevel switch"

**Value language:**
- "DRAM-like performance when the workload can enjoy the host-processor's cache"
- "expose the true performance of remote-side disaggregated memory resources"
- "tightly integrate computing and memory domains"

> ⚠ **피해야 할 어휘** (DirectCXL-signature, 그대로 echo하면 모방으로 보임):
> - "DirectCXL" (이 랩 고유 시스템명 — 내 시스템에 붙이면 안 됨)
> - "straight connects a host processor complex and remote memory resources" (abstract 원문 그대로)
> - "from the ground" (이들의 시그니처 표현)
> - "7× better performance than RDMA-based memory pooling" (이 논문 결과 수치 — 내 결과처럼 쓰면 안 됨)

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| Abstract, p.48 / Conclusion, p.57 | "around 7× better performance than RDMA-based memory pooling" (real-world workloads 평균) | CXL 직결이 RDMA 대비 갖는 이점 motivation |
| In-depth, p.52 | RDMA는 two DMA 필요, InfiniBand 오버헤드가 total latency의 "78.7% (2,129 cycles) of the total latency (2,705 cycles)" | RDMA disaggregation의 software/protocol 오버헤드 근거 |
| In-depth, p.52 | "DirectCXL only takes 328 cycles for memory load request, which is 8.3× faster than RDMA" | CXL.mem load/store 경로의 지연 우위 |
| Memory hierarchy, p.53 | RDMA best-case 2,027 cycles = DirectCXL 대비 6.2×, L1 대비 510.5× slower; DirectCXL bottleneck은 PCIe(CXL IP 포함) "77.8% of the total latency" | disaggregation latency가 결국 interconnect physical layer에 수렴함 |
| Real workload, p.53–54 | DirectCXL은 Swap·KVS 대비 각각 3×, 2.2× better; graph에서 KVS는 Swap 대비 3.9×, DirectCXL은 KVS 대비 2.4× better | page-based/object-based 대비 direct-access 이점 |
| Fig 11, p.56 | CXL switch 삽입 시 "1.54× slower than No Switch"; real-world에선 "1.45× slower than Local" | pooling switch의 비용 vs local DRAM 근접성 |
| Prototype, p.51–52 | 16-nm FPGA + 8× DDR4 (64 GB) CXL memory blade, RISC-V 4-OoO-core host, softcore 100 MHz / CXL·PCIe IP 250 MHz, 4 host × 4 device via CXL switch | feasibility-by-building 사례로 인용 |
| Table 1, p.52 | local per-node < 100 MB, remote DLRM 17 GB / MemDB 4 GB / Ligra 7 GB | memory pooling이 필요한 capacity 격차 예시 |

---

## 🎯 Strategic anchor

> "The back-invalidation snoop thus makes all types of CXL devices coherent, which can tightly integrate computing and memory domains into a single cache coherent domain." (Toward Composable Server Architecture, §Back-invalidation snoop, p.55)

바로 앞 문장과 함께: CXL 2.0의 virtual hierarchy는 "no host is sharing an HDM" (p.51)로 **의도적으로 sharing을 배제**한다. 즉 이 논문은 host 간 진짜 memory sharing = **cache coherence 문제**임을 명시하고, 그 해법(back-invalidation snoop, single cache coherent domain)을 CXL 3.0 on-going work로만 남겨 둔다.

→ **본인 활용**: 면담·자소서에서 "CAMEL Lab의 DirectCXL(IEEE Micro 2023)은 CXL 2.0 pooling이 p.51에서 'no host is sharing an HDM'으로 sharing을 배제하고, host 간 coherent sharing을 p.55의 back-invalidation snoop 기반 CXL 3.0 future work로 남겨 두었다. 저는 바로 이 multi-node cache coherence를 FPGA로 feasibility-by-building 하려 한다"로 진입점을 명확히 지목. 이 논문이 내 방향의 gap을 스스로 열어 준다는 서사.

---

## Connection to my research direction

| 차원 | 이 paper (DirectCXL) | 본인 방향 |
|---|---|---|
| Scope | 단일 host가 remote HDM을 직접 접근하는 disaggregation + switch로 pooling | 여러 host가 **동일 memory를 coherent하게 공유**(multi-node coherence, PGAS-over-CXL) |
| Mechanism | CXL 2.0 virtual hierarchy(path 격리), HDM base deduct translation, mmap runtime | CXL 3.0 back-invalidation snoop / MH-LD / DCD 기반 cross-host coherence protocol |
| Coherence | 없음 — "no host is sharing an HDM" (p.51), sharing은 명시적으로 배제 | coherence가 1급 목표. sharing 시 cache state 관리가 핵심 문제 |
| Workload | DLRM, MemDB, Ligra (single-host far-memory 관점) | multi-host 공유 자료구조 / PGAS 스타일 partitioned global address space |
| Feasibility | 16-nm FPGA로 IP를 "from the ground" 구현 (강한 align) | 동일 철학 — coherence protocol을 FPGA로 build해 검증 |
| Open space | back-invalidation snoop·DCD sharing이 on-going/future (p.55–56) | 바로 이 open space가 내 연구 자리 |

DirectCXL은 "remote memory를 host에 직접 붙이는" 문제를 CXL 2.0에서 실증적으로 닫았지만, **여러 host가 그 memory를 동시에·coherent하게 공유**하는 문제는 열어 두었다. 내 연구는 scope가 한 단계 위(pooling→sharing/coherence)이며, mechanism도 다르다(path isolation → active cache-state control). 이 논문의 prototype 방법론(from-the-ground FPGA IP)은 그대로 계승하되, 대상 문제를 CXL 3.0 multi-node coherence로 옮기는 것이 내 확장 방향이다.

---

## Open questions / gaps

- [ ] back-invalidation snoop의 실제 latency/bandwidth 비용은? (논문은 개념·opcode만, 정량 측정 없음 — p.55)
- [ ] MH-LD로 한 type-3 device를 여러 host가 공유할 때 **cache coherence traffic scaling**은 어떻게 되는가? (host 수 증가 시)
- [ ] DCD extent list의 host identifier tag(10 bytes) 기반 sharing에서 **동시 쓰기 충돌(write conflict)** 해결 프로토콜은 미정의 (p.56)
- [ ] CXL switch 삽입이 1.54× 느리게 하는데(p.56), multilevel switch(4,096 device)로 확장 시 coherence latency는 얼마나 악화되나?
- [ ] PGAS 프로그래밍 모델을 CXL 3.0 single cache coherent domain 위에 올릴 때 runtime 계층(cxl-namespace의 확장?)은 어떻게 설계되나?
- [ ] real CXL 3.0 silicon 부재 상황에서 coherence protocol의 feasibility를 FPGA로 어떻게 검증할 것인가 (내 연구 과제)

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [13] | D. Gouk et al., "Direct access, high-performance memory disaggregation with DirectCXL," USENIX ATC 2022, pp.287–294 | 이 IEEE Micro의 **원 conference 논문**. DirectCXL 6-page 원본 — 반드시 먼저 |
| ☐ | [14] | H. Li et al., "Pond: CXL-based memory pooling systems for cloud platforms," ASPLOS 2023 | 경쟁 pooling 접근(cloud, ML 기반 offload). CXL pooling 비교축 |
| ☐ | [12] | H. A. Maruf et al., "TPP: Transparent page placement for CXL-enabled tiered memory," arXiv:2206.02878, 2022 | CXL을 second-tier로 쓰는 NUMA balancing 접근. DirectCXL이 2.03× 우위라 주장한 baseline |
| ☐ | [6] | Z. Ruan et al., "AIFM: High-performance, application-integrated far memory," USENIX OSDI 2020 | object-based far memory의 대표. source 수정 비용 논거 |
| ☐ | [3] | C. Pinto et al., "ThymesisFlow: SW-defined HW/SW co-designed interconnect stack for rack-scale memory disaggregation," MICRO 2020 | HW/SW co-design disaggregation — feasibility-by-building 참고 |
| ☐ | [4] | Z. Guo et al., "CLIO: A hardware-software co-designed disaggregated memory system," ASPLOS 2022 | HW-SW co-design remote memory. 내 build 방법론 참고 |
| ☐ | [2] | K. Lim et al., "System-level implications of disaggregated memory," HPCA 2012 | disaggregation motivation의 고전 |
| ☐ | [1] | E. Amaro et al., "Can far memory improve job throughput?," EuroSys 2020 | far memory 효용 논거 |

---

## Personal annotations

<!-- user 전용 영역. workflow는 이 섹션을 수정하지 않음. -->

- (초기 note 자동 생성 시점 메모) 이 논문의 제목 "Memory Pooling"은 사실 **오해를 부른다** — 실질은 DirectCXL(disaggregation)이고 pooling은 switch virtual hierarchy로 device를 분배하는 수준. "no host is sharing an HDM"(p.51)이 계보에서 가장 중요한 한 문장. 내 연구는 이 문장을 뒤집는 데서 시작.
- 계보 상 위치: DirectCXL(disaggregation, ATC'22) → 본 논문(pooling 관점 확장, IEEE Micro'23) → CXL 3.0 coherence/sharing(future work). Phase 1 안에서 이 논문은 "pooling이 아직 sharing이 아님"을 증명하는 노드.
