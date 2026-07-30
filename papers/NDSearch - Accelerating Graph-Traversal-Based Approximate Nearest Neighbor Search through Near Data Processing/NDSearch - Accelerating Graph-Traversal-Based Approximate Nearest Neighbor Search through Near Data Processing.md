---
title: "NDSearch: Accelerating Graph-Traversal-Based Approximate Nearest Neighbor Search through Near Data Processing"
description: "SmartSSD 기반 in-storage 가속기 SearSSD와 LUN-level 병렬성·2-level 스케줄링으로 그래프 탐색 기반 ANN 검색을 가속하는 HW/SW co-design NDP 시스템"
venue: ISCA
year: 2024
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - cluster/search
  - venue/isca
  - year/2024
  - list/26s-v2
  - topic/in-storage-computing
  - topic/nand-flash
  - topic/graph-traversal
  - topic/hw-sw-co-design
---

# NDSearch: Accelerating Graph-Traversal-Based Approximate Nearest Neighbor Search through Near Data Processing

> **ISCA 2024** · cluster/search · Source: [NDSearch - Accelerating Graph-Traversal-Based Approximate Nearest Neighbor Search through Near Data Processing.pdf](<NDSearch - Accelerating Graph-Traversal-Based Approximate Nearest Neighbor Search through Near Data Processing.pdf>)

저자: Yitu Wang, Shiyu Li, Qilin Zheng, Hai "Helen" Li, Yiran Chen (Duke University, Durham, North Carolina, USA); Linghao Song (University of California, Los Angeles, California, USA); Zongwang Li, Andrew Chang (Samsung Semiconductor, Inc., San Jose, California, USA)

## TL;DR
그래프 탐색 기반 ANNS(HNSW, DiskANN)는 데이터셋이 커지면 메모리를 수백 GB~TB 소모해 SSD로 오프로딩해야 하는데, 이때 SSD I/O 대역폭(PCIe)이 병목이 된다(전체 지연의 최대 75%). NDSearch는 SmartSSD를 개조한 in-storage 가속기 SearSSD를 제안해 그래프 탐색과 거리 계산 커널을 NAND flash의 LUN(logic unit) 수준까지 내려 병렬화하고, LUNCSR이라는 새 그래프 포맷으로 FTL 주소 변환 오버헤드를 없앤다. 여기에 정적 스케줄링(degree-ascending BFS reordering + multi-plane mapping)과 동적 스케줄링(batch-wise allocating + speculative searching)의 2-level 스케줄링을 결합해 공간·시간 지역성을 모두 끌어올린다. 결과적으로 CPU/GPU/SmartSSD-only/DeepStore 대비 최대 31.7×/14.6×/7.4×/2.9× 처리량 향상과 CPU·GPU 대비 두 자릿수(최대 178.68×) 에너지 효율 향상을 달성한다.

## 문제 & 동기
Graph-traversal 기반 ANNS(HNSW, DiskANN)는 recall이 높아 널리 쓰이지만, 그래프가 커지면 vertex당 60~450 bytes를 차지해 수백 GB~수 TB 메모리가 필요해 단일 워크스테이션 메모리 용량을 초과한다(p.368). 이를 해결하려 SSD로 데이터를 내리면, CPU/GPU와 SSD 사이 PCIe 링크의 I/O 대역폭이 병목이 된다. Fig.1(p.368)의 실행시간 분석에서 SSD I/O read가 전체 지연의 최대 75%를 차지하며, Fig.2(a)에서는 batch size가 1024를 넘으면 PCIe 대역폭 이용률이 83%까지 포화된다(p.369). 기존 SmartSSD류 in-storage 가속기(DeepStore 등)는 채널/칩 수준 가속기라 그래프 탐색의 fine-grained·irregular 접근 패턴을 활용하지 못하고, 소프트웨어 처리 모델(GraphMat 등)도 in-storage 아키텍처 특성에 맞지 않는다(p.369).

