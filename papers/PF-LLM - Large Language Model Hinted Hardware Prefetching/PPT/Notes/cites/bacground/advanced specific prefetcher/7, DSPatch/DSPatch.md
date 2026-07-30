# DSPatch

## 분류
- **HW/SW**: HW — DRAM bandwidth utilization을 추적하며 실행 중 prefetch를 발행하는 전용 하드웨어 prefetcher(3.6KB storage)로, standalone 또는 SPP의 adjunct로 동작한다.
- **offline/online**: online — 런타임에 관측된 DRAM bandwidth 활용률에 따라 두 bit-pattern 중 하나를 동적으로 선택해 prefetch를 생성한다.
- **PC-localized / non-PC-localized**: non-PC-localized — PC가 아니라 physical page(4KB memory region) 단위로 접근을 spatial bit-pattern으로 추적한다(트리거 접근에 anchoring, PC 미사용).
- **핵심 아이디어**: 
- **메모리 패턴 인식 방식**: 한 physical page 내 접근을 첫 "trigger" 접근에 anchoring한 spatial bit-pattern으로 표현해, reordering으로 달라 보이는 패턴들을 단일 패턴으로 인식한다.
- **작동 원리**: 관측된 anchored bit-pattern을 bitwise OR(coverage-biased)와 AND(accuracy-biased)로 modulate해 두 패턴을 학습하고, DRAM bandwidth 여유가 크면 coverage-biased·포화에 가까우면 accuracy-biased 패턴을 골라 prefetch한다.
