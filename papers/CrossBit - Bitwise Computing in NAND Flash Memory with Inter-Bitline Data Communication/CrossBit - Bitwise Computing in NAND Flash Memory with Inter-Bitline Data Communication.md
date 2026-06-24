---
title: "CrossBit: Bitwise Computing in NAND Flash Memory with Inter-Bitline Data Communication"
aliases: [CrossBit]
description: "NAND 페이지 버퍼에 inter-bitline 연산을 추가해 MLC IFP와 in-flash ECC, DB 질의를 가속하는 IFP 아키텍처"
venue: MICRO
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/isc
  - topic/in-flash
  - topic/bitwise
  - venue/micro
  - year/2025
---

# CrossBit: Bitwise Computing in NAND Flash Memory with Inter-Bitline Data Communication

> **MICRO 2025** · `cluster/isc` · Source: [CrossBit - Bitwise Computing in NAND Flash Memory with Inter-Bitline Data Communication.pdf](<CrossBit - Bitwise Computing in NAND Flash Memory with Inter-Bitline Data Communication.pdf>)

**저자**: Hyunjin Kim, Seunghwan Song, Sukhyun Choi, Jeongin Choe, Sanghyeok Han, Jisung Park, Jinho Lee, Jae-Joon Kim (Seoul National University, Samsung Electronics, POSTECH)

## TL;DR
CrossBit은 기존 IFP(In-Flash Processing)가 같은 bitline(BL) 내부 연산(intra-bitline)에만 갇혀 있던 한계를, 페이지 버퍼에 최소 회로만 추가해 서로 다른 BL 간 연산(inter-bitline)을 가능케 하는 IFP 아키텍처다. 이로써 (1) Hamming code 기반 in-flash ECC(IF-ECC)를 NAND 칩 내부에서 수행해 MLC를 신뢰성 있게 IFP에 쓸 수 있게 하여 SLC-only 대비 1.8x 높은 bit density를 달성하고, (2) pattern match/range/join 같은 DB 질의를 SOTA IFP 대비 평균 2.1x 속도, 2.5x 에너지 효율로 가속한다. 면적 오버헤드는 2.2%.

## 문제 & 동기
IFP는 NAND flash 칩 내부에서 연산을 수행해 메모리 계층 간 데이터 이동을 근본적으로 줄이는 접근으로, 큰 용량을 저비용으로 제공하고(stacked NAND의 고집적) 내부 read bandwidth로 NDP 성능을 극대화한다. 그러나 기존 IFP에는 두 가지 본질적 한계가 있다. 첫째, ParaBit·Flash-Cosmos는 같은 BL을 통해 sensing된 bit끼리만 연산할 수 있어 **서로 다른 BL의 데이터 간 연산(inter-bitline)이 불가능**하다. AresFlash가 인접 BL 간 단방향 shift만 부분 지원하나 유연성이 떨어진다. 둘째, 기존 IFP는 NAND 칩 내부에서 연산하므로 SSD 컨트롤러의 ECC 모듈을 쓸 수 없어 **MLC의 신뢰성 있는 연산이 불가능**하고, 결국 program latency를 감수하며 error-robust한 SLC만 사용해 IFP의 고용량 이점을 포기해야 했다.

> [!quote]- 📄 원문 표현 (paper)
> "previous IFP designs were limited to performing bitwise operations on data sensed within the same bitline, thus lacking the capability to handle more complex functions requiring interactions between data from different bitlines." (p.1, Abstract)
>
> "the existing IFP techniques cannot support reliable computation in multi-level cell (MLC) NAND flash memory due to the lack of error-correction capability inside NAND flash chips." (p.2)

## 핵심 통찰