> [!quote]- 📄 원문 표현 (paper)
> - "NDSEARCH improves the throughput by up to 31.7×, 14.6×, 7.4×, 2.9× over CPU, GPU, a state-of-the-art SmartSSD-only design, and DeepStore, respectively." (p.368)
> - "The SSD I/O read accounts for up to 75% of the total latency." (p.369)
> - "the utilization of SSD I/O bandwidth saturates to 83%, after the batch size increases to 1024." (p.369)

## 핵심 통찰 (Key Insight)

**1) Fine-grained LUN-level in-storage 가속이 그래프 탐색의 산발적 접근에 맞다.**
그래프 탐색은 batch 안에서도 LUN 접근이 매우 흩어져 있지만(Fig.4(b)), batch size가 총 LUN 수보다 커지면 하나의 LUN에 여러 번 접근이 몰릴 수밖에 없다(inclusion-exclusion 원리). 채널/칩 단위 가속기는 버스 공유 때문에 이 병렬성을 살리지 못하므로, NAND flash 내부의 multi-LUN 연산을 활용해 LUN 단위로 가속기(SiN)를 두면 내부 대역폭과 연산 병렬성을 동시에 얻는다.

**2) 그래프 포맷 자체를 NDP에 맞게 재설계(LUNCSR)해야 주소 변환 오버헤드가 사라진다.**
기존 CSR은 vertex 배치 정보를 인코딩하지 않아 in-storage 가속에서 FTL의 logical-to-physical 주소 변환이 필요하다. LUNCSR은 LUN array와 BLK array를 추가해 vertex/neighbor ID에서 물리 주소(LUN·block)를 직접 유도, FTL 개입 없이 페이지/컬럼 주소를 계산한다(p.371). 추가 메모리 자원 없이 기존 매핑 테이블을 LUNCSR 배열로 치환한 것.

**3) 정적(공간) + 동적(시간) 2-level 스케줄링으로 지역성을 이중으로 확보.**
Static scheduling은 degree-ascending BFS reordering으로 vertex 간 bandwidth $\beta$를 최소화해 이웃을 같은 페이지에 몰아넣고(Eq.1, p.374), multi-plane addressing 제약에 맞춰 재배치한다. Dynamic scheduling은 같은 LUN을 타겟으로 하는 query를 batch 단위로 한 번에 할당(batch-wise allocating)하고, 다음 iteration에서 접근할 가능성이 높은 2차 이웃을 미리 가져오는 speculative searching으로 iteration 간 지연을 겹친다(Fig.12, p.376). 두 층위가 각각 공간 지역성과 시간 지역성을 담당해 상호보완적이다.

> [!quote]- 📄 원문 표현 (paper)
> - "Reordering graph vertices to get the minimum β has been proved to be an NP-Completeness problem." (p.374)
> - "we reorder the vertices based on their degrees in ascending order, which is a deterministic approach rather than a random one." (p.374)
> - "the second-order neighbors of the entry vertex in the current iteration are the potential candidates to access in the next search iteration." (p.376)
> - "The feature vectors of query and targeted vertices are “filtered” by the SEARSSD to reduce the PCIe bandwidth consumption, which could be as low as 1/32 of the data transferred via PCIe link" (p.372)

## 설계 / 메커니즘 (Design)

**SearSSD 아키텍처 (Fig.5(a), p.372).** FPGA(호스트-SSD 사이 PCIe 3.0×4)와 개조된 SSD로 구성. Query가 도착하면 (1) SSD controller가 DRAM에 query property table을 만들고, (2) Vgenerator가 LUNCSR에서 entry vertex의 offset/LUN/neighbor ID를 읽어오며, (3) Allocator가 이웃 vertex의 물리 주소를 LUN별로 산출해 (4)(5) 해당 LUN-level 가속기(SiN)로 query와 후보 주소를 전송, 거리 계산 결과를 query property table에 갱신한다. 이 루프가 batch가 끝날 때까지 반복된다.

