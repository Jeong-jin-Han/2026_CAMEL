---
title: "SAGe: A Lightweight Algorithm-Architecture Co-Design for Mitigating the Data Preparation Bottleneck in Large-Scale Genome Sequence Analysis"
description: "압축된 게놈 시퀀스 데이터의 압축해제/포맷팅 병목(data preparation bottleneck)을 해소하기 위해, 유전체 데이터의 통계적 특성을 이용한 경량 압축 알고리즘과 스트리밍 전용 하드웨어를 공동설계한 시스템"
venue: HPCA
year: 2026
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - cluster/isc
  - venue/hpca
  - year/2026
  - list/26s-v2
  - topic/genomics
  - topic/data-compression
  - topic/near-data-processing
  - topic/hardware-accelerator
---

# SAGe: A Lightweight Algorithm-Architecture Co-Design for Mitigating the Data Preparation Bottleneck in Large-Scale Genome Sequence Analysis

> **HPCA 2026** · cluster/isc · Source: [SAGe - A Lightweight Algorithm-Architecture Co-Design for Mitigating the Data Preparation Bottleneck in Large-Scale Genome Sequence Analysis.pdf](<SAGe - A Lightweight Algorithm-Architecture Co-Design for Mitigating the Data Preparation Bottleneck in Large-Scale Genome Sequence Analysis.pdf>)

저자: Nika Mansouri Ghiasi¹, Talu Güloglu¹, Harun Mustafa¹, Can Firtina¹,², Konstantina Koliogeorgi¹, Konstantinos Kanellopoulos¹, Haiyu Mao³, Rakesh Nadig¹, Mohammad Sadrosadati¹, Jisung Park⁴, Onur Mutlu¹ — ¹ETH Zürich, ²University of Maryland, ³King's College London, ⁴POSTECH

## TL;DR
게놈 시퀀스 데이터는 저장 공간 문제로 항상 압축 상태로 보관되는데, genome analysis accelerator가 이 데이터를 쓰려면 매번 압축해제·포맷팅(data preparation)을 거쳐야 하고, 이 과정이 accelerator 자체의 가속 이득을 크게 갉아먹는다는 것을 저자들은 실측으로 보인다. SAGe는 이 병목을 없애기 위한 algorithm-architecture co-design으로, (i) 유전체 데이터셋마다 다른 통계적 경향(mismatch position 분포, indel 길이, chimeric read 패턴 등)을 활용해 read를 consensus sequence 대비 튜닝 가능한 bit-width 배열로 인코딩하고, (ii) 이 배열을 랜덤 접근 없이 순차 스트리밍만으로 복원하는 매우 작은 하드웨어(Scan Unit/Read Construction Unit/Control Unit)를 설계하며, (iii) SSD 전 채널 대역폭을 활용하는 데이터 레이아웃과 (iv) 전용 인터페이스 커맨드(SAGe_Read/SAGe_Write)를 함께 제공한다. GEM read-mapping accelerator와 in-storage NDP 가속기 GenStore에 통합했을 때 각각 pigz/(Nano)Spring 대비 최대 32.1× 성능, 34.0× 에너지 효율 개선을 보이면서도 하드웨어 면적은 SSD 컨트롤러 코어의 0.7%에 불과하다.

## 문제 & 동기
게놈 시퀀스 데이터(FASTQ read set)는 매년 규모가 폭발적으로 늘고 있어(공개/사설 저장소 모두 몇 년마다 자릿수 단위로 증가), 압축 없이 저장하는 것은 비현실적이다(p.3). 그래서 genomics-specific compressor(예: Spring, NanoSpring 등)로 압축해 보관하는 것이 표준 관행인데, 문제는 이 압축이 read mapping 등 genome analysis의 매 실행마다 먼저 완전히 압축해제되어야 한다는 점이다. 저자들은 최신 hardware read-mapping accelerator(GEM)를 SW/HW 압축해제 도구와 결합해 실제 파이프라인을 시뮬레이션한 결과, accelerator가 분석 자체를 빠르게 만들수록 data preparation이 상대적으로 더 큰 병목이 되어 가속 이득의 상당 부분이 사라짐을 보였다(Fig.1, p.2). 8개 DRAM 채널·128 코어·256 스레드의 고사양 서버에서도 기존 genomic decompressor는 32 스레드를 넘으면 메모리 대역폭 포화로 더 이상 빨라지지 않는다(p.6) — 즉 decompression 자체가 대용량 랜덤 접근을 요구하는 자원 집약적 작업이라, FPGA/ASIC NDP·포터블 genomics 장치처럼 자원이 제한된 환경에는 아예 이식할 수 없다는 것이 핵심 동기다.

