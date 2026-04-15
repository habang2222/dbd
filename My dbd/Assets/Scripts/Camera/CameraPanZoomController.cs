using UnityEngine;
using UnityEngine.EventSystems;

// 카메라를 키보드와 마우스 휠로 움직이는 간단한 컨트롤러입니다.
// WASD: 상하좌우 이동, 마우스 휠: 확대/축소 느낌의 앞뒤 이동
public class CameraPanZoomController : MonoBehaviour
{
    // WASD 이동 속도입니다.
    [SerializeField] private float moveSpeed = 8f;

    // 마우스 휠 확대/축소 속도입니다.
    [SerializeField] private float zoomSpeed = 6f;

    // 카메라가 너무 낮게 내려가지 않도록 막는 최소 높이입니다.
    [SerializeField] private float minHeight = 3f;

    // 카메라가 너무 높게 올라가지 않도록 막는 최대 높이입니다.
    [SerializeField] private float maxHeight = 18f;

    // 매 프레임 입력을 읽어서 카메라 위치를 바꿉니다.
    private void Update()
    {
        // Vector3.zero는 (0, 0, 0), 즉 아직 이동 방향이 없다는 뜻입니다.
        Vector3 move = Vector3.zero;

        if (Input.GetKey(KeyCode.W))
        {
            move += Vector3.forward;
        }

        if (Input.GetKey(KeyCode.S))
        {
            move += Vector3.back;
        }

        if (Input.GetKey(KeyCode.A))
        {
            move += Vector3.left;
        }

        if (Input.GetKey(KeyCode.D))
        {
            move += Vector3.right;
        }

        // sqrMagnitude는 벡터 길이의 제곱입니다.
        // 0보다 크면 어떤 방향키든 눌렸다는 뜻입니다.
        if (move.sqrMagnitude > 0f)
        {
            // normalized는 방향만 남기고 길이를 1로 만든 값입니다.
            // 대각선 이동이 너무 빨라지는 것을 막습니다.
            transform.position += move.normalized * moveSpeed * Time.deltaTime;
        }

        // 마우스 휠 입력입니다. 위로 굴리면 양수, 아래로 굴리면 음수입니다.
        float scroll = Input.mouseScrollDelta.y;
        if (Mathf.Abs(scroll) > 0.01f && !IsPointerOverUi())
        {
            Vector3 currentPosition = transform.position;
            Vector3 zoomDelta = transform.forward * scroll * zoomSpeed;
            Vector3 nextPosition = currentPosition + zoomDelta;
            float clampedHeight = Mathf.Clamp(nextPosition.y, minHeight, maxHeight);

            if (!Mathf.Approximately(nextPosition.y, clampedHeight))
            {
                if (Mathf.Abs(zoomDelta.y) < 0.001f)
                {
                    return;
                }

                float allowedRatio = (clampedHeight - currentPosition.y) / zoomDelta.y;
                if (allowedRatio <= 0f)
                {
                    return;
                }

                nextPosition = currentPosition + (zoomDelta * allowedRatio);
            }

            // y 높이는 minHeight와 maxHeight 사이로 제한합니다.
            nextPosition.y = clampedHeight;
            transform.position = nextPosition;
        }
    }

    private static bool IsPointerOverUi()
    {
        return EventSystem.current != null && EventSystem.current.IsPointerOverGameObject();
    }

    public void FocusOn(Vector3 targetPosition)
    {
        Plane groundPlane = new Plane(Vector3.up, targetPosition);
        Ray centerRay = new Ray(transform.position, transform.forward);
        if (!groundPlane.Raycast(centerRay, out float distance))
        {
            return;
        }

        Vector3 currentCenter = centerRay.GetPoint(distance);
        transform.position += targetPosition - currentCenter;
    }
}
