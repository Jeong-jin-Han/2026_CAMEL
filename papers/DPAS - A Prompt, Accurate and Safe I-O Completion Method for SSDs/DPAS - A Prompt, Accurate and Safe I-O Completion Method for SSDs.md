---
title: "DPAS: A Prompt, Accurate and Safe I/O Completion Method for SSDs"
aliases: [DPAS, PAS]
description: "하이브리드 폴링의 sleep 시간을 최근 2개 I/O의 binary sleep 결과로 추적(PAS)하고 폴링·인터럽트·PAS를 런타임에 동적 전환(DPAS)하는 I/O 완료 기법"
venue: FAST
year: 2026
tier: deep
status: done
tags:
  - paper
  - cluster/fs
  - topic/io-completion
  - topic/interrupt
  - venue/fast
  - year/2026
---

# DPAS: A Prompt, Accurate and Safe I/O Completion Method for SSDs

> **FAST 2026** · `cluster/fs` · Source: [DPAS - A Prompt, Accurate and Safe I-O Completion Method for SSDs.pdf](DPAS%20-%20A%20Prompt,%20Accurate%20and%20Safe%20I-O%20Completion%20Method%20for%20SSDs.pdf)

**저자**: Dongjoo Seo (UC Irvine), Jihyeon Jung, Yeohwan Yoon (Kookmin Univ.), Ping-Xiang Chen (UC Irvine), Yongsoo Joo (Kookmin Univ., 교신저자), Sung-Soo Lim (Kookmin Univ.), Nikil Dutt (UC Irvine)

## TL;DR
SSD I/O 완료에서 hybrid polling은 sleep 시간을 정확히 맞추기 어렵다. 이 논문은 (1) **PAS** — 실제 I/O 시간을 재는 대신 최근 2개 I/O의 *binary* sleep 결과(undersleep/oversleep)만으로 sleep 시간을 곱셈식으로 조정해 lower envelope를 즉각 추종하는 latency tracking 기법과, (2) **DPAS** — timer failure와 I/O queue depth를 관찰해 classic polling / interrupt / PAS 세 모드를 런타임에 전환하는 기법을 제안한다. 4KB random read에서 PAS는 Linux hybrid polling 대비 CPU 사용량을 21 percentage point 줄이고, CPU 경합·I/O 간섭이 동시에 있는 상황에서 DPAS는 인터럽트 대비 YCSB 성능을 3D XPoint SSD에서 9%, TLC NAND SSD에서 5% 향상시킨다.

## 문제 & 동기
- 인터럽트는 context switch, cache pollution, CPU power state 전환 등 숨은 비용이 크고, 순수 polling은 CPU를 독점해 CPU 경합 시 성능이 급락한다.
- Hybrid polling은 먼저 자고(sleep) 나중에 폴링해 둘을 절충하지만, sleep 시간이 짧으면 **undersleeping**(CPU 절감 부족), 길면 **oversleeping**(앱 지연 증가)이 발생한다. oversleeping이 더 치명적이라 우선 회피해야 한다.
- 기존 방법(LHP, EHP, HyPI)은 주기적(epoch) 통계에 의존해 급변하는 latency를 못 따라가고, **측정한 I/O 시간이 device latency와 SW/scheduling delay를 구분하지 못한다**. 그 결과 oversleep을 device 지연으로 오인해 sleep 목표를 부풀리는 "latency shelving"에 빠진다.
- undersleep 시에도 hybrid polling은 timer-sleep primitive 호출에 따른 context-switch 비용을 그대로 부담한다.

> [!quote]- 📄 원문 표현 (paper)
> "Existing hybrid polling methods [16, 26, 34] determine the sleep duration from periodically updated I/O-time statistics. However, such epoch-based policies lose accuracy during abrupt latency changes. Furthermore, they fail to differentiate between device delays and oversleep errors because they rely exclusively on measured I/O duration. This lack of differentiation leads to 'latency shelving' under CPU contention, where OS-induced scheduling delays are misinterpreted as device latency..." (p.1)

