---
title: "Five-Minute Rule 40 Years Later: A First-Principles Revisit for Modern Memory Hierarchy"
description: "Jim Gray의 five-minute rule을 host cost·DRAM bandwidth/capacity·device-physics 기반 SSD IOPS 모델로 재정식화하여, GPU-centric host + Storage-Next SSD 환경에서 DRAM↔flash caching break-even interval이 분→초 단위로 붕괴함을 보이는 논문"
venue: arXiv
year: 2026
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - cluster/infra
  - venue/arxiv
  - year/2026
  - list/26s-v2
  - topic/memory-hierarchy
  - topic/ssd-nand-flash
  - topic/dram-caching-economics
  - topic/kv-store-ann-search
---

# Five-Minute Rule 40 Years Later: A First-Principles Revisit for Modern Memory Hierarchy

> **arXiv 2026** · cluster/infra · Source: [Five-Minute Rule 40 Years Later - A First-Principles Revisit for Modern Memory Hierarchy.pdf](<Five-Minute Rule 40 Years Later - A First-Principles Revisit for Modern Memory Hierarchy.pdf>)

저자: Tong Zhang (ScaleFlux), Vikram Sharma Mailthody (NVIDIA), Fei Sun (ScaleFlux), Linsen Ma (ScaleFlux), Chris J. Newburn (NVIDIA), Teresa Zhang (Stanford University), Yang Liu (ScaleFlux), Jiangpeng Li (ScaleFlux), Hao Zhong (ScaleFlux), Wen-mei Hwu (NVIDIA)

## TL;DR
1987년 Jim Gray의 five-minute rule("얼마나 자주 접근되는 페이지를 DRAM에 캐싱해야 이득인가")을 host I/O cost, DRAM bandwidth/capacity, NAND device-physics 기반 SSD 성능·비용 모델까지 포함한 first-principles 프레임워크로 재정식화한다. 여기에 host IOPS 용량과 tail-latency SLO라는 feasibility 제약을 추가하고, workload access-interval 분포를 결합한 viability/economics-optimal 분석까지 쌓아 올린다. 결론적으로 GPU-centric host를 NVIDIA "Storage-Next" 급 초고IOPS SSD(fine-grained random access에 최적화)와 짝지으면 DRAM↔flash caching break-even interval이 기존의 분(minute) 단위에서 초(second) 단위로 붕괴함을 보인다. 이를 검증하기 위해 MQSim 기반의 calibrated 시뮬레이터 MQSim-Next를 구축했고, SSD-resident KV store(blocked Cuckoo hashing)와 SSD-resident ANN search(two-stage progressive HNSW)라는 두 case study로 flash를 "passive storage"에서 "active memory tier"로 재정의할 수 있는 소프트웨어 설계 공간을 제시한다.

## 문제 & 동기
Gray와 Putzolu의 1987년 five-minute rule은 "1KB 페이지가 5분보다 자주 접근되면 DRAM에 상주시켜야 한다"는 economics-only 휴리스틱이었고, 이후 1997/2007/2019년 개정판들도 여전히 break-even을 minute-scale에 두었다("Tape is dead, disk is tape, flash is disk", p.1). 그러나 이 경제학 전용 관점은 host-side I/O cost, feasibility limit(processor IOPS, latency target), workload 접근 패턴을 무시한다(p.1, p.2). 한편 2025년 현재 AI 워크로드는 petabyte급 working set과 fine-grained random access를 요구하고, NVIDIA Storage-Next SSD는 "access granularity가 작아질수록 확장되는" 초고 IOPS/cost를 제공한다(p.1: "up to 10× higher IOPS per dollar... e.g., 50M IOPS at 512B vs. 10M IOPS at 4KB"). 이 산업 트렌드를 반영하지 못하는 고전 five-minute rule을 first-principles로 다시 세우는 것이 이 논문의 동기다.

> [!quote]- 📄 원문 표현 (paper)
> - "we show that, for modern AI platforms, especially GPU-centric hosts paired with ultra-high-IOPS SSDs engineered for fine-grained random access, the DRAM↔flash caching threshold collapses from minutes to a few seconds" (p.1)
> - "Later revisits in 1997 [18], 2007 [17], and 2019 [5] updated the rule for advancing technology, with the latest still placing the DRAM-SSD threshold at the minute scale" (p.1)
> - "Optimizing only device prices (e.g., $/GB or $/IOPS) cannot ensure deployability. Real feasibility depends on host-side constraints... Ignoring these factors can yield configurations unable to meet workload demands or service-level objectives." (p.2)