**Vgenerator / Allocator (Fig.7, p.373).** Vgenerator는 3-stage 파이프라인(OFS Fetcher → NBR Fetcher → LUN Fetcher)으로 entry vertex의 offset·neighbor ID·LUN ID를 가져온다. Allocator는 같은 LUN ID를 가진 query·neighbor를 Alloc Buffer 안에서 모아(Dispatcher) Alloc CTR이 FTL 변환 없이 물리 주소를 직접 생성한다.

**SiN (Search-in-NAND) 엔진 (Fig.8, p.373).** 하나의 SiN = 2개 LUN-level 가속기. Flash CTR이 modified multi-LUN instruction으로 같은 SiN 내 LUN-level 가속기들을 병렬 구동하고, multi-plane 명령으로 서로 다른 plane에서 vertex를 읽어 2개의 MAC 그룹(adder tree 기반)이 동시에 유클리드/각도 거리를 계산, output buffer에 임시 저장한다. Plane 단위 hard-decision LDPC ECC 디코더가 각 plane·MAC 그룹 사이에 삽입되어 있다(p.373).

**Multi-LUN 연산 개조 (Fig.9, p.373).** 기존 multi-LUN read(`<ReadPage>`)를 `<SearchPage>` instruction으로 확장 — 2-bit "Distance" 필드로 거리 종류(Euclidean/angular/inner-product)를 지정하고, page buffer가 아닌 output buffer에서 "계산된 거리만" 읽어오도록 `<ReadStatusEnhanced>`/`<ChangeReadColumn>`의 대상을 바꿔, 원본 feature vector 대신 스칼라 거리만 전송해 대역폭을 절약한다.

**처리 모델 (Algorithm 1, p.373).** GraphMat의 Scatter/Apply 모델을 NDP에 맞게 재구성: Scatter를 Allocating(batch-wise LUN 할당)과 Searching(Process Edge = 거리 계산, Reduce = 후보 리스트 갱신)으로, Apply를 Gathering(query property 갱신)과 Sorting(top-k bitonic sort, FPGA에서 실행)으로 분리해 stage 간 오버랩을 가능하게 한다.

**정적 스케줄링 — degree ascending BFS reordering (Fig.10, p.374).** 최소 차수(degree) vertex를 root로 선택해 BFS하며, 이웃을 차수 오름차순으로 재번호를 매겨(예시에서 β = 5.875 → 5.125(random BFS) → 3.625(제안)) NP-hard한 bandwidth 최소화 문제를 결정론적(한 번만 실행)으로 근사한다. 이후 재정렬된 vertex를 multi-plane addressing 제약(같은 multi-plane 명령 내 plane 주소는 서로 달라야 하고 page/LUN 주소는 같아야 함, Fig.11, p.375)에 맞춰 순차적으로 plane→LUN에 매핑한다.

**동적 스케줄링 — batch-wise allocating + speculative searching (Fig.12, p.376).** 같은 LUN을 타겟팅하는 query들을 한 번에 해당 LUN-level 가속기에 할당해 페이지 재로딩을 줄인다. Speculative searching은 iteration $i$의 Allocating 단계가 끝나는 즉시 iteration $i{+}1$을 위한 2차 이웃(second-order neighbor)을 Pref Unit이 미리 가져와 거리 계산까지 투기적으로 수행, 실제 겹치는 이웃($N_{id} \cap N_{id}^{Pref} \neq \varnothing$)이 있으면 그 결과를 재사용한다.

> [!quote]- 📄 원문 표현 (paper)
> - "our design is more realistic in terms of data refreshing and address translation" (p.370, DeepStore와의 비교)
> - "there is no additional memory resources for LUNCSR arrays compared to the standard SSD, where there exists a mapping table" (p.371)
> - "the compressed sparse row (CSR) is suitable for the NDP solution since the vertex and neighbor IDs are separately stored" (p.372)

