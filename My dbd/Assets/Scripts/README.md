# Scripts Folder Guide

이 문서는 동아리방에서 프로젝트 코드를 열었을 때 어디부터 보면 되는지 알려 주는 안내서입니다.

## 추천해서 읽는 순서

1. `People/Core/PersonComponent.cs`
   - 사람 한 명의 이름, 능력치, 인벤토리, 선택 상태를 관리합니다.
2. `People/Movement/PersonClickMoveController.cs`
   - 사람을 선택한 뒤 바닥을 우클릭하면 목적지를 정하는 입력 담당입니다.
3. `People/Movement/PersonMover.cs`
   - 실제로 NavMeshAgent를 이용해서 사람을 움직입니다.
4. `People/Runtime/PersonRuntimeBootstrap.cs`
   - 게임을 실행하면 사람, 카메라, 클릭 이동 컨트롤러가 준비되도록 도와줍니다.
5. `Environment/EnvironmentRuntimeBootstrap.cs`
   - 바닥, 장애물, NavMesh를 자동으로 준비합니다.
6. `Crafting/SimpleCraftingTestPanel.cs`
   - 선택된 사람의 인벤토리를 보여 주고 제작 테스트를 합니다.

## People 폴더 구조

- `Core`
  - 사람 오브젝트의 핵심 컴포넌트가 있습니다.
- `Data`
  - 능력치처럼 단순 데이터를 담는 클래스가 있습니다.
- `Inventory`
  - 아이템 목록과 아이템 한 칸을 다루는 클래스가 있습니다.
- `Management`
  - 씬 안의 사람 목록을 관리하는 클래스가 있습니다.
- `Movement`
  - 우클릭 목적지 지정과 실제 이동 코드가 있습니다.
- `Runtime`
  - 게임 실행 시 자동 세팅되는 코드가 있습니다.
- `Spawning`
  - 사람을 새로 만드는 코드가 있습니다.

## 현재 조작

- 사람 선택: 사람 큐브 좌클릭
- 목적지 설정: `Ground` 바닥 우클릭
- 카메라 이동: WASD
- 카메라 확대/축소: 마우스 휠
