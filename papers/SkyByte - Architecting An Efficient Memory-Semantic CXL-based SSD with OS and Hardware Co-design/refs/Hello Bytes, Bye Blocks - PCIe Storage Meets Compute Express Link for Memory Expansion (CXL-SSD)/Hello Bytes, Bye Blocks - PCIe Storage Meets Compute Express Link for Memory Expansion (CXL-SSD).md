---
title: "Hello Bytes, Bye Blocks: PCIe Storage Meets Compute Express Link for Memory Expansion (CXL-SSD)"
aliases: [CXL-SSD, Hello Bytes Bye Blocks]
type: paper-ref
venue: HotStorage
year: 2022
tags:
  - paper
  - cluster/cxl
  - topic/cxl-ssd
  - topic/memory-semantic-ssd
  - topic/memory-expander
  - venue/hotstorage
  - year/2022
---

# Hello Bytes, Bye Blocks: PCIe Storage Meets Compute Express Link for Memory Expansion (CXL-SSD)

> **Source PDF**: [Hello Bytes, Bye Blocks - PCIe Storage Meets Compute Express Link for Memory Expansion (CXL-SSD).pdf](<Hello Bytes, Bye Blocks - PCIe Storage Meets Compute Express Link for Memory Expansion (CXL-SSD).pdf>)
> 🕸️ NodeGraph: [CXL-SSD.html (새 탭에서 렌더링, push 후 유효)](https://raw.githack.com/Jeong-jin-Han/2026_CAMEL/main/papers/SkyByte%20-%20Architecting%20An%20Efficient%20Memory-Semantic%20CXL-based%20SSD%20with%20OS%20and%20Hardware%20Co-design/refs/Hello%20Bytes%2C%20Bye%20Blocks%20-%20PCIe%20Storage%20Meets%20Compute%20Express%20Link%20for%20Memory%20Expansion%20(CXL-SSD)/CXL-SSD.html)
> **Authors**: **Myoungsoo Jung** (single author) — KAIST (CAMELab)
> **Venue / Year**: HotStorage 2022 (position/vision paper) · **Length**: 7 pages
> **Read status**: ☑ Full read (2026-07-13)
> **My reading purpose**: **"CXL-SSD"라는 개념을 정립한 비전 논문**. block storage를 byte-semantic memory expander로 바꾸는 근거와, **왜 Type-3인가**를 논증. [[SkyByte]](Type-3 CXL-SSD)의 개념적 뿌리이자, 우리가 대화로 정리한 Type 1/2/3·MLD(16)·multi-host·pooling 논의의 1차 출처.

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
PCIe block storage를 CXL로 host CPU에 붙여 **byte-addressable working memory**로 쓰자는 비전/포지션 논문이자 **"CXL-SSD"라는 용어를 정립한 글**이다. 핵심 논증: (1) PCIe로도 SSD 내부 DRAM을 BAR로 노출하면 byte 접근은 되지만, x86이 PCIe 메모리 요청을 **non-cacheable**로 강제해 CPU cache를 못 써 성능이 무너진다 — CXL은 이 접근을 **cacheable**로 만들어 문제를 푼다. (2) 세 device type 중 **Type-3가 storage-integrated memory expander에 최적**이다(Type-2는 device당 1개 제한·CXL.cache 오버헤드·host 허가 필요). (3) MLD(최대 16)·virtual hierarchy로 multi-host 분해가 가능하나 대역폭 공유 문제. (4) storage 특성(GC 지연·persistence)을 위해 CXL.mem 예약 필드에 **determinism/bufferability** 힌트를 달자고 제안. FPGA 프로토타입으로 Apex-Map을 돌려, 고locality에서 CXL가 PCIe 대비 **129.5×**, 평균 **3×** 빠르고 DRAM에 근접함을 보인다.

---

## Core thesis
> "we advocate that CXL can open a new door that changes PCIe storage's block interface to a memory-like, byte interface. As CXL's multi-protocol can integrate PCIe storage into its cache coherent memory space, it can create a much bigger memory pool than DRAM-based or PMEM-based memory expansion technologies." (§2)

값싸고 대용량인 block SSD를, CXL의 cache-coherent load/store 인터페이스로 host 메모리 계층에 편입시키면 DRAM/PMEM보다 훨씬 큰 working memory pool을 만들 수 있다 — 단, **Type-3**로, 그리고 storage 특유의 지연·persistence를 다룰 프로토콜 힌트와 함께.

---

## Why this matters to me
이 논문은 [[SkyByte]]가 서 있는 **"CXL-SSD = Type-3 memory-semantic SSD"** 개념을 처음 명시적으로 세운 글이다. 우리가 며칠간 대화로 정리한 것들 — **Type 1/2/3의 프로토콜 조합, 왜 Type-3인가, MLD(최대 16)로 multi-host 분할, HDM, non-cacheable BAR의 한계** — 이 전부 여기서 1차로 논의된다([[CXL Overview]]에 정리해 둔 내용의 원출처). 특히 §7의 **determinism/bufferability** 제안은 "storage를 memory처럼 쓰되 GC·persistence 같은 storage 물성을 host에 노출"하는 co-design 사고로, [[SkyByte]]의 coordinated context switch(긴 flash 지연을 host에 알림)와 같은 계보다. 내 **"SSD 이중역할 × transparent co-design"** 발표축의 개념적 원점.

---

## Structure overview
| § | Title | Pages | Key takeaway |
|---|---|---|---|
| 1 | Introduction | p.1 | 4개 질문: why/how/which-type/what-to-improve |
| 2 | Why CXL for PCIe storage | p.2 | non-cacheable BAR의 한계 → CXL로 cacheable |
| 3 | Multi-protocol & device types | p.2-3 | Type 1/2/3, **Type-3가 최적** (3가지 이유) |
| 4 | Integrating storage into CXL | p.3-4 | 부팅 시 BAR/HDM 매핑, load/store→flit 흐름 |
| 5 | Performance projection | p.4 | Apex-Map: CXL vs PCIe 129.5×(best), 3×(avg) |
| 6 | Storage disaggregation | p.4-5 | switch/multi-switch/VH, MLD(16), multi-host |
| 7 | Extension for storage control | p.5-6 | determinism·bufferability 힌트 제안 |

---

## Section notes

### §2 왜 CXL인가 — non-cacheable BAR의 벽 (p.2)
- **byte-addressability는 오랜 꿈**: 2B-SSD·NVMe 표준이 SSD 내부 DRAM/buffer를 **BAR**로 노출해 byte 접근 가능. 내부 DRAM을 backend(Z-NAND/Flash/Optane)의 **write-back inclusive cache**로 써 긴 지연을 숨김.
- **non-cacheable의 치명타**: PCIe 대역폭은 far memory로 충분(Gen5/6 16× = 63~121 GB/s). 그러나 x86(Intel/AMD)은 PCIe 메모리 요청을 **캐시하면 시스템 실패·storage 단절** 위험이 있어 **non-cacheable로 강제** → BAR 접근 성능이 크게 무너지고, storage-integrated expander가 메모리 계층에서 배제됨.
- **CXL의 해법**: cache-coherent interconnect라 PCIe 주소공간으로의 load/store를 **cacheable**로 만든다. → block을 byte로 바꾸는 문이 열림.

> "this non-cacheable characteristic severely degrades the performance of all memory accesses targeting the BARs ... CXL ... can make the load/store requests (heading to the PCIe address space) cacheable in contrast to PCIe." (§2)

### §3 왜 Type-3인가 (p.2-3)
세 device type: **Type-1**(.io+.cache, 로컬 캐시만, TPU류), **Type-2**(.io+.cache+.mem, HDM+연산, host↔device 양방향 coherent), **Type-3**(.io+.mem, HDM만, 연산 없음). 저자는 **Type-3를 storage-integrated memory expander에 최적**으로 논증(3가지):
1. **확장성**: Type-2는 연산집약용 설계라 **CXL RP당 device 1개**만 연결 → Type-3만큼 scalable하지 않음.
2. **오버헤드**: CXL.cache+.mem 풀피처는 매 load/store가 PCIe storage 연산복합체의 **cache 상태 확인** → I/O마다 여러 CXL transaction → 성능 저하.
3. **허가 비용**: Type-2는 storage-side 연산이 자기 메모리를 접근할 때마다 host **허가 필요**(CXL.cache가 host local+HDM을 모두 coherent 관리) → device I/O 성능 악화.
- **Storage-side 수정 최소**: SSD의 PCIe endpoint 로직 재활용 + NVMe controller를 CXL.mem read/write로 단순화, firmware가 내부 DRAM+backend 관리 → 대부분 SSD가 **minor 수정으로 Type-3 지원 가능**.

### §4 시스템 통합 흐름 (p.3-4)
부팅 시 host가 RP의 CXL device를 enumerate, CXL **BAR+HDM** 크기를 받아 **cacheable 시스템 메모리**에 매핑. HDM이 원래 주소와 다른 곳에 매핑되므로 CXL RP가 CXL controller에 **remap offset을 동기화**(CXL capability/config에 기록). app load/store → RP가 **CXL flit** 생성 → endpoint+CXL controller가 flit 파싱 → firmware와 협력해 데이터 서비스.

### §5 성능 투영 — Apex-Map (p.4)
- 프로토타입: 자작 **RISC-V O3 dual-core**(128KB L1, 4MB L2) host + **OpenExpress 32GB NVMe**(16nm FPGA, backend=Z-NAND emul) storage, PCIe backplane. 비교: DRAM-only / PCIe expander / CXL. Apex-Map(locality param α, 64B, 512M inst).
- **Best case(α=0.001, 고locality)**: CXL가 PCIe 대비 **129.5×** 낮은 지연(PCIe는 CPU cache 못 씀). CXL ≈ DRAM.
- **Average**: CXL가 PCIe 대비 **3×**. DRAM 대비 9.3× 나쁘지만 block storage 감안 시 합리적.
- **Worst(α=1, fully random)**: CXL가 Z-NAND 지연을 못 숨겨 DRAM 대비 84.1× 나쁨. 그래도 PCIe 대비 **1.6×**(PCIe는 BAR 요청을 동기 처리).

### §6 Storage disaggregation (p.4-5)
세 토폴로지: (a) **Switch**(DSP→storage, USP→host RP; 단 switch 레인 64~128 → 16-lane storage 4~8개), (b) **Multi-switch**(top switch가 host RP+하위 switch 브리지, leaf switch가 다수 storage; 최대 **4PB**), (c) **Virtual Hierarchy(VH)**(FM crossbar가 USP-DSP 경로 기억 → host↔storage 완전 분해).
- **Multi-host + storage 가상화**: 각 storage를 **MLD(multiple logical device, 최대 16)**로 논리 분할, 각 MLD가 자기 HDM을 다른 host에 매핑. 단점: **대역폭 공유·트래픽 혼잡**, backend/internal DRAM 분할로 병렬성·대역폭 저하.

### §7 Storage 제어 확장 — determinism/bufferability (p.5-6)
Type-3는 memory pooling용이지 block storage용이 아니라 두 문제: **(i) latency fluctuation**(GC 등 internal task), **(ii) data persistence**. CXL.mem 예약 필드로 두 힌트 제안:
- **Determinism**: `DT`(internal task 개입 없이 즉시 서비스) / `ND`(fire-and-forget, storage가 GC 등 스케줄 가능).
- **Bufferability**: `BF`(SSD 내부 DRAM에 캐시/버퍼 허용) / `NB`(persistence first-class, block media에 기록).
- 조합(BF+DT/BF+ND/NB+DT/NB+ND) + **GPF(global persistent flush)**로 DB 로깅·트랜잭션·lock 시나리오 최적화. loads는 대개 DT 활용, prefetch엔 BF+DT/BF+ND.

---

## Key vocabulary
**Thesis / framing:**
- "CXL-SSD" / "storage-integrated memory expander"
- "block semantic to memory-compatible, byte semantics"
- "Hello Bytes, Bye Blocks"

**Technical concepts:**
- "non-cacheable BAR" → "cacheable via CXL"
- "Type-3 device" / "HDM" / "CXL flit"
- "MLD (multiple logical device, up to 16)" / "virtual hierarchy"
- "determinism (DT/ND)" / "bufferability (BF/NB)" / "GPF"

**Value language:**
- "much bigger memory pool than DRAM/PMEM-based expansion"
- "cost-effective and practical interconnect"

> ⚠ **피해야 할 어휘** (이 논문 signature):
> - "Hello Bytes, Bye Blocks"
> - "storage-integrated memory expander"

---

## Citable quantitative data
| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| §2 | PCIe Gen5/6 16× = 63~121 GB/s | PCIe 대역폭은 far memory로 충분 |
| §5 | 고locality에서 CXL vs PCIe **129.5×**, 평균 **3×** | non-cacheable 극복 효과 |
| §5 | worst-case CXL vs DRAM 84.1×↓, vs PCIe 1.6×↑ | 랜덤 접근 시 flash 지연 노출 |
| §6 | MLD **최대 16**, network 최대 **4PB** | 확장성 규모 |
| §3 | Type-2는 CXL RP당 device **1개** | Type-3 확장성 논거 |

---

## 🎯 Strategic anchor
> "CXL device types ... we advocate Type 3 for a storage-integrated memory expander in CXL. There are three reasons why we believe that Type 3 devices are better than Type 2 devices ... only one device (per CXL RP) can be connected ... having full features of CXL.cache and CXL.mem can introduce another type of communication burden ... the device should ask permission from the host." (§3)

→ **본인 활용**: "왜 CXL-SSD가 Type-3인가"의 **1차 논거**. 면담에서 SkyByte가 Type-3인 이유를 물으면 이 세 근거(확장성·CXL.cache 오버헤드·host 허가)를 인용. 그리고 "이 논문이 정립한 Type-3 single-host CXL-SSD를, 여러 host가 coherent하게 공유하는 지점(CXL 3.0)이 내 연구"로 연결. [[CXL Overview]]의 Type×개수 표와 직결.

---

## Connection to my research direction
| 차원 | CXL-SSD 비전 (HotStorage'22) | SkyByte (HPCA'25) | 내 방향 |
|---|---|---|---|
| 성격 | 비전/포지션(Type-3 논거) | 실제 설계·구현 | multi-host 확장 |
| device type | **Type-3** 옹호 | Type-3 채택 | Type-3 + HDM-DB |
| 지연/persistence | DT/BF 힌트 **제안** | context switch·write log로 **구현** | multi-node scheduling |
| multi-host | MLD(16) **분할**만 | single-host | **공유 + coherence** |

이 논문이 **개념(Type-3 CXL-SSD, storage 힌트)**을 던지고 SkyByte가 그걸 **실제 co-design으로 구현**한 관계다. determinism/bufferability(storage 물성을 host에 힌트로 노출)와 SkyByte의 coordinated context switch(긴 flash 지연을 host에 알림)는 **같은 transparent co-design 사고**. 내 연구는 이 계보의 다음 단계 — MLD 분할(pooling)을 넘어 **여러 host가 같은 CXL-SSD를 공유**할 때의 coherence — 를 잡는다. → [[CXL Multi-node Coherence]]

---

## Open questions / gaps
- [ ] MLD는 storage를 **분할**해 각 host에 줄 뿐, **여러 host가 같은 영역 공유** 시 coherence는 미해결.
- [ ] determinism/bufferability는 **제안**일 뿐 실제 프로토콜 표준화·구현 부재(SkyByte가 유사 문제를 다른 방식으로 해결).
- [ ] 성능 투영이 Apex-Map 합성 + FPGA emul 기반 — 실 워크로드·실 CXL 실리콘 부재(당시 상용 CXL 없음).
- [ ] MLD 분할이 backend/internal DRAM 병렬성을 깎아 per-MLD 대역폭 저하 → 공유 시 QoS 문제.

---

## References worth following up
| 상태 | Paper | 왜 봐야 |
|---|---|---|
| ☐ | [[FlatFlash - Exploiting the Byte-Accessibility of SSDs within a Unified Memory-Storage Hierarchy]] [10] | byte-addressable SSD의 OS 통합(직접 인용) |
| ☐ | 2B-SSD (Bae et al., ISCA'18) [18] | dual byte+block SSD |
| ☐ | Enabling efficient large-scale DL training w/ cache-coherent disaggregated memory (Wang et al., HPCA'22) [7] | CXL 분해 메모리 ML 학습 |
| ☐ | OpenExpress (Jung, ATC'20) [24] | 프로토타입에 쓴 NVMe 프레임워크 |
| ☐ | [[DirectCXL - Direct Access, High-Performance Memory Disaggregation]] | 같은 그룹의 실물 CXL.mem 분해 |

---

## Personal annotations
<!-- 본인 메모 영역 -->