> [!quote]- 📄 원문 표현 (paper)
> - "we demonstrate a major bottleneck that greatly limits and diminishes the benefits of state-of-the-art genome sequence analysis accelerators: the data preparation bottleneck, where genomic sequence data is stored in compressed form and needs to be first decompressed and formatted before an accelerator can operate on it." (p.1)
> - "If the data preparation bottleneck is eliminated, there would be 12.3× and 4.0× average speedup for pigz and (N)Spr." (p.6)
> - "across our datasets, we observe that the state-of-the-art genomic decompressors require random accesses to large amounts of data (up to 26 GB) and with high bandwidth. Consequently, even on a high-end system used in our analysis, with eight DRAM channels, 128 cores, and 256 hardware threads, the performance of these genomic decompressors saturates after 32 threads due to insufficient main memory bandwidth." (p.6)

## 핵심 통찰 (Key Insight)

**1. 유전체 데이터의 mismatch 정보는 데이터셋별로 통계적 경향을 갖는다 (property-based tunable encoding).** 저자들은 read set마다 (i) delta-encoded mismatch position을 표현하는 데 필요한 비트 수 분포, (ii) read당 mismatch 개수, (iii) indel block 길이 분포, (iv) chimeric read의 다중 matching position 등을 실측 분석해 6가지 property로 정리했다(§5.1.1–5.1.3, p.7–9). 이 경향을 매 read set마다 array/guide array의 bit-width로 "튜닝"하면, 비싼 backend general-purpose compressor(엔트로피 코딩 등) 없이도 genomics-specific compressor에 필적하는 압축률을 얻을 수 있다. 왜 효과적인가 하면, 압축률의 대부분은 애초에 consensus 기반 encoding 자체에서 나오고, backend compressor는 이미 많이 걸러진 잔여 정보에 대해 상대적으로 적은 추가 이득만 주기 때문에, 그 적은 이득을 포기하는 대신 훨씬 단순한 하드웨어로 대체할 수 있다.

> [!quote]- 📄 원문 표현 (paper)
> - "SAGe is based on the key insight that the information encoded in genomic sequence data follows specific trends, shaped by factors such as sequencing technology (e.g., error rates and read lengths) and common genetic phenomena (e.g., typical spatial distributions of genetic variations within genomes)." (p.2)
> - "We observe that most delta-encoded mismatch positions need only a few bits to store (Property 1)." (p.7)

**2. 스트리밍 접근만으로 복원 가능한 데이터 구조 설계.** 기존 genomic decompressor는 매칭 패턴을 찾기 위해 큰 자료구조에 대한 랜덤 접근이 필요한 반면, SAGe는 read의 mismatch 정보를 등장 순서대로 배열에 순차 저장하므로 decompression 시 레지스터 몇 개만으로 순차 스캔이 가능하다. 이 덕분에 대형 버퍼나 DRAM 대역폭 없이도 SSD 내부처럼 극도로 자원이 제한된 환경(NDP)에 하드웨어를 얹을 수 있다.

> [!quote]- 📄 원문 표현 (paper)
> - "SAGe (i) stores mismatch information in data structures that can be efficiently decoded by lightweight operations and streaming accesses, and (ii) minimizes their sizes by leveraging read set properties." (p.6)
> - "SAGe enables decompression with streaming accesses, and thus, the SU and RCU do not rely on large buffers, and instead only require small registers." (p.9)

**3. 압축(1회, host)과 압축해제(critical path, HW)의 역할 분리.** 압축은 genome analysis의 critical path에 있지 않으므로 host CPU에서 오프라인으로 한 번 수행하고, 대신 반복적으로 실행되는 압축해제만 lightweight 전용 하드웨어(SU/RCU/CU)로 옮긴다. 이렇게 하면 압축 시점에는 Algorithm 1(비트 카운트 탐색)처럼 다소 무거운 최적화를 자유롭게 적용하면서도, 실제 배포되는 하드웨어는 아주 단순하게 유지할 수 있다.

## 설계 / 메커니즘 (Design)