## 핵심 통찰 (Key Insight)
1. **Vendor peak spec 대신 device-physics 기반 SSD IOPS/cost 모델링**: NAND channel timing($\tau_{sense}, \tau_{prog}, \tau_{CMD}$), 컨트롤러 address-translation bandwidth, PCIe packet rate라는 세 가지 병목의 최솟값으로 SSD peak IOPS를 유도한다(Eq. 2, p.4). 이렇게 하면 IOPS/cost가 block size, read:write ratio, ECC, GC write-amplification 같은 아키텍처 파라미터의 함수로 명시적으로 드러나, "datasheet 상수"였던 것이 "설계 변수"가 된다 — 그래서 break-even이 하나의 숫자가 아니라 provisioning 설계 공간이 된다.
2. **Feasibility-aware refinement (host IOPS·tail latency)**: M/D/1 queueing 근사로 채널 utilization $\rho$의 함수인 mean/tail read latency를 유도하고, SLO를 만족하는 최대 $\rho_{max}$로 usable SSD IOPS를 스케일한다(Section IV, p.6-8). 이는 "SSD가 이론상 낼 수 있는 IOPS"와 "실제로 latency budget 안에서 쓸 수 있는 IOPS"를 분리해, 고전 rule이 암묵적으로 가정한 100% peak utilization 가정을 제거한다.
3. **Workload-aware viability/economics-optimal 프레임워크**: access-interval 분포로부터 DRAM-bandwidth 임계값 $T_B$, SSD-bandwidth 임계값 $T_S$, DRAM-capacity 임계값 $T_C$를 정의하고, $\max(T_B,T_S)\le T_C$이면 "viable", break-even이 $[\max(T_B,T_S), T_C]$ 구간에 있으면 "economics-optimal"이라 정의한다(Eq. 5-7, p.9-10). 이는 단순 break-even 계산을 넘어 "이 플랫폼에 DRAM을 얼마나 달아야 하는가"라는 실제 provisioning 질문에 답을 준다.

> [!quote]- 📄 원문 표현 (paper)
> - "This calibrated formulation preserves Gray's intuition: balance the DRAM 'rent' against the cost of serving accesses from storage." (p.3)
> - "the scaling factor $\rho_{max}$ reflects the impact of application-level read latency constraints on the usable SSD IOPS" (p.7)
> - "If $\max(T_B,T_S) \le T_C$, the platform is viable for the workload... The platform operates at the economics-optimal point if $\tau_{break-even} \in [\max(T_B,T_S), T_C]$." (p.9)

