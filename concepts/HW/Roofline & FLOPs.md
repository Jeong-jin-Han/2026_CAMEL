---
title: "Roofline & FLOP·FLOPs·FLOPS — 병목 판정의 언어"
aliases: [Roofline, Roofline Model, FLOP, FLOPs, FLOPS, GFLOPS, arithmetic intensity, 연산 밀도]
type: concept
tags:
  - concept
  - concept/hw
  - topic/performance
  - topic/accelerator
---

# Roofline & FLOP·FLOPs·FLOPS — 병목 판정의 언어

> [!abstract] 이 노트는 뭐지?
> [[Smart-Infinity]]가 "왜 Adam update를 GPU 밖(CPU/CSD)으로 옮기는 게 이기는가"를 설명하는 밑바탕 이론. FLOP 3형제(FLOP/FLOPs/FLOPS)의 구분, 연산 밀도(arithmetic intensity), 그리고 **Roofline 모델로 memory-bound vs compute-bound를 판정하는 법**을 정리한다. 원본: Smart-Infinity NodeGraph 「배경 — FLOPs·연산밀도·Roofline」 노드.

## 한 문장
어떤 연산의 도달 성능은 $\min(\text{peak FLOP/s},\ I \times \text{bandwidth})$ 두 천장 중 낮은 쪽이 정하며, 연산 밀도 $I$(옮긴 byte당 연산 수 = 데이터 재사용도)가 경계값 $I^{*}$보다 작으면 아무리 좋은 GPU도 대역폭에 묶인다(memory-bound) — Adam update가 정확히 그 경우라서 near-data로 옮기는 것이 정당화된다.

## 1. 약자 3형제 — s 하나로 갈리는 뜻

| 표기 | 뜻 | 성질 |
|---|---|---|
| **FLOP** | FLoating-point OPeration — 부동소수점 연산 1회 (덧셈·곱셈·나눗셈·제곱근 등) | 단위 |
| **FLOPs** | FLOP의 복수 = 연산 **총 개수** = "일의 양" | **개수 (고정)** |
| **FLOPS** = FLOP/s | **초당** 연산 수 = "속도" | **비율 (변함)** |

- GFLOPS = Giga($10^9$) FLOP/s, TFLOP/s = Tera($10^{12}$) FLOP/s.

> [!warning] 흔한 혼동 — "총 연산량은 일정한데 왜 GFLOPS가 변하지?"
> 맞다, **FLOPs(일의 양)는 워크로드가 정하므로 일정**하다. 변하는 건 **FLOPS = FLOPs ÷ 시간**이다. GPU가 데이터를 기다리며 놀면(idle) 시간이 늘어나 **비율이 떨어진다** — 일을 덜 한 게 아니라 같은 일을 느리게 한 것. [[Smart-Infinity]] Fig 15의 GFLOPS/달러가 speedup과 같이 움직이는 이유이자, "idle GPU = 낭비되는 고정 투자"라는 비용 논리의 근거.

## 2. 연산 밀도 (Arithmetic Intensity) — 데이터 재사용도

$$I = \frac{\text{FLOPs}}{\text{bytes}}$$

옮긴 1 byte당 연산 몇 번 = **한 번 가져온 값을 몇 번 써먹나**.
- matmul($N \times N$): 값 하나를 $N$번 재사용 → $I = O(N)$ 큼
- element-wise(Adam update): 값 하나를 1번 쓰고 끝 → $I$ 작음 (~0.4)

$I$가 작다 = 조금 계산하고 또 읽어야 한다 = **데이터 이동이 지배**한다.

## 3. Roofline 모델 — 두 천장의 $\min$

![[roofline.png]]

$$\text{도달 성능} = \min(\underbrace{\text{peak FLOP/s}}_{\text{수평 천장}},\ \underbrace{I \times \text{BW}}_{\text{대각 천장}}), \qquad I^{*} = \frac{\text{peak FLOP/s}}{\text{peak BW}}$$

