# ACE: Efficient GPU Kernel Concurrency for Input-Dependent Irregular Computational Graphs

> **Source PDF**: [ACE.pdf](ACE.pdf)
> 🕸️ NodeGraph: [ACE.html (새 탭에서 렌더링)](https://raw.githack.com/Jeong-jin-Han/2026_CAMEL/main/papers/ACE%20-%20Efficient%20GPU%20Kernel%20Concurrency%20for%20Input-Dependent%20Irregular%20Computational%20Graphs/ACE.html)
> **Authors**: Sankeerth Durvasula, Adrian Zhao, Raymond Kiguru, Yushi Guan, Zhonghan Chen, Nandita Vijaykumar (University of Toronto)
> **Venue / Year**: PACT '24 (International Conference on Parallel Architectures and Compilation Techniques), October 14–16, 2024, Long Beach, CA, USA
> **arXiv / DOI**: 10.1145/3656019.3676897
> **Length**: 13 pages
> **Read status**: ☑ Full read (2026-07-13)
> **My reading purpose**: H3 working hypothesis(GPU 패키지 내 FPGA 제어 평면으로 host의 kernel 스케줄링/launch 결정을 대체) 관련성 판단 — 특히 LLM decode 단계의 동적/데이터-의존적 제어 문제와의 정합성 확인

---

## 📋 목차

- [TL;DR](#tldr)
- [Core thesis](#core-thesis)
- [Why this matters to me](#why-this-matters-to-me)
- [Structure overview](#structure-overview)
- [Section notes](#section-notes)
- [Key vocabulary (for own writing)](#key-vocabulary-for-own-writing)
- [Citable quantitative data](#citable-quantitative-data)
- [🎯 Strategic anchor](#-strategic-anchor)
- [Connection to my research direction](#connection-to-my-research-direction)
- [Open questions / gaps](#open-questions--gaps)
- [References worth following up](#references-worth-following-up)
- [Personal annotations](#personal-annotations)

---

## TL;DR

딥 RL 물리 시뮬레이션 엔진과 dynamic DNN(입력에 따라 실행 경로가 달라지는 신경망)은 작은 kernel들로 구성되어 GPU를 심하게 underutilize한다(평균 achieved occupancy 34~39%). 이 커널들은 서로 독립적인 경우가 많아 **concurrent 실행**하면 이득이 있지만, 문제는 kernel 간 의존관계(computational graph)가 **입력마다 runtime에만 결정**된다는 점이다. CUDA Graph 같은 기존 DAG 프레임워크는 매 입력마다 전체 dependency graph를 새로 구축해야 해서 오히려 큰 오버헤드(평균 실행시간의 47%)를 유발한다. 이 논문은 **ACE**라는 프레임워크를 제안한다 — CPU의 out-of-order 명령어 스케줄링에서 착안해, 전체 그래프가 아니라 **작은 "scheduling window"** 안의 커널들만 놓고 read/write 메모리 영역 겹침(annotation 기반)으로 의존성을 판단하고, 준비된(ready) 커널을 즉시 동시 launch한다. 소프트웨어 전용 구현(ACE-SW)과, GPU 하드웨어에 스케줄링 윈도우를 직접 얹어 CPU-GPU 동기화를 줄이는 하드웨어-소프트웨어 협력 구현(ACE-HW) 두 가지를 제시하며, 실제 하드웨어(RTX3060/4090)와 GPU 시뮬레이터(Accel-Sim) 양쪽에서 평가해 최대 2.19배(평균 1.56배) 속도 향상을 보인다.

---

## Core thesis

> "The key idea behind ACE is to perform inter-kernel dependency checks for a small window of kernels at runtime, similar to out-of-order instruction scheduling." (Abstract, p.258)

추가 설명: 전체 computational graph를 미리 다 알 필요 없이, 이미 launch된 kernel들 중 "아직 실행 중인 작은 창(window)"만 검사해서 의존성을 판별하면 충분하다는 것이 핵심 통찰이다. 이는 (i) 입력마다 다시 그래프를 만들 필요를 없애고 (ii) 전체 그래프 대신 부분 정보로도 fine-grained 병렬성을 끌어낼 수 있음을 보여준다.

---

## Why this matters to me

내 H3 가설("GPU 옆에 작은 FPGA 제어 평면을 둬서 host CPU가 하던 kernel 스케줄링/launch 결정을 대신하게 하면 어떨까")과 정확히 같은 문제의식—"input-dependent 상황에서 host 왕복 없이 GPU 커널 실행 순서를 결정하는 문제"—을 다룬다는 점에서 반드시 읽어야 했다. 특히 ACE-HW가 스케줄링 윈도우 관리(SRAM 기반, ready/pending 상태 추적)를 **GPU 하드웨어 안**으로 옮겨서 커널 완료마다 발생하던 CPU-GPU 동기화를 없앤 설계는, 내가 상상하는 "GPU 옆 FPGA 제어 평면"과 구조적으로 유사하다. 다만 뒤에서 정직하게 밝히듯, ACE가 푸는 "의존성"은 **커널 간 메모리 read/write 겹침**이라는 정적으로 정의 가능한 관계이지, 내가 타겟하는 LLM decode 루프의 "생성된 토큰 값 자체에 따라 계속할지 멈출지 분기하는" 데이터-값-의존적 제어와는 다르다. 그래서 이 논문은 H3의 "host 개입 감소" 방향성은 강하게 뒷받침하지만, "토큰 값 기반 조건부 제어"라는 핵심 표적까지 커버하지는 않는다 — 이 갭이 오히려 내 연구가 채울 수 있는 자리일 수 있다.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| 1 | Introduction | p.258–259 | Deep RL 시뮬레이션·dynamic DNN이 작은 커널들로 GPU를 underutilize; ACE-SW/ACE-HW 두 구현 제안 |
| 2 | Motivation | p.260–261 | Case study 1(딥 RL 시뮬레이션 엔진), Case study 2(dynamic DNN), Key observations(input-dependent + irregular dependency의 2대 도전) |
| 3 | Approach | p.261–262 | 기존 스케줄링 방식들의 한계 정리(Table 1); ACE의 핵심 아이디어 = out-of-order kernel scheduling within a window |
| 4 | Detailed Design | p.263–266 | ACE kernel wrapper(read/write segment annotation), ACE-SW 설계(소프트웨어 런타임 + CUDA streams), ACE-HW 설계(하드웨어 scheduling window + upstream load module), 오버헤드 분석 |
| 5 | Methodology | p.266 | 실제 HW(RTX3060) + Accel-Sim 시뮬레이터(RTX3070/4090 설정), 7개 workload |
| 6 | Evaluation | p.266–267 | Deep RL(§6.1), Dynamic DNN(§6.2), Static DNN(§6.3), 민감도 분석(§6.4), 에너지(§6.5) |
| 7 | Related Work | p.268–269 | Multi-stream, DAG 프레임워크(CUDA Graph/ATMI), Task-based CPU 스케줄링, CDP/DE, HW 지원 연구들과 비교 |
| 8 | Conclusion | p.269 | ACE = 최초로 low-overhead runtime scheduling과 dependency check를 결합한 automatic concurrent execution 프레임워크 |

---

## Section notes

### §1 Introduction (p.258–259)

딥 RL(강화학습) 훈련의 30~70%는 물리 시뮬레이션을 통한 데이터 수집 단계이며, 이 시뮬레이션들은 "각 CTA(thread block)가 서로 다른 시나리오를 시뮬레이션"하기 때문에 하나의 큰 커널로 합칠 수 없고, 대신 짧고 작은 커널들로 프로그래밍된다. Dynamic DNN(NAS로 설계된 신경망, mixture-of-experts 등)도 입력에 따라 실행 경로가 달라지며 마찬가지로 작은 커널을 다수 생성한다. 저자들은 두 가지 도전을 짚는다: (1) input-dependent computational graph — 그래프와 의존성이 실행 시점의 입력에 따라서만 정해짐, (2) irregular kernel dependencies — 그래프가 규칙적인 스트림으로 잘 분할되지 않아 fine-grain 스케줄링이 필요함. 기존 CUDA Graph/AMD ATMI는 DAG를 완전히 구성한 뒤에야 실행할 수 있어 매 입력마다 큰 준비 오버헤드가 든다.

> "ACE addresses this challenge and provides an efficient approach to enable out-of-order kernel scheduling in GPUs." (§1, p.259)

### §2 Motivation (p.260–261)

**Case study 1 (RL 시뮬레이션, Brax 프레임워크, ant/human/ct/w2d/grasp 환경)**: achieved occupancy가 평균 34%(즉 GPU 코어의 최대 65%가 놀고 있음, Fig.1), 커널 크기 분포(Fig.4)를 보면 ant 환경 커널의 약 40%가 CTA 9개 이하(Fig.4). 저자들은 이것이 "fundamental problem"이라고 명시한다 — 각 thread가 서로 다른 시나리오를 시뮬레이션하므로 큰 커널로 합치면 thread divergence가 커지기 때문.

**Case study 2 (dynamic DNN, InstaNAS/AmoebaNet 등)**: InstaNAS-A 워크로드는 RTX3060에서 평균 achieved occupancy 39%. 작은 필터로 FLOPs를 줄이도록 최적화된 convolution 레이어가 작은 커널의 원인.

**Key observations (§2.3)**: (1) 의존성이 입력마다 다시 결정돼야 함 — Brax에서 DAG 구성 시간이 전체 실행시간의 평균 47%(Fig.8)에 달함. (2) 의존성이 불규칙(irregular)해서 스트림 단위 분할이 어렵고, fine-grain 동기화 오버헤드(kernel launch + CPU 실행 + sync overhead)가 5~20µs 수준으로 발생(Fig.9).

### §3 Approach (p.261–262)

**§3.1 Prior mechanisms**: baseline GPU 하드웨어 모델(command processor가 커널을 command queue에서 꺼내 실행, 큐 간에는 독립으로 간주하고 동시 실행, 같은 큐 내에서는 순서대로 실행)을 설명한 뒤, 기존 방법들의 한계를 짚는다 — multi-stream 방식(수동 분할, 동기화 오버헤드 큼), DAG 프레임워크(CUDA Graph/ATMI, барrier 패킷 기반, 준비 오버헤드 큼), persistent threads(동질적 커널에만 효과적), CDP/device enqueue(부모-자식 1:1 의존성만 지원, 다중 부모 의존성엔 부적합). Table 1이 이를 요약한다(Applicability / Sync+Launch Overhead / Preparation Overhead 3축).

**§3.2 Key idea**: 하나의 command queue(혹은 stream) 위에서, 고정 크기의 "scheduling window"만 의존성 검사 대상으로 삼는다. 커널이 완료되면 window 안에서 새로 ready가 된 커널들을 찾아 동시에 launch한다(Fig.11). 이는 CPU의 out-of-order instruction scheduling과 정확히 같은 구조 — "전체를 보지 않고 최근 window만 본다"는 것이 오버헤드를 줄이는 핵심 트릭이다.

### §4 Detailed Design (p.263–266)

**ACE kernel wrapper (§4.1)**: 프로그래머(또는 라이브러리 작성자)가 각 커널이 읽고 쓰는 메모리 주소 범위(`__read_segments__`, `__write_segments__`)를 `get_addresses` 함수로 정의한다(Fig.15, Fig.16). 이 annotation은 라이브러리 단위로 한 번만 작성하면 되고, 애플리케이션 프로그래머가 매번 작성할 필요는 없다고 저자들은 강조한다.

**ACE-SW (§4.2)**: 순수 소프트웨어 런타임. Window 모듈(입력 FIFO 큐 관리, 의존성 체크, ready 상태 관리, Algorithm 1)과 Scheduler 모듈(각 스레드가 독립 CUDA stream에 ready 커널을 launch하고 `StreamSync`로 완료를 기다림, Algorithm 2, Fig.17)로 구성된다. 문제는 커널 완료마다 `StreamSync`로 **CPU가 블로킹**된다는 점 — 즉 host는 여전히 매 커널 완료마다 개입한다.

**ACE-HW (§4.3)**: scheduling window 관리 자체를 GPU 하드웨어(SRAM 기반, N개 슬롯, 슬롯당 8-bit kernel id + (N-1)개 upstream kernel id + 2-bit 상태)로 옮긴다(Fig.18, Fig.19). CPU 소프트웨어 런타임은 여전히 입력 FIFO 큐를 유지하고 `scheduled_list`(GPU 상태의 CPU측 캐시, 최대 M개, stale할 수 있음) 대비 의존성 체크를 수행해 커널을 GPU로 보내지만, **커널 완료 시점마다 CPU와 동기화할 필요가 없어진다** — GPU 안의 "upstream load module"이 stale한 의존성 정보를 스스로 정제하고 ready 커널을 dispatch한다.

**Overheads (§4.4)**: window 크기 32 기준 SRAM 1KB(GPU 전체), read/write segment 메타데이터는 세그먼트당 48비트, 하드웨어 갱신 지연은 N-1 사이클(수십~100ns 수준, baseline 커널 launch 오버헤드인 수 마이크로초 대비 무시할만함), CPU 측 의존성 체크는 window=32/RW-segment=10 기준 1640ns(Table 2).

### §5 Methodology (p.266)

실제 하드웨어: Intel 11700K + NVIDIA RTX3060(ACE-SW). 시뮬레이터: Accel-Sim으로 RTX3070/RTX4090에 준하는 두 configuration(Table 3, 4)을 모델링해 ACE-HW 평가. 스케줄링 윈도우 크기는 기본 32. 워크로드: (1) Brax 기반 5개 RL 시뮬레이션 환경, (2) InstaNAS/Dynamic Routing/Conditional Convolution 3개 dynamic DNN, (3) NASNet/AmoebaNet/SqueezeNet/RandomWire 4개 static DNN(비교군).

### §6 Evaluation (p.266–267)

- **Deep RL (§6.1)**: ACE-SW 최대 1.87배, ACE-HW 최대 2.19배(§1 abstract 수치와 일치). CUDAGraph는 그래프 구성/전송 오버헤드 때문에 오히려 slowdown.
- **Dynamic DNN (§6.2)**: ACE-HW 최대 1.39배, 평균 1.3배(ACE-SW 평균 1.05배). I-NAS는 ACE-SW에서 오히려 느려짐 — 병렬화 시 커널 launch 오버헤드가 baseline(단일 스트림 순차 실행)에서는 숨겨져 있었기 때문.
- **Static DNN (§6.3)**: ACE-HW 평균 1.31배, ACE-SW 평균 1.16배. CUDAGraph와 거의 동일한 성능(정적 그래프이므로 DAG 구성 비용이 문제되지 않음) — 딱 하나의 워크로드에서만 CUDAGraph가 근소하게 앞섬(전체 커널에 걸친 의존성 정보를 활용할 수 있어서).
- **Sensitivity (§6.4)**: 윈도우 크기 16→32로 늘리면 Brax는 평균 4.5% 향상하지만 DNN들은 거의 영향 없음 — DNN들은 window 확대로 노출되는 inter-kernel parallelism 자체가 부족하기 때문.
- **Energy (§6.5)**: ACE-HW 평균 21.6%, ACE-SW 평균 6.1% 에너지 절감(실행시간 단축에 기인).

### §7 Related Work (p.268–269)

Multi-stream 소프트웨어 기법, CUDA Graph/ATMI 등 DAG 프레임워크, task-based CPU 스케줄링(task superscalar, carbon, TDM, ADM — "primary bottleneck in these applications is the long latency for dependency checks"인 CPU와 달리, GPU에서는 "primary bottleneck ... is the long latency to launch/signal completion of kernels instead"라고 명확히 구분), GPU dynamic parallelism/device enqueue(부모-자식 1:1만 지원), MIG(멀티유저 공유용, underutilization 자체는 해결 못함), dynamic DNN을 위한 컴파일러/런타임 기법(dynamic batching, kernel fusion — "orthogonal to our approach"라고 명시).

### §8 Conclusion (p.269)

> "We introduce ACE, the first framework that enables automatic concurrent kernel execution with low overhead runtime scheduling and dependency checks." (§8, p.269)

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "input-dependent irregular computational graphs" — 그래프와 의존성이 실행 시점 입력에 따라서만 정해지는 상황을 가리키는 정확한 표현
- "GPU underutilization ... as a result of small kernels" — 작은 커널로 인한 GPU 저활용이라는 문제 정의

**Technical concepts:**
- "scheduling window" — 전체 그래프 대신 최근 커널들의 작은 창만 놓고 의존성을 검사하는 개념 (CPU OoO scheduling과의 유비)
- "upstream kernel(s)" — 어떤 커널이 의존하는 선행 커널 집합을 가리키는 용어
- "read/write segments" — 메모리 주소 범위 기반 의존성 정의 방식
- "achieved occupancy" — GPU 활용도를 정량화하는 지표(활성 warp 비율)

**Value language:**
- "concurrent kernel execution" — 여러 독립 커널을 동시에 실행해 활용도를 높인다는 가치 언어
- "low overhead runtime scheduling"

> ⚠ **피해야 할 어휘** (paper-signature, 직접 echo하면 안 됨):
> - "ACE" / "Automatic Concurrent Execution" (이 논문 고유의 브랜드명)
> - "ACE-SW" / "ACE-HW" (이 논문의 구현 이름)
> - "scheduled_list" / "upstream load module" (이 논문의 구체적 컴포넌트 이름)

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| Abstract, p.258 | "speedups of up to 2.19× (1.56× on average)" | ACE 전체 성능 향상 인용 시 |
| §1, p.258–259 | RL 시뮬레이션 achieved occupancy 평균 34%; dynamic DNN 평균 39% | GPU underutilization 정량 근거 |
| §2.1, p.260, Fig.1 | "as much as 65% of the GPU cores are underutilized on average" | motivation 인용에 강력 |
| §2.1, p.260 | 딥 RL 훈련 시간의 "30–70%"가 시뮬레이션(데이터 수집) 단계 | 시뮬레이션 병목의 중요성 근거 |
| §2.3, p.261, Fig.8 | DAG 구성 시간이 전체 실행시간의 평균 47% (Brax) | 정적 DAG 캡처 방식의 비용을 지적할 때 |
| §2.3, p.261, Fig.9 | 커널 launch + 동기화 오버헤드 "vary between 5-20µs" | fine-grain 스케줄링 비용 인용 |
| §4.4, p.265 | window=32 기준 SRAM "1KB ... for the entire GPU" | ACE-HW의 하드웨어 비용이 매우 작다는 근거 |
| §4.4, p.265 | ACE-HW 추가 오버헤드 "negligible ... (in the order of a few microseconds)" 대비 50–100ns | host 개입 감소분의 크기 감을 잡을 때 |
| §6.1, p.266 | ACE-SW 최대 1.87×, ACE-HW 최대 2.19× (Deep RL) | 카테고리별 성능 |
| §6.2, p.267 | Dynamic DNN: ACE-HW 최대 1.39×, 평균 1.3× | |
| §6.3, p.267 | Static DNN: ACE-HW 평균 1.31×, ACE-SW 평균 1.16× | ACE가 정적 그래프에도 유효함을 보일 때 |
| §6.5, p.267 | 에너지 절감 ACE-HW 평균 21.6%, ACE-SW 평균 6.1% | |

---

## 🎯 Strategic anchor

> "Our approach is specifically designed to mitigate this scheduling cost by avoiding direct communication from the GPU to the CPU, thereby reducing potential overheads." (§3.1, p.262)

→ **본인 활용**: 이 문장은 정확히 H3의 문제의식("GPU가 host CPU와의 왕복 없이 스스로 실행 순서를 결정하게 하려면 어떻게 해야 하는가")과 맞닿아 있다. 면담에서 "ACE는 이 방향으로 한 걸음을 보여줬지만, 커널 간 메모리 겹침이라는 정적으로 판별 가능한 의존성에 국한되어 있고, 제 관심사인 '생성된 값 자체에 따른 조건부 제어'(LLM decode의 EOS 판정 등)까지는 다루지 않는다"는 식으로, ACE를 내 연구의 인접 선행 사례이자 갭을 짚는 근거로 쓸 수 있다.

---

## Connection to my research direction

| 차원 | 이 paper (ACE) | 본인 방향 (H3) |
|---|---|---|
| Scope | 하나의 command queue/stream 내에서 이미 launch된 커널들 사이의 **메모리 read/write 겹침 기반** 의존성 해소 | GPU 옆 FPGA 제어 평면이 LLM decode 루프에서 **매 토큰 생성값에 따라 달라지는 제어 결정**(EOS 판정, continuous batching 편입/이탈, KV 배치)을 대신 내리는 것 |
| Mechanism | annotation(programmer/library가 정의한 read/write segment)으로 정적 의존성 그래프의 부분집합(window)을 runtime에 재구성 | 커널 "사이"의 의존성이 아니라, 커널 "출력값"에 대한 데이터-의존적 분기(runtime value-dependent control flow) — ACE의 기법으로는 표현되지 않는 문제 |
| Host(CPU) 개입 | ACE-SW: 커널 완료마다 `StreamSync`로 CPU가 블로킹됨(개입 다량 존속). ACE-HW: 완료 이벤트마다의 동기화는 제거되지만, CPU는 여전히 입력 FIFO 큐에 커널을 넣고 `scheduled_list` 대비 의존성 체크를 수행하는 소프트웨어 런타임을 계속 돈다(§4.3, Fig.18) — **host가 완전히 사라지지 않는다.** | H3는 host의 "결정" 자체를 GPU 인접 FPGA로 옮기는 것을 목표로 하며, ACE-HW보다 한 단계 더 host를 배제하는 지점을 지향 |
| Open space | ACE는 "어떤 두 커널이 서로 독립인가"라는 질문만 푼다. "이 커널을 launch할지 말지가 방금 생성된 토큰이 EOS인지에 달려있다" 같은 값-조건부 결정은 다루지 않음(저자들도 §7에서 dynamic batching/kernel fusion을 "orthogonal to our approach"라고 명시) | 이 갭이 정확히 내가 채우려는 자리 — decode 루프의 "값 기반 조건부 launch/stop 결정"을 host 왕복 없이 GPU 인접에서 내리는 제어 평면 |

정직하게 정리하면: ACE는 H3의 **"host-GPU 통신을 줄여 input-dependent 상황에서 GPU 활용도를 높인다"는 큰 방향에는 align**된다 — 특히 ACE-HW가 스케줄링 윈도우 자체를 GPU 하드웨어로 옮겨 완료-이벤트 단위 동기화를 없앤 설계는, 내가 상상하는 FPGA 제어 평면의 축소판처럼 보인다. 그러나 **메커니즘 층위에서는 align되지 않는다**: ACE의 의존성은 커널이 어떤 메모리를 읽고 쓰는지로 컴파일 타임/annotation 타임에 알 수 있는 정적 관계이고, dependency check는 결국 "겹침 여부"라는 결정 가능한(deterministic) 함수다. 반면 내가 표적으로 삼는 LLM decode 제어(EOS 판정, continuous batching 편입)는 **커널 실행 결과값 자체를 읽어야 결정되는 조건부 제어**로, ACE의 read/write-segment 겹침 검사 프레임으로는 표현할 수 없다. 또한 ACE-HW조차 CPU 소프트웨어 런타임(입력 큐 관리 + 의존성 체크)을 완전히 없애지 않는다는 점에서, "host를 완전히 대체"하는 데까지는 가지 않는다 — 이 논문은 host 개입을 "줄이는" 사례이지 "없애는" 사례가 아니다.

---

## Open questions / gaps

- [ ] ACE의 read/write-segment 겹침 검사는 커널의 **입력 인자 값**이 아니라 **메모리 주소 범위**만 본다 — 값 자체에 의존하는 제어(예: "이 스칼라가 0이면 다음 커널을 건너뛴다")는 어떻게 표현/검출할 수 있는가? 논문은 이 케이스를 다루지 않는다.
- [ ] ACE-HW도 CPU 소프트웨어 런타임(입력 큐 관리, `scheduled_list` 유지, dependency check)을 완전히 없애지 않는다 — 이 CPU 상주 로직 자체를 GPU 인접 보조 프로세서(FPGA 등)로 완전히 옮기면 어떻게 되는가?
- [ ] 논문은 단일 GPU/단일 애플리케이션 맥락만 다룬다 — multi-tenant나 multi-GPU/coherence 맥락에서 scheduling window 개념이 어떻게 확장될 수 있는지는 언급이 없다(MIG는 "직교적" 문제로만 다룸, §7).
- [ ] LLM inference decode 단계(continuous batching, KV cache 관리)는 이 논문의 workload set(RL 시뮬레이션, dynamic DNN, static DNN)에 포함되지 않는다 — ACE의 window 기반 접근이 decode 루프의 초당 수십~수백 회 반복되는 초경량 커널 시퀀스에도 적용 가능한지는 미검증.

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [1] | NVIDIA, "Getting started with CUDA Graphs" (2020) | ACE가 비교 baseline으로 삼는 정적 DAG 프레임워크의 원 자료 |
| ☐ | [9] | Blockmaestro, "Enabling Programmer-Transparent Task Execution in GPU Systems" (ISCA'20) | GPU 커널 간 데이터 의존성 검사를 다루는 하드웨어 지원 선행연구 |
| ☐ | [10] | WIREFRAME, "Supporting Data-dependent Parallelism through Rapid Dependency Graph Execution in GPUs" (MICRO'17) | ACE와 유사한 문제의식의 이전 하드웨어 접근 — CTA 단위 의존성 |
| ☐ | [30] | "Inter-operator scheduling" — 계산 그래프를 병렬 실행 가능한 섹션으로 분할하는 소프트웨어 기법 | multi-stream 기반 접근의 대표 사례, 비교 관점 |
| ☐ | [31] | Task superscalar (2010) | CPU에서의 out-of-order task scheduling — ACE가 유비로 삼는 원류 아이디어 |
| ☐ | [33] | Brax — "A Differentiable Physics Engine for Large Scale Rigid Body Simulation" | 이 논문의 핵심 RL 시뮬레이션 벤치마크, deep RL 가속 맥락 이해에 필요 |
| ☐ | [47] | "In-Register Parameter Caching for Dynamic Neural Nets with Virtual Persistent Processor Specialization" (MICRO'18) | dynamic DNN을 겨냥한 하드웨어 기법, 비교 참고 |
| ☐ | [70] | ADM, "Flexible architectural support for fine-grain scheduling" | task-based 스케줄링에서 CPU 병목(의존성 체크 vs launch 지연)의 차이를 다루는 선행연구 |

---

## Personal annotations

<자유 형식 메모. 본인이 paper 읽으며 떠올린 생각·이견·후속 아이디어. user가 직접 추가하는 영역.>