## 평가 (Evaluation)
- **벤치마크**: HNSW(hnswlib/cuhnsw), DiskANN — glove-100, fashion-mnist, sift-1b, deep-1b, spacev1b, 5개 데이터셋; recall@10을 95/95/94/93/90%로 튜닝(p.376). 시뮬레이터는 SSD-Sim 기반 in-house trace-driven + cycle-level 시뮬레이터, SSD 내부 DRAM 4GB, Samsung 983 DCT 1.92T 파라미터 기반(p.376).
- **처리량/속도 (Fig.13, p.376)**: CPU/GPU 2×Xeon Gold 6254·NVIDIA Titan RTX(24GB) 대비, NDSearch는 CPU/GPU 대비 최대 31.7×/14.6× 처리량, DS-cp/DS-cp 대비 HNSW·DiskANN에서 각각 최대 2.81×/2.94× 속도 향상(p.376). DiskANN·sift-1b에서 SmartSSD-only 대비 최대 7.44× 속도 향상(p.376).
- **정적 스케줄링 (Fig.14, p.377)**: reordering으로 page access ratio를 최대 38% 감소, 최대 1.17× 속도 향상(무 reordering 대비).
- **동적 스케줄링 (Fig.15, p.377)**: batch-wise dynamic allocating으로 page access를 최대 73% 감소; speculative searching은 추가 page access를 유발하지만(투기 결과의 절반 이상 미채택) 최대 1.27× 추가 속도 향상.
- **Ablation (Fig.16, p.377)**: bare NDSearch(reordering 등 미적용)만으로도 PCIe 데이터 전송 제거 덕분에 CPU 대비 4× 이상 속도 향상; 모든 기법(re+mp+da+sp) 적용 시 bare 대비 4.1× 추가 성능 향상.
- **오버헤드 분석 (Fig.17, p.377)**: NAND read가 전체 실행시간의 24~38%로 최대 비중이지만, CPU+SSD 시스템 대비 SSD read 비중이 ~70%(Fig.1)에서 ~6%로 감소(SearSSD의 "filtering" 덕분). Bitonic sort(FPGA)는 최대 12%.
- **에너지 효율 (Fig.20, Table I, p.378)**: NDSearch는 CPU/GPU/SmartSSD-only/DS-cp 대비 최대 178.68×/120.87×/30.06×/3.48× 에너지 효율. SearSSD 전체 power 18.82W(Table I), FPGA bitonic sort 7.5W 포함 NDSearch 총 전력 26.32W로 SmartSSD의 ~55W PCIe 전력 예산 내(p.378).
- **면적/저장 밀도 (p.378-379)**: SearSSD 커스텀 로직 43.09 mm² (32nm), DS-cp/DS-c(236.8/320 mm²) 대비 각각 82%/87% 작음. 스토리지 밀도는 6Gb/mm²(Samsung 983 DCT 기준)에서 5.64 Gb/mm²로 "6% density degradation"만 발생(p.379).
- **ECC (Fig.18, p.378)**: raw BER 10⁻⁶ 가정, hard-decision LDPC 실패 확률 최악(30%) 시나리오에서도 속도 저하는 1.23×~1.66×에 그침 — "the plane-level hard-decision LDPC decoder is sufficient in most cases."
- **Batch size sweep (Fig.19, p.378)**: batch 256에서는 DS-cp 대비 이득이 미미(fine-grained LUN 병렬성 미활용), batch가 커질수록 이득 증가하다 4096 이후 일부 벤치마크에서 power budget 제약으로 다시 감소.
- **일반화 (Fig.21, p.379)**: HCNNG·TOGG(다른 그래프 기반 ANNS) sift-1b에서도 CPU/GPU/DS-cp 대비 우위 유지. CPU-T(TB급 DRAM 장착 CPU baseline)는 CPU 대비 최대 5.3× 속도지만 여전히 DeepStore/NDSearch를 넘지 못함(DRAM만으론 internal bandwidth·병렬성 부족).

> [!quote]- 📄 원문 표현 (paper)
> - "NDSEARCH achieves up to 178.68×, 120.87×, 30.06× and 3.48× higher energy efficiency than CPU, GPU, the SmartSSD-only design, and DS-cp, respectively." (p.378)
> - "which is acceptable with only 6% density degradation." (p.379)
> - "the plane-level hard-decision LDPC decoder is sufficient in most cases." (p.378)

