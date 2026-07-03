---
title: "Hello Bytes, Bye Blocks: PCIe Storage Meets Compute Express Link for Memory Expansion (CXL-SSD)"
aliases: [Hello Bytes Bye Blocks, Hello Bytes, CXL-SSD]
type: paper
status: read
tags: [paper, cluster/cxl, camel-cxl-lineage]
---
# Hello Bytes, Bye Blocks: PCIe Storage Meets Compute Express Link for Memory Expansion (CXL-SSD)

> **Source PDF**: [Hello Bytes, Bye Blocks.pdf](Hello%20Bytes,%20Bye%20Blocks.pdf)
> **Authors**: Myoungsoo Jung (단독) · Computer Architecture and Memory Systems Laboratory (CAMEL), KAIST
> **Venue / Year**: ACM HotStorage 2022 (June 27–28, 2022, Virtual Event, USA)
> **DOI**: [10.1145/3538643.3539745](https://doi.org/10.1145/3538643.3539745) · ACM ISBN 978-1-4503-9399-7/22/06
> **Length**: 7 pages (p.45–51)
> **Read status**: ☐ Skim · ☐ Partial · ☑ Full read (2026-07-04)
> **My reading purpose**: CAMEL Lab CXL 연구 계보의 **출발점(Phase 1, 2022)** 파악. 이 vision paper가 던진 미래 방향(CXL-SSD = byte-addressable block storage, storage disaggregation, host-side latency/persistence control)이 내 방향(CXL disaggregation, multi-node coherence, PGAS-over-CXL, feasibility-by-building)의 **문제 정의를 어디서 열어놨는지** 확인.

계보: [CAMEL Lab CXL 연구 계보](../CAMEL%20Lab%20CXL%20연구%20계보.md)

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

CXL(Compute Express Link)을 **메모리 확장기(memory expander) 관점**에서 뜯어보고, PCIe block storage(SSD)를 **byte-addressable·cache-coherent working memory**로 재편할 수 있다고 주장하는 **vision paper**다. 저자는 (1) 왜 block storage가 CXL의 혜택을 받는가, (2) 어떻게 host memory bus에 붙이는가, (3) 어떤 CXL device type을 쓸 것인가, (4) 더 나은 활용을 위해 CXL이 뭘 더 갖춰야 하는가 — 네 질문에 답한다. 결론: PCIe storage에는 **CXL Type 3(CXL.mem only)** 가 최적이며, storage 쪽은 기존 PCIe endpoint/NVMe 로직 재사용으로 **minor modification** 만으로 지원 가능하다고 본다. FPGA로 CXL agent/controller를 시제작해 Apex-Map으로 성능을 투영하고(§5), 나아가 CXL switch 기반 **storage disaggregation**(§6)과 host-side **determinism·bufferability** 제어(§7)라는 미래 과제를 던진다.

---

## Core thesis

> "we argue that it can also be useful to reform existing block storage as cost-efficient, large-scale working memory." (Abstract, p.45)

> "It is a long-standing dream for PCIe storage to have byte-addressability and be a part of working memory devices" (§2, p.46)

추가 설명: CXL의 multi-protocol(특히 CXL.mem)이 PCIe block storage의 **block(4KB) semantic을 memory-compatible byte semantic으로 bridge**할 수 있다는 것이 핵심. 기존 방식(SSD 내부 buffer를 BAR로 노출)은 **non-cacheable** 접근이라 성능이 무너지지만, CXL은 load/store를 **cacheable·coherent**하게 만들어 CPU cache 이점을 살린다. DRAM/PMEM 기반 pooling을 넘어 **block storage 기반의 훨씬 큰 memory pool**을 열 수 있다는 주장.

---

## Why this matters to me

이건 CAMEL Lab CXL-SSD 라인의 **원점(Phase 1)** 이고, 내가 하려는 memory system architecture 연구의 *문제 정의*가 어디서 발화됐는지를 보여준다. 이 paper는 "CXL로 storage를 byte-addressable하게"라는 **비전만 던지고 실체(하드웨어·OS·coherence)는 열어둔** 상태다 — "As there is no CPU and fabric for CXL yet, it is also unclear ... to see how CXL-enabled storage can be implemented and interact with CPU" (§4 서두, p.47). 즉 내 **feasibility-by-building** 지향(FPGA로 실제 만들어 검증)이 정확히 이 gap을 겨냥한다. 특히 §6의 disaggregation(virtual hierarchy, multi-host)과 §7의 latency/persistence 제어는 **multi-node coherence·PGAS-over-CXL** 문제로 그대로 확장되는 미완의 스케치라, 내 방향의 상류(upstream) motivation 문서로 인용 가치가 높다.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| Abstract | — | p.45 | CXL의 세 sub-protocol을 memory-expander 관점에서 검토, PCIe storage에 최적 device type 제안 |
| 1 | Introduction | p.45–46 | 네 개 질문(why/how/what type/what more) 제기, CXL = PCIe와 호환되는 cache-coherent interconnect |
| 2 | Why CXL for PCIe storage | p.46 | byte-addressability는 오랜 꿈; 기존 BAR 방식은 **non-cacheable** → 성능 붕괴. CXL은 cacheable하게 |
| 3 | Multi-protocol and CXL devices | p.46–47 | CXL.io/.mem/.cache와 Type 1/2/3 정의, FlexBus, HDM/CXL RP |
| 4 | Integrating storage into CXL | p.47–48 | **Type 3 채택** 근거 3가지, storage-side minor modification, CXL flit 기반 load/store 경로 |
| 5 | Performance projection | p.48 | FPGA RISC-V + OpenExpress NVMe 시제작, Apex-Map으로 locality별 latency 투영 |
| 6 | Storage disaggregation | p.48–49 | Switch / Multi-switch / Virtual hierarchy(MLD, multi-host) 세 topology |
| 7 | Extension for storage control | p.49–50 | **determinism(DT/ND)**, **bufferability(BF/NB)** 두 상태 + GPF(global persistent flush) 제안 |
| 8 | Conclusion and future work | p.50 | 미완의 성능투영, SW/HW 환경 확장이 향후 과제 |

---

## Section notes

### §1 Introduction (p.45–46)
CXL은 Gen-Z[1]·CCIX[2]에 이어 나온, 서로 다른 CPU·accelerator·memory를 하나의 coherent domain으로 묶는 **최초의 open interconnect protocol**이며 Gen-Z를 흡수(§1, p.45)하고 PCIe 표준과 완전 호환된다. 저자는 CXL이 DRAM·PMEM pooling을 넘어 **block storage까지 memory-compatible byte semantic으로 bridge**할 수 있다고 보고, storage designer/architect가 던질 네 질문을 명시한다.

> "i) *why and what can the block storage benefit from CXL?* ... ii) *how can we connect the underlying block storage to the host's system memory bus?*, iii) *what kind of CXL device type should be used for the block storage and memory expander?*, and iv) *what does CXL need to improve for better utilization of the block storage?*." (§1, p.45)

### §2 Why CXL for PCIe storage (p.46)
byte-addressability는 PCIe storage의 오랜 꿈이고, 이미 industry prototype·NVMe가 SSD 내부 DRAM/buffer를 **PCIe BAR**로 노출해 load/store로 접근하게 해왔다(§2, p.46). 그러나 PCIe는 storage를 그냥 peripheral로 보기 때문에 이런 BAR 접근은 **non-cacheable**이다. x86(Intel/AMD)은 PCIe 관련 memory request의 캐싱을 금지하는데, 이것이 storage-integrated expander를 memory hierarchy에서 배제하고 CPU cache 이점을 못 쓰게 만든다.

> "this non-cacheable characteristic severely degrades the performance of all memory accesses targeting the BARs." (§2, p.46)

> "enforces the storage-integrated memory expanders to be excluded from the conventional memory hierarchy and disables them from taking advantage of CPU caches." (§2, p.46)

CXL은 같은 hierarchy 내 모든 cache를 coherent하게 보장하므로 PCIe address space로 가는 load/store를 **cacheable**하게 만들 수 있고, 그래서 DRAM/PMEM보다 훨씬 큰 memory pool을 열 수 있다.

### §3 Multi-protocol and CXL devices (p.46–47)
CXL은 세 sub-protocol을 갖는다: **CXL.io**(모든 device가 쓰는 fundamental non-coherent load/store I/O, PCIe 계층 위 FlexBus 채널), **CXL.mem**, **CXL.cache**. 이 조합으로 세 device type이 정의된다(Figure 1, p.47):
- **Type 1**: internal DRAM 없이 local cache만 — CXL.cache + CXL.io로 host memory를 능동적으로 캐싱하는 accelerator(예: TPU[27]).
- **Type 2**: 고성능 memory(**HDM, host-managed device memory**)를 내장한 discrete accelerator — CXL.io/.cache/.mem 전부 사용, GPU의 GDDR과 달리 CXL host가 coherent load/store로 HDM 접근.
- **Type 3**: processing 없는 non-acceleration device — **CXL.mem only**(+ device-side CXL.io), host가 발행한 load/store만 serve, host로의 request(CXL.cache) 불가. host memory 확장용.

### §4 Integrating storage into CXL (p.47–48)
저자는 storage-integrated expander로 **Type 3를 옹호**하며 세 근거를 든다(§4, p.47):
1. Type 2는 computationally intensive용 설계라 **CXL RP당 device 1개**만 붙어 scale 불가; Type 3는 여럿 가능.
2. CXL.cache+CXL.mem 풀세트는 **추가 통신 부담**을 야기 — 모든 load/store가 storage의 computing complex cache state를 확인해야 해 I/O당 다중 CXL transaction 발생.
3. Type 2는 storage-side compute가 host memory 접근 시 **매번 host 허가**를 받아야 해 device-level I/O 성능이 더 나빠짐.

**Storage-side modification은 minor**하다: 기존 PCIe endpoint 로직을 재사용해 CXL transaction packet formatting·CXL.io 제어를 하는 CXL storage controller를 구성하고, 기존 NVMe controller의 command parsing·page copy를 단순화해 CXL.mem read/write로 구현. NVMe spec이 controller를 firmware/hardware 어느 쪽으로도 realize 가능케 하므로, CXL.mem read/write는 hardware로 자동화하고 internal DRAM·backend block media는 firmware가 관리하게 하는 편이 낫다.

**System integration(Figure 1b)**: boot 시 host가 CXL RP에 붙은 device를 enumerate, storage로부터 CXL BAR·HDM 크기를 받아 system memory에 매핑한다. **HDM은 cacheable address space(CXL RP reserved)** 로 매핑돼 user가 load/store 가능. application이 HDM에 load/store하면 CXL RP가 **CXL flit** 메시지를 만들어 CXL controller로 보내고, endpoint/controller가 flit을 parse해 command·target address를 뽑아 underlying storage firmware와 협업해 data를 serve한다.

### §5 Performance projection (p.48)
CXL.mem/.io를 지원하는 processor가 아직 없으므로 **FPGA로 host와 storage-integrated expander를 각각 시제작**했다: in-house RISC-V CPU(64-bit dual-core O3, 128KB L1 + 4MB L2)에 CXL.mem/.io agent 통합, storage node는 16nm FPGA의 **32GB OpenExpress[24] 기반 NVMe storage**. OpenExpress backend media는 **Z-NAND를 emulate**하고 CXL request를 internal DRAM에 buffer링. 비교군은 local DRAM-only(DRAM)와 PCIe-based expander(PCIe). Workload는 **Apex-Map[38]** global memory access benchmark로 locality parameter α(0.001=최고 locality ~ 1=최저)를 바꿔가며 request size 64B(= LLC cacheline)로 **512 million memory instruction** 생성.

결과(Figure 2, p.48, unit = CPU cycles):
- best-case(α=0.001): PCIe 555.7, CXL 4.29, DRAM 1.73 → PCIe는 CPU cache를 못 써서 CXL 대비 **129.5× 긴 latency**.
- average(0.001≤α≤1): PCIe 555.7, CXL 179.75, DRAM 19.2 → CXL이 PCIe보다 **3× 빠름**, DRAM보다 **9.3× 느림**(하지만 block storage 활용 감안 시 합리적 범위).
- worst-case(α=1): PCIe 555.7, CXL 344.9, DRAM 4.1 → locality 없으면 CXL이 Z-NAND latency를 못 숨겨 DRAM 대비 **84.1× worse**, 그래도 PCIe보다는 **1.6× 나음**.

저자는 worst-case가 DRAM과 멀다는 데 "다소 실망(somewhat disappointed)"했지만, graph processing 등 high-locality workload가 많고 대용량이라는 점에서 여전히 유효하다고 본다. long latency 최적화(PCIe internal DRAM·backend media 활용)는 §7로 이어짐.

### §6 Storage disaggregation (p.48–49)
CXL 2.0의 FlexBus가 **CXL switch**(USP: upstream port, DSP: downstream port를 reconfigurable crossbar로 연결)를 허용해 network를 scalable하게 만든다. 세 topology(Figure 3, p.49):
- **(a) Switch**: 한 host의 CXL RP에 여러 DSP를 통해 다수 storage device를 붙여 local memory 확장. host가 각 HDM을 물리 memory 각 위치에 매핑.
- **(b) Multi-switch**: switch당 lane 수 제한(보통 64~128 lanes, 16-lane device면 4~8 port)을 넘기 위해 leaf switch를 추가해 많은 storage device 관리. 현재 CXL이 다루는 capacity는 **4PB**.
- **(c) Virtual hierarchy(VH) / multi-host**: switch crossbar(**fabric manager**)가 USP-DSP 연결을 기억해 임의 개수 host CPU를 붙이고 각 host로 유니크 routing path(VH)를 구성. storage device 하나를 CXL network 어디에 붙은 host로도 매핑 → **full disaggregation**. **Storage device virtualization**: 한 endpoint를 최대 16개 Type 3 device(**MLD, multiple logical device**)로 논리 분할, 각 MLD가 자체 HDM을 서로 다른 host memory 위치로 매핑 → fine-granular sharing. 단점: multi-host VH의 **bandwidth sharing·traffic congestion**(backend/internal DRAM partitioning이 병렬성·MLD당 bandwidth를 떨어뜨림).

### §7 Extension for storage control (p.49–50)
Type 3는 memory pooling용이라 block storage에 두 문제가 남는다: **latency fluctuation**과 **data persistence**. CXL.mem/.io는 async라 loads/stores turn-around를 엄격히 관리하지 않아, storage internal task(GC 등)로 latency가 크게 튄다. 또 PMDK[39] 같은 라이브러리가 persistence를 요구하면 CXL의 현재 flush로는 부족. 저자는 CXL message에 붙일 두 hint state 제안:
- **Determinism**: DT(deterministic, internal task 없이 즉시 serve) / ND(non-deterministic, fire-and-forget — target이 후속 idle에 internal task 스케줄).
- **Bufferability**: BF(bufferable, SSD internal DRAM에 캐시/버퍼 허용) / NB(non-bufferable, persistence를 first-class로 — block media에 즉시 write-back). CXL은 **GPF(global persistent flush)** register로 network·SSD DRAM의 모든 data를 즉시 backend로 flush 가능.

조합(BF+DT, BF+ND, NB+DT, NB+ND)으로 user scenario 대응: transactional memory(libpmem/libpmemobj)의 log는 persistent 불필요 → BF+ND/NB+ND, commit 시 GPF로 flush(NB+DT). load는 통상 DT 활용, write-after-write dependency 없는 load는 non-sync BF+DT/BF+ND로 internal DRAM에 prefetch. lock/sync(spinlock, barrier)는 persistence 불필요·data보다 process에 lifetime 종속 → Type 3 + BF+DT 접근이 유리.

### §8 Conclusion and future work (p.50)
CXL을 memory-expander 관점에서 검토하고 block→byte 재편을 위한 여러 configuration을 탐색했음을 정리. 성능 투영은 불완전하고 storage-integrated expander의 다양한 관점 연구에 한정됐다고 인정하며, SW/HW 환경 확장을 향후 과제로 남긴다.

> "we believe that the several characteristics of CXL-based memory expansion that this paper discussed will lead to many architectural changes in both software and hardware in the near future." (§8, p.50)

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "reform existing block storage as cost-efficient, large-scale working memory"
- "bridge PCIe storage's block semantics to memory-compatible byte semantics"
- "memory expander viewpoint"

**Technical concepts:**
- "storage-integrated memory expander"
- "host-managed device memory (HDM)"
- "multiple logical device (MLD)" / "virtual hierarchy (VH)"
- "CXL flit" / "CXL RP reserved" cacheable memory space
- "determinism (DT/ND)" · "bufferability (BF/NB)" · "global persistent flush (GPF)"
- "non-cacheable characteristic" of PCIe BAR access

**Value language:**
- "long-standing dream for PCIe storage to have byte-addressability"
- "cost-effective and practical interconnect technology"
- "excluded from the conventional memory hierarchy"

> ⚠ **피해야 할 어휘** (paper-signature, 직접 echo하면 안 됨):
> - "Hello bytes, bye blocks" — 제목 자체, 인용은 하되 내 문장으로 echo 금지
> - "reform existing block storage as cost-efficient, large-scale working memory" — Jung의 시그니처 thesis 문장
> - "storage-integrated memory expander" — 이 라인 고유 용어, 무비판 반복 시 모방으로 보임

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| §2, p.46 | PCIe bandwidth "63GB/s~121GB/s for Gen5/6 16×" | PCIe가 빠르긴 하나 far memory 수준이라는 motivation |
| §5, p.48 (Fig.2a) | best-case(α=0.001): PCIe 555.7 / CXL 4.29 / DRAM 1.73 cycles | non-cacheable PCIe가 CXL 대비 **129.5× 느림** — cacheability의 가치 |
| §5, p.48 (Fig.2b) | average: PCIe 555.7 / CXL 179.75 / DRAM 19.2 cycles | CXL이 PCIe보다 **3×**, DRAM보다 **9.3×** — CXL-SSD의 현실적 위치 |
| §5, p.48 (Fig.2c) | worst-case(α=1): PCIe 555.7 / CXL 344.9 / DRAM 4.1 cycles | locality 없으면 DRAM 대비 **84.1× worse**, PCIe 대비 **1.6× 나음** — 한계 정직하게 인용 |
| §5, p.48 | 시제작: RISC-V 64-bit O3 dual-core(128KB L1, 4MB L2) + 32GB OpenExpress NVMe, 16nm FPGA, Apex-Map 512M instr, 64B req | feasibility-by-building 사례 인용 |
| §6, p.49 | CXL switch: 64~128 lanes, 16-lane device 기준 4~8 ports; 현재 capacity **4PB**; MLD 최대 **16** Type 3 | disaggregation scale 인용 |

---

## 🎯 Strategic anchor

> "we can virtualize each storage device to be shared by different hosts. Specifically, as shown in Figure 3c, CXL allows a system to logically splits each endpoint into multiple Type 3 devices (up to 16), called *multiple logical device* (MLD). ... As each MLD associated with the same storage device can be a part of different VHs, it is expected to utilize the underlying storage resources better by allocating the memory expanders in a fine granular manner." (§6, p.49)

→ **본인 활용**: 면담·자소서에서 "CAMEL의 CXL-SSD 라인은 2022년 vision 단계에서 이미 **multi-host virtual hierarchy + MLD**로 storage disaggregation을 스케치했지만, multi-host가 같은 device를 공유할 때의 **bandwidth sharing·coherence**는 열어둔 채였다(§6, p.49). 제 방향은 바로 이 지점 — **multi-node coherence와 PGAS-over-CXL** — 을 feasibility-by-building으로 실증하는 것"이라고 계보상 내 위치를 명확히 짚는 데 사용.

---

## Connection to my research direction

| 차원 | 이 paper | 본인 방향 |
|---|---|---|
| Scope | 단일 device 관점: block SSD를 CXL byte-addressable expander로 | System 관점: CXL disaggregation·multi-node **coherence**·PGAS |
| Mechanism | Type 3(CXL.mem) + minor storage-side mod + host-side DT/BF/GPF hint | Coherence protocol·address translation·kernel/OS 통합을 실제 HW로 |
| Workload | Apex-Map locality sweep(합성) | 실제 disaggregated/multi-node workload에서 correctness+성능 |
| Open space | multi-host VH의 bandwidth/coherence, CPU·fabric 부재 | 바로 그 gap을 FPGA prototype으로 **correct-by-construction** 실증 |

이 paper는 **vision·single-device 관점**에서 "CXL로 block storage를 byte로 만들 수 있다"를 주장하고 host-side hint(DT/BF)로 latency·persistence를 손보는 데 그친다. 반면 내 방향은 그 위 layer — 여러 host가 한 memory pool을 **coherent하게** 공유할 때의 protocol·translation·OS 통합 — 이라 **scope의 차원이 다르다**. 특히 저자 스스로 "there is no CPU and fabric for CXL yet"(§4, p.47), multi-host VH의 bandwidth/congestion 문제(§6, p.49)를 미해결로 남겨, 내 **feasibility-by-building** 접근이 이 계보의 자연스러운 다음 단계로 포지셔닝된다. 즉 이 paper는 내 연구의 **경쟁자가 아니라 상류 motivation 문서**다.

---

## Open questions / gaps

- [ ] **Multi-host coherence**: MLD를 여러 VH가 공유할 때 host 간 cache coherence는 어떻게 보장하나? (paper는 device-level sharing만 언급, host 간 coherence 미해결 — §6, p.49)
- [ ] **Real CPU/fabric 부재**: "there is no CPU and fabric for CXL yet"(§4, p.47) — FPGA emulation만으로 투영, 실 silicon에서의 coherence traffic·latency 미검증.
- [ ] **Bandwidth partitioning**: multi-host VH가 backend/internal DRAM을 partition하면 parallelism·MLD당 bandwidth가 준다(§6, p.49) — 정량 모델·해법 없음.
- [ ] **DT/BF hint의 구현**: CXL message에 determinism/bufferability를 annotate하는 실제 protocol 확장·ISA 노출 방식 미정(§7).
- [ ] **Persistence semantics**: GPF register의 CXL spec 통합, PMDK와의 end-to-end persistence ordering 미검증(§7).
- [ ] **Worst-case latency**: random access(α=1)에서 DRAM 대비 84.1× — Z-NAND latency hiding 기법 부재(§5, p.48).

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [3] | CXL Consortium. CXL Specification Rev 2.0 | 이 논문의 Type/HDM/MLD/VH 근거 원전 — 계보 전체의 base |
| ☐ | [5] | Guo et al. **Clio**: a hardware-software co-designed disaggregated memory system. ASPLOS 2022 | disaggregated memory HW-SW co-design — 내 disaggregation 방향 직접 관련 |
| ☐ | [6] | Calciu et al. Rethinking software runtimes for disaggregated memory. ASPLOS 2021 | disaggregated memory의 SW/OS layer — 내 kernel 통합 관점 |
| ☐ | [7] | Wang et al. Enabling efficient large-scale DL training with cache coherent disaggregated memory. HPCA 2022 | **cache coherent** disaggregated memory — multi-node coherence 핵심 참고 |
| ☐ | [18] | Bae et al. **2b-ssd**: dual byte- and block-addressable SSD. ISCA 2018 | CXL-SSD 이전의 byte/block dual SSD — 계보 전사(前史) |
| ☐ | [24] | Jung. **OpenExpress**: fully HW automated open research framework for NVMe. USENIX ATC 2020 | §5 시제작 기반 — CAMEL FPGA feasibility 인프라 |
| ☐ | [8] | Zhang et al. Revamping SCM with HW automated memory-over-storage. ISCA 2021 | memory-over-storage 자동화 — CXL-SSD 직전 계보 |
| ☐ | [37] | Cheong et al. Automatic-ssd: full HW automation over new memory. ICCAD 2020 | OpenExpress backend(Z-NAND emulation) — 시제작 detail |

---

## Personal annotations

<자유 형식 메모 — user 전용 영역.>

- (계보 메모) 이 paper는 "왜 CXL로 storage인가"를 **네 질문**으로 정형화한 게 핵심 기여. 이후 CAMEL의 CXL-SSD 후속 논문들이 이 네 질문 중 하나씩을 실체화하는 구조일 가능성 — 계보 노트에서 후속편이 어느 질문을 답했는지 매핑해볼 것.
- Jung이 §5 worst-case에서 "somewhat disappointed"라고 정직하게 쓴 건 인상적. random access에서 CXL-SSD가 DRAM 84.1× 느리다는 건 내 방향에서도 반드시 정면으로 다뤄야 할 한계.
