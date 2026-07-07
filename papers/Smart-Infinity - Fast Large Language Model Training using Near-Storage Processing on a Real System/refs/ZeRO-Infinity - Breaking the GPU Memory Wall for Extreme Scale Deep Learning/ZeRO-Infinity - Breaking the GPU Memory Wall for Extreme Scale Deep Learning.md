# ZeRO-Infinity: Breaking the GPU Memory Wall for Extreme Scale Deep Learning

> **Source PDF**: [ZeRO-Infinity - Breaking the GPU Memory Wall for Extreme Scale Deep Learning.pdf](<ZeRO-Infinity - Breaking the GPU Memory Wall for Extreme Scale Deep Learning.pdf>)
> **NodeGraph**: [ZeRO-Infinity.html (새 탭에서 렌더링, export+push 후 유효)](https://raw.githack.com/Jeong-jin-Han/2026_CAMEL/main/papers/Smart-Infinity%20-%20Fast%20Large%20Language%20Model%20Training%20using%20Near-Storage%20Processing%20on%20a%20Real%20System/refs/ZeRO-Infinity%20-%20Breaking%20the%20GPU%20Memory%20Wall%20for%20Extreme%20Scale%20Deep%20Learning/ZeRO-Infinity.html)
> **Authors**: Samyam Rajbhandari, Olatunji Ruwase, Jeff Rasley, Shaden Smith, Yuxiong He (Microsoft)
> **Venue / Year**: SC '21 (arXiv preprint, 2021-04-16)
> **arXiv / DOI**: arXiv:2104.07857v1 [cs.DC]
> **Length**: 14 pages
> **Read status**: ☑ Full read (2026-07-07)
> **My reading purpose**: [Smart-Infinity](<../../Smart-Infinity - Fast Large Language Model Training using Near-Storage Processing on a Real System.md>)의 **baseline**. storage-offloaded training의 원조 프레임(infinity offload engine · bandwidth-centric partitioning · memory-centric tiling)을 이해해, Smart-Infinity가 무엇을 **물려받고** 무엇을 **바꿨는지**(update를 CPU→CSD로) 파악하기 위함.

---

## 📋 목차
- [TL;DR](#tldr)
- [Core thesis](#core-thesis)
- [Why this matters to me](#why-this-matters-to-me)
- [Structure overview](#structure-overview)
- [Section notes](#section-notes)
- [Key vocabulary](#key-vocabulary)
- [Citable quantitative data](#citable-quantitative-data)
- [🎯 Strategic anchor](#-strategic-anchor)
- [Connection to my research direction](#connection-to-my-research-direction)
- [Open questions / gaps](#open-questions--gaps)
- [References worth following up](#references-worth-following-up)
- [Personal annotations](#personal-annotations)

---

## TL;DR
LLM이 3년간 1000배 커지는 동안 GPU 메모리는 5배(16→80GB)밖에 안 늘어 **GPU memory wall**에 부딪혔다. ZeRO-Infinity는 **GPU + CPU DRAM + NVMe SSD**를 하나의 이질적(heterogeneous) 메모리 계층으로 묶어, **model code 리팩터링 없이** 극단적 규모(**32조 파라미터, 512 GPU**)를 학습한다. 핵심은 (1) 모든 model state를 CPU/NVMe로 내리는 **infinity offload engine**, (2) 파라미터를 전 DP 프로세스에 쪼개 모든 PCIe를 병렬로 쓰는 **bandwidth-centric partitioning**, (3) 큰 layer를 tile로 순차 실행해 model parallelism을 없애는 **memory-centric tiling**, (4) prefetch/overlap engine이다. 3D parallelism 대비 **50배** 규모, 단일 노드(16 GPU)로 최대 1T 모델을 model parallelism 없이 학습해 **대형 모델 학습을 민주화**한다 (p.1, p.8~9).

---

## Core thesis
> "a novel heterogeneous system technology that leverages GPU, CPU, and NVMe memory to allow for unprecedented model scale on limited resources without requiring model code refactoring" (Abstract, p.1)

추가 설명: 기존엔 model state를 여러 GPU의 **aggregate GPU memory**에 억지로 맞추느라 3D parallelism(data+model+pipeline) 같은 복잡한 조합이 필요했다. ZeRO-Infinity는 GPU 메모리 대신 **훨씬 크고 싼 CPU/NVMe**를 확장 계층으로 써서, 규모의 병목을 "메모리 용량"이 아니라 "이질적 메모리 대역폭을 얼마나 병렬로 끌어오나"의 문제로 바꾼다.

---

## Why this matters to me
이 논문은 [Smart-Infinity](<../../Smart-Infinity - Fast Large Language Model Training using Near-Storage Processing on a Real System.md>)의 **직접 baseline**이다. Smart-Infinity가 "storage-offloaded training"이라 부르는 세팅이 바로 ZeRO-Infinity의 NVMe offload다. Smart-Infinity는 이 프레임에서 **update 연산을 host CPU가 아니라 CSD 내부 FPGA로** 옮긴 것이므로, ZeRO-Infinity를 알아야 "무엇이 물려받은 것이고 무엇이 새 기여인지" 정확히 나뉜다. 또 ZeRO-Infinity의 **bandwidth-centric partitioning**(대역폭이 device 수에 비례)은 Smart-Infinity의 **aggregate internal CSD bandwidth**(CSD 수에 비례)와 같은 발상 — 내 memory-system 연구(CXL/disaggregation에서 aggregate 대역폭 확장)와도 직결된다.

---

## Structure overview
| § | Title | 핵심 takeaway |
|---|---|---|
| 1~2 | Intro / Background | GPU memory wall, ZeRO-3(모든 model state 분할)의 배경 |
| 3~4 | Memory & Bandwidth 요구 분석 | model state·activation 메모리, 효율 유지에 필요한 대역폭(70GB/s·1.5TB/s·1~4GB/s) |
| 5 | Design overview | infinity offload engine, activation CPU offload, **memory-centric tiling**, bandwidth-centric partitioning, overlap |
| 6 | Efficiency optimizations | **bandwidth-centric partitioning**(allgather로 전 PCIe 병렬), overlap-centric(dynamic prefetcher) |
| 7 | Ease of use | 자동 data movement(hook), 자동 model partitioning(init) → 리팩터링 불필요 |
| 8 | Evaluation | 32T params, 512 GPU 최대 20T at 34~49 TFlops/GPU, superlinear scaling, 단일 노드 1T |
| 9 | Conclusion | 이질적 메모리로 memory wall 초월, 향후 대역폭 요구 |

---

## Section notes

### §2 Background — ZeRO-3 (p.2~3)
ZeRO는 data-parallel의 메모리 **중복을 제거**하는 최적화다. ZeRO-1/2/3은 각각 optimizer state / +gradient / +parameter를 DP rank들에 **분할(partition)** 해 복제를 없앤다. ZeRO-Infinity는 **ZeRO-3 위에** 세워진다(모든 model state 분할).

### §4 Bandwidth 요구 분석 (p.4)
offload가 실용적이려면 계층별 대역폭이 필요: parameter·gradient는 **>70 GB/s**(≈ DGX-2의 GPU-GPU 대역폭), optimizer state는 **>1.5 TB/s**, activation은 **1~4 GB/s** (p.4~5). 그런데 단일 GPU↔CPU/NVMe PCIe는 겨우 **~12 GB/s**라 그대로는 턱없이 부족 → §6의 병렬화가 필요.

### §5.1 Infinity offload engine + memory-centric tiling (p.5)
- **infinity offload engine**: ZeRO-3로 분할된 **모든 model state를 CPU 또는 NVMe로 offload**(또는 GPU 유지)하되 메모리 요구에 따라 배치. 96노드(1536 GPU) NVMe에 100T 모델 state가 들어감.
- **CPU offload for activations**: activation checkpoint를 CPU로 내림 (10T 모델의 0.76TB가 1.5TB CPU에 fit).
- **memory-centric tiling**: 큰 operator(예: 대형 linear)를 **수학적으로 동등한 작은 tile들의 순차 실행**으로 바꿔, ZeRO-3의 fetch/release 패턴을 이용해 tile 하나씩 가져오고 놓는다. → **임의 크기 layer를 model parallelism 없이** 처리. (내가 궁금해했던 "block을 어떻게 쪼개나"의 답 = layer + 큰 layer는 tile.)

### §5.2 / §6.1 Bandwidth-centric partitioning (p.5~6)
기존 ZeRO/ZeRO-Offload는 각 파라미터를 **한 DP 프로세스가 소유**하고 필요 시 broadcast → 소스에서 그 GPU로 **PCIe 하나만** 활성, 나머지 idle. ZeRO-Infinity는 **각 파라미터를 전 DP 프로세스에 분할**하고 **allgather**로 모음 → **모든 PCIe 링크가 병렬**로 각각 1/dp을 가져옴 → **CPU/NVMe→GPU 유효 대역폭이 dp(=device 수)에 선형 비례**. (p.6)

### §6.2 Overlap-centric design (p.6~7)
**dynamic prefetcher**가 forward/backward 연산 순서를 그때그때 추적해, i번째 연산 실행 중에 i+1/i+2/i+3 연산에 필요한 파라미터의 3단계 전송(NVMe→CPU, CPU→GPU, GPU→GPU)을 미리·병렬로 수행. backward에선 gradient의 reduce-scatter/offload를 backward 연산과 overlap. temporary buffer 재사용으로 CPU 메모리 fragmentation 최소화.

### §7 Ease of use (p.7~8)
- **자동 data movement**: PyTorch submodule에 pre/post forward-backward **hook**을 주입해, 파라미터를 쓰기 직전 allgather하고 쓴 직후 partition/offload.
- **자동 model partitioning at init**: 모듈 생성자를 감싸 초기화 즉시 파라미터를 분할·offload → 전체 모델이 한 프로세스에 절대 통째로 안 올라감. → **수동 리팩터링 불필요**.

### §8 Evaluation (p.8~10)
- **규모**: 32조 파라미터(3D parallelism의 ~650B 대비 **50배**).
- **속도/확장**: 512 GPU에서 최대 20T 모델을 **34~49 TFlops/GPU**로. 500B에선 3D parallelism과 동등 처리량. 1T 모델에서 64→512 GPU **superlinear scaling**(aggregate PCIe/대역폭 선형 증가 덕).
- **민주화**: 단일 노드(16 GPU)로 10B~1T를 **model parallelism 없이**, ≤100B는 **>40 TFlops/GPU** → GPT-3급을 DGX-2 한 대로 fine-tuning.
- **memory-centric tiling**: 2GB fragmentation 조건에서 tiling 없으면 hidden 8K가 한계, tiling factor 16이면 **64K**까지.
- **vs ZeRO-Offload**: gradient offload를 aggregate PCIe로 병렬화해 64 GPU에서 **약 2배** 빠름(ZeRO-Offload는 단일 PCIe 제한).

---

## Key vocabulary
**Thesis / framing:**
- "heterogeneous system technology (GPU + CPU + NVMe)"
- "breaking the GPU memory wall"
- "democratizing large model training"

**Technical concepts:**
- "infinity offload engine" (모든 model state를 CPU/NVMe로)
- "bandwidth-centric partitioning" (전 DP에 분할 + allgather → 전 PCIe 병렬)
- "memory-centric tiling" (큰 operator를 tile 순차 실행 → model parallelism 불필요)
- "overlap-centric design / dynamic prefetcher"
- "ZeRO-3 (partition all model states)"

**Value language:**
- "without requiring model code refactoring"
- "unprecedented model scale on limited resources"

> ⚠ **피해야 할 어휘** (paper-signature, 그대로 echo 금지):
> - "breaking the GPU memory wall"
> - "infinity offload engine"

---

## Citable quantitative data
| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| Abstract, p.1 | 모델 3년간 1000배 ↑ vs GPU 메모리 5배(16→80GB) | memory wall 동기 |
| §5.2.1, p.5 | 단일 GPU↔CPU/NVMe PCIe ~12 GB/s | offload 병목의 근거 |
| §4, p.4 | 효율 대역폭: param/grad >70 GB/s, optimizer >1.5 TB/s, activation 1~4 GB/s | 왜 병렬화가 필요한가 |
| §8.2, p.8 | 32T 파라미터, 3D parallelism 대비 50배 | 규모 |
| §8.4, p.9 | 단일 노드(16 GPU)로 ≤100B를 >40 TFlops/GPU | 민주화 |
| §8.6, p.10 | 64 GPU에서 ZeRO-Offload 대비 약 2배(gradient offload) | 병렬 PCIe 이득 |

---

## 🎯 Strategic anchor
> "with the partitioned parameter and allgather based approach in ZeRO-Infinity, all PCIe links are active in parallel, each bringing in 1/𝑑𝑝 portion of the parameter ... the effective communication bandwidth between NVMe or CPU to the GPU, increases linearly with the 𝑑𝑝 degree." (§6.1, p.6)

→ **본인 활용**: "aggregate 대역폭을 device 수에 비례해 끌어올린다"는 이 아이디어가 [Smart-Infinity](<../../Smart-Infinity - Fast Large Language Model Training using Near-Storage Processing on a Real System.md>)의 "aggregate internal CSD bandwidth"와 동일 발상임을 면담/발표에서 연결. 내 CXL/disaggregation 연구의 "per-device path를 병렬로 합산"과도 직접 대응.

---

## Connection to my research direction
| 차원 | ZeRO-Infinity | Smart-Infinity | 내 방향(memory-system/CXL) |
|---|---|---|---|
| 확장 계층 | GPU→CPU→NVMe (heterogeneous) | 동일 + CSD 내부 연산 | CXL로 GPU↔memory/SSD disaggregation |
| update 위치 | **host CPU** (NVMe→CPU로 올려서) | **CSD FPGA** (near-storage) | (연구 여지) coherent near-data |
| 대역폭 확장 | bandwidth-centric partitioning (dp 비례) | aggregate internal bandwidth (CSD 수 비례) | per-device path 병렬 합산 (fabric) |
| 병목 | 여전히 NVMe↔CPU↔GPU를 지나는 traffic | 그 traffic을 CSD 내부 P2P로 제거 | shared fabric vs per-device path |

ZeRO-Infinity는 "메모리를 계층적으로 확장"까지 했고, Smart-Infinity는 "그 계층에서 연산을 데이터 옆으로 옮김"으로 한 발 더 갔다. 내 관심은 그 다음 — **CXL coherence로 near-data 연산을 일관성 있게** 묶는 지점.

---

## Open questions / gaps
- [ ] update가 여전히 **host CPU** 부담(NVMe→CPU로 올려 처리) → Smart-Infinity가 이걸 CSD로 옮겨 해결. 그럼 update 이후 남는 병목은?
- [ ] activation을 20T에서 CPU 메모리 한계로 batch가 작아짐(p.8) → activation을 NVMe로도 내리면? (저자도 future work로 언급)
- [ ] bandwidth-centric partitioning은 **다수 device(dp)** 전제 → single-GPU에선 이 이득이 없음(Smart-Infinity의 single-GPU 세팅과 대비).

---

## References worth following up
| 상태 | Ref | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [11] ZeRO (Rajbhandari et al., SC'20) | Zero Redundancy Optimizer | ZeRO-3 분할의 원리 |
| ☐ | [12] ZeRO-Offload (Ren et al., ATC'21) | CPU offload 학습 | update를 CPU가 하는 원조, Smart-Infinity 대비군 |
| ☐ | — | Megatron-LM (3D parallelism) | 비교 대상 baseline |

---

## Personal annotations
- (이 노트는 [Smart-Infinity](<../../Smart-Infinity - Fast Large Language Model Training using Near-Storage Processing on a Real System.md>)의 baseline 이해용으로 정독. "block을 어떻게 쪼개나"의 답 = §5.1.3 memory-centric tiling(layer + 큰 layer는 tile).)