**압축 알고리즘과 데이터 구조 (§5.1, Fig.6 p.7).** SAGe는 기존 genomic compressor처럼 consensus sequence 대비 각 read의 mismatch(위치·염기·타입)를 인코딩하되, 이를 세 개의 배열 — Matching Position Array(MPA/MMPA), Mismatch Base and Type Array(MBTA), 그리고 각 배열의 entry별 bit 폭을 지정하는 guide array(MMPGA 등) — 로 순차 저장한다. Guide array의 bit-width는 read set마다 Algorithm 1(bit-count boundary를 히스토그램 기반으로 탐색, p.7)로 튜닝되고, 자주 등장하는 bit-count에는 더 짧은 prefix code를 배정한다. Mismatch position은 Property 1·2를 이용해 짧게, indel은 Property 3에 따라 "첫 mismatch 위치 + indel 길이"만 저장(단일-길이 indel엔 1비트, 그 외엔 8비트, p.8), chimeric read는 Property 4에 따라 top-N(N=3) matching position만 고려해 mismatch 수를 줄인다(Fig.9, p.9). Mismatch base/type은 Property 5를 이용해 consensus와 다르면 substitution, 같으면 indel이라는 논리로 타입 비트를 생략한다. Matching position 자체도 Property 6(reads를 독립적으로 재정렬 가능)에 따라 delta-encoding된다(Fig.10, p.10). Corner case(N 염기 포함, read 시작/끝의 clip)는 MBTA에 1비트만 추가해 처리한다(§5.1.4, p.10). Quality score는 선택적으로 별도 스트림으로 무손실 압축되며, host CPU에서 압축해제해도 read mapping이 전체 quality score의 극히 일부(평균 0.03%, 최대 10.7%, p.10)만 필요로 하므로 critical path를 막지 않는다(§5.1.5).

**하드웨어 (§5.2, Fig.11 p.10).** SAGe hardware는 Scan Unit(SU, position array·guide array를 순차 스캔해 mismatch 정보 디코딩), Read Construction Unit(RCU, SU가 준 mismatch 정보를 consensus sequence에 꽂아 넣어 read를 재구성), Control Unit(CU, SU-RCU 조정)의 3개 컴포넌트로 구성된다. 레지스터는 8비트(배열 최소 단위)와 150 base-pair short-read 청크 크기 기준으로 설계되었다(p.10).

**데이터 레이아웃 (§5.3, p.11).** 압축된 read set의 consensus sequence + 각 partition의 mismatch 정보를 SSD 채널에 round-robin으로 균등 분산 배치해, 동일 페이지 오프셋을 유지함으로써 multi-plane read를 가능케 하고 SSD의 full bandwidth를 활용한다. Garbage collection 시에도 병렬 유닛 내 모든 블록이 동일 페이지 오프셋을 유지하도록 victim block을 선택한다.

**인터페이스 커맨드 (§5.4, p.11).** `SAGe_Read`(원하는 출력 포맷을 지정해 데이터를 요청하고 SAGe HW가 압축해제/포맷팅해 반환)와 `SAGe_Write`(genomic data를 쓰고 FTL 매핑 메타데이터 갱신)의 2개 신규 커맨드로 baseline FTL을 최소한만 수정한다.

**하드웨어 통합 3가지 케이스 (§6, Fig.12 p.11):** ❶ genome analysis 시스템에 PCIe/CXL로 SAGe HW를 연결(개별 accelerator/GPU처럼), ❷ genome analysis accelerator와 동일 칩에 SAGe HW를 통합(PCIe/CXL 포트 절약), ❸ SSD controller 내부에 in-storage NDP(예: GenStore) 형태로 통합 — NAND flash chip에서 스트리밍으로 읽어와 double-buffering만으로 동작.

> [!quote]- 📄 원문 표현 (paper)
> - "SAGe consists of three components. ① Scan Unit (SU) sequentially scans through input position arrays and position guide arrays to find mismatch information. ② Read Construction Unit (RCU) receives the mismatch information from the SU and reconstructs full reads by plugging the mismatches into the correct positions of the input consensus sequence. ③ Control Unit (CU) coordinates operations between the SU and the RCU." (p.9)
> - "SAGe introduces two new interface commands: one to request genomic data in the desired format, and another to write compressed genomic data to the storage system." (p.11)

