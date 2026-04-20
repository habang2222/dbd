using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using UnityEngine;

public sealed class ServerBackupService : MonoBehaviour
{
    private const float BackupIntervalSeconds = 30f;
    private const int MaxBackupFiles = 12;
    private const string BackupFolderName = "server_backups";
    private const string LatestBackupFileName = "latest_server_backup.json";
    private const string BackupStatusFileName = "backup_status.json";
    private const string BackupCommandFileName = "backup_command.txt";
    private const string MapPlanStatusFileName = "map_plan_status.json";
    private const string CleanShutdownKey = "DBD.ServerBackupCleanShutdown";

    private float nextBackupAt;
    private bool restoring;
    private string lastBackupContentHash;
    private string lastBackupCommandToken;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateOnSceneLoad()
    {
        if (FindFirstObjectByType<ServerBackupService>() != null)
        {
            return;
        }

        GameObject serviceObject = new GameObject("Server Backup Service");
        DontDestroyOnLoad(serviceObject);
        serviceObject.AddComponent<ServerBackupService>();
    }

    public static void RequestImmediateBackup(string reason)
    {
        ServerBackupService service = FindFirstObjectByType<ServerBackupService>();
        if (service != null)
        {
            service.WriteBackup(reason);
        }
    }

    private void Awake()
    {
        bool previousSessionWasClean = PlayerPrefs.GetInt(CleanShutdownKey, 1) == 1;
        PlayerPrefs.SetInt(CleanShutdownKey, 0);
        PlayerPrefs.Save();
        nextBackupAt = Time.unscaledTime + 5f;
        lastBackupContentHash = TryReadLatestBackupHash();
        Application.quitting += HandleApplicationQuitting;

        if (!previousSessionWasClean && File.Exists(GetLatestBackupPath()))
        {
            StartCoroutine(RestoreLatestBackupAfterSceneBoot());
        }
    }

    private void OnDestroy()
    {
        Application.quitting -= HandleApplicationQuitting;
    }

    private void Update()
    {
        CheckBackupCommand();

        if (Time.unscaledTime < nextBackupAt)
        {
            return;
        }

        WriteBackup("interval");
        nextBackupAt = Time.unscaledTime + BackupIntervalSeconds;
    }

    private void OnApplicationPause(bool paused)
    {
        if (paused)
        {
            WriteBackup("application_pause");
        }
    }

    private void HandleApplicationQuitting()
    {
        WriteBackup("application_quit");
        PlayerPrefs.SetInt(CleanShutdownKey, 1);
        PlayerPrefs.Save();
    }

    private void WriteBackup(string reason)
    {
        if (restoring)
        {
            return;
        }

        try
        {
            ServerBackupSnapshot snapshot = ServerBackupSnapshot.Capture(reason);
            string json = JsonUtility.ToJson(snapshot, true);
            string contentHash = ComputeSnapshotContentHash(snapshot);
            string folder = GetBackupFolder();
            Directory.CreateDirectory(folder);

            if (!string.IsNullOrEmpty(lastBackupContentHash) && lastBackupContentHash == contentHash)
            {
                WriteBackupStatus(folder, reason, contentHash, false, "unchanged");
                return;
            }

            string stamp = DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff");
            string backupPath = Path.Combine(folder, "server_backup_" + stamp + ".json");
            WriteAllTextAtomic(backupPath, json);
            WriteAllTextAtomic(Path.Combine(folder, LatestBackupFileName), json);
            lastBackupContentHash = contentHash;
            WriteBackupStatus(folder, reason, contentHash, true, "written");
            PruneOldBackups(folder);
        }
        catch (Exception exception)
        {
            Debug.LogWarning("Server backup failed: " + exception.Message);
        }
    }

    private static void WriteAllTextAtomic(string path, string contents)
    {
        string tempPath = path + ".tmp";
        File.WriteAllText(tempPath, contents);
        if (File.Exists(path))
        {
            File.Replace(tempPath, path, null);
        }
        else
        {
            File.Move(tempPath, path);
        }
    }

    private static void PruneOldBackups(string folder)
    {
        DirectoryInfo directory = new DirectoryInfo(folder);
        FileInfo[] files = directory.GetFiles("server_backup_*.json");
        Array.Sort(files, (left, right) => right.CreationTimeUtc.CompareTo(left.CreationTimeUtc));

        for (int i = MaxBackupFiles; i < files.Length; i++)
        {
            files[i].Delete();
        }
    }

    private static string GetBackupFolder()
    {
        return Path.Combine(Application.persistentDataPath, BackupFolderName);
    }

    private static string GetLatestBackupPath()
    {
        return Path.Combine(GetBackupFolder(), LatestBackupFileName);
    }

    private static string GetBackupCommandPath()
    {
        return Path.Combine(Application.persistentDataPath, BackupCommandFileName);
    }