> [!quote]- 📄 원문 표현 (latency shelving)
> "Because these methods cannot differentiate between the two, they misinterpret oversleep as elevated device latency. Consequently, they incorrectly inflate the sleep target for subsequent I/Os, locking the system into a state of persistent excessive sleeping, which we term 'latency shelving.'" (p.4)

## 핵심 통찰

> [!note]- 통찰 1: 실제 I/O 시간을 재지 말고 binary sleep 결과만 쓰자
> 기존 방법은 측정한 I/O latency 평균/최소에 의존해 oversleep(=scheduling delay)을 device latency로 오인한다. PAS는 poll 직후 I/O가 아직 미완료면 UNDER, 완료됐으면 OVER라는 *이진 신호*만 device로부터 얻는다. 이 신호는 oversleep을 정확히 OVER로 분류하므로 latency shelving을 원천 차단하고, lower envelope를 추종할 수 있다.

> [!note]- 통찰 2: 최근 2개 결과쌍으로 envelope 교차를 감지
> 직전(sr_pnlt)과 마지막(sr_last) 두 결과의 조합 4가지로 상황을 구분한다. (UNDER,UNDER)=아직 짧음→키운다, (OVER,OVER)=아직 김→줄인다, (UNDER,OVER)=아래에서 envelope 교차→약간 줄임, (OVER,UNDER)=위에서 교차→약간 키움. epoch을 기다리지 않고 per-I/O로 즉각 반응한다.

> [!note]- 통찰 3: timer failure를 device slowdown과 구분하는 신호로 활용
> CPU 과부하 시 OS scheduler가 제때 깨우지 못해 sleep 시간과 무관하게 oversleep이 발생한다. PAS는 이를 OVER로 보고 sleep을 공격적으로 줄이는데, 지속되면 sleep이 0으로 붕괴하는 "timer failure"(고비용 busy-wait) 상태가 된다. DPAS는 sleep 시간이 0으로 수렴하는 PAS의 특징적 행동 자체를 CPU 경합 탐지 신호로 삼아 모드를 전환한다.

## 설계 / 메커니즘

> [!abstract]- PAS: I/O Latency Tracking (per-bucket 곱셈 조정)
> - LHP와 동일하게 I/O를 size·read/write로 16 bucket에 분류하고, 각 bucket마다 control 변수(adjust, duration, UP, DN, sr_pnlt, sr_last)를 둔다 (bucket당 37 byte, core당 592 byte; p.7).
> - 워크플로(Fig.3): ① 초기화(sr_pnlt=OVER, sr_last=UNDER, duration=0.1µs) → ② submit_bio → ③ 두 결과쌍으로 adjust 갱신: case1(UNDER,UNDER) adjust+=UP / case2(OVER,OVER) adjust-=DN / case3(UNDER,OVER) adjust=1-DN / case4(OVER,UNDER) adjust=1+UP → ④ duration=duration·adjust → ⑤ hrtimer로 sleep → ⑥ nvme_poll_cq로 binary 결과 ret 획득 → ⑦ sr_pnlt=sr_last, sr_last=ret.
> - **설정 공간 탐색**: PAS-Sim(spreadsheet 시뮬)으로 (UP,DN) 조합별 T_under(폴링 CPU 오버헤드 추정)·T_over(oversleep 성능 패널티) 계산. T_over<0.05인 셀들 중 T_under 최소인 **(UP,DN)=(0.01,0.1)**, 즉 DN/UP=10을 baseline으로 선정 (Table 1, p.5). DN/UP ratio가 4 이상이면 T_over는 거의 변하지 않고 T_under만 증가 (Fig.6, p.6).

