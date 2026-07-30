# Power7

## 분류
- **HW/SW**: HW prefetcher + SW(runtime) 재구성 — POWER7의 전용 hardware data prefetcher engine이 데이터를 fetch하고, 컴파일러가 삽입한 prefetch 명령은 비활성화한 채 OmpSs runtime software가 DSCR(Data Stream Control Register)로 그 HW 엔진의 aggressiveness/depth를 동적으로 재구성한다.
- **offline/online**: online — 실행 중 exploration phase에서 여러 prefetcher 설정을 hardware counter(PAPI)로 평가하고 stable phase에서 최적 설정을 적용하는 runtime 동작이다.
- **PC-localized / non-PC-localized**: non-PC-localized — POWER7 HW prefetcher는 data stream 내의 prefetch stream·stride를 추적하며(스트림 기반), runtime의 설정 선택은 PC가 아니라 SMT thread/OmpSs task type 단위로 이루어진다.
- **핵심 아이디어**: (측정) exploration/stable 두 phase로 나눠 task type별 성능 카운터를 모으고, IPC 기반 ε-임계값으로 power-performance가 최적인 prefetcher 설정을 자동 선택한다.

## 핵심 아이디어
- **메모리 패턴 인식 방식**: POWER7 stream prefetcher가 메모리 접근 스트림에서 고정 stride의 prefetch stream을 감지하며, 어떤 aggressiveness(depth·stride-N·store-stream)가 맞는지는 runtime이 workload별로 탐색해 판별한다.
- **작동 원리**: runtime이 DSCR 32개 설정을 exploration phase에서 시험→hardware counter로 IPC/bandwidth 비교→task type별 최적 설정을 stable phase 동안 프로그래밍하여 HW 엔진을 dynamic reconfigure한다.