    private void CheckBackupCommand()
    {
        string path = GetBackupCommandPath();
        if (!File.Exists(path))
        {
            return;
        }

        try
        {
            string token = File.ReadAllText(path).Trim();
            if (string.IsNullOrWhiteSpace(token) || token == lastBackupCommandToken)
            {
                return;
            }

            lastBackupCommandToken = token;
            WriteBackup("external_update_request");
        }
        catch (Exception exception)
        {
            Debug.LogWarning("Server backup command failed: " + exception.Message);
        }
    }

    private static string TryReadLatestBackupHash()
    {
        try
        {
            string path = GetLatestBackupPath();
            if (!File.Exists(path))
            {
                return null;
            }

            ServerBackupSnapshot snapshot = JsonUtility.FromJson<ServerBackupSnapshot>(File.ReadAllText(path));
            return snapshot != null ? ComputeSnapshotContentHash(snapshot) : null;
        }
        catch (Exception exception)
        {
            Debug.LogWarning("Server backup hash read failed: " + exception.Message);
            return null;
        }
    }

    private static string ComputeSnapshotContentHash(ServerBackupSnapshot snapshot)
    {
        string reason = snapshot.reason;
        string utcTime = snapshot.utcTime;
        snapshot.reason = string.Empty;
        snapshot.utcTime = string.Empty;

        string stableJson = JsonUtility.ToJson(snapshot, false);

        snapshot.reason = reason;
        snapshot.utcTime = utcTime;

        using SHA256 sha = SHA256.Create();
        byte[] bytes = sha.ComputeHash(Encoding.UTF8.GetBytes(stableJson));
        StringBuilder builder = new StringBuilder(bytes.Length * 2);
        foreach (byte value in bytes)
        {
            builder.Append(value.ToString("x2"));
        }

        return builder.ToString();
    }

    private static void WriteBackupStatus(string folder, string reason, string contentHash, bool wroteSnapshot, string status)
    {
        ServerBackupStatus backupStatus = new ServerBackupStatus
        {
            reason = reason,
            status = status,
            utcTime = DateTime.UtcNow.ToString("O"),
            wroteSnapshot = wroteSnapshot,
            contentHash = contentHash
        };

        WriteAllTextAtomic(Path.Combine(folder, BackupStatusFileName), JsonUtility.ToJson(backupStatus, true));
    }

    private IEnumerator RestoreLatestBackupAfterSceneBoot()
    {
        restoring = true;
        yield return null;
        yield return null;

        ServerBackupSnapshot snapshot = null;
        try
        {
            string json = File.ReadAllText(GetLatestBackupPath());
            snapshot = JsonUtility.FromJson<ServerBackupSnapshot>(json);
        }
        catch (Exception exception)
        {
            Debug.LogWarning("Server backup restore failed: " + exception.Message);
        }

        if (snapshot != null)
        {
            DestroyExistingRuntimeObjects();
            yield return null;
            RestoreSnapshot(snapshot);
            Debug.Log("Restored server backup from " + snapshot.utcTime);
        }

        restoring = false;
        WriteBackup("post_restore");
    }

    private static void RestoreSnapshot(ServerBackupSnapshot snapshot)
    {
        ApplySnapshot(snapshot);
    }

    public static void ApplySnapshot(ServerBackupSnapshot snapshot)
    {
        if (snapshot == null)
        {
            return;
        }

        RestorePeople(snapshot.people);
        RestoreEnemies(snapshot.enemies);
        RestoreResources(snapshot.resources);
        RefreshRuntimeUi();
    }

    public static void ReplaceWorldFromSnapshot(ServerBackupSnapshot snapshot)
    {
        if (snapshot == null)
        {
            return;
        }

        DestroyExistingRuntimeObjects();
        ApplySnapshot(snapshot);
    }

    private static void DestroyExistingRuntimeObjects()
    {
        foreach (PersonComponent person in UnityEngine.Object.FindObjectsByType<PersonComponent>(FindObjectsSortMode.None))
        {
            if (person != null)
            {
                Destroy(person.gameObject);
            }
        }

        foreach (EnemyComponent enemy in UnityEngine.Object.FindObjectsByType<EnemyComponent>(FindObjectsSortMode.None))
        {
            if (enemy != null)
            {
                Destroy(enemy.gameObject);
            }
        }

        foreach (BranchResource resource in UnityEngine.Object.FindObjectsByType<BranchResource>(FindObjectsSortMode.None))
        {
            if (resource != null)
            {
                Destroy(resource.gameObject);
            }
        }
    }

