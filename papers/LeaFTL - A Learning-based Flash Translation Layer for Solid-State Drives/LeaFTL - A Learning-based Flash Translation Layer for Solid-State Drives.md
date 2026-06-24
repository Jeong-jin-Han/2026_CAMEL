---
title: "LeaFTL: A Learning-based Flash Translation Layer for Solid-State Drives"
aliases: [LeaFTL]
description: "Piecewise linear regression으로 LPA-PPA 매핑을 학습해 FTL 주소 매핑 테이블 메모리를 줄이는 learning-based FTL."
venue: ASPLOS
year: 2023
arxiv: "2301.00072"
tier: deep
status: done
tags: [paper, cluster/reliability, topic/ftl, topic/learned-index, topic/machine-learning, venue/asplos, year/2023]
---

# LeaFTL: A Learning-based Flash Translation Layer for Solid-State Drives

> **ASPLOS 2023** · cluster/reliability · Source: [LeaFTL - A Learning-based Flash Translation Layer for Solid-State Drives.pdf](<LeaFTL - A Learning-based Flash Translation Layer for Solid-State Drives.pdf>)

**저자:** Jinghan Sun (UIUC), Shaobo Li (UIUC), Yunxin Sun (ETH Zurich), Chao Sun (Western Digital Research), Dejan Vucinic (Western Digital Research), Jian Huang (UIUC)

---

## TL;DR

LeaFTL은 SSD 컨트롤러의 FTL에서 logical page address(LPA)와 physical page address(PPA) 사이의 매핑을 **piecewise linear regression(PLR)** 으로 런타임에 학습하여, 여러 매핑 엔트리를 하나의 *learned index segment* (8 bytes)로 압축한다. 이를 통해 page-level mapping 대비 address mapping table 메모리를 평균 **2.9배** 줄이고, 절약된 in-device DRAM을 데이터 캐싱에 활용하여 storage 성능을 평균 **1.4배** 향상시킨다. 학습된 segment가 부정확한 PPA를 반환할 수 있으므로(*address misprediction*), out-of-band(OOB) metadata에 저장된 reverse mapping을 이용해 1번의 추가 flash access만으로 오류를 검증/보정한다.

---

## 문제 & 동기

- 현대 SSD는 빠른 flash page lookup과 적은 GC overhead 때문에 **page-level mapping**을 주로 사용한다. 그러나 page-level mapping table은 모든 flash page마다 LPA-PPA 엔트리(보통 8 bytes: LPA 4B + PPA 4B)를 저장하므로 메모리 footprint가 가장 크다.
- SSD 컨트롤러의 **in-device DRAM**은 cost와 power budget 제약으로 용량을 키우기 어렵다. flash 용량이 커지고 TLC/QLC로 cell당 용량이 늘어날수록 더 큰 매핑 테이블이 필요해 이 문제가 악화된다. 그래서 컨트롤러는 자주 접근하는 metadata/data만 DRAM에 on-demand caching한다.
- 기존 매핑 최적화 기법들(DFTL, SFTL 등)은 대부분 **human-driven heuristics**에 기반하여, 런타임의 dynamic data access pattern을 포착하지 못한다. GraphSSD처럼 host의 application semantics를 FTL에 넣는 방식은 application-specific이고 generic SSD 개발에 맞지 않는다.
- 목표: application semantics에 의존하지 않고, **간단하지만 효과적인 ML 기법**으로 다양한/동적 access pattern을 자동으로 학습하여 매핑 테이블을 압축하는 generic FTL을 만드는 것.

> [!quote]- 📄 원문 표현 (paper)
> "Among these data structures, the address mapping table has the largest memory footprint." (p.1)
> "due to the limitations of the cost and power budget in SSD controllers, it is challenging for SSD vendors to scale the in-device DRAM capacity. This challenge becomes even worse with the increasing flash memory capacity in an SSD" (p.1)
> "most of them were developed from human-driven heuristics, and cannot capture dynamic data access patterns at runtime." (p.1)
> "we focus on utilizing simple yet effective machine learning (ML) techniques to automate the address mapping table management in the SSDs, with the capability of learning diverse and dynamic data access patterns." (p.1)

