---
title: "CIPHERMATCH: Accelerating Homomorphic Encryption-Based String Matching via Memory-Efficient Data Packing and In-Flash Processing"
aliases: [CIPHERMATCH]
description: "동형암호 기반 secure exact string matching을 memory-efficient data packing과 in-flash processing으로 가속하는 알고리즘-하드웨어 공동설계"
venue: ASPLOS
year: 2025
arxiv: "2503.08968"
tier: deep
status: done
tags:
  - paper
  - cluster/isc
  - topic/in-flash
  - topic/homomorphic-encryption
  - topic/security
  - venue/asplos
  - year/2025
---

# CIPHERMATCH: Accelerating Homomorphic Encryption-Based String Matching via Memory-Efficient Data Packing and In-Flash Processing

> **ASPLOS 2025** · cluster/isc · Source: [CIPHERMATCH ….pdf](<CIPHERMATCH ….pdf>)

저자: Mayank Kabra, Rakesh Nadig, Harshita Gupta, Rahul Bera, Manos Frouzakis, Vamanan Arulchelvan, Yu Liang, Haiyu Mao, Mohammad Sadrosadati, Onur Mutlu (ETH Zurich, King's College London)

## TL;DR

- HE(Homomorphic Encryption) 기반 secure **exact string matching**은 (1) 암호화 후 큰 데이터 크기로 인한 메모리 footprint 증가, (2) homomorphic multiplication·rotation 등 값비싼 연산, (3) 대용량 암호화 DB의 data movement 병목 때문에 매우 느리다.
- **CIPHERMATCH**는 알고리즘-하드웨어 공동설계로 세 가지를 동시에 해결한다: (i) 암호화 후 메모리 footprint를 줄이는 최적화된 software 기반 **memory-efficient data packing**, (ii) **homomorphic addition만** 사용해 multiplication/rotation을 제거한 string matching 알고리즘, (iii) NAND-flash의 array-level·bit-level 병렬성을 활용하는 새로운 **in-flash processing (IFP)** 아키텍처.
- 순수 software 구현(CM-SW)은 state-of-the-art arithmetic baseline 대비 성능 **42.9배**, 에너지 **17.6배** 개선. IFP 결합 시(CM-IFP) CM-SW 대비 성능 **136.9배**, 에너지 **256.4배** 추가 개선.
- 두 가지 case study로 입증: (1) exact DNA string matching, (2) encrypted database search. HE operation을 IFP로 수행하는 최초의 연구.

> [!quote]- 📄 원문 표현 (paper)
> - "We propose CIPHERMATCH, which (i) reduces the increase in memory footprint after encryption using an optimized software-based data packing scheme, (ii) eliminates the use of costly homomorphic operations (e.g., multiplication and rotation), and (iii) reduces data movement by designing a new in-flash processing (IFP) architecture." (p.1)
> - "Our pure software-based CIPHERMATCH implementation that uses our memory-efficient data packing scheme improves performance and reduces energy consumption by 42.9× and 17.6×, respectively, compared to the state-of-the-art software baseline." (p.1)
> - "Integrating CIPHERMATCH with IFP improves performance and reduces energy consumption by 136.9× and 256.4×, respectively, compared to the software-based CIPHERMATCH implementation." (p.1)

## 문제 & 동기

- HE는 복호화 없이 암호문 위에서 직접 연산할 수 있어 DNA read mapping, biometric matching, web search 등 privacy-sensitive exact string matching 응용에 핵심이다. 그러나 homomorphic operation은 평문 연산보다 **10^4~10^5배** 느리다.
- 기존 HE 기반 string matching에는 두 접근이 있다.
  - **Boolean approach** (TFHE 기반): 각 bit를 개별 암호화하고 homomorphic XNOR·AND 사용. 임의 길이 query 지원하지만 암호화 후 footprint가 평문 대비 **200배 이상** 커지고 execution time이 arithmetic 대비 평균 600배 높다.
  - **Arithmetic approach** (SHE/BFV 기반): data packing으로 여러 bit를 한 polynomial에 묶고 Hamming Distance(HD)를 homomorphic multiplication·addition으로 계산. 낮은 latency지만 비싼 multiplication에 의존하며(latency의 98.2%가 multiplication) 특정 query 길이만 지원, scalability 부족.
- 두 번째 병목은 **data movement**: 암호화 DB는 평문 대비 최대 1000배 커서 storage→compute 데이터 이동이 성능을 크게 좌우. 암호화 DB가 256GB일 때 SSD controller 내부 연산은 CPU 대비 data transfer latency를 94% 줄일 수 있다.
- 기존 HE accelerator(SHARP 180MB, CraterLake 256MB, BTS 512MB on-chip)는 대용량 암호화 DB string matching에 on-chip 저장이 부족하고, 기존 NDP/PnM/ISP 연구는 data packing을 최적화하지 않거나 SSD controller bandwidth·power 제약에 묶인다.

> [!quote]- 📄 원문 표현 (paper)
> - "homomorphic operations are typically 10^4× to 10^5× slower than their traditional unencrypted counterparts in existing systems." (p.1)
> - "the memory footprint after encrypting individual bits (see §2.2) is larger (by more than 200×) compared to the memory footprint of unencrypted data." (p.5)
> - "98.2% of the latency in secure string matching using the arithmetic approach is due to the expensive homomorphic multiplication operations required to perform secure string matching." (p.5)
> - "For the largest encrypted database size of 256GB, performing all computations within the SSD controller results in a 94% reduction in data transfer latency." (p.6)

## 핵심 통찰

- **Key Takeaway 1**: arithmetic의 data packing은 큰 이득을 주지만 더 최적화 가능하다 — 값비싼 homomorphic multiplication을 최소화하고 단순한 homomorphic **addition**만으로 SIMD 병렬 실행하도록 만들 수 있다.
- **Key Takeaway 2**: 암호화가 데이터 크기를 크게 키우므로 응용이 storage로부터의 data movement에 점점 더 묶인다 — SSD controller·flash memory 내부에서 연산을 수행하면 data movement overhead를 크게 줄일 수 있다.
- 핵심 아이디어: query를 negate(~Q)한 뒤 data(d)에 homomorphic addition하면, 일치 시 결과가 **전부 1로 채워진 string**("match polynomial")이 된다. 즉 string matching을 multiplication 없이 addition만으로 환원.
- packing 통찰: 각 plaintext coefficient에 가능한 최대 bit 수(여기선 16 bit)를 채우면 암호화 후 데이터가 평문 대비 **4배**(lower bound)에 그친다. 기존 arithmetic은 single-bit packing으로 64배까지 커지므로 **16배 footprint 감소**.
- IFP 통찰: NAND-flash의 sensing/data latch 회로로 AND·OR·XOR bitwise 연산을 수행하고, **vertical data layout**으로 carry propagation을 가능케 해 flash 내부에서 **bit-serial addition**(= homomorphic addition)을 수행. P/E cycle 없이 read·latch 연산만 사용.

> [!quote]- 📄 원문 표현 (paper)
> - "Data packing schemes used in arithmetic approaches offer significant performance benefits; however, they can be further optimized to minimize the use of costly homomorphic multiplication operations and to exploit highly parallel SIMD units to execute simpler homomorphic addition operations." (p.5, Key Takeaway 1)
> - "As encryption greatly increases the data size, applications become increasingly bound by data movement from storage, i.e., inside the SSD controller and flash memory can greatly reduce the data movement overheads." (p.6, Key Takeaway 2)
> - "Our technique achieves this 16× memory overhead reduction by packing the maximum possible bits (in our case, 16 bits) into the plaintext coefficients, whereas prior work [27] uses a single-bit data packing approach." (p.7)
> - "Assume a binary query (Q); it is first negated (∼Q) and added to a binary data (d) such that, if there is a match, the result will be a string of all 1's." (p.7)

## 설계 / 메커니즘

### 알고리즘 (§4.2)
- **Memory-efficient data packing (§4.2.1)**: 길이 k의 binary string P를 t-bit(=16) 단위 partition으로 나눠 packed message m(T) 구성 → 최대 degree n(=1024) plaintext polynomial M(x)로 변환 → 원소 수가 n 초과 시 여러 polynomial로 분할 → public key로 암호화하여 C^(j)(x) 생성. parameter: n=1024, q=32-bit coefficient, t=16-bit.
- **Secure string matching (§4.2.2)**: query Q를 negate(~Q)해 packed vector로 만들고 plaintext polynomial 전반에 replicate해 SIMD 병렬 매칭. n번 left-shift로 모든 정렬(alignment) variant P_1..P_8 생성 → 각각 암호화 → DB와 homomorphic addition → 결과 ciphertext의 어떤 coefficient가 "match polynomial"(전부 1)이면 match, location index 생성해 client에 반환.
- client-server 모델(Algorithm 1): Database Preparation(server) → Query Preparation(client) → Secure String Search(server).

### 하드웨어 (§4.3)
- **IFP architecture (§4.3.1)**: ParaBit·Flash-Cosmos에서 영감. S-latch와 D-latch에 트랜지스터 2개(M7, M8) 추가해 **bi-directional data flow**를 가능케 함 → AND, OR, XOR을 latch 사이에서 수행하고 중간 결과 재사용. TLC NAND를 SLC 모드로 운용하고 ESP(Enhanced SLC Programming)로 신뢰성 확보.
- **Bit-serial addition (Figure 5)**: full-adder(S_i = A_i⊕B_i⊕C_i, C_o = (A_i⊕C_i)·B_i + A_i·C_i)를 6-step으로 latch 연산만으로 수행, 32-bit coefficient를 32개 wordline에 vertical하게 배치해 bitline 단위로 직렬 가산. bitline-/wordline-/chip-/channel-level 병렬성 모두 활용.
- **End-to-end system (§4.3.2, Figure 6)**: SSD 물리 주소를 conventional 영역(TLC, horizontal)과 CIPHERMATCH 영역(SLC, vertical)으로 분할. software-based **data transposition unit**으로 vertical↔horizontal 변환(4KB granularity). 신규 명령 CM-read, CM-write, **CM-search**, FTL 내 **bop_add** μ-program 추가. index generation은 SSD controller가 수행(추정 3.42μs, flash read와 overlap).
- **Overhead (§6.3)**: SSD-internal DRAM space 0.5MB(homomorphic addition 결과 저장), μ-program <1KB. NAND die 면적 overhead 약 0.6%(ParaBit 0.6% + Flash-Cosmos ESP 0%). hardware data transposition unit(선택): 4KB transpose 158ns, 0.24mm². AES index 암호화 unit: 16B 12.6ns, 0.13mm².

> [!quote]- 📄 원문 표현 (paper)
> - "CIPHERMATCH algorithm comprises two key components: (1) a memory-efficient data packing scheme to reduce the memory footprint of encrypted data, and (2) a secure string matching algorithm that utilizes only homomorphic addition operations." (p.6)
> - "we adopt a vertical data layout, where the bits of the operands are arranged along bitlines instead of wordlines. In this layout, the bits of each operand are distributed across multiple wordlines, with each bit of a 32-bit element stored in a separate bitline." (p.9)
> - "We add two transistors (M7 and M8 in Figure 4) in the existing NAND flash peripheral circuit. We utilize these transistors to enable bi-directional data flow." (p.8)
> - "CIPHERMATCH performs bit-serial addition completely using the latching circuit present in the flash chips, which avoids performing costly program/erase (P/E) cycle operations in the flash cell array that degrade the lifetime of flash memory." (p.10)

## 평가

- **방법론(§5)**: 실시스템 — Intel Xeon Gold 5118(6 cores OoO, 32GB DDR4-2400, Samsung 980 Pro 2TB NVMe), RAPL로 에너지 측정. 시뮬레이션 — CM-SW(compute-centric), CM-PuM(SIMDRAM 기반 memory-centric), CM-PuM-SSD(storage-centric), CM-IFP(in-flash). baseline: Boolean[17] TFHE-rs, arithmetic[27] Microsoft SEAL. workload: (1) 32GB DNA DB(암호화 후 128GB), query 8~128bp; (2) encrypted DB search 2~32GB(암호화 후 8~128GB), 1000 queries.
- **CM-SW(software, §6.1)**: Boolean 대비 20.7~62.2배, arithmetic 대비 2.0×10^5 ~ 6.2×10^5배 speedup(query size별). 에너지는 arithmetic 대비 17.6~28.0배, Boolean 대비 1.6×10^5 ~ 1.6×10^5배 절감(평균 arithmetic 17.6배). DB 크기별로 arithmetic 62.2~72.1배, Boolean 7.6×10^4 ~ 8.8×10^4배. DB가 32GB 초과 시 CM-SW 이점이 1.16배로 감소(잦은 I/O transfer 때문).
- **CM-IFP(hardware, §6.2)**: CM-SW 대비 76.6~216.0배 speedup(평균 136.9배), CM-PuM-SSD 대비 2.89~4.03배(외부 전송 제거 + array/chip/channel 병렬성). 에너지는 CM-SW 대비 156.2~454.5배 절감(평균 256.4배). DB 크기별 CM-IFP는 CM-SW 대비 250.1~295.1배. 작은 DB(≤32GB)에선 CM-PuM이 CM-IFP보다 1.41배 빠르지만(DRAM 상주), 큰 DB(>32GB)에선 CM-IFP가 CM-PuM 대비 8.29배 우위.
- 결론: 모든 hardware 가속 구현이 software 대비 우위이며, **CM-IFP가 모든 query·DB 크기에서 최고 성능·최저 에너지** 제공.

> [!quote]- 📄 원문 표현 (paper)
> - "CM-SW outperforms the arithmetic [27] and Boolean [17] approaches by 20.7-62.2× and 2.0×10^5 - 6.2×10^5× for different query sizes, respectively." (p.13)
> - "First, CM-IFP, CM-PuM-SSD, and CM-PuM outperform CM-SW by 76.6-216.0×, 81.7-105.8×, and 26.4-53.9×, for different query sizes." (p.14)
> - "CM-IFP outperforms CM-PuM-SSD by 2.89-4.03× for different query sizes." (p.14)
> - "for large encrypted databases (> 32GB, exceeding the size of external DRAM), CM-IFP achieves 8.29× higher performance than CM-PuM." (p.15)

## 섹션 노트

- **§2 Background**: BFV(Brakerski-Fan-Vercauteren) HE scheme 사용. Ring-LWE 기반, R = Z_q[X]/(X^n+1). Hom-Add는 ciphertext polynomial들을 coefficient-wise로 더함(Eq.4). NAND flash 구조(NAND string→sub-block→block→plane→die→chip), sensing latch(S-latch)와 multiple data latch(D-latch) 설명. NDP 분류: PIM(PuM/PnM), processing-in-storage(ISP/IFP).
- **§3 Motivation**: Table 1로 Boolean[33][17] vs arithmetic[27][34][29]을 execution time/scalability/SIMD/flexible query 기준 비교. Figure 2로 footprint·execution time·latency breakdown 정량화. Figure 3로 8GB~256GB DB에서 CPU/DRAM/SSD-controller 연산 시 transfer latency 비교.
- **§7 Discussion**: hardware data transposition unit(선택적 가속), 그리고 match index를 256-bit AES로 암호화해 채널 전송하는 privacy mitigation.
- **§8 Related Work**: SSE(symmetric searchable encryption)는 leakage profile로 plaintext recovery 공격에 취약. HE는 수학적으로 secure하다고 증명됨. 기존 HE용 NDP는 storage 데이터셋엔 한계 → CIPHERMATCH는 NAND flash 내부에서 직접 homomorphic addition 수행.

## 핵심 용어

- **Homomorphic Encryption (HE)**: 복호화 없이 암호문 위에서 직접 연산하는 암호 패러다임. 대부분 LWE 문제 기반.
- **BFV (Brakerski-Fan-Vercauteren)**: 본 논문이 사용한 Ring-LWE 기반 HE scheme. polynomial ring으로 암호 데이터 연산.
- **Hom-Add (Homomorphic Addition)**: 두 ciphertext polynomial을 coefficient-wise로 더하는 연산. CIPHERMATCH가 유일하게 사용하는 homomorphic 연산.
- **Data packing**: 여러 bit를 하나의 plaintext polynomial coefficient에 묶어 암호화하는 기법. 본 논문은 coefficient당 최대 16-bit를 채워 footprint를 16배 줄임.
- **Match polynomial**: query를 negate해 data에 더했을 때 일치 시 나타나는, 전부 1로 채워진 polynomial. 암호화된 match polynomial과 비교해 match 여부·위치 판정.
- **In-Flash Processing (IFP)**: NAND-flash cell·latch 회로로 bulk-bitwise 연산을 수행해 데이터 이동 없이 flash 내부에서 계산하는 NDP 방식.
- **Bit-serial addition**: full-adder를 bit 단위로 직렬 수행하는 가산. vertical data layout과 latch 연산으로 flash 내부에서 homomorphic addition 구현.
- **Vertical data layout**: 32-bit operand의 각 bit를 서로 다른 wordline(bitline 따라)에 배치해 bitline 내 carry propagation을 가능케 하는 배치.
- **ESP (Enhanced SLC Programming)**: SLC 모드에서 두 상태 간 전압차를 최대화해 bitwise 연산의 신뢰성을 높이는 기법(Flash-Cosmos에서 차용).
- **CM-SW / CM-PuM / CM-PuM-SSD / CM-IFP**: 각각 compute-centric(CPU), memory-centric(in-DRAM PuM), storage-centric(SSD-internal DRAM PuM), in-flash 구현.

## 강점 · 한계 · 열린 질문

- **강점**
  - multiplication·rotation 제거로 알고리즘 복잡도와 latency를 근본적으로 낮춤(latency 98.2% 차지하던 multiplication 제거).
  - 16-bit packing으로 암호화 footprint를 16배 줄여 NDP/IFP 적합성 확보.
  - HE operation을 IFP로 수행한 최초 연구이며, NAND 면적 overhead 약 0.6%로 commodity SSD에 최소 수정.
  - software-only(CM-SW)만으로도 기존 대비 큰 이득 → 즉시 적용 가능 + IFP로 추가 가속.
- **한계**
  - **exact** string matching에 한정(approximate matching 미지원). HD가 필요한 일반 비교는 다룸이 제한적.
  - vertical layout 유지를 위한 data transposition overhead 존재(software unit은 22.5μs flash read와 overlap, hardware unit은 별도 면적).
  - SLC 모드 운용으로 CIPHERMATCH 영역 용량이 줄어듦(TLC 대비).
  - 평가가 시뮬레이션 기반(CM-IFP)이며 실제 NAND chip 제작 검증은 아님.
- **열린 질문**
  - approximate/wildcard matching으로 multiplication 없이 확장 가능한가?
  - Z-NAND(3μs read) 등 빠른 flash에서 transposition latency를 어떻게 숨길 것인가(논문은 hardware unit 제안).
  - 다른 HE scheme(CKKS 등)이나 더 큰 t(packing bit)에서 trade-off는?

## ❓ Q&A (자가 점검)

> [!question]- Q1. CIPHERMATCH가 기존 arithmetic 접근의 어떤 연산을 제거했고 왜 중요한가?
> homomorphic multiplication과 rotation을 제거하고 homomorphic addition만 사용한다. arithmetic 접근에서는 latency의 98.2%가 multiplication에서 발생하므로, 이를 제거하면 근본적으로 큰 성능·에너지 이득을 얻는다. query를 negate(~Q)해 data에 더하면 일치 시 전부 1인 match polynomial이 나오는 통찰을 이용한다.

> [!question]- Q2. memory-efficient data packing이 footprint를 줄이는 원리와 정도는?
> 각 plaintext coefficient에 가능한 최대 bit 수(16-bit)를 채운다. 이러면 암호화 후 데이터가 평문 대비 4배(lower bound)에 그친다. single-bit packing을 쓰는 기존 arithmetic[27]은 64배까지 커지므로 16배의 footprint 감소를 달성한다.

> [!question]- Q3. NAND flash에서 homomorphic addition을 어떻게 구현하는가?
> S-latch와 D-latch에 트랜지스터 2개를 추가해 bi-directional data flow와 AND/OR/XOR bitwise 연산을 가능케 하고, full-adder를 6-step bit-serial addition으로 수행한다. 32-bit coefficient를 32개 wordline에 vertical layout으로 배치해 bitline 단위로 carry를 전파하며, P/E cycle 없이 read·latch 연산만 사용한다.

> [!question]- Q4. 왜 vertical data layout이 필요한가?
> bit-serial addition은 carry bit를 다음 bit position으로 전파해야 한다. 전통적 horizontal layout(한 wordline에 operand의 bit들이 연속)은 carry가 sensing latch 사이를 이동해야 해 제약이 크다. vertical layout은 각 bit를 별도 bitline에 두어 각 position의 carry를 D-latch에 보관한 채 다음 계산에 쓸 수 있게 한다.

> [!question]- Q5. CM-IFP가 CM-PuM(in-DRAM)보다 항상 빠른가?
> 아니다. 작은 DB(≤32GB, 외부 DRAM에 상주)에서는 CM-PuM이 CM-IFP보다 1.41배 빠르다. 데이터를 DRAM에서 반복 접근하고 외부 I/O 비용을 amortize하기 때문이다. 그러나 DB가 외부 DRAM 크기를 초과(>32GB)하면 잦은 SSD 데이터 이동으로 CM-PuM이 저하되어 CM-IFP가 8.29배 우위가 된다. 평균적으로 CM-IFP가 모든 크기에서 최고 성능을 보인다.

> [!question]- Q6. 대표 정량 결과를 요약하면?
> CM-SW는 arithmetic baseline 대비 성능 42.9배·에너지 17.6배 개선. CM-IFP는 CM-SW 대비 성능 136.9배·에너지 256.4배 추가 개선. CM-IFP는 CM-PuM-SSD 대비 2.89~4.03배, 큰 DB에서 CM-PuM 대비 8.29배 우위.

> [!question]- Q7. CIPHERMATCH를 commodity SSD에 도입하는 비용은?
> NAND die 면적 overhead 약 0.6%(ParaBit 수정 0.6% + Flash-Cosmos ESP 0%), SSD-internal DRAM 0.5MB(연산 결과 저장)와 μ-program <1KB. 신규 명령 CM-read/CM-write/CM-search와 FTL의 bop_add μ-program, vertical layout용 data transposition unit이 필요하다.

> [!question]- Q8. SSE 대신 HE를 쓰는 이유는?
> SSE(symmetric searchable encryption)는 저장·연산이 효율적이지만 leakage profile이 커 plaintext recovery 공격에 취약하다. HE는 LWE 등 강한 수학적 기반 위에서 secure함이 증명되어, 최대 데이터 크기 외에는 정보 누출이 없는 secure한 string matching을 제공한다.

## 🔗 Connections

[[In-Storage Computing]] · [[ASPLOS]] · [[2025]]

## References worth following

- M. Yasuda et al., "Secure Pattern Matching Using Somewhat Homomorphic Encryption," CCSW 2013 ([27], 본 논문의 arithmetic baseline).
- M. M. A. Aziz et al., "Secure Genomic Search with Parallel Homomorphic Encryption," 2024 ([17], Boolean baseline, TFHE-rs).
- J. Park, R. Azizi, G. F. Oliveira et al., "Flash-Cosmos: In-Flash Bulk Bitwise Operations Using Inherent Computation Capability of NAND Flash Memory," MICRO 2022 ([60], ESP·bitwise 기반).
- C. Gao et al., "ParaBit: Processing Parallel Bitwise Operations Using NAND Flash Memory Based SSDs," MICRO 2021 ([62], IFP 페리페럴 회로 기반).
- N. Hajinazar et al., "SIMDRAM: A Framework for Bit-Serial SIMD Processing using DRAM," ASPLOS 2021 ([49], CM-PuM·data transposition 기반).
- J. Fan and F. Vercauteren, "Somewhat Practical Fully Homomorphic Encryption," Cryptology ePrint 2012 ([9], BFV scheme).

## Personal annotations

<본인 메모 영역>