## 평가 (Evaluation)
GEM(read-mapping accelerator)에 통합했을 때, SAGe는 pigz·(Nano)Spring·hardware-accelerated (Nano)Spring 대비 평균 12.3×, 3.9×, 3.0× 성능 및 34.0×, 16.9×, 13.0× 에너지 효율 개선을 보인다(p.2, Abstract/Fig.13, p.13 GMean). GenStore(SSD 내 in-storage filter NDP accelerator)에 통합하면 같은 baseline 대비 평균 32.1×, 10.4×, 7.8× speedup을 얻으며, 이때 SAGe HW 면적은 SSD controller 내 3개 코어의 0.7%에 불과하다(p.2). PCIe SSD 시스템에서 SAGe는 이상적 압축해제(`0TimeDec`, 압축해제 시간 0으로 가정)와 성능이 동일해, 파이프라인에서 압축해제가 더 이상 병목이 아님을 보인다(p.13). Data preparation만 따로 보면 pigz·(N)Spr·(N)SprAC 대비 91.3×, 29.5×, 22.3× 속도 향상(Fig.14, p.13). SW 구현(SAGeSW)도 (N)Spr 대비 평균 2.3× 빠르지만, HW 대비 최대 4.0× 느리다(p.13). 다중 SSD 구성에서도 SAGe/SAGeSSD+ISF의 이득이 유지된다(Fig.15, p.14). 압축률은 pigz 대비 평균 2.9× 우수하고 (N)Spr 대비 평균 4.6% 낮은 수준으로 comparable하다(Table 2, p.15). 하드웨어 면적/전력은 8-channel SSD 기준 총 0.002 mm², 0.49 mW(모드 ❸ 포함 시 +0.28 mW)로, 22nm 노드의 mid-range FPGA LUT의 2.5%·flip-flop의 0.8% 수준(Table 1, p.14). Table 3에서 SAGe는 기존 genomics-specific/general-purpose decompression tool(zDEFLATE, GZIP engine, xz, ZSTandard HW, GPUFastqLZ, repaq 등) 대비 가장 높은 decompression throughput을 내면서 압축률도 comparable함을 보인다(p.14).

> [!quote]- 📄 원문 표현 (paper)
> - "SAGe leads to 12.3× (8.1×), 3.9× (2.7×), and 3.0× (2.1×) average speedup over pigz, (N)Spr, and (N)SprAC, respectively" (p.13)
> - "SAGe's logic units (§8.2) consume only 2.5% of the lookup tables and 0.8% of the flip-flops of a mid-range FPGA [323]." (p.2)
> - "SAGe achieves (i) 2.9× better average compression ratio than pigz, and (ii) comparable compression ratios to (N)Spr, with a modest 4.6% average reduction." (p.15)

## 섹션 노트
- **1. Introduction**: data preparation bottleneck을 정의하고 Fig.1 실측으로 근거 제시, SAGe의 4가지 co-design 요소와 key result(GEM/GenStore 각각 대비 speedup) 요약.
- **2. Background**: genomic workflow(sequencing→basecalling→analysis)와 genomics-specific compression(consensus + mismatch encoding, Fig.3) 배경 설명. quality score는 read mapping에서 대부분 불필요함을 언급.
- **3. Motivational Analysis**: pigz/(N)Spr/Ideal 3가지 data prep 설정으로 GEM read mapping과 결합한 end-to-end throughput 측정, 8-DRAM-channel/128-core 서버에서도 32 스레드 이후 메모리 대역폭 포화 관찰.
- **4. SAGe: Overview**: co-design 4요소(compression algorithm/data structure, lightweight HW, data layout, interface command)와 Fig.5의 preparation/compression 흐름 소개.
- **5. SAGe: Detailed Design**: compression algorithm(§5.1, Property 1–6, Algorithm 1), hardware(§5.2, SU/RCU/CU), data layout(§5.3), interface command(§5.4)를 상세 기술.
- **6. Case Studies**: PCIe/CXL 연결, 동일 칩 통합, SSD 내부 NDP 통합 3가지 실제 통합 시나리오 제시.
- **7. Methodology**: 시뮬레이터, 데이터셋(단/장 read 5종, Table 2), 하드웨어 모델링(Verilog+Design Compiler 22nm, Ramulator, MQSim) 설명.
- **8. Evaluation**: 성능(8.1), 면적/전력(8.2), 에너지(8.3), 압축률(8.4), 자원 요구량 비교(8.5), 압축 시간(8.6) 순으로 결과 제시.
- **9. Related Work**: SAGe가 data preparation bottleneck을 다루는 첫 시스템이라 주장, 기존 genome analysis 가속 연구와는 orthogonal하다고 정리.
- **10. Conclusion**: SAGe가 알고리즘-아키텍처 co-design으로 lightweight·high-compression·high-performance를 동시에 달성했다고 요약.

