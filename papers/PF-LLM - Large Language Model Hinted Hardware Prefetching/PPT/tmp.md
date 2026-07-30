prefetching이 무엇인지 설명해주고

각각의 prefetcher가 무엇이 있는지 정성적으로 말로 풀어서 설명해주고 

기존의 ensemble prefetcher에는 어떤 문제가 있었고

왜 LLM을 사용하게 된 것인지

기존의 access pattern에 대해서 어떤 식으로 하는지 

어떤 차이점이 있는지 

----------------------
abstract에서는 
논문의 모든 부분을 다 설명하는 것

Introduction에서는 
논문에서 중요하지 않은 background를 간략하게 정리해서 설명해주는 느낌이 있음
method가 아님에도 불구하고 key insight에 대해서 소개하는 부분이 존재함


-----------------------
의문점

- introduction
논문에서 
Figure1에서 lock을 도입해서 multi thread환경을 생각함
그런데 실제 실험에서는 어떻게 했는지

multi thread더라도 단일 코어인 상황에서는 coherence가 필요하지 않다

흠이 있어도 아이디어가 좋으면 통과한다


---------------------
그래프에서 나오는 metric의 경우에는 

그래프를 설명하기 직전에 설명하는 것이 좋을 것 같고

사실 전달 목적보다는 본질을 전달하는 방식을 배우고 싶음

사실에 대한 질문이 들어올 경우를 대비해서 backup slide를 준비해오기


---------------------
SPEC2006, SPEC2017를 선택한 점은 기존의 선행연구에서 했던 방식을 따라한 듯


----------------------

lockd이 걸려 있는 부분에 있어서 
no prefetching에 대한 설명을 한다고 했을 때 
false sharing 문제를 언급했다면 해결되었을 문제인듯

정확히 말하면 false sharing이라 할 수 있을까?
false sharing은 같은 cache block 내에서 cpu의 load 연산 단위가 word로 다르기 때문에 발생하는 문제이긴에
여기서 false sharing이라고 언급하는 것은 적절하지 않음