---

## 핵심 통찰

- **LPA-PPA 매핑은 선형 패턴을 가진다.** Regular/strided/irregular access pattern은 PLR로 학습 가능한 선형 segment로 표현된다. 각 segment는 `(S, L, K, I)`로 인코딩되는데, `[S, S+L]`은 LPA interval, `K`는 slope, `I`는 intercept이며 `PPA = ⌈K * LPA + I⌉`로 계산된다.
- 최적화 후 각 segment는 **8 bytes**(S와 L 각 1B, K 2B, I 4B)로 표현 가능하고, 한 segment가 평균 약 **20.3개**의 LPA-PPA 매핑을 담는다(여러 워크로드 분석 결과). 따라서 page-level mapping 대비 `m * avg(L) / 8` 배의 압축이 가능하다.
- **두 종류의 segment**: *accurate segment*(정확한 PPA 반환)와 *approximate segment*(error bound `[-γ, γ]` 내의 근사 PPA 반환). γ를 키우면 더 많은 패턴을 더 적은 segment로 커버하지만 misprediction 가능성이 커진다.
- **OOB metadata 활용**: 모든 flash page는 64~256 bytes의 OOB 영역을 가지며, LeaFTL은 여기에 이웃 PPA들의 reverse mapping(PPA→LPA)을 저장한다. misprediction 시 예측된 PPA의 flash page를 읽어 OOB로 검증하므로, error tolerance를 **1번의 추가 flash read**로 보장한다.
- **flash block allocation과의 협조**: contiguous LPA에 consecutive PPA를 best-effort로 할당하면 더 space-efficient한 segment 학습이 가능해진다.

> [!quote]- 📄 원문 표현 (paper)
> "each learned index segment can be represented in 8 bytes: 1 byte for SLPA and L, respectively; 2 bytes for K, and 4 bytes for I." (p.4)
> "avg(L) is the average number of LPA-PPA mappings that can be represented in a learned index segment, avg(L) is 20.3 according to our study of various storage workloads." (p.2)
> "Since the OOB usually has 64–256 bytes, we use it to store the accurate LPAs mapped to the neighbor PPAs. Thus, upon an address misprediction, we use the stored reverse mappings to find the correct PPA, avoiding additional flash accesses." (p.2)

---

## 설계 / 메커니즘

**Learned Index Segment (§3.2)**
- LPA를 256개씩 group으로 partition하여, `S_LPA`를 group 내 offset(1 byte, 2^8=256)으로 인덱싱한다. group size 256은 학습된 segment 길이가 보통 256 미만이기 때문.
- accurate segment는 `f(LPA) = ⌈K*LPA + I⌉`로 정확한 PPA를, approximate segment는 같은 식이지만 `[-γ, γ]` error bound 내의 근사값을 반환한다. `K`의 least significant bit으로 segment type(0=accurate, 1=approximate)을 표시(K∈[0,1]이라 영향 미미).

**Learning Efficiency 향상 (§3.3)**
- SSD 컨트롤러의 **data buffer**(기본 8MB)에 버퍼된 write들을 LPA 오름차순으로 **정렬(sorting)** 한 뒤 PLR로 학습한다. 정렬하면 PPA도 오름차순이 되어 monotonic 매핑이 만들어지고 segment 수가 줄어든다(예시: 5개 → 3개 segment). 데이터 수집 overhead 없음.

**Log-Structured Mapping Table 관리 (§3.4)**
- SSD의 out-of-place write 특성 때문에 learned segment가 새 write/GC로 disrupted된다. in-place update는 비싸므로(relearning 시 평균 21 flash access, 1.2배 추가 segment) **log-structured manner**로 관리한다.
- 최상위 level(level 0)에 가장 최신 segment를, 하위 level에 오래된 segment를 둔다. 같은 level 내 segment는 sorted/non-overlapping(빠른 binary search), level 간에는 overlap 허용. 최신 매핑은 항상 top level에서 찾는다.