## 핵심 용어 (Key terms)
- **Data preparation bottleneck**: 압축 저장된 게놈 데이터가 accelerator 처리 전에 압축해제·포맷팅되어야 하는 데서 발생하는 병목.
- **Consensus sequence**: read set 내 각 위치에서 가장 흔한 염기로 구성한 근사 genome 서열(reference 또는 read set에서 유도).
- **Mismatch (position/base/type)**: read가 consensus와 다른 위치·염기·종류(substitution/insertion/deletion).
- **MMPA / MMPGA**: Matching (Mismatch) Position Array와 그 bit-width를 지정하는 Position Guide Array.
- **MBTA**: Mismatch Base and Type Array — mismatch 염기와 타입 정보를 저장.
- **Chimeric read**: 서로 다른 genome 영역이 이어붙은 read로, 하나의 matching position만으로는 mismatch가 많아지는 read.
- **Indel block**: 연속된 insertion 또는 deletion 염기들의 묶음.
- **Scan Unit (SU) / Read Construction Unit (RCU) / Control Unit (CU)**: SAGe의 3개 경량 하드웨어 컴포넌트 — 각각 배열 스캔, read 재구성, 제어.
- **SAGe_Read / SAGe_Write**: SAGe가 도입한 전용 스토리지 인터페이스 커맨드(원하는 포맷 지정 읽기 / 압축 데이터 쓰기).
- **In-storage filter (ISF) / GenStore**: SSD 컨트롤러 내부에서 불필요한 read를 걸러 read mapping 부담을 줄이는 in-storage NDP 가속기.

## 강점 · 한계 · 열린 질문
**강점**: (1) 매우 작은 하드웨어 오버헤드(0.002mm², SSD 코어 대비 0.7% 면적)로 genomics-specific compressor에 필적하는 압축률 유지. (2) PCIe/CXL 연결, 동일 칩 통합, SSD 내부 NDP 통합 등 폭넓은 통합 시나리오에서 모두 검증. (3) 압축(오프라인, host)과 압축해제(critical path, HW)의 역할을 분리해 실용적 배포 경로 제시. (4) 다양한 sequencing 기술(short/long read)·species 데이터셋에서 일관된 property 분석과 결과 제시.

**한계**: (1) 평가가 시뮬레이터(Ramulator, MQSim, Design Compiler 합성) 기반이며 실제 실리콘 프로토타입은 없음(p.12에서 스스로 언급). (2) Quality score decompression은 여전히 host CPU 소프트웨어에 의존하며, 논문 스스로 "최대 10.7%까지 접근 비율이 늘면" 안전 마진이 줄어들 수 있음을 인정(p.10). (3) 비교 대상 accelerator가 GEM과 GenStore 두 가지로 한정되어, 다른 유형의 analysis accelerator(assembly, variant calling 등)에 대한 일반화는 추가 검증이 필요. (4) Property 기반 튜닝은 사용된 5개 데이터셋의 통계 특성에 최적화되어 있어, 매우 다른 오류율/read 길이 분포를 갖는 신흥 sequencing 기술(예: 초장 read, 초고오류율)에 대한 재적응 비용은 논문에서 다뤄지지 않음.

**열린 질문**: Reference genome이 자주 갱신되거나 개인화(personalized reference)되는 워크플로우에서 consensus sequence 재구성 비용은 어떻게 상쇄되는가? 다중 SSD/분산 스토리지에서 SAGe의 채널 분산 레이아웃이 GC 오버헤드나 wear-leveling에 미치는 장기적 영향은? Algorithm 1의 bit-count 탐색이 데이터셋 특성 변화(예: 시간에 따른 population diversity 증가)에 얼마나 강건한가?

## ❓ Q&A (자가 점검)

> [!question]- SAGe가 해결하려는 "data preparation bottleneck"이란 정확히 무엇인가?
> 압축 저장된 게놈 시퀀스 데이터를 genome analysis accelerator가 처리하려면 매번 압축해제·포맷팅을 거쳐야 하는데, accelerator 자체가 빨라질수록 이 압축해제/포맷팅 단계가 상대적으로 더 큰 병목이 되어 가속 이득을 갉아먹는 현상(Fig.1, p.2).