그래프 읽는 법:
- **대각선 (I/O bandwidth roof)** = $I \times \text{BW}$. 대역폭이 고정이라 $I$가 커질수록 선형 상승(기울기 = BW) — 같은 공급 속도로 byte당 일을 더 하니 총 FLOP/s가 오른다
- **수평선 (computational roof)** = peak FLOP/s = $(\text{PE 수}) \times (\text{FLOP/cycle}) \times (\text{clock})$. 데이터가 무한히 공급돼도 ALU가 한계
- **교점** = ridge point $I^{*}$. **왼쪽 = memory-bound**(대각 천장에 걸림), **오른쪽 = compute-bound**(수평 천장에 걸림)

### A100 예시 (공개 사양 기준)
- peak $\approx 19.5$ TFLOP/s (FP32), BW $\approx 2$ TB/s → $I^{*} \approx 10$
- **Adam update** ($I \approx 0.4$): $0.4 \times 2 = 0.8$ vs $19.5$ → **0.8 TFLOP/s** — peak의 ~4%, 나머지 96%는 데이터 대기
- **matmul** ($I \sim 100$): $200$ vs $19.5$ → **19.5 TFLOP/s** — ALU 포화

→ update는 메모리 천장(0.8)에 걸려 GPU의 peak(19.5)가 무의미 → **states가 있는 곳(CPU/CSD)에서 하는 게 이긴다.** 이게 ZeRO-Offload(CPU update)와 [[Smart-Infinity]](CSD update)의 이론적 근거.

## 4. 그 수치들은 무엇이 정하나 (원리)

- **peak FLOP/s** $= (\text{cores}) \times 2_{\text{FMA}} \times (\text{clock})$ — A100: $6912 \times 2 \times 1.41\,\text{GHz} \approx 19.5$ TFLOP/s. FMA = 곱셈+덧셈을 1 cycle에 (2 FLOP). Tensor Core(matmul 전용 유닛)를 쓰면 FP16 기준 ~312 TFLOP/s로 급등 — "GPU FLOPS"는 **어느 유닛 기준이냐에 따라 다름**
- **용어**: PE(Processing Element) = vendor 중립 "연산 유닛" 일반명 · NVIDIA의 CUDA core = FP32 ALU lane — 같은 것의 다른 이름
- **peak BW** $= \dfrac{(\text{bus width}) \times (\text{data rate})}{8}$ — A100 HBM2e: $\dfrac{5120\,\text{bit} \times 3.2\,\text{Gbit/s}}{8} \approx 2$ TB/s
- **왜 HBM ≫ host DRAM ≫ PCIe** (원리 = 폭 × 주파수 × 근접성): HBM은 수천-bit 초광폭 버스를 on-package 적층(TB/s) · host DRAM은 채널당 64-bit × 수 채널(~수백 GB/s) · PCIe는 serial 16 lane(~16 GB/s Gen3 x16). **물리적 버스 폭과 거리**가 메모리 계층을 가른다
- **연결 고리**: Tensor Core로 peak을 올리면 $I^{*}$도 커져(~150) **더 많은 연산이 memory-bound로 밀려남** → GPU는 고밀도 matmul 전용으로 진화, element-wise는 점점 더 near-data로 옮길 유인 증가

> [!note] 수치 출처 (정직 표기)
> A100 19.5 TFLOP/s · 2 TB/s · 6912 core · 5120-bit = NVIDIA 공개 사양. Adam의 FLOPs·bytes(~12 FLOP, ~28 B/param — $g,m,v,w$ 읽고 $m,v,w$ 쓰기) = element-wise 연산·변수 개수로부터의 **추정치**(감 잡기용). DRAM/PCIe 수치는 대표적 order of magnitude(기기별 상이).

## 4.5 Smart-Infinity를 Roofline으로 — "올라간 건 cap이 아니라 대각선" (2026-07-15 문답)

CSD 전환을 Roofline 위에서 정확히 서술하면:

| 요소 | baseline (host update) | Smart-Infinity (CSD update) |
|---|---|---|
| $I$ (워크로드 성질) | $\approx 0.4$ | $\approx 0.4$ — **불변** |
| 총 FLOPs | 동일 | 동일 — **불변** |
| **대각 천장** ($I \times \text{BW}$) | 공유 PCIe (포화, 고정) | **내부 8GB/s × N — N배로 상승** ← 올라간 것 |
| **수평 천장** (peak FLOP/s) | CPU/GPU (거대) | FPGA (소박) — **오히려 하락** |
| 달성 성능 | 낮은 대각선에 깔림 | 올라간 대각선을 타고 N배 |