> [!abstract]- PAS 확장: 동적 민감도 + 동시 I/O 지원
> - **Dynamic sensitivity**(Fig.7, p.6): 두 결과가 같으면(UNDER,UNDER 또는 OVER,OVER) 민감도가 낮다고 보고 UP,DN을 (1+HEATUP)배로 키우고, 다르면(교차) (1−COOLDN)배로 줄인다. UP:DN=1:10 고정, UP∈[0.001,0.01]. 실험에서 (HEATUP,COOLDN)=(0.05,0.1) 사용.
> - **Concurrent I/O 문제**: I/O 직렬화 가정이 깨지면 stale한 sleep 결과로 oversleep 누적/방향 전환 누락이 생긴다 (Fig.9a). **해결**: per-core mode로 코어마다 별도 PAS 변수 집합(메모리 비용↑, core당 +100 byte 전역 변수). 같은 duration을 쓰는 I/O 중 *가장 먼저 완료된 것만* 결과를 제출하고, 새 결과 후 *첫 I/O만* duration을 갱신 (Fig.7 ③⑨, p.7).

> [!abstract]- DPAS: Dynamic Mode Switching (3-mode FSM)
> - **PAS의 두 한계**: ① kernel timer가 indirect task-switching(cache eviction 등)으로 latency를 늘려 hybrid polling이 classic polling보다 느릴 수 있음(Optane에서 4% 저하). ② CPU 과부하 시 timer failure로 sleep이 0으로 붕괴해 LHP/interrupt보다도 나빠짐.
> - **모드 전환 FSM**(Fig.10, p.7): Classic Polling ↔ PAS-normal ↔ PAS-overloaded ↔ Interrupt.
>   - PAS-normal에서 N_PAS(=100) I/O마다 평균 QD 측정 → QD=1(단일 스레드)이면 **Classic Polling**으로 전환. N_CP(=1000) I/O 후 자동 복귀.
>   - PAS-normal에서 N_PAS 동안 timer failure ≥1회면 **PAS-overloaded**로. QD가 θ 초과면 N_INT(=10000) 동안 **Interrupt**로 전환 후 복귀. QD=1로 떨어지면 PAS-normal 복귀.
>   - θ: NAND flash=1, 3D XPoint=3. N_INT를 N_CP의 10배로 둬 성급한 복귀(busy-wait) 방지.
> - **구현**: Linux multi-queue block layer 수정. 9개 파일, +1224/−30 lines. EHP처럼 CPU당 두 device queue(polled/interrupt) 사용.

## 평가

> [!success]- 환경 & 디바이스
> Intel Xeon Gold 6230 (20코어, 2.10GHz), 192GB DDR4, Ubuntu 18.04 / kernel 5.18, hyper-threading off. 디바이스: Intel Optane DC P5800X(3D XPoint), Samsung 983 ZET(Z-NAND, SLC), SK hynix P41(TLC NAND). 비교 대상: INT, CP, LHP, EHP, PAS, DPAS. FIO direct + pvsync2 hipri (p.8).

> [!success]- 마이크로벤치마크 (4KB random read)
> - **Thread scalability**(Fig.11): Optane에서 CP가 INT 대비 read IOPS 최대 30%↑(write 27%↑)지만 8스레드 이상에선 saturation으로 이점 소멸. PAS는 hybrid polling 중 최저 CPU 사용 — LHP 대비 **평균 21 percentage point 절감** (p.8).
> - **Parameter sensitivity**(Fig.12): N_CP=1000이 throughput 대부분을 확보하며 반응성 유지. N_INT=100은 mode thrashing으로 0.84×까지 저하, N_INT=10000에서 안정(0.97×).
> - **Larger I/O**(Fig.13): I/O 크기↑면 polling 이점 감소. 128KB read에서 EHP는 INT 대비 IOPS 이득 없음. P41 128KB에서 DPAS는 INT보다 약 1% IOPS↓(99.95th-pct latency 증가 동반).