> [!note]- 통찰 1 — 기존 page buffer의 latch + DC-interconnect를 그대로 재활용해 컴퓨팅
> 현대 NAND의 bitline buffer는 이미 여러 latch(L1[0:2], L2)와, latch 간 통신을 위한 dynamic circuit-based interconnect(DC-interconnect)를 갖는다. DC-interconnect는 wire node에 charge를 holding하는 dynamic circuit로 static circuit 대비 면적을 5x 줄인 구조다. CrossBit은 여기에 추가 하드웨어 거의 없이 control signal만 조작해 intra-bitline 연산을 구현하고, L2 latch 간 양방향 통신을 추가해 inter-bitline 연산까지 확장한다.

> [!note]- 통찰 2 — Boolean-complete를 위한 NOR 한 종류면 충분 (prims)
> 모든 컴퓨팅 latch 연산의 핵심은 d-NOR(선택된 latch들의 Q를 Shared_node에 NOR)이다. NOR는 그 자체로 Boolean-complete이므로, init/d-NOR/store를 묶은 두 primitive(prim_OR, prim_AND)로 임의 논리를 표현할 수 있다. 이는 회로 추가 없이 control signal sequencing만으로 범용 연산을 가능케 한다.

> [!note]- 통찰 3 — ECC도 결국 XOR이므로 inter-bitline로 in-flash 수행 가능
> ECC 연산 대부분은 XOR이고, XOR은 서로 다른 BL의 bit를 묶어야 한다. CrossBit의 inter-bitline 능력은 이 XOR을 NAND 내부에서 처리하게 해, Hamming code 기반 IF-ECC를 추가 하드웨어 없이 구현한다. 덕분에 MLC를 IFP에 안전하게 쓸 수 있어 SLC-only 제약을 깬다.

> [!note]- 통찰 4 — 면적 폭증을 막는 hierarchical(local/global) 구조
> 한 plane의 수십만 BL을 직접 연결하면 면적이 폭증하고 병렬성이 죽는다. CrossBit은 32 BL을 묶은 local group(+local module)으로 intra/local inter-bitline을 처리하고, local module 결과를 Data_line으로 모아 작은 global module이 순차 통합한다. 이로써 plane당 4096 local group이 병렬 연산하면서 면적은 2.2%만 추가된다.

## 설계 / 메커니즘

> [!abstract]- CrossBit 아키텍처 전체 구조 (Fig. 3, Fig. 5)
> - **3계층 컴퓨팅 유닛**: (1) intra-bitline computing module(기존 page buffer latch+DC-interconnect 재활용), (2) local inter-bitline module(=local module, 32 BL을 local group으로 묶음), (3) global inter-bitline module(=global module, local module 결과를 Data_line으로 통합).
> - 한 plane = 128K BL = 4096 local group(각 32 BL = 1 page는 16KiB). 모든 local group이 병렬 연산, plane 단위 병렬성 활용.
> - 신규 추가 모듈은 Fig. 3의 blue(global module)와 red(local module) 박스뿐. 데이터 레이아웃은 그대로 유지.

> [!abstract]- 컴퓨팅 latch의 기본 3연산 (Fig. 4)
> - **init**(Fig.4b): Charge=0이면 Shared_node를 1로 charge.
> - **d-NOR**(Fig.4c): Select=1인 latch들의 Q값을 Shared_node에 NOR. Q=1이 하나라도 있으면 Shared_node를 0으로 discharge → 선택 latch들의 NOR 결과 반영.
> - **store**(Fig.4d): Shared_node=1이면 Set/Reset으로 Q를 1 또는 0으로 덮어씀. Shared_node=0이면 latch의 discharge path를 끊어 기존 Q 보존.
> - 면적 오버헤드: BL당 2개 transistor + DC-interconnect용 1개 + WEN/SO용 2개 logic gate. static logic 대비 1.4x 면적 효율(Fig. 6).

