using System.Collections.Generic;
using UnityEngine;

public static class PlayerProfileService
{
    private const string NicknameKey = "DBD.PlayerNickname";

    public static string LocalNickname
    {
        get
        {
            string nickname = PlayerPrefs.GetString(NicknameKey, string.Empty).Trim();
            return string.IsNullOrWhiteSpace(nickname) ? "Unknown1" : nickname;
        }
    }

    public static bool HasNickname => !string.IsNullOrWhiteSpace(PlayerPrefs.GetString(NicknameKey, string.Empty));

    public static void SetLocalNickname(string nickname)
    {
        string cleaned = SanitizeNickname(nickname);
        PlayerPrefs.SetString(NicknameKey, cleaned);
        PlayerPrefs.Save();
        RefreshUnitLabels();
        ServerBackupService.RequestImmediateBackup("player_nickname_changed");
    }

    public static string GetNicknameForOwner(string ownerClientId)
    {
        if (ownerClientId == SessionRoleService.PlayerClientId || ownerClientId == GameAuthority.LocalClientId)
        {
            return LocalNickname;
        }

        if (ownerClientId == SessionRoleService.DirectorControlledClientId)
        {
            return "Director";
        }

        if (ownerClientId == SessionRoleService.DirectorClientId)
        {
            return "Director";
        }

        return "Unknown1";
    }

    public static string GetUnitLabel(PersonComponent person)
    {
        if (person == null)
        {
            return "Unknown1/player0";
        }

        return GetNicknameForOwner(person.OwnerClientId) + "/" + person.PersonName;
    }

    public static void RefreshUnitLabels()
    {
        foreach (PersonOwnerNameplate label in Object.FindObjectsByType<PersonOwnerNameplate>(FindObjectsSortMode.None))
        {
            if (label != null)
            {
                label.Refresh();
            }
        }
    }

    private static string SanitizeNickname(string nickname)
    {
        if (string.IsNullOrWhiteSpace(nickname))
        {
            return "Unknown1";
        }

        List<char> chars = new List<char>();
        foreach (char value in nickname.Trim())
        {
            if (!char.IsControl(value))
            {
                chars.Add(value);
            }

            if (chars.Count >= 18)
            {
                break;
            }
        }

        return chars.Count > 0 ? new string(chars.ToArray()) : "Unknown1";
    }
}