    private static void RestorePeople(List<PersonBackupRecord> people)
    {
        if (people == null)
        {
            return;
        }

        foreach (PersonBackupRecord record in people)
        {
            GameObject personObject = GameObject.CreatePrimitive(PrimitiveType.Cube);
            personObject.name = string.IsNullOrWhiteSpace(record.displayName) ? "Restored Person" : record.displayName;
            personObject.transform.position = record.position;
            personObject.transform.rotation = Quaternion.Euler(record.rotationEuler);
            personObject.transform.localScale = Vector3.one;

            PersonInventory inventory = new PersonInventory();
            if (record.inventory != null)
            {
                foreach (PersonInventoryItem item in record.inventory)
                {
                    if (item != null)
                    {
                        inventory.items.Add(new PersonInventoryItem(item.itemId, item.count));
                    }
                }
            }

            PersonComponent person = personObject.AddComponent<PersonComponent>();
            person.Initialize(record.id, record.displayName, CopyStats(record.stats), inventory);
            person.SetOwnerClient(record.ownerClientId);
            person.GetComponent<PersonOwnerNameplate>()?.Refresh();
            person.SetTeam(record.teamId);
            person.SetUnitStatus(record.state, record.action);
            person.SetSelected(record.selected);
        }
    }

    private static void RestoreEnemies(List<EnemyBackupRecord> enemies)
    {
        if (enemies == null)
        {
            return;
        }

        foreach (EnemyBackupRecord record in enemies)
        {
            GameObject enemyObject = GameObject.CreatePrimitive(PrimitiveType.Sphere);
            enemyObject.name = string.IsNullOrWhiteSpace(record.displayName) ? "Restored Enemy" : record.displayName;
            enemyObject.transform.position = record.position;
            enemyObject.transform.rotation = Quaternion.Euler(record.rotationEuler);
            enemyObject.transform.localScale = record.scale == Vector3.zero ? Vector3.one : record.scale;
            enemyObject.GetComponent<Renderer>().material.color = new Color(0.85f, 0.12f, 0.12f, 1f);

            PersonStats stats = CopyStats(record.stats);
            EnemyComponent enemy = enemyObject.AddComponent<EnemyComponent>();
            enemy.Initialize(record.id, record.displayName, stats.health, stats.strength, stats.stamina);
            enemyObject.AddComponent<EnemyWanderer>().Initialize(enemyObject.transform.position, 12f);
            enemyObject.AddComponent<UnitCombatController>();
            enemyObject.AddComponent<UnitDeathShrink>();
        }
    }

    private static void RestoreResources(List<ResourceBackupRecord> resources)
    {
        if (ResourceRuntimeBootstrap.IsWorldCleared || IsMapClearPending())
        {
            return;
        }

        if (resources == null)
        {
            return;
        }

        foreach (ResourceBackupRecord record in resources)
        {
            GameObject resourceObject = GameObject.CreatePrimitive(PrimitiveType.Cube);
            resourceObject.name = string.IsNullOrWhiteSpace(record.displayName) ? record.objectName : record.displayName;
            resourceObject.transform.position = record.position;
            resourceObject.transform.rotation = Quaternion.Euler(record.rotationEuler);
            resourceObject.transform.localScale = record.scale == Vector3.zero ? Vector3.one : record.scale;

            BranchResource resource = resourceObject.AddComponent<BranchResource>();
            if (record.extraYields != null && record.extraYields.Count > 0)
            {
                resource.ConfigureYields(record.displayName, Mathf.Max(0.05f, record.gatherDuration), record.extraYields);
            }
            else
            {
                resource.Configure(record.itemId, record.displayName, Mathf.Max(0.05f, record.gatherDuration), Mathf.Max(1, record.gatherAmount));
            }
        }
    }

    private static bool IsMapClearPending()
    {
        try
        {
            string path = Path.Combine(Application.persistentDataPath, MapPlanStatusFileName);
            if (!File.Exists(path))
            {
                return false;
            }

            string json = File.ReadAllText(path);
            return json.Contains("\"pending_clear\"", StringComparison.Ordinal)
                || json.Contains("\"cleared\"", StringComparison.Ordinal);
        }
        catch (Exception exception)
        {
            Debug.LogWarning("Map clear status read failed: " + exception.Message);
            return false;
        }
    }

    private static PersonStats CopyStats(PersonStats stats)
    {
        return stats != null
            ? new PersonStats(stats.health, stats.strength, stats.stamina)
            : new PersonStats();
    }

    private static void RefreshRuntimeUi()
    {
        if (UnitListPanel.Instance != null)
        {
            UnitListPanel.Instance.RefreshList();
        }

        if (OwnedItemsWindow.Instance != null)
        {
            OwnedItemsWindow.Instance.Refresh();
        }

        if (ActionWindow.Instance != null)
        {
            ActionWindow.Instance.RefreshForSelectedPerson();
        }
    }
}