> [!abstract]- Local / Global inter-bitline 연산 (Fig. 5)
> - **Local module**: local group(32 BL)의 각 BL의 L2 latch를 DC-interconnect로 묶어 inter-bitline 연산. inter-bitline 미사용 시 WEN=0으로 path 차단.
> - **Global module**(tri-state inverter style dynamic-logic, module당 16 transistor): local module 결과(Data_line)를 Shared_node로 모아 순차 통합. **Group AND**(GAND=1, Fig.5c①): Data_line[4095]=1이면 pull-down/pull-up 둘 다 deactivate해 이전 값 보존, 0이면 reset 전파 → 전체 AND. **Group OR**(GOR=1): 전체 OR.

> [!abstract]- Programming method: prims와 optimization (Sec. 5, Fig. 7)
> - **prim_OR**: init→d-NOR→store. dst = dst + (src1+...+srcN)'. N=0이면 dst=src.
> - **prim_AND**: d-NOR→store(init 생략). dst = dst·(src1+...+srcN)'. N=0이면 dst=src.
> - 두 prim은 NOR 기반이라 Boolean-complete.
> - **최적화 3종**: d-NOR-skip(d-NOR가 latch 선택 안 하면 prim 우회), subset-skip(현재 prim의 선택이 이전 prim을 포함하면 init·d-NOR 재사용), parallel-prim(독립 dst를 병렬 실행).
> - inter-bitline XOR 예시: AND/OR/NOT 18 steps → prims 12 steps → 최적화 8 steps(2.2x 감소).

> [!abstract]- IF-ECC: in-flash Hamming code (Sec. 6.3, Fig. 8)
> - **EMP(Enhanced MLC Programming)**: Flash-Cosmos의 ESP에서 유도, ISPP voltage로 분포를 조여 read margin 확보. EMP만으로는 BER 부족 → IF-ECC와 결합.
> - 각 local group에 parity bit 저장용 6 BL 추가. Hamming code로 검출·정정.
> - **Phase 1(Error detection)**: 정해진 parity bit(PN)/data bit(DN) 집합에 inter-bitline XOR (예: P1=XOR(P1,D1,D2,D4,D5)).
> - **Phase 2(Error bitline marking)**: phase 1 결과로 error BL을 mark bit(MN)로 식별 (예: M1=P1+P2+P3'+P4').
> - **Phase 3(Error bit flip)**: MN=0이면 해당 DN을 intra-bitline으로 flip (DN=MN·DN+MN'·DN').
> - JEDEC standard(1e-15) 충족, MLC를 IFP에 사용 가능. AND/OR/NOT 2217 → prims 1485 → prim+opt 783 step.

> [!abstract]- System 통합 / consistency (Sec. 6.1~6.2)
> - control signal을 NAND die 내 추가 40KiB SRAM에 저장(전체 면적 0.3%). IF-ECC/pattern/range/mult/add control signal은 각 8.5/0.2/2.7/16.8/5.0 KiB로 총 33.2 KiB.
> - ONFI 호환: vendor-specific opcode(33h/34h)로 control signal·external input을 broadcast, modified cache read(36h)로 cell read→IF-ECC→target app→결과 전송.
> - cache read 명령을 수정해 intra/inter-bitline 연산을 기존 FSM scheduler로 수행. ONFI 호환 유지(p.2).

