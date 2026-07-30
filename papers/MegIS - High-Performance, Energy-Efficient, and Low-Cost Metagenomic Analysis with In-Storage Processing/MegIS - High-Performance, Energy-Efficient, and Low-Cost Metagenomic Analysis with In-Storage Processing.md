---
title: "MegIS: High-Performance, Energy-Efficient, and Low-Cost Metagenomic Analysis with In-Storage Processing"
description: "메타지노믹 분석의 SSD I/O 병목을 host-SSD 협력형(cooperative) in-storage processing으로 해결하는 최초의 ISP 시스템"
venue: ISCA
year: 2024
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - cluster/isc
  - venue/isca
  - year/2024
  - list/26s-v2
  - topic/in-storage-processing
  - topic/genomics
  - topic/ssd-ftl
  - topic/hw-sw-codesign
---

# MegIS: High-Performance, Energy-Efficient, and Low-Cost Metagenomic Analysis with In-Storage Processing

> **ISCA 2024** · cluster/isc · Source: [MegIS - High-Performance, Energy-Efficient, and Low-Cost Metagenomic Analysis with In-Storage Processing.pdf](<MegIS - High-Performance, Energy-Efficient, and Low-Cost Metagenomic Analysis with In-Storage Processing.pdf>)

저자: Nika Mansouri Ghiasi¹, Mohammad Sadrosadati¹, Harun Mustafa¹, Arvid Gollwitzer¹, Can Firtina¹, Julien Eudine¹, Haiyu Mao¹, Joël Lindegger¹, Meryem Banu Cavlak¹, Mohammed Alser¹, Jisung Park², Onur Mutlu¹ (¹ETH Zürich, ²POSTECH)