- **흔한 오해**: "FPGA 가속기 덕에 연산 cap이 올라갔다" ✗ — 수평 천장은 오히려 낮아졌다. memory-bound($I \ll I^*$)에서 달성 성능 $= I \times \text{BW}$이므로, **대각선(BW)이 오른 것**이 전부다.
- **FPGA의 실제 역할 2가지**: ① 높은 aggregate BW가 있는 **위치**(장치 내부)에 연산을 놓게 함 ② 수평 천장이 대각선 아래로 내려오지 않을 만큼의 충분조건 제공 — Fig 14(updater >7GB/s > SSD 내부 BW)가 정확히 이 검증
- **우아함**: GPU에선 peak의 96%가 노는데, FPGA updater는 수평 천장을 $I \times \text{BW}$ 바로 위에 맞춤(ridge point ≈ 워크로드 $I$) → 낭비 ~0 = Fig 15 GFLOPS/달러 우위의 Roofline적 해석
- 한 줄: **memory-bound에서 유일한 손잡이는 대각선이고, Smart-Infinity는 연산 cap을 낮추는 대신 대각선을 N배 올린 trade다.**
- **"BW만 올리면 cap에 닿아 멈추지 않나?" (후속 문답)**: 맞다 — $\min(\text{peak},\ I{\times}BW)$는 불연속 없이 연속이지만(교점 $I^*{=}\text{peak}/BW$가 왼쪽으로 이동할 뿐), $I \times BW$가 peak에 닿는 순간 compute-bound로 **포화**돼 BW 증가가 무효가 된다. Smart-Infinity가 이 벽을 안 만나는 이유 = **CSD 추가 시 BW와 cap(FPGA)이 하나씩 같이 추가** → 두 천장이 비례 상승 → $I^*$ 불변 → 영원히 memory-bound 쪽 → 선형 확장. 반례: FPGA 1개 고정 + SSD만 증설이면 updater(~7GB/s)에서 포화 — baseline이 공유 PCIe 고정 천장에서 4-SSD 포화(Fig 3b)한 것과 같은 구조. **"CSD마다 FPGA"인 이유가 바로 이것.** (단 BW↔cap 동반 상승은 필연이 아니라 **설계 선택** — HBM만 올린 GPU, FPGA 고정+SSD 증설처럼 한쪽만 오르는 반례가 실존.)
- **cap은 "부딪힐 때만 보인다" (후속 문답 2)**: memory-bound에선 달성 성능 $= I \times BW$에 cap이 아예 등장하지 않음 → **달성 성능에서 cap을 역추론 불가** (둘 다 memory-bound면 cap이 10배 달라도 성능 동일). 2.11×에 "FPGA cap 증가의 기여"는 없다 — cap의 기여는 "올림"이 아니라 **"막지 않음"**(enabling, Fig 14). 비유: 진입로(BW)가 병목이면 차선 수(cap)를 늘려도 통과량 불변.
- **★ baseline vs Smart-Infinity roofline은 반드시 교차한다**: SI는 대각선(내부 BW×N)이 높지만 cap(FPGA×N)은 상대 하드웨어 peak보다 훨씬 낮음 → 낮은 $I$에선 SI가 위, 높은 $I$에선 상대가 위 — **한 번 교차**. GPU 비교든 **ZeRO-Inf(host CPU Xeon, ~TFLOPS) 비교든 동일** — updater×10은 $0.4 \times 7\text{GB/s} \times 10 \approx$ 수십 GFLOPS로 CPU cap의 ~1/100 (order 추정). 교차점 왼쪽(update, $I{\approx}0.4$) → CSD가 이김 / 오른쪽 → CPU/GPU가 이김. **이 교차점이 "왜 update만 CSD로 보내고 FW/BW는 GPU에 남겼나"의 수학적 국경선.** (같은 설계의 1-CSD vs N-CSD 비교라면 전 구간 N배 비례라 원점 외 교점 없음 — 비교 대상에 따라 다름.)
- **"BW도 cap도 다 높이면 전 구간 지배(교점 없음) 아닌가?"**: 논리는 맞다(양쪽 다 높으면 strict domination). 그러나 SI는 **일부러 그 길을 안 감** — $I{=}0.4$ 워크로드에선 큰 cap이 한 FLOP도 안 쓰이므로, cap은 최소한(안 막힐 만큼, Fig 14)·대각선은 최대한으로 **왼쪽 반평면에서만 지배**를 선택. 교점의 존재 = 설계 실패가 아니라 **낭비를 안 했다는 증거** = Fig 15 GFLOPS/달러 우위의 근원.
- **가설 기각 연습 — "FPGA cap을 조기에 만나 compute-bound 전환된 게 2.11×의 원인?"**: 그래프적으로 성립 가능한 가설이지만 3중 반증 — (전제 오류 주의: baseline의 update 연산기는 "없음"이 아니라 **host CPU(Xeon, ~TFLOPS)** — cap은 CSD 전환으로 오히려 크게 *하락*했는데도 무해했다. $I{=}0.4$에서 공급 80GB/s를 꽉 채워도 필요 연산은 $0.4{\times}80{=}32$ GFLOPS뿐이라 양쪽 모두 연산기가 공급을 소화하고 남음 — **구속은 한 번도 cap이 아니었음**) — ① 구조: $I^*_{agg} = \frac{N \cdot \text{updater}}{N \cdot \text{BW}}$로 **N과 무관**(CSD마다 FPGA가 딸려와 교점이 안 다가옴; cap이 N보다 느리게 클 때만 성립 — 중앙 FPGA 공유 설계라면 참) ② Fig 14: per-device에서 updater(>7GB/s) > SSD BW — 천장이 대각선 위 ③ Fig 11a: compute-bound 전환의 서명 = scaling 곡선 plateau인데 실측은 10개까지 almost linear (plateau의 실물은 오히려 baseline의 4-SSD 포화 = 고정 천장의 모습). **검증 습관**: "cap에 막혔다" 주장은 (a) per-device 처리량 비교 (b) scaling 곡선의 꺾임 (c) 관련 없는 자원(GPU 등급) 민감성, 3개로 판정.
- **"낮아진 cap이 2.11 높이에 있는 것 아닌가?" (기각 최종 라운드)**: 기하로는 그릴 수 있으나 2.11은 그 종류의 수가 아님. ① **어느 $I$의 수직선에도 2.11이 없다** — $I{\approx}0.4$에선 ~5×(막대 축소폭), FW/BW의 $I$에선 1×(같은 GPU); 2.11은 둘의 **시간 가중 혼합**이라 roofline 평면 위에 못 찍음. ② **2.11은 포화값이 아니라 아직 상승 중인 직선의 N=10 끝점** — cap 설명이 참이면 N을 늘려도 2.11에 머물러야 하는데 Fig 11a는 10개까지 계속 상승(11개면 >2.11). 상수함수는 "성장이 멈춘 값"만 만들 수 있음. ③ 산수: 진짜 cap(70+GB/s)/옛 공급(16) ≈ 4.4× — 2.11 높이의 수평선(34GB/s)이 되려면 updater가 5개여야 함(실제 10개).
- **"BW ~5×인데 왜 2.11×뿐?" — Roofline과 Amdahl은 다른 층이다**: 대각선 이득(~16 → 80GB/s ≈ 5×)은 **update 구간**의 숫자, 2.11×는 **iteration 전체(end-to-end)**의 숫자. 가속 안 된 FW/BW(GPU)가 남아 Amdahl로 희석: $f{\approx}0.8,\ s{\approx}5 \Rightarrow \frac{1}{0.2+0.16}{\approx}2.8\times$ (잔여 2M offload·오버헤드로 실측 2.11×). **update 구간은 끝까지 memory-bound**(FPGA cap에 안 막힘 — 증거: GPU 등급 바꾸면 speedup이 변함, A5000 1.85~1.98 vs A100 2.11 → 한계가 GPU 쪽 시간에 있음). 남은 지배 항이 FW/BW로 이동한 것 = "병목 벗기면 다음 병목 노출"(Amdahl 렌즈 ①). **Roofline = phase 안 병목 판정 / Amdahl = phase 합산 상한 — 층을 섞지 말 것.**