> [!abstract]- DB 질의 가속 (Sec. 7, Fig. 9~10)
> - **Pattern match**: 각 bit를 S1/S2 두 latch로 표현(1=10, 0=01, don't care=00). intra-bitline로 Match_bit=S1·C2+S2·C1, inter-bitline word matching으로 Match_word, global group OR로 임의 매치 검출. 32 bit→1 bit로 전송량 감소.
> - **Range query**: 부호 없는 정수 비교를 4 phase(공통 1 제거 → MSB부터 단방향 OR → bitwise A>B → global OR)로 수행.
> - **Join**: partitioned join. dimension table을 NAND에 broadcast 후 pattern match로 검색.

## 평가

> [!success]- 핵심 수치 (p.9~13)
> - **MLC bit density**: SLC-only IFP 대비 1.8x 높음(Fig. 13a, p.9). IF-ECC로 SLC→MLC 가능.
> - **BER 감소(IF-ECC)**: P/E=2K 고정·elapsed time 변화 시 평균 1.1E+9x, elapsed time=1day 고정·P/E 변화 시 평균 3.7E+10x 감소(Fig. 12, p.9). JEDEC standard(client 1e-15, enterprise 1e-16) 충족. EMP 단독은 program 직후도 standard 미달.
> - **IF-ECC latency**: AresFlash+IF-ECC 대비 71x, on-die ECC module 대비 2.8x speedup(Fig. 13b, p.10).
> - **DB 질의 (vs ParaBit/AresFlash, geomean)**: pattern match MLC 5.8x/3.9x, SLC 4.2x/2.9x/1.0x(vs ParaBit-LocFree/Flash-Cosmos/AresFlash). range MLC 9.3x/2.4x. join MLC 3.7K x/2.4x (p.11~12).
> - **end-to-end SSB(scale factor 1000)**: 질의 전반 geomean 1.7x speedup(p.1, p.12).
> - **에너지**: pattern match MLC 15x/7.0x, SLC 5.8x/5.3x/2.0x 절감(vs ParaBit/AresFlash, ParaBit-LocFree/Flash-Cosmos/AresFlash) (Fig. 17, p.13). 전체 평균 2.5x 에너지 효율(p.1).
> - **Arithmetic**: AresFlash+CrossBit 통합이 VADD MLC 1.1x/1.4x, TPC-H 3.6x/1.9x, MLP 1.4x/3.0x speedup (Fig. 18~19, p.13).
> - **inter-bitline 비중**: IF-ECC/pattern/range의 93.1%/78.4%/94.8%가 CrossBit 연산, SSB 전체 평균 64%가 inter-bitline (p.12).
> - **면적 오버헤드**: 전체 die의 2.2%(Fig. 21, p.14). cell array가 81.1% 차지. CrossBit 2.2% + page buffer 5.7% + 추가 SRAM 0.3% 등.
> - **기술 스케일링**: 28nm CMOS로 computing module latency/energy를 각 2.7x/12.0x 개선(p.14).
> - **실험 환경**: MQSim SSD simulator + HSPICE circuit simulation, 150nm CMOS, 8ch/8die/4plane, 16KiB page, MLC tR=25us·tPROG=500us (Table 1, p.11).

## 섹션 노트
- **§1 Introduction**: IFP의 두 한계(inter-bitline 불가, MLC ECC 불가) 제기. CrossBit = flexible inter-BL + efficient in-flash error correction.
- **§2 Background**: SSD/NAND 구조, page buffer의 latch+DC-interconnect(5x 면적 절감). ParaBit(첫 bulk bitwise IFP, intra-bitline), Flash-Cosmos, AresFlash(단방향 shift)와 ISP 비교.
- **§3 Challenges**: MLC reliability(Flash-Cosmos BER도 JEDEC 미달, Fig. 2a), generic inter-bitline 부재. ParaBit/Flash-Cosmos는 pattern match 시간의 87.0%/84.8%를 cell program에 소비.
- **§4 Architecture**: design objective(면적 효율/BL 병렬성/Boolean-completeness), 3계층 컴퓨팅 유닛, computing latch 3연산, local/global module.
- **§5 Programming**: prim_OR/prim_AND, 최적화 3종.
- **§6 IF-ECC system**: workflow, ONFI consistency, EMP+IF-ECC Hamming code 3-phase.
- **§7 DB applications**: pattern match/range/join.
- **§8 Evaluation**: BER, bit density, DB acceleration, arithmetic, area.
- **§9 Discussion**: technology scaling, ECC 관계, data layout(columnar에 유리).

## 핵심 용어
- **IFP (In-Flash Processing)**: NAND flash 칩 내부에서 연산을 수행해 칩-외부 데이터 이동을 줄이는 처리 방식. ISP(컨트롤러 내 연산)보다 throughput이 높음.
- **intra-bitline operation**: 같은 bitline(BL)을 통해 sensing된 bit들 간의 연산. 기존 IFP가 지원하던 범위.
- **inter-bitline operation**: 서로 다른 BL의 데이터 간 연산. CrossBit의 핵심 기여.
- **bitline buffer / page buffer**: cell read/write의 임시 저장소. 다수 latch(L1[0:2], L2)와 DC-interconnect로 구성.
- **DC-interconnect (dynamic circuit-based interconnect)**: wire node에 charge를 holding하는 dynamic circuit 방식 latch 간 통신로. static 대비 면적 5x 절감.
- **computing latch**: 기존 bitline buffer 회로(추가 HW 없음)를 init/d-NOR/store로 활용하는 CrossBit의 컴퓨팅 방식.
- **d-NOR**: 선택된 latch들의 Q를 Shared_node에 NOR하는 기본 연산. Boolean-complete의 근간.
- **local group / local module**: 32 BL을 묶은 단위와 그 inter-bitline 컴퓨팅 모듈.
- **global module**: local module 결과(Data_line)를 순차 통합하는 dynamic-logic 모듈(Group AND/OR).
- **prims (prim_OR, prim_AND)**: circuit-aware primitive 함수. NOR 기반 Boolean-complete building block.
- **IF-ECC (In-Flash ECC)**: CrossBit의 inter-bitline XOR로 Hamming code를 NAND 내부에서 수행하는 ECC.
- **EMP (Enhanced MLC Programming)**: ISPP voltage로 MLC 분포를 조여 read margin을 늘리는 program 기법(Flash-Cosmos ESP 유도).
- **MLC / SLC**: cell당 다중/단일 bit 저장. MLC는 고용량이나 BER 높음. CrossBit은 MLC를 IFP에 사용 가능케 함.

## 강점 · 한계 · 열린 질문
**강점**
- 추가 하드웨어를 최소화하면서(면적 2.2%) inter-bitline이라는 근본 한계를 해소.
- MLC를 IFP에 안전하게 사용 → SLC-only 대비 1.8x bit density, 비용/에너지/성능 이점.
- ONFI 호환으로 기존 NAND 인터페이스에 통합 가능, 실 SSB end-to-end 질의까지 검증.
- AresFlash 등 기존 IFP와 직교적으로 통합 가능(arithmetic 가속 시너지).

**한계**
- 성능이 data layout에 의존적 — columnar(dense array)에서 최대, row-major는 일회성 변환 필요.
- IF-ECC는 Hamming code(1-bit 정정)로 제한 → LDPC/BCH 같은 강력한 off-chip ECC는 여전히 컨트롤러에서 사용(연산 복잡도 때문에 IFP 부적합).
- logic optimizer를 미구현(수동 변환)해 prims 변환의 자동화·최적화 여지 남음.
- IF-ECC parity bit가 conventional ECC와 달라 IFP data와 conventional data의 물리 주소 공간 분리 필요.

**열린 질문**
- multi-bit 정정이 필요한 고밀도 MLC/TLC/QLC에서 IF-ECC 확장 가능성은?
- tree-style 등 더 깊은 hierarchy로 면적 vs 병렬성/merging trade-off를 어떻게 최적화할 것인가?
- logic optimizer 자동화 시 prims step 수를 얼마나 더 줄일 수 있나?

## ❓ Q&A (자가 점검)

> [!question]- Q1. 기존 IFP(ParaBit, Flash-Cosmos)의 두 가지 핵심 한계는?
> > 답: (1) 같은 BL로 sensing된 bit끼리만 연산 가능해 서로 다른 BL 간 inter-bitline 연산이 불가능, (2) NAND 칩 내부에서 연산하므로 SSD 컨트롤러 ECC를 못 써서 MLC를 신뢰성 있게 쓸 수 없고 결국 SLC-only로 제약됨.

> [!question]- Q2. CrossBit이 추가 하드웨어를 거의 안 쓰고도 컴퓨팅을 구현하는 방법은?
> > 답: 현대 NAND page buffer가 이미 가진 다수 latch(L1[0:2], L2)와 DC-interconnect를 control signal sequencing(init/d-NOR/store)만으로 재활용한다. inter-bitline용으로 L2 latch 간 양방향 통신만 최소 추가(BL당 transistor 2~3개 + gate 2개)한다.

> [!question]- Q3. CrossBit의 연산이 Boolean-complete임을 어떻게 보장하나?
> > 답: 모든 연산의 핵심인 d-NOR(선택 latch들의 NOR)는 그 자체로 Boolean-complete이고, 이를 묶은 prim_OR/prim_AND 두 primitive로 임의의 논리식을 표현할 수 있다.

> [!question]- Q4. local/global hierarchy가 필요한 이유는?
> > 답: plane의 수십만 BL을 직접 연결하면 Shared_node wire 용량 증가와 병렬성 손실로 면적이 폭증한다. 32 BL을 local group으로 묶어 병렬 연산(plane당 4096 group)하고, 작은 global module이 local 결과를 순차 통합해 병렬성과 면적 효율을 동시에 확보한다.

> [!question]- Q5. IF-ECC는 어떻게 NAND 내부에서 동작하며 왜 inter-bitline이 필수인가?
> > 답: ECC는 대부분 XOR이고 XOR은 서로 다른 BL의 bit를 묶어야 하므로 inter-bitline이 필수다. CrossBit은 Hamming code를 3-phase(detection: parity·data XOR, marking: error BL 식별, flip: 오류 bit 정정)로 NAND 내부에서 수행해 MLC의 BER을 JEDEC standard까지 낮춘다.

> [!question]- Q6. MLC를 IFP에 쓰는 것이 왜 중요한가?
> > 답: MLC는 cell당 다중 bit로 고용량·저비용이지만 BER이 높아 기존 IFP는 error-robust한 SLC만 썼다. IF-ECC로 MLC를 안전하게 쓰면 SLC-only 대비 1.8x 높은 bit density를 얻어 IFP의 고용량 이점을 살린다.

> [!question]- Q7. CrossBit은 기존 SSD 인터페이스와 어떻게 호환되나?
> > 답: ONFI 호환. vendor-specific opcode(33h/34h)로 control signal/external input을 broadcast하고, modified cache read(36h)로 cell read→IF-ECC→target app→결과 전송을 수행한다. control signal은 NAND die 내 40KiB SRAM(면적 0.3%)에 저장.

> [!question]- Q8. CrossBit 성능이 data layout에 의존하는 이유와 대응은?
> > 답: dense array(columnar)에서 BL 병렬성이 최대화되어 성능이 좋다. row-major도 지원하나 columnar로의 변환이 필요하며, 이는 일회성 single access 오버헤드다. OLAP/columnar 추세에 부합한다.

## 🔗 Connections
[[In-Storage Computing]] · [[MICRO]] · [[2025]]

## References worth following
- **ParaBit** (Gao et al., MICRO 2021, [24]): 첫 bulk bitwise IFP. CrossBit의 직접 baseline.
- **Flash-Cosmos** (Park et al., MICRO 2022, [60]): inherent computation 기반 IFP + ESP. EMP의 기반.
- **AresFlash** (Chen et al., MICRO 2024, [13]): 단방향 inter-BL shift 지원 IFP. arithmetic 통합 대상.
- **MQSim** (Tavakkol et al., FAST 2018, [68]): 본 논문이 사용한 SSD simulator.
- **Hamming code as error-reducing codes** (Rurik & Mazumdar, ITW 2016, [63]): IF-ECC의 코드 이론 근거.
- **Star Schema Benchmark (SSB)** (O'Neil et al., 2007, [59]): end-to-end DB 질의 평가 벤치마크.

## Personal annotations
<본인 메모 영역>