> [!success]- 실제 트레이스 & 매크로(YCSB/RocksDB)
> - **Trace replay**(Fig.14, SNIA IOTTA: Baleen/Systor'17/Slacker): DPAS가 전 디바이스·워크로드에서 near-best, P41에서는 CP를 능가.
> - **YCSB thread scalability**(Fig.15): 8스레드 이하에서 LHP/EHP/PAS가 INT 대비 OPS 5~8%↑, CP/DPAS는 추가 이득.
> - **CPU contention**(Fig.16, 4 CPU, 2~32 thread): 스레드>CPU면 CP의 OPS 급락. DPAS는 timer-failure 알림에 반응해 interrupt mode로 전환, Optane·ZSSD 16스레드까지 INT 우위.
> - **DPAS mode breakdown**(Fig.17): Optane은 평균 QD가 θ(=3) 미만이라 interrupt 미전환, PAS-overloaded로 INT 대비 OPS **12.3%↑**. ZSSD는 PAS-normal/CP 위주 + 약 2% interrupt로 INT 대비 **10.3%↑** (p.10).
> - **I/O interference**(Fig.18, pulse generator): PAS·DPAS는 40~640ms pulse 전 구간에서 OPS 안정. LHP는 빠른 디바이스에서 저하, EHP는 160~640ms에서 OPS 급락.
> - **Latency**(Fig.19, YCSB B, 80ms pulse): ZSSD에서 CP의 UPDATE 99.99th-pct·max latency가 INT 대비 17×·30×. DPAS는 간섭 중 최대 90% I/O를 classic-polling으로 처리.
> - **Varying CPU+I/O contention**(Fig.21, 4 CPU): DPAS가 INT 대비 평균 OPS Optane 9%·ZSSD 7%·P41 5%↑.
> - **무튜닝 일반화**(Fig.22, 추가 8 NAND + 1 XPoint): DPAS가 모든 디바이스에서 타 방법 능가; CP/LHP/EHP는 일부 SSD에서 INT보다 저조.
> - **에너지**(Fig.23): CPU 경합이 클 때 CP가 실행 시간이 길어 에너지 최다 소비; 경합 감소 시 격차 축소.

## 섹션 노트
- **§1 Introduction**: 인터럽트/폴링/하이브리드 폴링의 trade-off, PAS·DPAS 기여 요약.
- **§2 Background**: hybrid polling 문제 정의(under/oversleep, Fig.1), Linux LHP(16 bucket, 100ms epoch, ½·mean 50% attenuation), 대안(HyPI=offline profiling, EHP=min+10ms epoch, LinnOS=fast/slow 이진 예측), latency shelving(Fig.2), mode switching 선행(Select-ISR, CINT, vIC).
- **§3 Proposed Method**: PAS latency tracking(§3.1), PAS-Sim(§3.2), dynamic sensitivity(§3.3), concurrent I/O(§3.4), DPAS mode switching(§3.5), 구현(§3.6).
- **§4 Evaluation**: setup, microbench, trace, macrobench, interference, energy.
- **§5 Discussion**: 각 방법 trade-off, future — interrupt coalescing(vIC식 동적 urgency), CPU-saving mode.
- **§6 Related Work**: polling 기원(networking→storage), 적응형 polling-interval(NVMe), hybrid polling 계열.
- **§7 Conclusion** / **Appendix A**: Artifact(github.com/DongDongJu/DPAS_FAST26, ACM Available/Functional/Reproduced).

## 핵심 용어
- **Hybrid polling**: I/O 제출 후 일정 시간 sleep했다가 깨어나 완료 큐를 폴링하는 완료 방식. 인터럽트의 wake-up context-switch와 순수 폴링의 CPU 독점을 절충.
- **Undersleeping / Oversleeping**: sleep이 너무 짧아 폴링 구간이 길어 CPU 낭비 / sleep이 너무 길어 I/O 완료 후에야 깨어나 앱 지연 증가.
- **Latency shelving**: oversleep(특히 OS scheduling delay)을 device latency 상승으로 오인해 sleep 목표를 계속 부풀려 과도 sleep 상태에 고착되는 현상.
- **Lower envelope**: 연속 변동하는 I/O latency 값들의 하단 포락선. PAS가 추종 목표로 삼는 값.
- **Binary sleep result (UNDER/OVER)**: poll 시점에 I/O 미완료면 UNDER, 완료면 OVER. 실제 I/O 시간 대신 device가 주는 이진 신호.
- **adjust / UP / DN / duration**: sleep 시간 곱셈 인자(adjust), 증분(UP)·감분(DN) 파라미터, 현재 sleep 시간(duration). next duration = duration·adjust.
- **HEATUP / COOLDN**: 민감도 동적 조정 계수. 결과 일치 시 UP·DN을 (1+HEATUP)배 확대, 교차 시 (1−COOLDN)배 축소.
- **Timer failure**: CPU 과부하로 hrtimer가 제때 깨우지 못해 PAS sleep이 0으로 붕괴, 고비용 busy-wait가 되는 상태. DPAS의 모드 전환 트리거.
- **QD (queue depth) / θ**: 평균 I/O 큐 깊이 / interrupt 모드 전환 임계값(NAND=1, XPoint=3).
- **N_PAS / N_CP / N_INT**: 모드별 측정·유지 I/O 수(100 / 1000 / 10000).

## 강점 · 한계 · 열린 질문
**강점**
- 실제 I/O 시간 측정 의존을 버리고 binary 신호만 사용해 latency shelving을 구조적으로 제거, OS delay와 device delay를 분리.
- per-I/O 곱셈 조정으로 epoch 지연 없이 abrupt latency 변화에 즉각 반응.
- timer failure를 *버그가 아니라 신호*로 활용한 모드 전환 — per-device 튜닝 없이 8 NAND+XPoint에서 일관 우위.
- 충실한 artifact(ACM 3 배지) 및 광범위한 디바이스·워크로드 검증.

**한계**
- per-core mode는 메모리 비용 증가; CPU 수 > device queue 수면 queue 공유로 성능 저하(OS 변경 필요).
- 큰 I/O(128KB) 및 P41 일부에서 INT 대비 IOPS·tail latency 약간 열세.
- interrupt coalescing 미지원 — interrupt mode에서 동시 I/O가 interrupt storm 유발 가능.
- θ 등 일부 파라미터는 매체 유형(NAND/XPoint)별로 다른 값을 수동 지정.

**열린 질문**
- 동적 urgency 임계 기반 interrupt coalescing을 결합하면 throughput-heavy 워크로드까지 확장 가능한가?
- 다중 스레드가 한 CPU를 공유할 때 per-core mode의 잔여 concurrent I/O 처리가 충분히 견고한가?
- θ를 런타임에 자동 학습할 수 있는가?

## ❓ Q&A (자가 점검)

> [!question]- Q1. PAS가 기존 hybrid polling과 근본적으로 다른 점은?
> 답: 기존(LHP/EHP/HyPI)은 측정한 실제 I/O latency 통계(평균·최소)로 sleep을 정한다. PAS는 실제 시간을 재지 않고, poll 시점의 binary 결과(UNDER/OVER)만으로 sleep을 곱셈식으로 조정한다. 이로써 oversleep(scheduling delay)을 device latency로 오인하는 latency shelving을 차단한다.

> [!question]- Q2. PAS는 최근 몇 개의 sleep 결과를 쓰며 어떻게 조합하는가?
> 답: 직전(sr_pnlt)과 마지막(sr_last) 2개. 4가지 case로 adjust를 갱신한다 — (UNDER,UNDER) adjust+=UP, (OVER,OVER) adjust-=DN, (UNDER,OVER) adjust=1-DN, (OVER,UNDER) adjust=1+UP. 같은 부호면 같은 방향 가감속, 다른 부호면 envelope 교차로 보고 1 기준으로 미세 조정한다.

> [!question]- Q3. baseline (UP,DN)=(0.01,0.1)은 어떻게 선정됐나?
> 답: PAS-Sim으로 (UP,DN) 조합별 T_over(oversleep 패널티)와 T_under(폴링 CPU 오버헤드)를 계산하고, T_over<0.05인 셀들 중 T_under가 최소인 셀을 골랐다(Table 1). DN/UP=10이 최적이며, DN/UP≥4부터 T_over는 거의 안정되고 T_under만 늘어난다.

> [!question]- Q4. timer failure란 무엇이고 DPAS는 이를 어떻게 활용하나?
> 답: CPU 과부하로 hrtimer가 제때 깨우지 못해 PAS가 sleep을 0까지 줄여 고비용 busy-wait가 되는 상태다. DPAS는 sleep이 0으로 붕괴하는 PAS의 특징적 행동을 CPU 경합 탐지 신호로 삼아, N_PAS 동안 timer failure가 보이면 PAS-overloaded로, QD>θ면 interrupt mode로 전환한다.

> [!question]- Q5. DPAS의 네 가지 모드와 주요 전환 조건은?
> 답: Classic Polling, PAS-normal, PAS-overloaded, Interrupt. PAS-normal에서 QD=1이면 classic polling(N_CP=1000 후 복귀), timer failure면 PAS-overloaded, QD>θ면 interrupt(N_INT=10000 후 복귀). QD=1로 떨어지면 PAS-normal 복귀.

> [!question]- Q6. concurrent I/O가 PAS를 어떻게 망가뜨리며 해결책은?
> 답: I/O 직렬화 가정이 깨지면 아직 미완료인 stale sleep 결과를 참조해 oversleep 누적·방향전환 누락이 생긴다. 해결은 (1) per-core mode로 코어마다 별도 변수, (2) 같은 duration I/O 중 먼저 끝난 것만 결과 제출, (3) 새 결과 후 첫 I/O만 duration 갱신.

> [!question]- Q7. 핵심 수치 성과는?
> 답: 4KB random read에서 PAS는 LHP 대비 CPU를 21 percentage point 절감. CPU 경합+I/O 간섭 동시 상황에서 DPAS는 INT 대비 YCSB OPS를 Optane 9%·ZSSD 7%·P41 5% 향상(Fig.21). mode breakdown에서 Optane 12.3%↑, ZSSD 10.3%↑(Fig.17).

> [!question]- Q8. DPAS가 INT보다 불리한 경우는?
> 답: 큰 I/O(128KB)에서는 polling 이점이 사라져 P41 128KB read에서 DPAS가 INT보다 약 1% IOPS 낮고 99.95th-pct latency가 증가한다. 또 N_INT가 너무 작으면(=100) mode thrashing으로 0.84×까지 저하한다.

## 🔗 Connections
[[File System]] · [[FAST]] · [[2026]]

## References worth following
- [16] A. Eisenman et al., *Reducing DRAM footprint with NVM in Facebook*, EuroSys 2018 — Linux Hybrid Polling(LHP)의 기반.
- [26] G. Shin, S. Shin, J. Jeong, *Efficient hybrid polling for ultra-low latency storage devices*, JSA 2022 — EHP(min + 짧은 epoch).
- [34] Y. Song, Y. I. Eom, *HyPI: Reducing CPU consumption of the I/O completion method*, IMCOM 2019 — size별 attenuation rate.
- [38] J. Yang, D. B. Minturn, F. Hady, *When poll is better than interrupt*, FAST 2012 — polling 기반 완료의 출발점.
- [35] A. Tai et al., *Optimizing storage performance with calibrated interrupts*, OSDI 2021 — CINT, interrupt coalescing 동기.
- [18] M. Hao et al., *LinnOS: Predictability on unpredictable flash storage with a light neural network*, OSDI 2020 — fast/slow 이진 예측.

## Personal annotations
<본인 메모 영역>