## 5. ⚠️ 적용 단위 규칙 & 실수 방지 체크리스트 (2026-07-15, Smart-Infinity 문답 총정리)

> [!warning] 대원칙 — Roofline 1장 = **한 하드웨어 × 한 phase**
> 그 안에서는 완벽한 도구. **여러 phase/여러 하드웨어의 합성은 시간표(Amdahl)의 일** — roofline 평면 위에 표현 불가.
> **이유**: y축은 rate(FLOP/s)인데 phase 합성에서 더해지는 건 **시간**. 게다가 하드웨어가 다르면 축(peak·BW) 자체가 달라 한 평면에 안 올라감.
> **논문의 실례**: Smart-Infinity도 두 층을 안 섞었다 — 가속기 방어는 rate 층(**Fig 14**), speedup 주장은 전부 시간 층(**Fig 3a·9·11b** stacked bar).

### 밟아본 실수 목록 (전부 실제로 시도했다 기각된 것)
1. **end-to-end 숫자를 roofline에 찍기** — 2.11×는 어느 $I$의 수직 간격도 아님 (update $I{\approx}0.4$에선 ~5×, FW/BW $I$에선 1× — 시간 가중 혼합이라 그 평면의 수가 아님)
2. **"BW N배 = 성능 N배"** — phase 안에서만 참. 전체는 가속 안 된 phase가 분모에 남아 Amdahl 희석
3. **"cap이 낮아 조기 포화" 가설을 데이터 확인 없이 그리기** — 아래 판정 3종부터
4. **baseline의 연산기를 '없음'으로 가정** — ZeRO-Inf의 update 연산기는 host CPU(Xeon, ~TFLOPS). cap은 CSD 전환으로 오히려 하락했는데도 무해했음 (수요 32 GFLOPS ≪ 어느 쪽 cap)
5. **스케일링에서 cap 고정 가정** — per-device 가속기(CSD당 FPGA)면 cap도 N배 → $I^*$ 불변, "확장하다 조기 교점" 사건은 구조적으로 불가
6. **명목 링크 BW(8×N)와 유효 공급(SSD media, 그 이하) 혼동** — 대각선의 실제 기울기는 가장 느린 공급자가 정함

