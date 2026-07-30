# SMS (Spatial Memory Streaming)

## 분류
- **HW/SW**: HW — AGT(Active Generation Table)와 PHT(Pattern History Table)라는 전용 온칩 하드웨어 구조로 패턴을 학습·예측하는 practical on-chip hardware technique이다.
- **offline/online**: online — 런타임에 spatial region generation을 관찰하며 패턴을 학습하고, trigger access 시점에 즉시 예측·스트리밍한다.
- **PC-localized / non-PC-localized**: PC-localized — trigger access의 PC와 spatial region offset을 결합한 인덱스(PC+offset)로 spatial pattern을 correlate·저장한다.

## 핵심 아이디어
- **메모리 패턴 인식 방식**: 큰 메모리 영역(수 kB)에서 반복되는 비연속 접근을 spatial region 단위 bit-vector 패턴으로 포착하고, 이를 코드(PC+offset)와 code-correlate한다.
- **작동 원리**: generation 동안 AGT가 접근된 블록들을 패턴으로 기록해 PHT에 저장하고, 이후 같은 PC+offset의 trigger access가 오면 PHT에서 패턴을 꺼내 예측 블록들을 primary cache로 미리 스트리밍한다.
