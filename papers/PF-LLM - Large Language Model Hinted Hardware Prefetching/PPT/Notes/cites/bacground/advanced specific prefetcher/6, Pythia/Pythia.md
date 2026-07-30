# Pythia

## 분류
- **HW/SW**: HW — 프리페처를 RL 에이전트로 구현한 전용 하드웨어(테이블 기반, 데스크톱급 프로세서 대비 약 1.03% 면적 오버헤드)이며 워크로드에 소프트웨어 변경이 전혀 없다.
- **offline/online**: online — 매 demand request마다 program feature를 state로 관찰하고 prefetch action에 대한 numerical reward를 받아 런타임에 Q-value를 갱신하는 online reinforcement learning(SARSA) 방식이다.
- **PC-localized / non-PC-localized**: PC-localized — RL의 state로 사용하는 program feature가 PC 기반(예: 기본 구성의 PC+Delta 등)이어서 상태·상관관계를 PC(명령 주소)별로 추적한다(단, feature는 configuration register로 교체 가능해 non-PC feature도 지원).
- **★ 정체성(rule-making)**: "어떤 feature가 유용한지"만 아키텍트가 지정하고 "정확히 어떻게 활용할지"는 RL이 스스로 학습하는 customizable 프레임워크 — 개별 패턴 규칙 대신 학습 규칙(reward)을 설계하는 접근.

## 핵심 아이디어
- **메모리 패턴 인식 방식**: 여러 종류의 program feature(program context)와 memory bandwidth 같은 system-level feedback을 동시에 관찰해, 각 feature-상황이 어떤 prefetch offset과 상관되는지를 강화학습으로 인식한다.
- **작동 원리**: prefetch를 RL 문제로 정식화하여 (state=program features)에서 (action=prefetch offset 또는 no-prefetch)을 취하고, prefetch 품질을 대역폭 사용량까지 반영한 reward로 평가해 Q-value를 갱신함으로써 정확·적시·system-aware한 prefetch를 온라인으로 강화한다.