> [!question]- SAGe의 압축 알고리즘이 기존 genomic compressor(예: Spring)와 근본적으로 다른 점은?
> 압축률 목표는 같지만(consensus 기반 mismatch encoding), backend에 비싼 general-purpose compressor(엔트로피 코딩 등)를 쓰는 대신, read set별 통계 property(mismatch position bit 분포, indel 길이 등)를 반영해 array/guide array의 bit-width를 튜닝하는 방식으로 lossless하게 압축한다(§5.1, p.7).

> [!question]- SAGe hardware가 왜 큰 버퍼나 DRAM 대역폭 없이 동작 가능한가?
> Mismatch 정보를 read 내 등장 순서대로 배열에 순차 저장하고, matching position도 consensus 순서대로 저장하기 때문에, 압축해제 시 레지스터 몇 개만으로 순차 스캔(streaming access)만 하면 되고 랜덤 패턴 매칭이 필요 없다(§5.2.1, p.9).

> [!question]- Chimeric read는 SAGe에서 어떻게 처리되는가?
> 하나의 matching position만 쓰면 mismatch가 매우 많아지므로(최대 80%), SAGe는 top-N(N=3) matching position을 함께 고려해 mismatch 수를 줄이는 Property 4를 이용한다(Fig.9, p.9).

> [!question]- GenStore와 SAGe를 결합했을 때 얻는 이득의 원천은?
> GenStore는 SSD 내부에서 read mapping이 불필요한 read를 걸러내는 in-storage filter(ISF)인데, 기존에는 필터링 전에 데이터를 SSD 밖으로 꺼내 압축해제해야 해 NDP의 이점이 사라졌다. SAGe는 SSD 컨트롤러 안에서 경량으로 압축해제가 가능한 유일한 구성이라, GenStore의 ISF를 SSD 내부에 그대로 유지하면서 데이터 이동을 없앨 수 있다(§7, p.12).

> [!question]- SAGe가 압축률을 어느 정도 희생하는가?
> pigz 대비로는 오히려 2.9배 더 좋고, state-of-the-art genomics compressor((N)Spr) 대비로는 평균 4.6% 낮은 수준의 comparable한 압축률을 유지한다(Table 2, p.15).

> [!question]- Quality score는 왜 SAGe hardware가 아니라 host CPU에서 압축해제되는가?
> Read mapping 등 대부분의 analysis 단계는 quality score의 극히 일부(평균 0.03%, 최대 10.7%)만 필요로 하므로, quality score decompression 처리량이 파이프라인의 critical path를 막지 않아 host 소프트웨어로도 충분하기 때문이다(§5.1.5, p.10).

## 🔗 Connections
[[In-Storage Computing]] · [[HPCA]] · [[2026]]

## References worth following
- Chandak, Tatwawadi, Ochoa, Hernaez, Weissman. "SPRING: a next-generation compressor for FASTQ data" (Bioinformatics, 2019) — SAGe의 핵심 비교 baseline인 short-read genomics compressor.
- Chandak, Tatwawadi, Weissman. "FaStore: a space-saving solution for raw sequencing data" (Bioinformatics, 2018) — genomics-specific compression의 또 다른 대표 사례로 압축 기법 비교에 참고.
- Mansouri Ghiasi, Cali, Kanellopoulos, Firtina, et al. "GenStore: A High-Performance In-Storage Processing System for Genome Sequence Analysis" (ASPLOS, 2022) — SAGe가 in-storage NDP 통합 사례로 직접 사용하는 accelerator, 저자 그룹의 선행 연구.
- Cali, Kim, Ghiasi, et al. "GenASM: A High-Performance, Low-Power Approximate String Matching Acceleration Framework for Genome Sequence Analysis" (MICRO, 2020) — genome analysis accelerator의 또 다른 대표 사례, GEM과 비교되는 계열.
- Kim, Cali, Xin, et al. "GRIM-Filter: Fast Seed Location Filtering in DNA Read Mapping Using Processing-in-memory Technologies" (BMC Genomics, 2018) — near-data processing 기반 genome analysis 가속의 초기 연구 흐름.

## Personal annotations
<!-- 본인 메모 영역 -->
