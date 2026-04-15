using System.Collections;

public static class GatherCommandService
{
    public static IEnumerator Gather(PersonComponent person, BranchResource resource)
    {
        if (resource == null)
        {
            yield break;
        }

        if (!GameAuthority.CanIssueCommand(person))
        {
            string reason = GameAuthority.GetCommandRejectReason(person);
            if (person != null && !GameAuthority.IsOwnedByLocalClient(person))
            {
                AntiCheatService.Punish(person, reason);
            }

            yield break;
        }

        yield return resource.GatherWith(person);
    }
}