**Conflict Resolution Buffer (CRB, §3.4)**
- approximate segment들이 overlapping LPA를 가지면 잘못된 PPA를 얻을 수 있으므로, 각 LPA group마다 CRB(거의 정렬된 list, null byte로 segment 구분)를 두어 어떤 LPA가 어떤 approximate segment에 속하는지 확인한다. CRB는 평균 **13.9 bytes**만 차지(Figure 10), 최대 256 LPA 저장.

**Address Misprediction 처리 (§3.5)**
- approximate segment로 예측한 `PPA_learned`의 flash page를 읽고 그 OOB의 reverse mapping으로 LPA 일치 여부 확인. 정확한 PPA는 `[PPA_learned - γ, PPA_learned + γ]` 범위 내에 있음이 보장되므로, 이웃 `2γ+1`개 PPA의 reverse mapping을 OOB에 저장해두면 **단 1번의 추가 flash read**로 보정 가능.

**Core FTL 기능 보존 (§3.6)**: GC는 greedy algorithm으로 valid page 최소인 block 선택, valid page를 DRAM buffer로 옮기고 LPA 정렬 후 재학습. wear leveling은 기존 throttling/swapping 메커니즘 사용, swap된 block을 재학습.

**LeaFTL Operations (§3.7, Algorithm 1 & 2)**: segment creation(data buffer가 차면 정렬 후 greedy PLR), insert/update(top level에 추가, binary search로 위치 결정, CRB 갱신, overlapping victim segment 탐지 후 `seg_merge`), LPA lookup(top level부터 binary search, `has_lpa` 확인, approximate면 CRB 확인), compaction(level 간 overlapping segment 병합, 기본 **100만 write마다**, 평균 4.1ms 소요).

**Put It All Together (§3.8)**: AMC를 log-structured mapping table로 대체하고 CRB를 추가. battery-backed DRAM이 있으면 매핑 테이블을 flush, 없으면 주기적으로 BVC와 함께 flush 후 crash 시 BVC 비교로 재구성.

**Implementation (§3.9)**: trace-driven simulator **WiscSim** 확장(LRU read-write cache 추가) + 1TB **open-channel SSD prototype**(16 channels, 16K flash blocks/channel, 256 pages/block, 16KB flash page, C로 4,016 lines).

> [!quote]- 📄 원문 표현 (paper)
> "In LeaFTL, we set the L as 1 byte. Thus, each segment can index 256 LPA-PPA mappings. We used a 16-bit floating point to store the value of the slope K. And the intercept I of a segment can be represented in 4 bytes." (p.4)
> "the newly learned index segments will be appended to the log structure (level 0 in Figure 8) and used to index the updated LPA-PPA mappings, while the existing learned segments ... can still serve address translations for LPAs whose mappings have not been updated." (p.5)
> "LeaFTL ensures that it will incur only one extra flash access for address mispredictions." (p.6)
> "LeaFTL performs segment compaction after each 1 million writes by default ... the segment compaction of the entire mapping table will take 4.1 milliseconds ... the compaction incurs trivial performance overhead to storage operations." (p.8)

---

## 평가

**Setup**: 2TB SSD, 4KB flash page, 1GB DRAM, 16 channels, 256 pages/block, read 20μs / write 200μs / erase 1.5ms, OP ratio 20%(Table 1). 워크로드: MSR Cambridge enterprise traces, FIU traces, 그리고 real SSD에서 FileBench(OLTP, CompFlow), BenchBase(TPCC, AuctionMark, SEATS, DB 10~30GB). 비교 대상: **DFTL**(demand-based), **SFTL**(spatial-locality-aware).

