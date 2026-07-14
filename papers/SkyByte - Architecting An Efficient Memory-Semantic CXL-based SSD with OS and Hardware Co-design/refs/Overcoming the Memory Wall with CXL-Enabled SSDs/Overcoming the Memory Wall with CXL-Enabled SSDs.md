---
title: "Overcoming the Memory Wall with CXL-Enabled SSDs"
aliases: [Overcoming the Memory Wall, CXL-Enabled SSDs]
type: paper-ref
venue: USENIX ATC
year: 2023
ref-of: "SkyByte"
tags:
  - paper
  - ref
  - topic/cxl-ssd
  - topic/memory-semantic-ssd
  - topic/flash-as-memory
  - topic/caching-prefetching
  - venue/usenix-atc
  - year/2023
---

# Overcoming the Memory Wall with CXL-Enabled SSDs

> **Source PDF**: [Overcoming the Memory Wall with CXL-Enabled SSDs.pdf](<Overcoming the Memory Wall with CXL-Enabled SSDs.pdf>)
> 🕸️ NodeGraph: [OvercomingMemoryWall.html (새 탭에서 렌더링, push 후 유효)](https://raw.githack.com/Jeong-jin-Han/2026_CAMEL/main/papers/SkyByte%20-%20Architecting%20An%20Efficient%20Memory-Semantic%20CXL-based%20SSD%20with%20OS%20and%20Hardware%20Co-design/refs/Overcoming%20the%20Memory%20Wall%20with%20CXL-Enabled%20SSDs/OvercomingMemoryWall.html)
> **Authors**: Shao-Peng Yang, **Bryan S. Kim** (Syracuse) · Minjae Kim, Juhyung Park, **Sungjin Lee** (DGIST) · Sanghyun Nam, **Eunji Lee** (Soongsil) · Jin-yong Choi, Eyee Hyun Nam (FADU Inc.)
> **Venue / Year**: USENIX ATC 2023 (July 10–12, Boston)
> **DOI**: [usenix.org/conference/atc23/presentation/yang-shao-peng](https://www.usenix.org/conference/atc23/presentation/yang-shao-peng) · **Length**: 18 pages (본문 12 + refs)
> **Read status**: ☑ Full read (2026-07-14)
> **My reading purpose**: [[SkyByte]]가 baseline으로 삼는 **레퍼런스 CXL-SSD 아키텍처**(SkyByte의 "Base-CSSD" 계보). "flash-backed CXL memory가 과연 동작하는가, 그리고 무엇이 필요한가"를 real-application trace + open-source 시뮬레이터로 **특성화(characterization)**한 논문. SkyByte의 더 공격적인 OS+HW co-design이 왜 필요한지를 실증적으로 세팅해주는 motivation ref.

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
NLP 모델 크기는 연 **14.1×**로 느는데 GPU 메모리 용량은 연 **1.3×**밖에 안 늘어 생기는 **memory wall**을, DRAM보다 훨씬 싸고 TB급으로 스케일하는 **flash를 CXL Type-3 메모리 확장 장치(CXL-flash)**로 써서 넘을 수 있는지 검증한 **feasibility characterization** 논문이다. flash를 memory로 쓸 때의 3대 난관(**granularity mismatch**, **µs 단위 지연**, **제한된 endurance**)을, 내부 **DRAM cache · MSHR(miss status holding register) · prefetcher**로 얼마나 가릴 수 있는지 자체 제작 **physical memory tracing tool + MQSim 기반 CXL-flash 시뮬레이터**(둘 다 open-source)로 정량 탐색한다. 핵심 결론: real-application trace에서 메모리 요청의 **68–91%를 sub-µs**로 서비스 가능하고 장치 수명은 **최소 3.1년**, DRAM-only 대비 **11–91× 성능/비용** 이득. 단 (1) caching만으론 부족해 traffic 감축용 MSHR이 필수고, (2) 현대 prefetcher는 **virtual→physical 주소 변환(V2P)**이 접근 패턴을 흐려 대부분 무력하며 오히려 성능을 깎을 수 있어, **kernel이 access-pattern hint를 장치에 넘기는 co-design**을 제안한다. 즉 "flash-CXL memory는 된다, 단 하드웨어 caching/prefetching만으로는 DRAM-급이 안 되고 OS 협력이 필요하다"는 것이 이 논문의 메시지 — 정확히 [[SkyByte]]가 밀어붙이는 방향의 출발선.

---

## Core thesis
> "This paper investigates the feasibility of using inexpensive flash memory on new interconnect technologies such as CXL ... We show that techniques such as caching and prefetching can help mitigate the concerns regarding flash memory's performance and lifetime." (Abstract)
> "we are the first open-sourced in-depth study on the design choices of a CXL-flash device and on the effectiveness of existing optimization techniques." (§1)

CXL의 coherent load/store 덕에 SSD를 memory 확장 장치로 노출할 수 있게 되었고, 그렇다면 flash 특유의 세 병목을 **장치 내부 caching·prefetching**으로 얼마나 감출 수 있는지를 처음으로 open-source로 깊게 파본다. 답은 "상당히 가능하지만, 기존 기법의 한계가 뚜렷하고 OS-level hint 같은 **시스템 차원 변경**이 있어야 near-DRAM에 도달한다."

---

## Why this matters to me
이 논문은 [[SkyByte]]의 **직계 baseline**이다. SkyByte는 "flash-backed CXL-SSD를 memory로"라는 판을 이 논문이 세운 위에서, 여기서 "부족하다"고 진단한 지점들(caching/prefetching만으론 DRAM-급 불가, µs latency 은닉 실패, OS 협력 필요)을 **coordinated context switch + cacheline write log + OS/HW co-design**으로 공격적으로 밀어붙인다. 즉 이 논문 = **"문제·판·한계를 실증한 특성화"**, SkyByte = **"그 한계를 넘는 co-design 설계"**. 내 "SSD 이중역할 × transparent co-design" 발표축에서, 이 논문은 co-design이 왜 필요한지를 데이터로 보여주는 **motivation 슬라이드**에 해당한다. 또한 둘 다 **single-host** 전제라는 공통 한계가 내 multi-node CXL coherence 방향이 채울 빈칸을 드러낸다.

---

## Structure overview
| § | Title | Pages | Key takeaway |
|---|---|---|---|
| 1 | Introduction | p.1-3 | memory wall(NLP 14.1×/yr vs mem 1.3×/yr), flash-as-CXL-memory 가설, first open-source in-depth study |
| 2 | Background | p.3-4 | CXL Type-3 HDM 기회, flash 3대 난관(granularity/µs latency/endurance) + Table 1 |
| 3 | Tool & Methodology | p.4-6 | **physical** memory tracing tool(Valgrind+page-fault V2P), virtual trace는 과대낙관 |
| 4 | Design Space for CXL-flash | p.6-9 | DRAM cache·MSHR(repeated reads>90%)·prefetcher·flash tech/parallelism 탐색 |
| 5 | Evaluation of Policies | p.9-13 | 실제 workload 5종, cache replacement(CFLRU)·prefetcher 5종, **7 Observations** |
| 6 | Related Work | p.13 | Pond·DirectCXL·CXL-SSD(Hello Bytes)·FlashMap·FlatFlash 대비, "device-internal 설계"가 차별점 |
| 7 | Conclusion | p.13 | 68–91% sub-µs, ≥3.1yr, V2P가 prefetch 방해 → kernel hint 제안 |

---

## Section notes

### §1 Introduction (p.1-3)
memory wall: NLP 모델 파라미터는 **연 14.1×**로 폭증하는데 GPU 메모리 용량은 **연 1.3×**(Figure 1). DRAM은 GB급으로만 스케일하지만 flash SSD는 3D 적층·multi-bit cell로 **TB급**으로 싸게 스케일한다. CXL·Gen-Z·CCIX·OpenCAPI 같은 coherent 인터커넥트가 PCIe 장치를 CPU의 load/store로 직접 접근 가능하게 해주면서, flash를 main memory로 쓰는 길이 열렸다. 저자들은 **CXL Type-3 device로서의 flash(CXL-flash)** feasibility를 처음으로 open-source로 깊게 판다.

기여: (1) page-fault 이벤트로 **physical memory trace**를 수집하는 도구 + MQSim 기반 **CXL-flash 시뮬레이터**(`github.com/spypaul/MQSim_CXL`), (2) 합성 workload로 caching·prefetching의 latency 감축 잠재력 정량화(§4), (3) real workload로 현 prefetcher의 한계 분석 + **system-level 변경(kernel hint)** 제안(§5).

> "we question whether it is possible to overcome the memory wall using flash memory—a memory technology that is typically used in storage due to its high density and capacity scaling." (§1)

### §2 Background (p.3-4)
**CXL Type-3 기회(§2.1)**: Type-3는 host-managed device memory(**HDM**)를 노출하고 CPU가 load/store로 직접 조작. CXL은 CPU–device 간 coherent access(동기화 오버헤드 감소) + switch로 손쉬운 scale-out을 준다. CXL은 현재 DRAM/PMEM만 주 확장 매체로 보지만, coherent access 덕에 **SSD도 가능**.

**flash 3대 난관(§2.2)**: (1) **Granularity mismatch** — flash는 KB 단위 page로만 read/write, overwrite 불가(block erase 선행). 64B cacheline flush 하나가 16KiB page read → 64B update → 16KiB program(다른 위치)으로 **엄청난 write amplification**. (2) **µs-level latency** — flash read는 수십 µs, program/erase는 수백~수천 µs로 DRAM(수십~수백 ns)보다 orders of magnitude 느림. cell 기술별로 심화(SLC→TLC). block device일 땐 software stack이 µs를 흡수하지만 **load/store로 직접 접근하는 memory device에선 µs가 그대로 노출**. (3) **Limited endurance** — program/erase가 cell을 마모. block device SSD는 caching/buffering으로 write가 줄어 endurance가 충분하지만, memory device로 쓰면 잦은 write로 금방 소진. storage에선 firmware가 이 문제들을 처리하지만 CXL-flash는 timescale이 훨씬 짧아 **hardware가 처리**해야 함.

> "a 64B cache line flush to the CXL-enabled flash would result in 16KiB flash memory page read, 64B update, and 16KiB flash program to a different location (assuming a 16KiB page-level mapping)." (§1)

**Table 1 — memory 기술 특성**:

| Tech | Read | Program | Erase | Endurance |
|---|---|---|---|---|
| DRAM | 46ns | 46ns | N/A | N/A |
| ULL(SLC 변종) | 3µs | 100µs | 10ms | 100K |
| SLC | 25µs | 200µs | 10ms | 100K |
| MLC | 50µs | 600µs | 3000µs | 10K |
| TLC | 75µs | 900µs | 4500µs | 3K |

### §3 Tool & Methodology (p.4-6)
main memory와 CXL-flash는 **physical address**로 접근되는데, LLC↔memory controller 사이 physical transaction을 HW 수정 없이 잡는 공개 도구가 없었다. 그래서 **Valgrind/Cachegrind**로 LLC miss/eviction을 걸러낸 memory access(가상주소)를 얻고, 동시에 **page-fault 시 kernel PTE 설치 함수(`do_anonymous_page()`, `do_set_pte()`)를 후킹**해 대상 PID의 V2P 매핑을 `/proc`에 기록, 둘을 합쳐 physical trace를 만든다(Figure 2).

핵심 관찰(§3.2): **virtual 접근 패턴은 규칙적이지만 physical은 V2P 변환 때문에 전혀 달라 보인다**(Figure 3). virtual trace로 시뮬레이션하면 sub-µs 비율이 과대낙관(matrix multiply는 오차 25%↑). 따라서 physical trace가 필수. huge page로 변환 횟수를 줄여 완화 가능하나, 앱 메모리 요구가 14.1×/yr로 커지면 huge page도 곧 같은 문제에 봉착 → 일반 설정 유지.

> "the virtual-to-physical address translation obfuscates access patterns for existing prefetchers to perform adequately." (§1)

### §4 Design Space for CXL-flash (p.6-9)
MQSim[68] + MQSim-E[49] 기반 시뮬레이터. 합성 workload 5종(hashmap/matrix-mult/minheap/random/stride)으로 설계 옵션 평가. 아키텍처(Figure 4): CXL interface → indexing → prefetcher → DRAM cache → MSHR → FTL → flash channel.

- **§4.1 Caching**: flash 앞단 DRAM cache는 (1) hot data를 빠르게 서빙, (2) hit 시 flash traffic 감소. 하지만 **cache만으론 부족** — 큰 cache(footprint보다 커도)에서도 짧은 inter-arrival time이 flash backend를 압도해(queuing) 평균 latency가 DRAM보다 훨씬 높다.
- **§4.2 Reducing flash traffic (MSHR)**: cache miss 시 4KiB fetch가 진행 중인데 같은 4KiB 안의 후속 64B miss가 **또 flash read를 발행** → **repeated reads**. hashmap/matrix-mult/heap에선 flash read의 **90%↑가 repeat!** CPU cache에서 빌려온 **MSHR(miss status holding register)**로 outstanding read를 추적해 하나의 flash read로 여러 64B를 서비스 → **긴 tail latency 대폭 감소**. (SSD엔 MSHR이 드묾 — storage stack이 block I/O를 merge하기 때문. CXL-flash엔 그 software layer가 없어 HW가 해야 함.)
- **§4.3 Prefetching**: Next-N-line prefetcher, 파라미터 **degree**(공격성)·**offset**(얼마나 앞서). degree↑는 대체로 성능↑(matrix-mult sub-µs 64→76%). 단 best config은 **workload 의존**, offset 64는 너무 멀리 fetch해 악화.
- **§4.4 Flash tech & parallelism**: **cache가 있으면 ULL(3µs) vs SLC(25µs) 차이 미미**(cache 없을 때만 ULL 압도적). MLC/TLC는 성능·수명 둘 다 크게 악화(1GiB cache로는 1년도 못 감). ULL/SLC + cache면 수명 4년↑. parallelism은 cache가 크면 8×4로 줄여도 무방, cache가 작으면 중요.

**§4.5 요약**: ① caching만으론 불충분, traffic 감축(MSHR) 필요. ② prefetching은 도움되나 best config/알고리즘이 workload 의존. ③ ULL vs SLC는 marginal, MLC/TLC는 어려움.

### §5 Evaluation of Policies (p.9-13)
Real workload 5종(Table 5): **BERT**(NLP), **PageRank**(graph), **Radiosity**(HPC), **XZ**(SPEC 압축), **YCSB**(KVS/Redis). footprint·spatial/temporal locality·read-write 비율 상이. cache는 일부러 작게(**64MiB**) 잡아 큰 workload로 스케일업 가정, ULL flash, 8×8 parallelism.

- **§5.1 Cache replacement**: FIFO/Random/LRU/**CFLRU**(clean을 modified보다 먼저 evict). associativity↑ → hit rate↑ → 성능↑(miss penalty가 커서 hit time보다 hit rate가 중요). **CFLRU가 최고**(write traffic 감소, 수명↑). read-dominant > write-heavy. 고locality(Radiosity)는 정책에 둔감.
- **§5.2 Prefetching policy**: NP/NL(Next-N-line)/FD(feedback-directed)/BO(best-offset)/LP(Leap). **7개 Observation**:
  - **#1** 68–91%가 sub-µs지만 prefetcher가 real workload엔 **해로울 수 있음**(BERT/XZ/YCSB 악화, Radiosity는 +36%).
  - **#2** 고강도에서도 CXL-flash 수명 **≥3.1년**(worst=PageRank; Radiosity는 최대 403년).
  - **#3** DRAM-only 대비 **성능/비용 우수** — DRAM이 NAND보다 17–100× 비싸 **11–91× perf-per-cost** 이득(Figure 14).
  - **#4** prefetcher가 도움될 땐 **높은 accuracy** 때문(Leap: Radiosity 85% vs BERT 27%).
  - **#5** accuracy 낮을 때 성능 저하의 주범은 **cache pollution**(BERT/YCSB: 낮은 accuracy + 높은 pollution).
  - **#6** **V2P 변환이 prefetch를 어렵게** 함(BO on PageRank: virtual 99% → physical 42% accuracy; BERT coverage 76→26%).
  - **#7** **kernel이 access-pattern hint를 주면** hit-under-miss를 cache hit으로 전환해 성능↑(BERT sub-µs 86→91%). 자주 접근되는 top physical frame 정보를 clairvoyant kernel이 사전 전달.

> "If the kernel were to provide memory access pattern hints to the device, the CXL-flash performance improves by converting hit-under-misses into cache hits." (§5, Observation #7)

### §6-7 Related Work & Conclusion (p.13)
Pond[50](CXL DRAM pooling, 클라우드), DirectCXL[36](real HW로 외부 DRAM을 CXL로), **CXL-SSD[42] = Hello Bytes, Bye Blocks**(Jung, HotStorage'22 — CXL+SSD 결합 옹호, 주로 인터커넥트·확장성 논의). FlashMap[40]·**FlatFlash[23]**는 SSD 주소변환을 page table에 통합. 이 논문의 차별점: 앞 연구들이 OS-level 관리·host-device 상호작용에 집중한 반면 **memory 확장 장치 내부(device-internal)의 설계 결정**을 판다. Conclusion: 68–91% sub-µs·≥3.1년, V2P가 prefetch를 무력화 → **kernel-level access-pattern hint** 제안. 한계: flash 내부 task(GC·wear leveling) 미고려, host system이 CXL 신특성을 다 반영 못함 → "future research가 build upon할 platform".

---

## Key vocabulary
**Thesis / framing:**
- "feasibility of using flash memory as a CXL memory expansion device"
- "CXL-flash" (CXL Type-3 flash-backed memory device)
- "overcoming the memory wall in an application-transparent manner"

**Technical concepts:**
- "granularity mismatch" (64B access vs KB flash page)
- "repeated reads" / "MSHR (miss status holding register)"
- "the virtual-to-physical address translation obfuscates access patterns"
- "host-generated access pattern hints" (kernel → device co-design)
- "CFLRU" (clean-first replacement for flash)

**Value language:**
- "cost-effective memory expansion option"
- "performance-per-cost benefit"
- "near-DRAM performance"

> ⚠ **피해야 할 어휘** (이 논문 signature, echo 주의):
> - "CXL-flash" (이 논문 고유 명명 — 인용 시 출처 명시)
> - "the address translation obfuscates access patterns" (Obs#6의 시그니처 문장)

---

## Citable quantitative data
| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| §1/Fig.1, p.1-2 | NLP 모델 14.1×/yr vs GPU mem 1.3×/yr | memory wall 정량 근거 |
| §1, p.2 | 64B flush → 16KiB read + 64B update + 16KiB program | flash granularity mismatch/WA |
| Table 1, p.4 | DRAM 46ns vs SLC read 25µs / ULL 3µs | flash-vs-DRAM latency gap |
| §4.2, p.7 | flash read의 90%↑가 repeated reads | MSHR 필요성 |
| Abstract/§5, p.1 | 68–91% sub-µs, 수명 ≥3.1년 | CXL-flash feasibility headline |
| §5 Obs#3, p.11 | DRAM이 NAND보다 17–100× 비쌈 → 11–91× perf/cost | cost-effectiveness |
| §5 Obs#7, p.12 | kernel hint로 BERT sub-µs 86→91% | OS co-design 효과 |

---

## 🎯 Strategic anchor
> "we analyze the limitations of the current prefetchers and suggest system-level changes for future CXL-flash to achieve near-DRAM performance ... we explore passing memory access hints from the kernel to the CXL-flash to further improve performance." (§1, §5 Obs#7)

→ **본인 활용**: 이 문장이 **"hardware caching/prefetching만으론 flash-CXL memory가 DRAM-급에 못 간다 → OS가 개입하는 co-design이 필요하다"**를 저자 스스로 인정하는 지점이다. 면담·자소서에서 "[[SkyByte]]가 왜 OS+HW co-design으로 갔는가?"를 설명할 때, "그 필요성을 ATC'23 CXL-flash 논문이 데이터(Obs#6 V2P 방해, Obs#7 hint 효과)로 먼저 세팅했다"고 인용 가능. 내 "transparent co-design" 축의 실증적 뿌리.

---

## Connection to my research direction
| 차원 | 이 논문 (ATC'23) | SkyByte (HPCA'25) | 내 방향 |
|---|---|---|---|
| 성격 | **특성화**(characterization, 시뮬레이션+real trace) | **설계**(co-designed architecture) | multi-node coherence 설계 |
| 지연 은닉 | HW caching/prefetching/MSHR | +coordinated context switch, write log(HW/OS) | cross-host scheduling |
| OS 협력 | kernel **hint** 제안(방향만) | OS+HW **co-design** 구현 | OS-transparent multi-host |
| 인터페이스 | CXL Type-3 HDM (memory-semantic) | pure memory-semantic CSSD | 공유 memory pool |
| 공통 한계 | **single-host** | **single-host** | 이걸 넘는 게 목표 |

이 논문은 CXL-flash를 **한 host가 보는 memory 확장 장치**로 특성화한다. prefetch·cache·MSHR·kernel-hint 모두 단일 host address space·단일 page table·단일 V2P 관계 위에서 성립한다. **여러 host가 같은 CXL-SSD를 공유**하면 이 계산이 다 바뀐다 — cross-host 간섭이 inter-arrival time과 cache pollution을 재정의하고, 한 host의 promotion/캐싱이 다른 host에게 stale를 만들어 **coherence(HDM-DB/back-invalidate)**가 필요해진다. 즉 이 논문의 "무엇이 필요한가" 특성화를 **multi-host로 재수행**하는 게 내 gap이다. (→ [[SkyByte]], [[CXL Multi-node Coherence]])

---

## Open questions / gaps
- [ ] flash 내부 task(**GC·wear leveling**)를 시뮬레이터가 미고려 — 실제 CXL-flash에선 GC가 tail latency·coherence에 개입.
- [ ] 전부 **single-host** 전제 — 여러 host가 공유하면 inter-arrival time·pollution·prefetch accuracy 계산이 붕괴.
- [ ] kernel hint(Obs#7)는 **clairvoyant kernel** 가정 — 실제 profiling 오버헤드·multi-host 시 누가 hint를 소유하나 미정의.
- [ ] **hardware 시뮬레이션**이지 real CXL-flash 프로토타입이 아님 — CXL.mem 실측 특성(back-pressure, retry) 미반영.
- [ ] MSHR·CFLRU가 cross-host write ordering/coherence와 어떻게 상호작용하는지 미정의.

---

## References worth following up
| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☑ | [23] | Abulila et al., **FlatFlash** (ASPLOS'19) | SSD 주소변환+page table 통합의 원류. 이미 정독(같은 refs 폴더) |
| ☑ | [42] | Jung, **Hello Bytes, Bye Blocks / CXL-SSD** (HotStorage'22) | CXL+SSD 결합 vision. 같은 refs 폴더 |
| ☑ | [36] | Gouk et al., **DirectCXL** (ATC'22) | real HW CXL DRAM disaggregation. 같은 refs 폴더 |
| ☐ | [50] | Li et al., **Pond** (ASPLOS'23) | CXL DRAM pooling + ML 관리 — 클라우드 memory pool 관점 |
| ☐ | [40] | Huang et al., **FlashMap** (ISCA'15) | FTL+page table 통합 주소변환의 공통 토대 |
| ☐ | [53] | Al Maruf & Chowdhury, **Leap** (ATC'20) | remote memory prefetch — LP prefetcher 근거 |
| ☐ | [68] | Tavakkol et al., **MQSim** (FAST'18) | CXL-flash 시뮬레이터의 base |

---

## Personal annotations
<!-- 본인 메모 영역 -->