## 설계 / 메커니즘 (Design)
- **Calibrated economic break-even (Section III, Fig. 1, Eq. 1, p.3-4)**: host processor cost($\$_{CORE}/IOPS_{CORE}$), host DRAM bandwidth cost, SSD access cost($\$_{SSD}/IOPS_{SSD}$)를 모두 더해 "DRAM에 캐싱해서 절약되는 비용"과 "DRAM rent"가 같아지는 $\tau_{break-even}$을 계산한다.
- **First-principles SSD model (Section III-B, Fig. 2, p.4-5)**: SSD = controller + SSD-internal DRAM(FTL) + $N_{CH}$개 채널 × $N_{NAND}$개 다이. $IOPS^{(peak)}_{SSD} = \min(IOPS^{(peak)}_{dev}, IOPS^{(peak)}_{xlat}, IOPS^{(peak)}_{pcie})$ (Eq. 2, p.4). $IOPS_{dev}$는 NAND sense/program latency와 채널 대역폭으로, $IOPS_{xlat}$는 FTL 매핑 엔트리(8B/entry) 처리에 필요한 컨트롤러 내부 DRAM 대역폭(40GB/s→약 5G IOPS, 실질적으로 non-binding)으로, $IOPS_{pcie}$는 PCIe 링크 대역폭/패킷 처리율로 결정된다(p.4-5).
- **Table I 파라미터**로 SLC/pSLC/TLC 세 NAND 타입을 정량화하고 (SCA 프로토콜로 $\tau_{CMD}\approx150ns$까지 단축), Fig. 3에서 512B 블록의 Storage-Next SSD IOPS가 SLC 기준 약 57M(→4KB에서 약 11M)까지 나옴을 보인다(p.5). Table II는 $N_{CH}, N_{NAND}, \tau_{CMD}$에 대한 민감도(pessimistic 39.5M~optimistic 79.3M @512B)를 제공한다(p.6).
- **Break-even 정량 사례 (Fig. 4, p.6-7)**: CPU+DDR + Normal-SSD 대비 GPU+GDDR + Storage-Next SSD 조합에서 512B SLC 기준 break-even이 ~34s→~5s로 약 7배 단축된다.
- **Feasibility constraint (Section IV, Fig. 5, Table III/IV, p.6-8)**: 정규화 비용 $\alpha_{CTRL}=15, \alpha_{S\_DRAM}=1, \alpha_{H\_DRAM}=1(DDR)/2(GDDR), \alpha_{CORE}=4(CPU)/3(GPU)$, GPU는 4M IOPS/SM(NVIDIA SCADA 기준) vs CPU 1M IOPS/core (Table III, p.6). host IOPS budget이 host-limited 영역과 device-limited 영역을 가르며, CPU+DDR에서 512B 기준 host budget을 40M→100M IOPS로 올리면 break-even이 83s→47s로 줄지만 4KB에서는 여전히 ~10s로 device-limited다(p.8).
- **Workload-aware platform analysis (Section V, Fig. 6, p.9-11)**: CPU+DDR(DDR5-5600, 12채널, 540GB/s)와 GPU+GDDR(GDDR6-20, 8채널, 640GB/s) 플랫폼에 1B 블록·log-normal access-interval 분포(총 처리량 200GB/s) 워크로드를 적용, viable/economics-optimal DRAM capacity를 계산한다. CPU+DDR은 512B/1KB에서 economics-optimal DRAM이 사실상 전체 dataset(512GB/1TB) 규모까지 커지는 반면, GPU+GDDR+Storage-Next는 $T_B, T_S$ 모두 5초 미만으로 작아 훨씬 적은 DRAM으로도 viable/optimal 하다(p.11).
- **MQSim-Next 시뮬레이터 (Section VI, p.11-12)**: MQSim을 확장해 (1) SCA 채널 프로토콜, (2) independent multi-plane reads, (3) explicit transfer-sense overlap을 반영하고, BCH(512B inner)+LDPC(8-sector outer) 2-layer concatenated ECC 모델을 도입해 tunable $p_{BCH}$ 파라미터로 small-read tail latency/증폭 효과를 탐구할 수 있게 했다(p.11-12). Gen7 x8 PCIe 링크 기준으로 검증했다.

> [!quote]- 📄 원문 표현 (paper)
> - "Since Eq. 2 includes costs in both numerator and denominator, we normalize all components to the NAND-die cost for fair comparison." (p.6)
> - "the usable SSD IOPS is $IOPS_{SSD} = \min(\rho_{max}\cdot IOPS^{(peak)}_{SSD}, IOPS^{(peak)}_{proc}/N_{SSD})$" (p.7, 서술적 재현)
> - "MQSim-Next adopts a two-layer concatenated code: a BCH inner code per 512 B sector and an LDPC outer code spanning eight sectors." (p.11)