## 섹션 노트
- **I. Introduction**: ANNS 배경, RAG 응용 언급, graph-traversal ANNS의 3단계(graph traversal / distance computation / bitonic sorting)와 메모리 문제 제시, 기존 3가지 접근(sharding, SSD 인덱스, 다중 GPU 로딩)의 한계로 SSD I/O 병목을 지목.
- **II. Background**: ANNS 정의(NNS의 근사), HNSW/DiskANN의 construction/search phase 설명, SSD 내부 조직(channel-chip-LUN-plane-page 계층)과 FTL의 data refreshing/address translation 역할.
- **III. Motivation**: Fig.4로 batch-wise page/LUN 접근 패턴이 산발적임을 보이고, reordering+remapping 및 LUN-level 가속기·동적 스케줄링의 필요성을 도출.
- **IV. NDSearch Architecture**: SearSSD 전체 구조, LUNCSR 데이터 레이아웃(기존 padding 낭비 지적), Vgenerator/Allocator/SiN 세부 구조와 multi-LUN 명령 개조, ECC 배치.
- **V. Processing Model**: GraphMat 기반 Scatter/Apply를 Allocating/Searching/Gathering/Sorting 4단계로 세분화한 Algorithm 1.
- **VI. Two-Level Scheduling**: 정적(degree-ascending BFS reordering, NP-hard bandwidth 최소화, multi-plane mapping)과 동적(batch-wise allocating, speculative searching) 스케줄링 각각의 알고리즘과 예시.
- **VII. Evaluation**: 실험 방법론, 처리량/속도, 스케줄링별 기여도, ablation, overhead, ECC/내구성, batch size 민감도, 에너지 효율, 면적/밀도.
- **VIII. Discussion**: HCNNG/TOGG로 일반화 평가, quantization-based·tree-based ANNS로의 미확장 등 한계 명시.
- **IX. Related Works**: In-storage computing 3분류(firmware 개조, HW 모듈 추가, NAND flash 내부 회로 개조)와 DeepStore/GraphSSD/GraphBoost 등 비교, SmartSSD-only([47])는 PCIe 대역폭 한계를 근본적으로 해결 못함.
- **X. Conclusion**: SearSSD in-storage 가속기 + 맞춤 처리 모델로 그래프 탐색 기반 ANNS의 throughput·에너지 효율을 동시에 개선했다는 요약.

## 핵심 용어 (Key terms)
- **ANNS (Approximate Nearest Neighbor Search)**: 정확한 최근접 이웃 대신 recall을 일부 희생하고 속도를 얻는 검색 기법.
- **HNSW / DiskANN**: 대표적인 graph-traversal 기반 ANNS 알고리즘 (계층적 navigable small world graph / SSD 캐시 기반 단일 노드 알고리즘).
- **SmartSSD**: SSD 컨트롤러 옆에 FPGA를 붙여 in-storage computing을 지원하는 상용 플랫폼, NDSearch의 하드웨어 베이스.
- **LUN (Logic Unit)**: NAND flash 내에서 독립적으로 명령을 수행할 수 있는 최소 단위(플레인들의 그룹).
- **SearSSD**: 이 논문이 제안하는, LUN-level 가속기(SiN)와 Vgenerator/Allocator를 내장한 개조 SSD.
- **SiN (Search-in-NAND)**: LUN-level 가속기 2개로 구성된 거리 계산 엔진.
- **LUNCSR**: CSR에 LUN array·BLK array를 추가해 vertex의 물리 위치(LUN/block)를 직접 인코딩한 그래프 포맷.
- **Degree-ascending BFS reordering**: vertex 차수 오름차순으로 BFS 재번호를 매겨 vertex bandwidth $\beta$(이웃 간 인덱스 거리)를 근사 최소화하는 정적 재정렬 기법.
- **Speculative searching**: 다음 search iteration에서 접근할 2차 이웃(second-order neighbor)을 현재 iteration의 Allocating 단계와 겹쳐 미리 가져오는 기법.
- **Bitonic sorting**: top-k 후보를 정렬하는 병렬 정렬 알고리즘, FPGA에서 실행.
- **FTL (Flash Translation Layer)**: SSD의 logical-to-physical 주소 변환·data refreshing·wear leveling을 담당하는 임베디드 코어 소프트웨어.

