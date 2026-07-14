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

## 5. 내 연구와의 연결
- **[[Smart-Infinity]]**: update가 memory-bound임을 전제로 compute placement를 CSD로 — 이 노트가 그 "왜"
- **[[Design Principles]]**: Roofline은 Amdahl(어디가 병목인가)·Hierarchy(메모리 계층) 렌즈의 정량 버전 — 논문 읽을 때 "이 연산의 $I$는 얼마인가"를 습관적으로 묻기
- **[[H3 — DockerGPU, in-GPU control plane|H3]]**: 제어 평면 트래픽은 FLOPs도 bytes도 극소 — Roofline의 두 천장이 아니라 **latency**가 지배하는 제3의 영역이라는 점이 H3의 차별 지점

---
**관련**: [[Smart-Infinity]] · [[FPGA Programmability]] · [[Design Principles]] · [[Adam Optimizer]]