## 평가 (Evaluation)
- **Analytic vs. simulation 일치 (Fig. 7, p.13)**: 90:10 read:write 기준 modeled와 MQSim-Next IOPS가 잘 정렬(MQSim-Next가 보수적 $\Phi_{WA}=3$ 가정 때문에 약간 더 높게 보고). read-only 82M → 90:10 68M → 70:30 52M → 50:50 34M로 GC write 트래픽 경쟁이 IOPS를 낮춤(Fig. 7b). 채널 대역폭을 3.6GB/s→5.6GB/s로 올리면 512B IOPS가 68M→85M로 증가(Fig. 7c). BCH decode 실패율 $p_{BCH}\le1\%$에서는 error-free 대비 처리량 저하가 완만함(Fig. 7d).
- **Case study 1 — SSD-resident KV store (Section VII-A, Fig. 8, p.13-14)**: 5TB KV store, 800억 개 64B 아이템, load factor 0.7, block size는 device class에 맞춤(Storage-Next 512B / Normal 4KB); GET:PUT 100:0/90:10/70:30/50:50, strong/weak locality(log-normal σ=1.2/0.4) 조합. GPU+Storage-Next(SN) 조합이 read-heavy mix에서 100+ Mops/s를 달성해 FASTER 같은 in-memory KV store와 견줄만한 수준에 도달한 반면, Normal SSD 시스템은 CPU/GPU 무관하게 device/bandwidth-limited 곡선으로 수렴한다(p.14).
- **Case study 2 — SSD-resident ANN search (Section VII-B, Fig. 9/10, p.14-16)**: two-stage progressive 설계(reduced-dim 512B 벡터로 pruning → 소수만 full-dim 벡터로 rerank), MRL 기반 코퍼스(MS MARCO, 20 Newsgroups, DBpedia)에서 recall >98% 유지(p.15). Fig. 10에서 512B→2KB/4KB/6KB/8KB(promotion rate 5%/10%/15%/20%) 전 구간에서 GPU+Storage-Next가 가장 높은 KQPS를 달성하며, Storage-Next SSD가 Normal SSD 대비 일관되게 2–3× throughput 이득을 보인다. 참고로 DiskANN이 billion-scale에서 약 5 KQPS를 내는데, 이 논문의 (illustrative, 비직접비교) GPU+Storage-Next 구성은 "tens of KQPS" 수준까지 밀어올린다(p.15-16, 각주 4에서 직접 비교 아님을 명시).

> [!quote]- 📄 원문 표현 (paper)
> - "GPU+Storage-Next is especially advantageous. On read-heavy mixes, GPU+SN sustains 100+ Mops/s, comparable to in-memory KV stores such as FASTER" (p.14)
> - "Storage-Next SSDs deliver a consistent 2–3× throughput advantage over Normal SSDs" (p.15)
> - "Our results are illustrative and model-based, not a direct performance comparison with DiskANN." (p.15, footnote 4)

## 섹션 노트
- **I. Introduction**: five-minute rule의 역사와 한계, Storage-Next SSD·GPU-centric AI 워크로드라는 새 맥락, 4가지 research question(RQ1-RQ4) 제시.
- **II. Background and Motivation**: 고전 rule의 economics-only 정식화와 두 가지 한계(insufficient realism, missing feasibility)를 formal하게 짚음.
- **III. Calibrated Economic Model (RQ1)**: host cost + first-principles SSD IOPS/cost 모델로 break-even을 재유도, Storage-Next 정량 사례 제시.
- **IV. Constraint-aware Break-even (RQ2)**: M/D/1 큐잉 기반 tail-latency 제약으로 usable IOPS를 스케일.
- **V. Workload-aware Platform Analysis (RQ3)**: access-interval 분포 기반 viability/economics-optimal DRAM capacity 프레임워크.
- **VI. MQSim-Next**: SCA/multi-plane/ECC를 반영한 calibrated SSD 시뮬레이터로 모델 검증 및 민감도 분석.
- **VII. Re-think Data-intensive Software (RQ4)**: SSD-resident KV store, SSD-resident ANN search 두 case study로 seconds-scale caching이 여는 알고리즘/자료구조 설계 공간을 시연.
- **VIII. Limitations and Future Work**: device/cost modeling의 정규화 가정, endurance 미모델링, read-dominant/single-tenant 워크로드 한정, single-node topology 가정(CXL/fabric-attached 확장은 future work) 명시.
- **IX. Conclusion**: five-minute rule을 quantitative, cross-layer한 feasibility-aware provisioning 프레임워크로 재정립.