- **메모리 절감 (Figure 15)**: page-level DFTL 대비 매핑 테이블 크기를 **7.5~37.7배** 축소. SFTL 대비 최대 **5.3배**(γ=0일 때 평균 2.9배) 축소.
- **성능 (Figure 16)**: DRAM을 주로 매핑에 쓰는 설정에서 SFTL 대비 latency 평균 **1.6배**(최대 2.7배) 감소. DRAM의 최대 80%만 매핑에 쓰는 설정에서 SFTL 대비 **1.4배**(최대 3.4배), DFTL 대비 **1.6배**(최대 4.9배) 향상.
- **Real SSD (Figure 17)**: 전 워크로드에서 평균 **1.4배**(최대 1.5배) 성능 향상. tail latency 증가 없음(Figure 18).
- **γ sensitivity (Figure 19~21)**: γ=16에서 γ=0 대비 매핑 테이블 평균 1.3배(real SSD 1.2배) 추가 축소, 성능 1.3배 향상. γ=16일 때 학습된 segment의 평균 **26.5%** 만 approximate(Figure 20).
- **Overhead (§4.5, Table 3)**: ARM Cortex-A72에서 256 매핑 학습에 9.8~10.8μs(flash write latency의 0.02%), LPA lookup당 40.2~67.5ns. lookup overhead는 flash read의 평균 0.21%. 90%의 lookup이 top level, 99%가 10 level 내(Figure 23). misprediction ratio는 γ=16에서 대부분 <10%(Figure 24).
- **SSD Lifetime (§4.6, Figure 25)**: write amplification factor(WAF)가 DFTL/SFTL과 comparable. DFTL이 대부분 워크로드에서 WAF가 더 큼. LeaFTL의 flush cost는 negligible.
- **Recovery**: TPCC 워크로드 reboot 시 평균 **15.8분**(기존 page-level과 유사), recently learned segment 재학습은 평균 101.3ms.

> [!quote]- 📄 원문 표현 (paper)
> "LeaFTL saves the memory consumption of the mapping table by 2.9× on average and improves the storage performance by 1.4× on average, in comparison with state-of-the-art FTL schemes." (p.1)
> "LeaFTL reduces the mapping table size by 7.5–37.7×, compared to the page-level mapping scheme DFTL ... In comparison with SFTL, LeaFTL achieves up to 5.3× (2.9× on average) reduction" (p.11)
> "The learning time for a batch of 256 mapping entries is 9.8–10.8 μs ... the learning overhead is only 0.02% of their flash write latency." (p.12)
> "the WAF of LeaFTL is comparable to DFTL and SFTL. DFTL has larger WAF in most workloads." (p.12)

---

## 섹션 노트

- **§1 Introduction**: page/block/hybrid mapping 비교, page-level의 메모리 문제, GraphSSD 같은 semantic 접근의 한계, LeaFTL의 4가지 contribution(learning-based FTL, error-tolerant translation, core FTL 기능 보존+협조, log-structured segment 관리).
- **§2 Background**: SSD 구조(Figure 2), FTL의 4가지 metadata 구조(Figure 3 — Address Mapping Cache/AMC, Global Mapping Directory/GMD, Block Validity Counter/BVC, Page Validity Table/PVT), out-of-place write와 GC, limited DRAM 문제.
- **§3 Design**: 4가지 research challenge(diverse pattern 학습, misprediction tolerance, core FTL 협조, lightweight) 제시 후 §3.1~3.9에서 해결. Figure 1(PLR 예시), Figure 6(accurate vs approximate 인코딩), Figure 8(log-structured table), Figure 9(CRB), Figure 11(OOB), Figure 13(operation 예시), Figure 14(key data structure).
- **§4 Evaluation**: 6가지 결과(메모리/성능, real SSD 검증, γ sensitivity, DRAM/page size sensitivity, overhead, lifetime).
- **§5 Discussion**: 왜 linear regression인가(deep NN과 달리 lightweight, guaranteed error bound, well-studied), adaptivity(TLC/QLC와 무관), recovery.
- **§6 Related Work**: address translation(FlashMap, Nameless Writes 등), ML for storage(learned index, cache replacement), SSD hardware(near-data computing accelerator). LeaFTL은 이들과 orthogonal.

---

## 핵심 용어