## 강점 · 한계 · 열린 질문
- **강점**: LUN 수준까지 내려간 fine-grained in-storage 가속과, 그래프 포맷(LUNCSR) 자체를 NDP 친화적으로 재설계해 FTL 오버헤드를 근본적으로 제거한 점이 인상적. 정적(공간)·동적(시간) 스케줄링을 분리해 각각의 지역성 문제를 독립적으로 공략한 구조가 명확하다. Ablation(Fig.16)과 batch size sweep(Fig.19)으로 각 기법의 기여도를 투명하게 분해해 보여준다.
- **한계**: 저자들이 스스로 명시하듯 quantization-based ANNS, tree-based ANNS로는 일반화되지 않음(p.379, "not generalized to some other types of ANNS algorithms"). 그래프 construction phase는 CPU/GPU에서 수행하고 search phase만 가속 대상이라 construction 비용은 논의되지 않음. Data refreshing/wear leveling과 block-level FTL refreshing이 LUNCSR 정합성에 미치는 영향은 다뤄지나(p.371, p.375), 실제 field 조건에서의 장기 내구성 데이터는 제한적(BER 시뮬레이션 기반, p.378).
- **열린 질문**: batch size가 매우 크거나(>4096) power budget이 더 완화된 환경에서 LUN-level 병렬성의 스케일링 한계는 어디인가? Speculative searching의 misprediction으로 인한 추가 page access(최대 2.67×, p.377)를 줄이는 더 정교한 예측 기법이 가능한가? HCNNG/TOGG 이상의 최신 ANNS(quantization 결합형 등)로 확장 시 LUNCSR·SiN 설계가 어떻게 바뀌어야 하는가?

## ❓ Q&A (자가 점검)
> [!question]- NDSearch가 해결하려는 근본 병목은 무엇이고, 어디서 정량적으로 드러나는가?
> 그래프 탐색 기반 ANNS를 SSD로 오프로딩할 때 PCIe를 통한 SSD I/O read 대역폭이 병목이 된다. Fig.1(p.368)에서 SSD I/O read가 전체 지연의 최대 75%를 차지하고, Fig.2(a)에서 batch size 1024 이상에서 PCIe 대역폭 이용률이 83%로 포화된다(p.369).

> [!question]- LUNCSR이 기존 CSR과 다른 점, 그리고 왜 FTL 주소 변환을 없앨 수 있는가?
> LUNCSR은 CSR의 offset/neighbor/vertex array에 LUN array와 BLK array를 추가해 각 vertex의 물리적 LUN 할당과 block 내 상대 위치를 직접 저장한다(p.371). 이 배열들은 vertex ID/neighbor ID로 인덱싱되고 block-level refreshing 시 FTL이 갱신하므로, page/column 주소를 논리 인덱스에서 직접 유도할 수 있어 기존 SSD의 logical-to-physical 매핑 테이블 없이도 주소를 생성한다.

> [!question]- 정적 스케줄링(degree-ascending BFS reordering)이 왜 "한 번만 실행해도" 근최적인가?
> 무작위로 높은 차수의 vertex를 먼저 재번호 매기면 나중에 그 이웃들(차수 낮은 vertex 포함)을 가깝게 배치하기 어려워 bandwidth $\beta$가 커진다. 반대로 차수가 낮은 vertex부터 결정론적으로 재번호를 매기면 무작위성이 개입할 여지가 줄어 특정 조건(이웃들의 차수가 같은 경우만 예외)에서 근최적 결과를 한 번의 실행으로 얻는다(p.374, Fig.10 예시에서 β=5.875→3.625).