## 핵심 용어 (Key terms)
- **Five-minute rule**: 데이터의 재접근 간격이 특정 임계값보다 짧으면 DRAM에 상주시키는 것이 economically 유리하다는 Gray & Putzolu(1987)의 경제성 휴리스틱.
- **Break-even interval ($\tau_{break-even}$)**: DRAM에 페이지를 caching하는 "rent" 비용과 매번 storage에서 fetch하는 비용이 같아지는 접근 간격.
- **Storage-Next SSD**: NVIDIA 주도로 fine-grained random access에서 IOPS/$를 극대화하도록 설계된 차세대 SSD(예: 512B에서 50M IOPS급).
- **SCA (Scaled Command Address) protocol**: NAND 채널의 command/address 전송을 짧게 만들어 per-command latency를 ~1.2μs에서 ~100-200ns로 줄이는 프로토콜.
- **FTL (Flash Translation Layer)**: 논리 주소를 물리 NAND 주소로 매핑하는 SSD 컨트롤러 내부 계층; 그 매핑 테이블을 저장하는 SSD-internal DRAM 대역폭이 IOPS 상한($IOPS_{xlat}$)이 될 수 있음.
- **MQSim-Next**: MQSim을 확장해 SCA, independent multi-plane read, transfer-sense overlap, 2-layer BCH+LDPC ECC 모델을 반영한 calibrated multi-queue SSD 시뮬레이터.
- **$\rho_{max}$ (usable channel utilization)**: M/D/1 큐잉 근사에서 mean/tail latency SLO를 만족하는 최대 채널 utilization, usable IOPS를 peak IOPS에서 스케일다운하는 계수.
- **Viable / economics-optimal platform**: DRAM-bandwidth 임계값 $T_B$, SSD-bandwidth 임계값 $T_S$, DRAM-capacity 임계값 $T_C$로 정의되는, 워크로드를 감당(viable)하거나 비용 최적(economics-optimal)인 플랫폼 상태.
- **Blocked Cuckoo hashing**: 각 키를 2개 후보 버킷에 매핑하고 overflow 시 relocation으로 처리하는 hash table 기법; SSD-resident KV store의 index-free 설계에 사용.
- **Two-stage progressive ANN search**: reduced-dimension embedding(e.g., MRL/PCA)으로 1차 pruning 후 소수 후보만 full-dimension으로 재랭킹하는 SSD-resident ANN 검색 기법.

## 강점 · 한계 · 열린 질문
- **강점**: (1) 고전 rule의 "경제성만" 관점을 host cost·feasibility·workload까지 통합한 일관된 first-principles 프레임워크로 확장; (2) datasheet peak IOPS가 아니라 NAND device physics에서 IOPS를 유도해 아키텍처 설계 공간 탐색이 가능; (3) MQSim-Next라는 재사용 가능한 오픈 시뮬레이션 도구와 두 개의 concrete case study(KV store, ANN search)로 이론을 실제 시스템/알고리즘 설계 지침으로 연결.
- **한계**: (1) 모든 비용 파라미터가 "정규화된" 값이며 실제 시장가/공정노드에 따라 절대 수치는 달라질 수 있음(저자도 p.16에서 인정, 상대적 트레이드오프만 강건하다고 주장); (2) endurance(retention, refresh, lifetime 비용)와 energy/OpEx를 전혀 모델링하지 않음(TCO 미포함); (3) read-dominant, large-footprint, single-tenant 워크로드에 국한 — write-intensive/multi-tenant/GC 간섭·bursty 패턴은 다루지 않음; (4) single-node, local PCIe/NVMe 토폴로지만 가정 — multi-socket, disaggregated/CXL-attached memory, NVMe-oF로의 확장은 future work로 남김(p.16).
- **열린 질문**: CXL-attached memory나 fabric-attached storage 같은 추가 계층으로 break-even/feasibility 프레임워크를 pairwise하게 확장하면 어떤 새로운 임계값 구조가 나올까? Endurance-aware 비용을 포함하면 seconds-scale caching 결론이 얼마나 흔들릴까? Host I/O stack(제출 latency, lightweight I/O accelerator)을 함께 co-design하면 host-limited regime을 얼마나 더 밀어낼 수 있을까(p.16, Host-side I/O optimization)?

## ❓ Q&A (자가 점검)
> [!question]- 고전 five-minute rule이 가진 두 가지 핵심 한계는 무엇인가?
> (A) Insufficient realism — host 자원(processor IOPS, DRAM bandwidth)을 공짜로 취급함; (B) Missing feasibility — vendor peak spec(예: $/GB, $/IOPS)만 최적화하고 host-side 제약(IOPS 용량, latency/throughput target, DRAM bandwidth/capacity)을 무시해 실제 배포 불가능한 구성을 내놓을 수 있음 (p.2).

> [!question]- 이 논문이 SSD peak IOPS를 유도할 때 고려하는 세 가지 병목은?
> NAND/channel(device-memory) 한계 $IOPS_{dev}$, 컨트롤러 address-translation bandwidth 한계 $IOPS_{xlat}$, PCIe 링크 대역폭/패킷 처리율 한계 $IOPS_{pcie}$의 최솟값이 $IOPS^{(peak)}_{SSD}$ (Eq. 2, p.4).