- **FTL (Flash Translation Layer)**: LPA를 PPA로 변환하고 GC/wear-leveling을 담당하는 SSD firmware 핵심 컴포넌트.
- **LPA / PPA**: Logical/Physical Page Address. host가 보는 주소 vs flash의 실제 위치.
- **Piecewise Linear Regression (PLR)**: LPA-PPA 매핑을 여러 선형 구간으로 근사하는 학습 기법. LeaFTL의 핵심.
- **Learned Index Segment**: `(S, L, K, I)`로 인코딩된 8-byte 선형 매핑 단위.
- **Accurate / Approximate segment**: 정확 PPA 반환 / `[-γ, γ]` error bound 내 근사 PPA 반환.
- **γ (gamma)**: 선형 회귀의 error bound 파라미터. 크게 할수록 압축↑, misprediction↑.
- **Address Misprediction**: approximate segment가 부정확한 PPA를 반환하는 현상.
- **OOB (Out-of-Band) metadata**: flash page당 64~256B 영역, LeaFTL은 reverse mapping 저장에 사용.
- **CRB (Conflict Resolution Buffer)**: LPA group별 거의 정렬된 list, overlapping approximate segment 충돌 해결.
- **Log-Structured Mapping Table**: 최신 segment를 top level에 append하는 multi-level 구조.
- **WAF (Write Amplification Factor)**: 실제 flash write / 요청된 write 비율. SSD lifetime 지표.

---

## 강점 · 한계 · 열린 질문

**강점**
- ML을 SSD의 critical system problem(address mapping)에 적용한 최초 연구. lightweight linear regression이라 ARM core에서 μs 단위로 학습.
- OOB reverse mapping을 활용한 error tolerance로 misprediction을 1 flash read로 한정 — 실용적.
- application semantics에 의존하지 않는 generic 접근, 기존 FTL/GC/wear-leveling과 호환.
- simulator + real open-channel SSD prototype 양쪽 검증으로 신뢰성 확보.

**한계**
- approximate segment 사용 시 misprediction에 의한 추가 flash read가 발생(γ가 클수록 빈번). 워크로드에 따라 최대 ~15% misprediction(CompF).
- log-structured table의 level 깊이가 깊어지면(MSR-prn은 99%가 10 level 내지만 일부 워크로드는 더 많음) lookup이 느려질 수 있어 주기적 compaction 필요.
- linear pattern이 적은 완전 random workload는 single-point segment(L=0)로 떨어져 page-level mapping 수준의 메모리만 절약(이상은 안 됨).
- battery-backed DRAM이 없으면 crash recovery 시 flash block scan 필요(평균 15.8분).

**열린 질문**
- deep learning 등 더 강력한 모델을 near-data computing accelerator로 SSD 내부에 배치하면 더 복잡한 패턴도 압축 가능할까? (저자가 future work로 언급)
- γ를 워크로드별로 동적 조정하는 adaptive 메커니즘이 가능한가?
- learned segment를 host로 offloading하는 FlashMap/Nameless Writes 계열과 결합하면 추가 이득이 있을까?

---

## ❓ Q&A (자가 점검)

> [!question]- Q1. LeaFTL이 해결하려는 핵심 문제는 무엇인가?
> SSD의 page-level address mapping table은 메모리 footprint가 가장 큰데, in-device DRAM은 cost/power 제약으로 확장이 어렵고 flash 용량 증가로 문제가 악화된다. 기존 최적화는 human-driven heuristic이라 동적 access pattern을 못 잡는다. LeaFTL은 ML로 LPA-PPA 매핑을 학습/압축하여 매핑 테이블 메모리를 줄이고 절약된 DRAM을 데이터 캐싱에 쓴다.

> [!question]- Q2. learned index segment는 어떻게 인코딩되며 몇 byte인가?
> `(S_LPA, L, K, I)`로 표현된다. S_LPA(group 내 offset) 1B, L(길이) 1B, K(slope, 16-bit float) 2B, I(intercept) 4B로 총 **8 bytes**이며 memory-aligned이다. 한 segment가 최대 256개(평균 20.3개) LPA-PPA 매핑을 인덱싱한다.

> [!question]- Q3. accurate segment와 approximate segment의 차이는?
> 둘 다 `f(LPA)=⌈K*LPA+I⌉`로 PPA를 계산하지만, accurate는 정확한 PPA를 반환하고 approximate는 error bound `[-γ, γ]` 내의 근사 PPA를 반환한다. approximate는 더 많은 irregular 패턴을 적은 segment로 커버하지만 misprediction을 일으킬 수 있다. K의 least significant bit으로 type을 구분한다.

