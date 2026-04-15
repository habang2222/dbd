using UnityEngine;

public static class MovementCommandService
{
    public static bool TryMove(PersonComponent person, Vector3 destination, bool run, string actionLabel = "특정 지역 이동")
    {
        if (!AntiCheatService.CanAcceptMoveCommand(person, destination, out string reason))
        {
            Debug.LogWarning($"Move command rejected for {(person != null ? person.PersonName : "unknown")}: {reason}");
            return false;
        }

        PersonMover mover = person.GetComponent<PersonMover>();
        if (mover == null)
        {
            mover = person.gameObject.AddComponent<PersonMover>();
        }

        mover.MoveToDestination(destination, run);
        person.SetUnitStatus(run ? "달리는 중" : "이동 중", actionLabel);
        return true;
    }

    public static bool TryStop(PersonComponent person)
    {
        if (person == null)
        {
            return false;
        }

        if (!GameAuthority.CanIssueCommand(person))
        {
            string reason = GameAuthority.GetCommandRejectReason(person);
            if (!GameAuthority.IsOwnedByLocalClient(person))
            {
                AntiCheatService.Punish(person, reason);
            }

            Debug.LogWarning($"Stop command rejected for {person.PersonName}: {reason}");
            return false;
        }

        if (AntiCheatService.IsFrozen(person))
        {
            return false;
        }

        PersonMover mover = person.GetComponent<PersonMover>();
        if (mover != null)
        {
            mover.StopByCommand();
        }

        person.SetUnitStatus("Idle", "None");
        return true;
    }
}