[Serializable]
public sealed class ServerBackupSnapshot
{
    public string schemaVersion = "1";
    public string reason;
    public string utcTime;
    public string serverState;
    public int worldSeed;
    public List<PersonBackupRecord> people = new();
    public List<EnemyBackupRecord> enemies = new();
    public List<ResourceBackupRecord> resources = new();

    public static ServerBackupSnapshot Capture(string reason)
    {
        ServerBackupSnapshot snapshot = new ServerBackupSnapshot
        {
            reason = reason,
            utcTime = DateTime.UtcNow.ToString("O"),
            serverState = ServerWorldStateService.GetStateName(),
            worldSeed = EnvironmentRuntimeBootstrap.GetWorldSeed()
        };

        foreach (PersonComponent person in UnityEngine.Object.FindObjectsByType<PersonComponent>(FindObjectsSortMode.None))
        {
            snapshot.people.Add(PersonBackupRecord.Capture(person));
        }

        foreach (EnemyComponent enemy in UnityEngine.Object.FindObjectsByType<EnemyComponent>(FindObjectsSortMode.None))
        {
            snapshot.enemies.Add(EnemyBackupRecord.Capture(enemy));
        }

        foreach (BranchResource resource in UnityEngine.Object.FindObjectsByType<BranchResource>(FindObjectsSortMode.None))
        {
            if (resource != null && resource.gameObject.activeInHierarchy)
            {
                snapshot.resources.Add(ResourceBackupRecord.Capture(resource));
            }
        }

        return snapshot;
    }
}

[Serializable]
public sealed class ServerBackupStatus
{
    public string reason;
    public string status;
    public string utcTime;
    public bool wroteSnapshot;
    public string contentHash;
}

[Serializable]
public sealed class PersonBackupRecord
{
    public string id;
    public string displayName;
    public string ownerClientId;
    public string ownerNickname;
    public string teamId;
    public string state;
    public string action;
    public bool selected;
    public Vector3 position;
    public Vector3 rotationEuler;
    public PersonStats stats;
    public List<PersonInventoryItem> inventory = new();

    public static PersonBackupRecord Capture(PersonComponent person)
    {
        PersonBackupRecord record = new PersonBackupRecord
        {
            id = person.PersonId,
            displayName = person.PersonName,
            ownerClientId = person.OwnerClientId,
            ownerNickname = PlayerProfileService.GetNicknameForOwner(person.OwnerClientId),
            teamId = person.TeamId,
            state = person.CurrentState,
            action = person.CurrentAction,
            selected = person.IsSelected,
            position = person.transform.position,
            rotationEuler = person.transform.rotation.eulerAngles,
            stats = CopyStats(person.Stats)
        };

        if (person.Inventory != null && person.Inventory.items != null)
        {
            foreach (PersonInventoryItem item in person.Inventory.items)
            {
                if (item != null)
                {
                    record.inventory.Add(new PersonInventoryItem(item.itemId, item.count));
                }
            }
        }

        return record;
    }

    private static PersonStats CopyStats(PersonStats stats)
    {
        return stats != null
            ? new PersonStats(stats.health, stats.strength, stats.stamina)
            : new PersonStats();
    }
}

[Serializable]
public sealed class EnemyBackupRecord
{
    public string id;
    public string displayName;
    public Vector3 position;
    public Vector3 rotationEuler;
    public Vector3 scale;
    public PersonStats stats;

    public static EnemyBackupRecord Capture(EnemyComponent enemy)
    {
        return new EnemyBackupRecord
        {
            id = enemy.EnemyId,
            displayName = enemy.EnemyName,
            position = enemy.transform.position,
            rotationEuler = enemy.transform.rotation.eulerAngles,
            scale = enemy.transform.localScale,
            stats = new PersonStats(enemy.Health, enemy.Strength, enemy.Stamina)
        };
    }
}

[Serializable]
public sealed class ResourceBackupRecord
{
    public string objectName;
    public string itemId;
    public string displayName;
    public float gatherDuration;
    public int gatherAmount;
    public Vector3 position;
    public Vector3 rotationEuler;
    public Vector3 scale;
    public List<PersonInventoryItem> extraYields = new();

    public static ResourceBackupRecord Capture(BranchResource resource)
    {
        ResourceBackupRecord record = new ResourceBackupRecord
        {
            objectName = resource.gameObject.name,
            itemId = resource.ItemId,
            displayName = resource.DisplayName,
            gatherDuration = resource.GatherDuration,
            gatherAmount = resource.GatherAmount,
            position = resource.transform.position,
            rotationEuler = resource.transform.rotation.eulerAngles,
            scale = resource.transform.localScale
        };

        if (resource.ExtraYields != null)
        {
            foreach (PersonInventoryItem item in resource.ExtraYields)
            {
                if (item != null)
                {
                    record.extraYields.Add(new PersonInventoryItem(item.itemId, item.count));
                }
            }
        }

        return record;
    }
}