> [!question]- Table III에서 CPU와 GPU의 IOPS 용량은 어떻게 정규화되었나?
> 서버급 CPU 코어는 1M IOPS/core, GPU SM은 NVIDIA SCADA(Hopper 세대 기준)를 따라 4M IOPS/SM으로 정규화; normalized cost는 CPU core $\alpha_{CORE}=4$, GPU $\alpha_{CORE}=3$ (p.6, Table III).

> [!question]- Fig. 4의 대표적 정량 결과 하나를 들면?
> 512B block, SLC NAND 기준으로 break-even interval이 CPU+DDR/Normal-SSD에서 약 34s인데 GPU+GDDR/Storage-Next SSD에서는 약 5s로, 약 7배 단축된다(p.6-7).

> [!question]- Section IV에서 tail latency 제약이 usable SSD IOPS에 미치는 영향은 host IOPS 제약과 비교해 어떤가?
> Host processor IOPS capacity가 break-even interval을 줄이는 지배적 요인이고, tail-latency target 조정(99th percentile 90%→70%)은 상대적으로 부차적 영향만 준다 — 예: 512B on GPU+GDDR에서 tail target을 7μs→85μs로 완화해도 break-even은 약 1.5s만 줄어든다(p.8).

> [!question]- MQSim-Next가 기존 MQSim 대비 추가한 핵심 3가지 device-modeling 요소는?
> (1) SCA 채널 프로토콜(짧아진 command/address 오버헤드), (2) independent multi-plane reads(다이 내 병렬성), (3) explicit transfer-sense overlap(한 요청의 command/data 이동과 다른 요청의 sensing/programming 중첩) — 그리고 tunable BCH+LDPC 2-layer ECC 모델(p.11).

> [!question]- SSD-resident KV store case study에서 DRAM-resident index를 완전히 없앨 수 있는 핵심 자료구조 선택은 무엇이며 왜인가?
> Blocked Cuckoo hashing — 각 키가 2개 후보 SSD 버킷을 가지므로 삽입 실패 시 discard 대신 relocation으로 처리 가능해, persistent KV store에 필요한 "항목을 버리지 않는다"는 요구를 만족시키면서도 load factor를 $\alpha_{critical}$ 이하로 유지하면 평균 lookup이 1.5 SSD block read로 근사 일정하게 유지된다(p.13-14).

> [!question]- Case study 2에서 two-stage progressive ANN search가 성립하는 경험적 근거는?
> Gao et al.[15]에 따르면 distance 비교의 90% 이상이 reduced-dimension 단계에서 이미 기각(reject)되어 full-vector 평가가 불필요하며, MRL 기반 3개 코퍼스(MS MARCO, 20 Newsgroups, DBpedia) 실험에서 recall이 98% 이상 유지됨(p.15).

## 🔗 Connections
[[Infra]] · [[arXiv]] · [[2026]]

## References worth following
- Appuswamy, Graefe, Borovica-Gajic, Ailamaki, "The five-minute rule 30 years later and its impact on the storage hierarchy," CACM 62(11), 2019 [5] — 직전 revisit으로, 이 논문이 극복하려는 minute-scale 결론의 근거.
- Tavakkol, Gómez-Luna, Sadrosadati, Ghose, Mutlu, "MQSim: A framework for enabling realistic studies of modern multi-queue SSD devices," FAST 2018 [47] — MQSim-Next의 base가 된 원본 SSD 시뮬레이터.
- Subramanya, Devvrit, Simhadri, Krishnaswamy, Kadekodi, "DiskANN: Fast accurate billion-point nearest neighbor search on a single node," NeurIPS 2019 [22] — ANN search case study의 비교 대상(illustrative reference point, ~5 KQPS).
- Kusupati et al., "Matryoshka Representation Learning," NeurIPS 2022 [29] — two-stage progressive ANN search의 reduced-dimension embedding 생성 방법 중 하나(MRL).
- Malkov & Yashunin, "Efficient and robust approximate nearest neighbor search using hierarchical navigable small world graphs," IEEE TPAMI 42(4), 2018 [35] — case study 2에서 사용한 HNSW 알고리즘의 원 논문.
- Pagh & Rodler, "Cuckoo hashing," Journal of Algorithms 51(2), 2004 [51] — SSD-resident KV store의 blocked Cuckoo hashing 기반 기법.

## Personal annotations
<!-- 본인 메모 영역 -->