> [!question]- Speculative searching이 잘못된 예측을 했을 때의 대가는 무엇이고, 논문은 이를 어떻게 정량화하는가?
> 예측된 2차 이웃 중 다음 iteration에서 실제로 필요하지 않은 것들은 낭비된 page access가 된다. Fig.15(p.377)에서 speculative searching 적용 시 page access가 증가(투기 결과의 절반 이상이 미채택)하지만, 그럼에도 iteration 간 지연 오버랩 덕분에 최대 1.27×의 추가 속도 향상을 얻는다.

> [!question]- SmartSSD-only baseline과 DS-cp(DeepStore chip-level accelerator) 대비 NDSearch가 우위인 근본 이유는?
> SmartSSD-only([47])는 FPGA-SSD 간 PCIe 연결만 활용해 SSD 내부 대역폭/병렬성을 쓰지 못한다(p.379). DS-cp는 chip-level 가속기라 그래프 탐색의 fine-grained·비압축 연산 특성상 컴퓨팅 자원이 병목이 아니라(p.376, DeepStore 논문의 신경망과 달리) 오히려 LUN 단위까지 세분화한 가속기가 더 효율적이며, NDSearch는 sift-1b DiskANN에서 SmartSSD-only 대비 7.44×, DS-cp 대비 HNSW/DiskANN 각각 2.81×/2.94× 속도 향상을 보인다(p.376).

> [!question]- SearSSD 도입으로 인한 스토리지 밀도·전력상 대가는 얼마나 되는가?
> 저장 밀도는 6Gb/mm²에서 5.64Gb/mm²로 약 6% 저하(p.379)되고, 커스텀 로직 면적은 43.09mm²(32nm, Table I)로 DS-cp/DS-c(236.8/320mm²) 대비 오히려 82%/87% 작다. 전력은 총 18.82W(SearSSD) + 7.5W(FPGA bitonic) = 26.32W로 SmartSSD의 ~55W PCIe 전력 예산 내에 들어온다(p.378).

> [!question]- 논문이 명시한 일반화 한계는 무엇인가?
> NDSearch는 graph-traversal 기반 ANNS(HNSW, DiskANN, 그리고 HCNNG/TOGG로 확장 검증)를 대상으로 하며, quantization-based ANNS나 tree-based ANNS로는 일반화되지 않는다고 저자들이 명시한다(p.379, VIII-B Limitations).

## 🔗 Connections
[[Vector Search]] · [[ISCA]] · [[2024]]
관련: In-storage computing 계열(DeepStore, SmartSSD 기반 가속기)과 비교되므로 랩 리스트 내 다른 in-storage computing / CXL-SSD 논문들과 "SSD 내부 병렬성 활용" 축에서 연결 가능.

## References worth following
- Y. A. Malkov and D. A. Yashunin, "Efficient and robust approximate nearest neighbor search using hierarchical navigable small world graphs," TPAMI 2018 [59] — NDSearch가 가속하는 HNSW 알고리즘의 원 논문.
- S. J. Subramanya et al., "DiskANN: Fast accurate billion-point nearest neighbor search on a single node," NeurIPS 2019 [70] — 또 다른 주요 대상 알고리즘, SSD를 캐시로 쓰는 설계.
- V. S. Mailthody et al., "DeepStore: In-storage acceleration for intelligent queries," MICRO 2019 [58] — 주요 baseline(DS-c/DS-cp)이 되는 기존 in-storage 가속기.
- J.-H. Kim et al., "Accelerating large-scale graph-based nearest neighbor search on a computational storage platform," IEEE TC 2022 [47] — SmartSSD-only baseline([47]) 설계의 원 논문.
- N. Sundaram et al., "GraphMat: high performance graph analytics made productive," VLDB 2015 [71] — NDSearch의 처리 모델(Scatter/Apply)이 확장한 원 그래프 분석 모델.
- K. Matam et al., "GraphSSD: Graph semantics aware SSD," ISCA 2019 [60] — 그래프 구조를 SSD 내부에 반영한 관련 in-storage 그래프 가속기.

## Personal annotations
<!-- 본인 메모 영역 -->
