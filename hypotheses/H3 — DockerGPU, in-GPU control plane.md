---
title: "H3 — DockerGPU: in-GPU control plane"
aliases: [H3, DockerGPU, in-GPU control plane, in-GPU instruction fetching]
type: hypothesis
status: working          # working(검증 전) / testing / supported / revised / dropped
formed: 2026-07-10
tags:
  - hypothesis
  - topic/gpu
  - topic/near-data
---

# H3 — DockerGPU: in-GPU control plane

> [!warning] 검증 전 가설 (구현 계획 없음, 상상 단계)
> 본인이 세운 **working hypothesis** (2026-07-10 형성, [[Smart-Infinity]] 발표 준비 중 착상). 사실 아님 — 검증 대상.

## 한 줄 가설
> **[[DockerSSD]]가 "실행환경(컨테이너)을 데이터가 있는 SSD로 내려보냈듯", GPU 패키지 안에 FPGA로 만든 작은 CPU/VM(제어 평면)을 넣고 VRAM을 공유시키면, host CPU↔GPU 메모리 왕복·제어 왕복을 GPU 안에서 끝낼 수 있다.**

- 최초 컴파일/실행만 host가 담당 → 이후 **패키지 내 FPGA가 GPU를 통솔**(kernel 스케줄링·instruction 공급).
- 유추의 뼈대: **in-storage compute → "in-GPU instruction fetching"** — near-data 불변 원리("연산/제어를 데이터 옆으로")를 storage에서 GPU로 일반화.
- 방법 원리: [[Venice]]식 cross-domain 이식 (다른 도메인의 패턴을 새 문제에 적용).

## 문제 (왜)
- CPU↔GPU 전송 + kernel launch 오버헤드 = 실측으로 검증된 병목 ([[Smart-Infinity]]의 공유 PCIe 병목, [[BaM]]의 CPU-중심 오케스트레이션 비용).
- 지금 구조는 제어 평면(CPU)이 데이터(VRAM)에서 멀리 있음 → 매 결정마다 왕복.