### "cap에 막혔다" 판정 3종 체크 (주장 전 필수)
- (a) **per-device 처리량 비교** — 소화(가속기) vs 공급(매체): Fig 14식 직접 측정
- (b) **scaling 곡선의 plateau** — 상수함수의 서명은 성장 정지. 아직 상승 중인 끝점(2.11)은 cap이 만든 수가 아님
- (c) **무관 자원 민감성** — 진짜 cap이면 GPU 등급 따위에 숫자가 안 움직여야 (실측: A5000 1.85~1.98 → A100 2.11)

### 다음 논문 분석 템플릿 (SkyByte부터 적용)
1. **phase 분해 + 시간 비중** (Amdahl 층 — Fig 3a 역할의 그림 찾기)
2. **phase마다**: 어느 device에서 도나? $I$는? 공급 vs cap은? (roofline 층)
3. 제안 기법이 **어느 phase의 어느 천장**을 움직이나 (대각선인가 수평선인가, 명목인가 유효인가)
4. 합성해 end-to-end 예측 → 논문 실측과 대조 (안 맞으면 잔여 병목·오버헤드 찾기)

## 6. 내 연구와의 연결
- **[[Smart-Infinity]]**: update가 memory-bound임을 전제로 compute placement를 CSD로 — 이 노트가 그 "왜"
- **[[Design Principles]]**: Roofline은 Amdahl(어디가 병목인가)·Hierarchy(메모리 계층) 렌즈의 정량 버전 — 논문 읽을 때 "이 연산의 $I$는 얼마인가"를 습관적으로 묻기
- **[[H3 — DockerGPU, in-GPU control plane|H3]]**: 제어 평면 트래픽은 FLOPs도 bytes도 극소 — Roofline의 두 천장이 아니라 **latency**가 지배하는 제3의 영역이라는 점이 H3의 차별 지점

---
**관련**: [[Smart-Infinity]] · [[FPGA Programmability]] · [[Design Principles]] · [[Adam Optimizer]]