> [!question]- Q4. address misprediction을 어떻게 1번의 추가 flash access로 보정하는가?
> 정확한 PPA는 예측값 기준 `[PPA_learned - γ, PPA_learned + γ]` 범위에 있음이 보장된다. 각 flash page의 OOB에 이웃 `2γ+1`개 PPA의 reverse mapping(PPA→LPA)을 저장해두므로, 예측 PPA의 flash page를 한 번 읽으면 그 OOB로 올바른 LPA에 해당하는 PPA를 찾아 보정할 수 있다.

> [!question]- Q5. 왜 매핑 테이블을 log-structured 방식으로 관리하는가?
> SSD의 out-of-place write로 learned segment가 새 write/GC에 의해 disrupted되는데, in-place update(relearning)는 평균 21 flash access로 비싸고 1.2배 추가 segment를 만든다. 대신 최신 segment를 top level에 append하면 기존 segment는 그대로 두고 최신 매핑만 top에서 찾을 수 있어 다른 segment 영향을 최소화한다.

> [!question]- Q6. CRB(Conflict Resolution Buffer)는 왜 필요한가?
> approximate segment들이 LPA 범위가 overlap하면 한 LPA에 대해 잘못된 segment에서 PPA를 얻을 수 있다. CRB는 LPA group별로 어떤 LPA가 어떤 approximate segment에 속하는지(starting LPA로 거의 정렬, null byte 구분) 저장하여, lookup 시 binary search로 올바른 segment를 식별하게 한다. 평균 13.9 bytes만 차지한다.

> [!question]- Q7. LeaFTL이 가져온 정량적 이득은?
> page-level DFTL 대비 매핑 테이블 7.5~37.7배 축소, SFTL 대비 평균 2.9배(최대 5.3배) 축소. 절약된 DRAM 캐싱으로 storage 성능 평균 1.4배(real SSD) 향상. 학습 overhead는 flash write latency의 0.02%, lookup overhead는 flash read의 0.21%.

> [!question]- Q8. 왜 deep neural network 대신 linear regression을 썼는가?
> linear regression은 simple/lightweight하여 SSD 컨트롤러의 ARM core에서 몇 μs 만에 segment를 학습할 수 있고, guaranteed error bound를 제공하며 well-studied되어 있다. deep NN은 SSD 컨트롤러의 제한된 compute budget에 맞지 않는다. LeaFTL은 ML로 address mapping을 푼 최초 연구다.

---

## 🔗 Connections

[[Reliability]] · [[ASPLOS]] · [[2023]]

- **Learned Index**: Tim Kraska et al.의 "The Case for Learned Index Structures"(SIGMOD'18)에서 출발한 learned index 계열을 SSD FTL에 적용. PLR 기반 streaming 알고리즘은 Xie et al.(VLDB'14)을 차용.
- **FTL 매핑 최적화**: DFTL(ASPLOS'09), SFTL(MSST'11) 대비 평가. FlashMap(ISCA'15), Nameless Writes(FAST'12) 같은 host-offloading 계열과 orthogonal.
- **In-storage ML / near-data computing**: IceClave(MICRO'21), DeepStore(MICRO'19) 등 SSD 내 accelerator 연구가 future work로 연결.

---

## References worth following

- Gupta et al., **DFTL**: A Flash Translation Layer Employing Demand-based Selective Caching of Page-level Address Mappings, ASPLOS'09 [20] — 비교 baseline, page-level demand caching.
- Jiang et al., **SFTL**: An Efficient Address Translation for Flash Memory by Exploiting Spatial Locality, MSST'11 [25] — spatial-locality 기반 비교 baseline.
- Kraska et al., The Case for **Learned Index** Structures, SIGMOD'18 [33] — learned index의 출발점.
- Xie et al., Maximum Error-Bounded Piecewise Linear Representation for Online Stream Approximation, VLDB'14 [64, 65] — LeaFTL이 쓰는 PLR 알고리즘.
- Huang et al., **FlashMap**: Unified Address Translation for Memory-mapped SSDs, ISCA'15 [23] — host-side mapping offloading.
- Zhang et al., De-indirection for Flash-based SSDs with **Nameless Writes**, FAST'12 [66] — host로 매핑 offloading.

---

## Personal annotations

<본인 메모 영역>