## 이웃 지형 (novelty의 적 — 정직하게)
| 이웃 | 뭘 했나 | 내 가설과 차이 |
|---|---|---|
| **AMD MI300A** | CPU+GPU chiplet 한 패키지, **HBM3 공유** (실리콘) | 제어 평면이 **고정된 CPU 코어** |
| **NVIDIA GH200** | CPU-GPU coherent link (NVLink-C2C) | 패키지 분리, 링크로 좁힘 |
| **[[BaM]]** (ASPLOS'23) | GPU가 CPU 없이 스스로 storage 접근 | 데이터 평면 자율화 (제어 평면은 아님) |
| persistent kernels / CUDA Graphs | launch 왕복을 SW로 절감 | HW 구조는 그대로 |
| [[DockerSSD]] (HPCA'24) | SSD에 컨테이너 실행환경 | 대상이 storage (본 가설의 원형) |

→ **"CPU↔GPU 왕복 제거" 자체는 산업·학계 총력전 중** = raw 아이디어로는 novelty 없음. 검증된 문제라는 뜻이기도.

## ★ 살아남는 알맹이 — "왜 하필 FPGA인가"
- MI300A는 고정 CPU 코어를 넣었다. FPGA를 넣는 이유가 있어야 연구가 됨:
> **워크로드마다 제어 평면(스케줄링·prefetch·instruction 공급 정책)을 재구성할 수 있다면?**
- = **[[H1 — 워크로드 특화로 multi-node coherence 줄이기|H1]](워크로드 특화)의 GPU 버전.** H1·H3가 같은 취향(범용 포기 → 특화로 비용 제거)으로 수렴 — 우연 아님.

## 메모리 공유 우려에 대한 답 (2026-07-10 문답)
- "같은 chip에서 mem 공유하면 느려지지 않나?" → **공유 자체는 느리지 않다. "무슨 메모리를 공유하냐"의 문제.**
- 전통 iGPU/APU가 느린 이유 = 좁은 DDR(수십 GB/s)을 CPU와 **경쟁하며** 공유해서. HBM 미사용이 핵심 원인 (본인 직감 적중).
- MI300A는 공유 메모리가 **HBM3 (~5.3TB/s)** → 공유해도 안 느림. Apple M Ultra도 wide LPDDR(~800GB/s)로 동일 원리.
- 남는 진짜 trade-off: ① HBM 용량 한계 ② 전력/발열 예산 공유 ③ CPU 트래픽이 GPU 대역폭 오염(QoS/간섭) — 이게 연구 지점.
- H3 관점: FPGA 제어 평면의 트래픽은 **데이터 평면 대비 극소** → 대역폭 오염 걱정은 작고, 쟁점은 **latency**(제어 결정의 왕복 시간)임.

## 구체화 (2026-07-10 2차 문답)
- **수준 확정: ① kernel launch/스케줄링(제어 흐름) 수준** — ISA fetch 대체(②) 아님. 컴파일 시점에 host 제어 코드를 분할해 FPGA에 배치, 제어 트래픽을 device 안에 가둬 **CPU↔GPU 상호 간섭 자체를 차단**.
- ★ **multi-GPU 확장이 최강 조각**: GPU마다 FPGA 1개 → 기존 "모든 GPU 제어가 단일 CPU로 집중 → contention" 병목을 분산으로 해소.
- = **[[Smart-Infinity]] 논리 구조의 제어-평면 이식**: (공유 PCIe로 데이터 집중→포화 / CSD당 FPGA→선형 확장) ↔ (CPU로 제어 집중→contention / GPU당 FPGA→선형 확장?). 데이터 평면이 아니라 **제어 평면**이라는 점이 고유 지점.
- 이웃 재점검: CUDA Dynamic Parallelism(GPU 자가 launch 가능하나 스케줄링·동기화는 CPU 몫) · CUDA Graphs(정적 시퀀스만) · NVSHMEM/GPU-initiated comm(**데이터 평면**의 CPU 우회는 진행 중, **제어 평면 분산은 미완** ← 빈 자리) · DPU/BlueField("device 옆 제어 프로세서" 트렌드의 방증).
- **최대 난관 = 컴파일러**: host 코드엔 launch 외에 Python 런타임·데이터 로딩·optimizer·동적 분기가 섞임 → "어디까지 FPGA로 내릴지 자동 분할"이 핵심 문제. **본인 KECC 배경이 정확히 이 지점의 무기.** H1의 "누가 명시하나"와 동형 → **H1·H2·H3이 전부 "특화/분할의 경계를 누가·어떻게 긋나"로 수렴** (박사 주제 원석 후보).

## 실현 경로 (2026-07-10 3차 문답 — "직접 만들어야 하나 / 왜 없나 / 코드 다 다시 짜나")
- **칩 제조 불필요**: [[Smart-Infinity]]가 CSD를 제조하지 않고 시판 SmartSSD로 증명했듯, **시판 FPGA 카드(Alveo) + GPU를 같은 PCIe switch에서 P2P**(GPUDirect/DirectGMA)로 묶어 원리를 증명. "같은 패키지"는 비전, PCIe 프로토타입이 feasibility 증거. 이 장비 구성 = CAMEL 기존 인프라 그대로.
- **왜 여태 없었나 = 자물쇠**: ① NVIDIA 제어 평면(command buffer·doorbell)이 proprietary — CPU 아닌 장치의 launch가 막혀 있었음 ② 통합 시도는 전부 범용 CPU 코어를 넣음(MI300A·GH200·DPU) — "왜 FPGA인가"를 아무도 주장 안 함 ③ SW 우회(CUDA Graphs·persistent kernels)가 그럭저럭 버팀.
- ★ **Why now (timing argument)**: AMD **ROCm/HSA는 공개** — kernel dispatch = 메모리에 AQL packet 쓰기 + doorbell (공개 스펙) → P2P 가능한 FPGA가 CPU 없이 dispatch 가능. [[BaM]]이 역방향(GPU가 NVMe doorbell로 SSD 조종)을 이미 실증 = 장치가 장치를 제어하는 선례. "이제야 가능해졌다"는 intro 논거가 됨.
- **라이브러리 재작성 불필요 — interception**: PyTorch 수정 0. 런타임 shim이 launch를 가로채 1 iteration의 kernel 시퀀스를 캡처(CUDA Graphs식) → FPGA에 스케줄 업로드 → FPGA가 AQL queue에 재생 dispatch. **학습 루프 = 반복 동일**이라 "한 번 캡처, 무한 재생"과 정확히 맞음. Smart-Infinity의 DeepSpeed drop-in과 같은 통합 철학.
- **단계**: Phase 0 제어 트래픽·launch 오버헤드 실측(코딩만, Fig 3 역할) → Phase 1 별도 CPU 코어가 FPGA 에뮬레이션(분리 이득 선증명, 코딩만) → Phase 2 Alveo 직접 doorbell → Phase 3 multi-GPU(GPU당 dispatcher).
- **제약**: 이 경로는 AMD GPU(ROCm)가 열려 있음 — NVIDIA는 자물쇠와 싸워야.
- **FPGA 요건 (아무거나 X, 조건 3개)**: ① PCIe endpoint + bus mastering(능동 write) ② GPU와 같은 switch 아래 P2P(ACS/IOMMU) ③ 성숙한 스택. HBM·대형 로직 불필요(제어라서 극소). 추천: **Alveo(XRT — Smart-Infinity와 같은 생태계)** > 랩 SmartSSD(KU15P)를 P2P dispatcher 실험대로 재활용(장비 0원) > Zynq/Versal(ARM 내장 — Phase 1→2를 한 보드에서). **진짜 관문은 FPGA가 아니라 GPU 쪽**: Large/Resizable BAR 노출 + ROCm user-mode queue를 P2P 접근 가능 위치에 배치 + ACS/IOMMU 커널 설정(본인 OS/kernel 경험이 쓰이는 지점).

## ★ 전제 교정 & 재조준 (2026-07-10 4차 문답 — "어떤 병목을 푸는 거지?")
- **용어**: kernel = GPU에서 실행되는 함수(matmul kernel 등). 학습 1 step = kernel 수백~수천 launch. OS kernel과 무관.
- **멘탈 모델 교정**: "CPU↔GPU를 왔다갔다"의 실체는 **데이터가 아니라 제어**. 가중치는 VRAM 상주, activation/gradient는 VRAM 내부(multi-GPU면 NVLink 직행) — **데이터는 안 왔다갔다**. 왕복하는 건 ① kernel launch(매 kernel마다 CPU가 큐에 명령, 개당 ~5–30μs) ② 동기화/분기(`loss.item()`, 배칭 결정 등 CPU가 GPU 결과를 기다렸다 판단).
- **Training**: 큰 kernel(ms) ≫ launch(μs) → 오버헤드 묻힘 + 정적 반복이라 CUDA Graphs로 이미 해결 → **H3 이득 작음** (multi-GPU 제어 집중 정도만 남음). 학습의 진짜 병목은 메모리 용량·GPU간 통신 (Smart-Infinity 영역).
- **Inference (LLM decode) = 진짜 표적**: 토큰 1개씩 → kernel이 작고 짧음(수십 μs) → **launch-bound** (CPU가 병목, 실존 현상). 결정타 = **동적 제어**: EOS 판정·continuous batching·KV 배치·spec-decode accept/reject를 매 토큰 CPU가 판단 → 왕복+정지 반복. **CUDA Graphs는 정적 반복만 캡처 — 데이터 의존적 동적 제어는 못 담음** ← 기존 SW 해법의 빈틈 = H3의 좁고 정확한 표적.
- **재조준**: ~~"학습의 데이터 왕복 제거"~~(잘못된 전제) → **"LLM inference decode의 동적 제어 평면을 GPU 옆(FPGA)으로"**. [[SwiftSpec]](async spec-decoding, CPU 제어 오버헤드와의 싸움)과 접속 — 시대 흐름(LLM serving)과 자연 접점.

## 열린 질문 (다음에 답할 것)
- [ ] FPGA 재구성이 고정 CPU 코어 대비 이기는 워크로드 시나리오 구체화 (H1의 "특화가 이기는 지점" 질문과 동형)
- [ ] MI300A·GH200 아키텍처 자료 정독 → 이들이 *못* 하는 것 목록화
- [ ] host 코드 분할 가능성 분석: 실제 학습 루프에서 "FPGA로 내릴 수 있는 제어"의 비율 측정 (컴파일러 각도)
- [ ] 읽기: [[DockerSSD]] · [[BaM]] (vault) → CUDA Dynamic Parallelism·Graphs·NVSHMEM 문헌

---
**관련**: [[DockerSSD]] · [[BaM]] · [[Smart-Infinity]] · [[H1 — 워크로드 특화로 multi-node coherence 줄이기]] · [[Communication Tax]]