> ⚠️ 시트에는 venue year가 2023(ISCA'23)으로 기재되어 있었으나, PDF 1페이지 헤더("2024 ACM/IEEE 51st Annual International Symposium on Computer Architecture (ISCA)")와 DOI(10.1109/ISCA59077.2024.00054) 기준 **ISCA 2024**가 정확한 값이다.

## TL;DR
메타지노믹 분석(metagenomic analysis)은 시료 속 미지의 종을 찾기 위해 GB~TB급 reference 데이터베이스를 low-reuse 패턴으로 뒤져야 해서 storage-to-host 데이터 이동이 성능 병목이 된다. MegIS는 이 파이프라인을 위한 최초의 in-storage processing(ISP) 시스템으로, host와 SSD 각각에 가장 적합한 하위 작업을 배치하고(task partitioning) 두 부분을 파이프라인으로 겹쳐 실행(data/computation flow coordination)하며, k-mer 정렬·교집합(intersection) 탐색·taxID 조회를 SSD 내부 자원(내부 DRAM, 채널 대역폭)에 맞춰 재설계한 알고리즘과 초경량 하드웨어 가속기(채널당 0.04mm², 7.658mW급)로 구현한다. 그 결과 성능 최적화 SW 도구 Kraken2 대비 2.7-37.2×, 정확도 최적화 SW 도구 Metalign과는 동일 정확도로 6.9-100.2×, 최신 PIM 가속기 Sieve 대비 1.5-5.1× 속도 향상과 함께 평균 5.4×(Kraken2 대비)/1.9×(Sieve 대비)/15.2×(Metalign 대비) 에너지 절감을 달성한다(p.660-661).

## 문제 & 동기
메타지노믹 분석은 (i) presence/absence 판별과 (ii) relative abundance 추정 두 단계로 구성되며, 각 단계는 종을 모르는 채로 수백 GB~수백 TB 크기의 reference 데이터베이스에서 k-mer를 탐색해야 한다(p.661-662, Fig.1). 시퀀싱·베이스콜링은 샘플당 한 번만 수행되지만, 메타지노믹 분석 자체는 동일 샘플이 여러 연구·시점에 걸쳐 반복 재분석되고(p.661), 데이터베이스가 몇 달 단위로 두 배씩 증가하고 있어(p.661) 분석 단계가 end-to-end 워크플로의 병목이자 에너지 지배 요인이 되고 있다. 저자들이 고성능 서버에서 측정한 결과, 최신 정확도 지향 도구(Metalign)로 100M read 샘플을 분석하는 데 48시간이 걸리고(p.664), 이 분석 단계 하나가 전체 에너지의 63%를 차지한다(§3.1, p.663).

I/O 오버헤드를 직접 정량화한 모티베이션 실험(§3.2, Fig.3, p.664)에서, 무한 대역폭을 가정한 이상적 구성(No-I/O) 대비 저가 SSD(SSD-C)는 random-access 도구(R-Qry, Kraken2류)에서 9.4×(최대 17.7×), streaming-access 도구(S-Qry, Metalign류)에서 32.9×(최대 3.6×) 느리다. 고성능 SSD(SSD-P)를 써도 No-I/O와의 격차는 여전히 크며, 데이터베이스가 0.3TB→0.6TB로 커지면 R-Qry의 SSD-C/No-I/O 격차는 7.1×→12.5×로 더 벌어진다(p.664). 심지어 PIM 가속기(Sieve류)로 compute/memory 병목을 없애도, I/O를 그대로 두면 0.3TB/0.6TB Kraken2 데이터셋에서 No-I/O가 SSD-P보다 평균 26.1×(최대 3.0×) 빠르다(p.665) — 즉 다른 병목을 없앨수록 I/O 병목의 상대적 비중이 더 커진다.

> [!quote]- 📄 원문 표현 (paper)
> - "Metagenomic analysis suffers from significant data movement overhead due to moving large amounts of low-reuse data from the storage system to the rest of the system." (p.660)
> - "analyzing the data, sequenced and basecalled by a sequencer in 48 hours, takes 38 days on a high-end server node" (p.661)
> - "for the 0.3-TB and 0.6-TB Kraken2 databases, using a state-of-the-art PIM accelerator [64] of Kraken2, No-I/O is on average 26.1× (3.0×) faster than SSD-P" (p.665)

## 핵심 통찰 (Key Insight)
1. **Cooperative ISP** — SSD 내부의 제한된 하드웨어 자원(작은 내부 DRAM, 다수의 경량 코어) 때문에 파이프라인 전체를 SSD 안에 욱여넣는 접근은 비현실적이다. MegIS는 대신 각 하위 작업을 host와 SSD 중 "가장 적합한" 곳에 배치하고(task partitioning), 두 시스템의 강점(host의 큰 DRAM·범용 연산력, SSD의 큰 내부 대역폭·low-reuse 데이터 근접성)을 파이프라인으로 결합한다(p.660, §4). 이 통찰이 효과적인 이유는, 기존 ISP 연구들이 "전부 SSD 안에서" 혹은 "SSD는 안 쓰고" 식의 이분법에 갇혀 R-Qry(random access, SSD 대역폭 활용 실패)나 S-Qry(streaming이지만 연산·메모리 요구량 과다)의 함정에 빠졌던 것을 회피하기 때문이다(p.660, §3.3).
2. **Storage-technology-aware algorithmic redesign** — k-mer 매칭이라는 동일 알고리즘 문제를, NAND flash의 순차 read에 최적화된 형태로 재구성한다. 구체적으로 k-mer를 lexicographic range로 버킷팅해 정렬-전송을 파이프라인화하고(§4.2), 교집합 탐색을 채널당 2-register만으로 flash data stream 위에서 직접 수행하며(§4.3.1), taxID 조회를 포인터 체이싱이 필요한 ternary search tree 대신 순차 스트리밍이 가능한 K-mer Sketch Streaming(KSS) 테이블 구조로 대체한다(§4.3.2, Fig.7). 이 재설계 덕분에 SSD 내부에 필요한 버퍼가 채널당 입출력 각 64KB 수준으로 작아지고(p.666), 정규 FTL 대비 L2P 메타데이터도 0.1%→더 coarse한 block-level 매핑으로 축소된다(§4.5, p.668).
3. **경량 in-storage 가속기 + FTL 확장** — 새로운 대형 로직 유닛을 추가하는 대신 SSD 컨트롤러의 기존 임베디드 코어/FTL을 확장하는 방식(Intersect 유닛, k-mer 레지스터, Index Generator, Control Unit)으로 구현해, ISP 미사용 시 SSD가 범용 SSD로 그대로 동작할 수 있게 한다(p.660, §4). 8-채널 SSD 기준 가속기 총 면적 0.04mm², 전력 7.658mW로, SATA3 SSD 컨트롤러 면적의 1.7% 오버헤드에 불과하다(Table 2, p.671-672).

> [!quote]- 📄 원문 표현 (paper)
> - "The key idea of MegIS is to enable cooperative ISP for metagenomics, where we do not solely focus on processing inside the storage system but, instead, capitalize on the strengths of processing both inside and outside the storage system." (p.660)
> - "By only using two registers, MegIS directly computes on the flash data stream at low cost." (p.666)
> - "MegIS's hardware accelerator area and power requirements are small: only 0.04 mm² and 7.658 mW ... which is 1.7% of the three 28-nm ARM Cortex R4 cores ... in a SATA3 SSD controller." (p.671-672)

## 설계 / 메커니즘 (Design)
MegIS는 host와 SSD가 협력하는 3단계 파이프라인으로 구성된다(Fig.4, p.665).

- **Step 1 — Preparing the Input Queries (host 실행, §4.2)**: 입력 read set에서 k-mer를 추출(§4.2.1, Fig.5)하고 lexicographic 범위 기준 버킷으로 분할·정렬하며(§4.2.2), 사용자 지정 빈도 기준으로 지나치게 흔하거나(빈번) 매우 드문 k-mer를 제외한다(§4.2.3). 이 단계는 host의 큰 DRAM과 연산력을 쓰는 것이 유리하고, SSD 내부 쓰기(flash write)를 유발하지 않는다는 lifetime 이점도 있다(p.665). 버킷 i를 SSD로 전송하는 동안 host는 버킷 i+1을 정렬해 Step 2와 파이프라인으로 겹친다.
- **Step 2 — Finding Candidate Species (SSD 내부, §4.3)**: (a) Intersection Finding — 채널마다 배치된 Intersect 유닛이 host에서 온 정렬된 query k-mer 버킷과 flash에서 스트리밍되는 database k-mer를 비교해 교집합을 찾는다(Fig.6, p.667). Current/Next k-mer 레지스터 두 개만으로 순차 비교를 수행해 SSD 내부 DRAM 대역폭 압박을 피한다. (b) Retrieving TaxIDs — 교집합 k-mer들의 taxID를, 기존 ternary search tree(포인터 체이싱, flash 접근 시 costly) 대신 새로 제안한 K-mer Sketch Streaming(KSS) 데이터 구조로 조회한다(Fig.7-8, p.667-668). KSS는 최대 k(=k_max)의 sketch만 저장하고 짧은 k-mer는 긴 k-mer의 prefix로 유도해, 순차 스트리밍만으로 모든 taxID를 단일 패스에 찾는다.
- **Step 3 — Abundance Estimation (선택적, §4.4)**: read mapping 기반 정밀 abundance 추정이 필요할 경우, 개별 종 reference genome 인덱스 대신 SSD 내부에서 후보 종들의 unified reference index를 생성한다(Fig.9, p.668). 각 flash 채널에서 종별 인덱스를 순차로 읽어 공통 k-mer의 위치를 오프셋 조정하며 병합, host/가속기의 read mapper(GenCache 등)에 넘긴다.
- **MegIS FTL (§4.5, Fig.10, p.668-669)**: 데이터베이스를 모든 채널에 균등·순차 배치하고, 표준 page-level L2P 대신 block-level 매핑(시작 LPA + DB 크기 + 채널 순서만 저장)을 사용해 4TB DB 기준 L2P 메타데이터가 페이지당 4바이트에서 블록당 4바이트 수준(약 1.3MB)으로 축소된다. ISP 동안 필요한 전체 MegIS FTL 메타데이터는 최대 2.6MB(p.669).

> [!quote]- 📄 원문 표현 (paper)
> - "MegIS FTL needs simple changes to the baseline FTL to handle communication between the host and the SSD." (p.668)
> - "MegIS only requires ∼1.3 MB to store a 4-TB database, assuming a physical block size of 12 MB" (p.669)
> - "KSS leads to 7.5× smaller data structures compared to the 107-GB data structure in [ternary search tree], and 2.1× larger compared to [flat sketch tables]" (p.668)

## 평가 (Evaluation)
평가는 자체 제작 시뮬레이터(Verilog 합성 + Design Compiler, Ramulator/MQSim 기반 SSD 시뮬레이션)와 실제 AMD EPYC 7742(128코어, 1TB DRAM) 서버 측정을 결합했다(§5, p.669). 두 SSD 구성(SSD-C: SATA3 600MB/s, SSD-P: PCIe Gen4 8GB/s, Table 1)과 CAMI 벤치마크(CAMI-L/M/H, 각 100M read)를 사용, 데이터베이스는 NCBI 155,442개 microbial genome 기반으로 Kraken2용 293GB, Metalign/MegIS용 701GB k-mer DB + 6.9GB(Metalign) 혹은 14GB(MegIS KSS) sketch DB를 생성했다(p.669).

- **End-to-end 속도**: MegIS 전체 구현(MS)은 P-Opt(Kraken2, 성능 최적화) 대비 SSD-C에서 5.3-6.4×, SSD-P에서 2.7-6.5× 빠르고, A-Opt(Metalign, 정확도 최적화) 대비 SSD-C 12.4-18.2×, SSD-P 6.9-20.4× 빠르다(Fig.11, p.670). 논문 요약(p.660)에서는 이를 "2.7-37.2× (vs Kraken2), 6.9-100.2× (vs Metalign)"로 표현한다(데이터베이스 크기 스케일까지 포함한 범위, Fig.13 참조).
- **정확도**: MegIS의 KSS 기반 taxID 조회는 Metalign(A-Opt)과 동일한 k-mer/sketch 집합을 사용하므로 end-to-end 정확도가 A-Opt와 "동일"하다(p.660-661). A-Opt는 P-Opt 대비 4.8× 높은 F1, 13% 낮은 L1 norm error를 보인다(p.669).
- **PIM 가속기 대비**: 최신 processing-in-memory k-mer 매칭 가속기 Sieve(ISCA'21) 통합 파이프라인 대비, MegIS는 SSD-C/SSD-P에서 4.8-5.1×(1.5-2.7×) 속도 향상과 4.8× 높은 F1을 동시에 달성한다(Fig.18, p.671).
- **에너지/면적**: 전 SSD·데이터셋 평균 에너지 감소는 P-Opt 대비 5.4×(최대 9.8×), A-Opt 대비 15.2×(최대 25.7×), PIM 대비 1.9×(최대 3.5×)이며(§6.4, p.672), 가속기 면적 오버헤드는 SATA3 컨트롤러 대비 1.7%(Table 2, p.671-672).
- **스케일링**: 데이터베이스가 3배 커지면 MegIS의 P-Opt 대비 속도 향상도 최대 5.6×(SSD-C)/3.7×(SSD-P)로 증가하고(Fig.13, p.670), SSD 8개로 확장 시에도 여전히 큰 속도 향상(6.9-9.9×/5.2×대)을 유지한다(Fig.14, p.670). Host DRAM이 32GB로 작아져도 MegIS는 최대 38.5× 속도 향상을 유지(Fig.15, p.671) — 이는 MegIS의 bucketing이 host DRAM-SSD 간 불필요한 page swap을 없애기 때문(p.671).

> [!quote]- 📄 원문 표현 (paper)
> - "MegIS provides 2.7–37.2× and 1.5–5.1× speedup compared to Kraken2 and Sieve, respectively, while achieving significantly higher accuracy." (p.660)
> - "MegIS provides large average energy reductions of 5.4× and 1.9× compared to Kraken2 and Sieve, respectively, and 15.2× compared to accuracy-optimized Metalign." (p.661)
> - "MegIS's benefits come at a low area cost of 1.7% over the area of the three cores [in an SSD controller]." (p.661)

## 섹션 노트
- **§1 Introduction**: 메타지노믹의 정의·중요성과, 시퀀싱/베이스콜링 대비 분석 단계가 반복성·처리량 격차로 인해 병목이 되는 이유 3가지(다중 재분석, 시퀀싱 처리량 급증, 신기술 필요성)를 제시.
- **§2 Background**: 메타지노믹 분석의 두 태스크(presence/absence, abundance estimation)와 R-Qry/S-Qry 도구 분류(Kraken2 vs Metalign), 그리고 NAND SSD 구조(package/controller/DRAM)와 ISP의 3가지 이점을 정리.
- **§3 Motivational Analysis**: Kraken2/Metalign을 SSD-C/SSD-P/No-I/O로 비교해 I/O 오버헤드가 압도적임을 정량 입증하고, 샘플링이나 전체 DRAM 상주 같은 대안이 왜 부적합한지(정확도 손실 vs 에너지·확장성 문제) 논증.
- **§4 MegIS**: task partitioning, 3단계 파이프라인, k-mer bucketing/intersection/KSS taxID 조회, unified index 기반 abundance estimation, MegIS FTL의 block-level L2P 설계를 상세 기술.
- **§5 Evaluation Methodology**: 시뮬레이터 구성(Verilog+Design Compiler+Ramulator+MQSim), SSD-C/SSD-P 스펙(Table 1), baseline 도구(P-Opt=Kraken2+Bracken, A-Opt=Metalign, PIM=Sieve), CAMI 데이터셋 생성 절차.
- **§6 Evaluation**: presence/absence 속도·시간분해(Fig.12)·DB 크기/SSD 개수/내부 대역폭/host DRAM 민감도 분석, 비용 효율(SSD-C+저사양 host로도 우위), PIM 비교, abundance estimation 별도 분석(Fig.19, MS-NIdx ablation), area/power/energy.
- **§7 Related Work / §8 Conclusion**: 기존 유전체 분석 가속(GPU/FPGA/PIM)과 범용 ISP 프레임워크들을 나열하며, MegIS가 end-to-end 메타지노믹 파이프라인을 겨냥한 최초의 cooperative ISP 시스템임을 재확인.

## 핵심 용어 (Key terms)
- **k-mer**: 시퀀스(read 또는 reference genome)에서 추출한 길이 k의 부분 문자열. presence/absence 판별의 기본 단위.
- **taxID (taxonomic identifier)**: reference genome이 속한 종/분류군을 나타내는 정수 식별자.
- **ISP (In-Storage Processing)**: 데이터를 host로 옮기지 않고 저장 장치(SSD) 내부에서 직접 처리하는 기법.
- **Cooperative ISP**: MegIS가 제안한 개념으로, 파이프라인의 하위 작업을 host와 SSD 중 최적의 위치에 나누어 배치하고 겹쳐 실행하는 방식.
- **R-Qry / S-Qry**: 데이터베이스 접근 패턴 분류 — random-access query(예: Kraken2)와 streaming-access query(예: Metalign).
- **KSS (K-mer Sketch Streaming)**: MegIS가 제안한, ternary search tree 대신 순차 스트리밍만으로 다양한 길이의 k-mer에 대한 taxID를 조회하는 sketch 데이터 구조(Fig.7).
- **L2P mapping (Logical-to-Physical)**: SSD FTL이 논리 페이지 주소를 물리 페이지 주소로 매핑하는 메타데이터; MegIS FTL은 이를 block-level로 coarse화.
- **Sketch database**: variable-sized k-mer를 위해 대표 부분집합만 저장해 taxID를 조회하는 공간 효율적 자료구조(CMash 기반).
- **Abundance estimation**: presence/absence로 식별된 후보 종들의 시료 내 상대 빈도를 추정하는 단계(read mapping 또는 lightweight statistics로 수행).
- **Bucketing (lexicographic range partitioning)**: query k-mer를 lexicographic 범위별 버킷으로 나눠 정렬·전송·Step2 교집합 탐색을 파이프라인화하는 MegIS의 입력 처리 기법.

## 강점 · 한계 · 열린 질문
- **강점**: 새 가속기 로직을 최소화(면적 1.7% 오버헤드)하면서도 host/SSD 양쪽 자원을 모두 활용하는 설계로 실제 SSD 하드웨어 제약(작은 내부 DRAM, 제한된 코어 성능) 안에서 구현 가능성을 구체적으로 논증한 점. Kraken2/Metalign/Sieve 세 가지 이질적 baseline(SW 성능·SW 정확도·PIM HW)과 모두 비교해 우위를 다각도로 검증. ISP 비활성 시 SSD가 범용 SSD로 그대로 기능해 실사용 배치 장벽을 낮춤(p.665, FTL 확장만).
- **한계**: 평가가 시뮬레이터(Ramulator/MQSim + Verilog synthesis) 기반이며 실리콘 프로토타입 검증은 아님. NAND flash 기반 SSD에 특화되어 있고(p.663 각주 1) emerging memory-semantic storage(CXL 등)로의 일반화는 향후 과제로 남겨짐. Step 1(k-mer 추출·정렬)이 여전히 host DRAM·연산에 의존하므로, host 자원이 매우 제한적인 환경(예: edge 배치)에서의 이점은 상대적으로 작아질 수 있음(§6.1 DRAM sensitivity 분석은 32GB까지만 다룸).
- **열린 질문**: 여러 SSD로 분산할 때 병목이 host의 정렬 단계로 이동한다는 관찰(§6.1, Fig.14, p.670)이 지적하듯, host-side 정렬을 가속(예: 정렬 가속기 [204-206])하지 않으면 초대형 SSD 클러스터 스케일링의 상한이 host 쪽으로 넘어갈 가능성. 다른 read mapping 알고리즘(long-read seed-and-extend 등)과의 통합 시 KSS/unified index 설계가 그대로 유효한지도 추가 검증이 필요해 보임.

## ❓ Q&A (자가 점검)
> [!question]- MegIS가 "cooperative ISP"라고 부르는 이유는 무엇이고, 왜 "전부 SSD 안에서 처리"하는 접근과 다른가?
> MegIS는 파이프라인의 각 하위 작업(k-mer 추출/정렬은 host, intersection/taxID 조회는 SSD, index 병합은 SSD)을 가장 적합한 곳에 배치하고 두 부분을 겹쳐 실행한다. 전부 SSD에서 처리하려는 접근은 SSD의 작은 DRAM·연산력 한계에 부딪히고(§3.3), R-Qry(랜덤 접근으로 SSD 채널 충돌)나 S-Qry(스트리밍은 되지만 연산·메모리 요구 과다) 어느 쪽도 그대로 이식하기 어렵다(p.660).

> [!question]- MegIS가 taxID 조회에 ternary search tree 대신 KSS를 쓰는 이유는?
> Ternary search tree는 임의 길이 k-mer의 taxID를 찾기 위해 포인터 체이싱이 필요하고, 이는 SSD 내부 DRAM보다 훨씬 느린 flash 접근 시 특히 비용이 크다(p.667). KSS는 가장 긴 k-mer(k_max)의 sketch만 저장하고 더 짧은 k-mer는 그 prefix로 유도해, 정렬된 두 테이블을 순차 스트리밍만으로 한 번에 훑어 taxID를 찾을 수 있게 한다(Fig.7, p.667-668).

> [!question]- Intersection Finding 유닛이 SSD 내부 DRAM 대역폭을 절약하는 구체적 방법은?
> Intersect 유닛은 채널당 두 개의 k-mer 레지스터(현재/다음)만 사용해 flash에서 스트리밍되는 database k-mer와 host에서 온 정렬된 query k-mer를 직접 비교한다. 데이터 전체를 내부 DRAM에 버퍼링하지 않고 flash data stream 위에서 바로 연산하므로, 채널당 64KB급의 작은 버퍼만 필요하다(§4.3.1, p.666-667).

> [!question]- 데이터베이스 크기가 커질수록 MegIS의 상대적 이점이 왜 커지는가?
> I/O 오버헤드는 데이터베이스 크기에 비례해 커지는데(§3.2, Fig.3에서 R-Qry의 SSD-C/No-I/O 격차가 0.3TB→0.6TB에서 7.1×→12.5×로 확대), MegIS는 이 I/O를 SSD 내부 대역폭으로 흡수하므로 DB가 3배 커질 때 P-Opt 대비 속도 향상이 최대 5.6×/3.7×까지 늘어난다(Fig.13, p.670).

> [!question]- MegIS FTL이 표준 FTL 대비 L2P 메타데이터를 어떻게 줄이는가?
> 표준 FTL은 4KiB 페이지 단위로 L2P를 유지해 저장 데이터의 약 0.1% 크기가 필요하지만(p.663), MegIS FTL은 데이터베이스를 채널에 순차·균등 배치하는 것을 활용해 시작 LPA + DB 크기 + 채널 순서만 저장하는 block-level 매핑을 쓴다. 4TB DB 기준 필요 메타데이터가 약 1.3MB로 줄어든다(§4.5, p.668-669).

> [!question]- MegIS를 PIM 가속기(Sieve)와 비교했을 때의 핵심 차이는 무엇인가?
> Sieve 같은 PIM 가속기는 compute/memory 병목은 없애지만 storage에서 데이터를 가져오는 I/O 오버헤드는 그대로 남는다(§3.2 말미, p.665). MegIS는 이 I/O 자체를 줄이므로 Sieve 통합 파이프라인 대비 SSD-C/SSD-P에서 4.8-5.1×(1.5-2.7×) 빠르고, 더 풍부한 k-mer/sketch를 쓰기 때문에 정확도(F1)도 더 높다(Fig.18, p.671).

> [!question]- MegIS의 하드웨어 오버헤드가 작다고 주장하는 근거는?
> 8채널 SSD 기준 전체 가속기(Intersect, k-mer 레지스터, Index Generator, Control Unit) 면적이 0.04mm², 전력 7.658mW이며, 이는 SATA3 SSD 컨트롤러 내 3개 ARM Cortex R4 코어 면적의 1.7%에 불과하다(Table 2, §6.3, p.671-672).

## 🔗 Connections
[[In-Storage Computing]] · [[ISCA]] · [[2024]]
관련: [[BeaconGNN - Large-Scale GNN Acceleration with Asynchronous In-Storage Computing]] · [[OptimStore - In-Storage Optimization of Large Scale DNNs with On-Die Processing]] · [[Smart-Infinity - Fast Large Language Model Training using Near-Storage Processing on a Real System]]

## References worth following
- Wood & Salzberg, "Kraken2" (Genome Biology, 2019) — MegIS가 P-Opt(성능 최적화) baseline으로 사용한 대표적 R-Qry 메타지노믹 분류 도구.
- Firtina et al., "Metalign" (Bioinformatics, 2020) — A-Opt(정확도 최적화) baseline, S-Qry 접근과 sketch 기반 taxID 조회 방식의 근거.
- Wu et al., "Sieve: Scalable In-situ DRAM-based Accelerator Designs for Massively Parallel k-mer Matching" (ISCA, 2021) — MegIS가 비교한 최신 PIM 가속기, k-mer 매칭 자체 가속의 한계(I/O 미해결)를 보여주는 대조군.
- Ounit et al., "CLARK" 및 Marcelino et al. 관련 references — 다양한 sketch/k-mer 기반 taxonomic classification 방법론으로 MegIS의 sketch database 설계와 비교 참고할만함.
- Liu & Schmidt, "CMash" — MegIS의 sketch 생성 방식이 기반한 알고리즘.

## Personal annotations
<!-- 본인 메모 영역 -->
